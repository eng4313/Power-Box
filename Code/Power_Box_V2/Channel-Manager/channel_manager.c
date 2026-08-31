/**
  ******************************************************************************
  * @file    channel_manager.c
  * @brief   See channel_manager.h.
  *
  * All per-locker bookkeeping now lives inside LockerRecordTypeDef itself
  * (see typedef.h) -- door_close_deadline_unix_time, failed_fp_attempts, and
  * the flags union (in_use, phone_type, lockout_active, door_open,
  * opened_by_admin, fault_logged, pending_is_deposit) replace what used to
  * be four separate parallel arrays here. Only in_use, phone_type,
  * lockout_active/lockout_until_unix_time, fingerprint_id, phone_number and
  * deposit_unix_time are persisted by Storage; everything else is RAM-only
  * and safe to lose on reboot (a mid-operation locker simply reverts to its
  * last-persisted state and the customer restarts that step).
  ******************************************************************************
  */

#include "channel_manager.h"
#include "channel_hw.h"
#include "storage.h"
#include "log.h"
#include <string.h>

static LockerRecordTypeDef lockers[LOCKER_COUNT];

static void FinalizeSuccess(uint8_t locker_index, uint32_t unix_time, bool was_deposit);
static void EnterDoorLeftOpenFault(uint8_t locker_index, uint32_t unix_time, bool was_deposit,
                                    uint32_t *out_reminder_bitmask);

System_StatusTypeDef ChannelManager_Init(void)
{
    if (Storage_LoadAllLockers(lockers) != SYS_OK)
    {
        return SYS_ERROR;
    }

    /* Boot-time door reconciliation: Storage only ever persists a locker as
     * EMPTY or OCCUPIED (the transient AWAITING_FAULT states are RAM-only
     * and do not survive a reboot), so seed the live door_open cache from
     * the actual sensor right now rather than leaving it at its
     * zero-initialized "closed" default. This does not by itself change
     * any locker's `state` -- an OCCUPIED locker whose door happens to
     * read open at boot stays OCCUPIED; it is up to the caller (UI/flow
     * layer) to notice flags.bits.door_open on an OCCUPIED locker and
     * decide what to show/announce for that anomaly, since it is not one
     * of the deposit/retrieve timeout faults this module's state machine
     * models. */
    for (uint8_t i = 0U; i < LOCKER_COUNT; i++)
    {
        lockers[i].door_close_deadline_unix_time = 0U;
        lockers[i].failed_fp_attempts            = 0U;
        lockers[i].flags.bits.door_open          = ChannelHW_IsDoorClosed(i) ? 0U : 1U;
        lockers[i].flags.bits.opened_by_admin    = 0U;
        lockers[i].flags.bits.fault_logged       = 0U;
        lockers[i].flags.bits.pending_is_deposit = 0U;

        /* Lock/LED start de-energized/off (ChannelHW_Init() already did
         * this; repeated here so the two stay explicitly consistent). */
        ChannelHW_SetLock(i, false);
        ChannelHW_SetLED(i, false);
    }

    return SYS_OK;
}

/* --------------------------------------------------------------- Deposit */

System_StatusTypeDef ChannelManager_StartDeposit(const char *phone_number, PhoneTypeTypeDef phone_type,
                                                  uint16_t fingerprint_id, uint32_t unix_time,
                                                  uint8_t *out_locker_index)
{
    if ((phone_number == NULL) || (out_locker_index == NULL))
    {
        return SYS_INVALID_PARAM;
    }

    for (uint8_t i = 0U; i < LOCKER_COUNT; i++)
    {
        if (lockers[i].state == LOCKER_STATE_EMPTY)
        {
            strncpy(lockers[i].phone_number, phone_number, PHONE_NUMBER_DIGIT_COUNT);
            lockers[i].phone_number[PHONE_NUMBER_DIGIT_COUNT] = '\0';
            lockers[i].flags.bits.phone_type = (uint8_t)phone_type;
            lockers[i].fingerprint_id        = fingerprint_id;
            lockers[i].deposit_unix_time     = unix_time;
            lockers[i].state                 = LOCKER_STATE_AWAITING_DEPOSIT_CLOSE;
            /* in_use / lockout fields stay as they were (0 / inactive for
             * an EMPTY locker) until FinalizeSuccess() confirms the door closed. */

            lockers[i].door_close_deadline_unix_time = unix_time + TIMEOUT_DOOR_CLOSE_AFTER_DEPOSIT_SEC;
            lockers[i].flags.bits.pending_is_deposit = 1U;
            lockers[i].flags.bits.fault_logged       = 0U;

            ChannelHW_SetLock(i, true);
            ChannelHW_SetLED(i, true);

            *out_locker_index = i;
            return SYS_OK;
        }
    }

    return SYS_FULL;
}

/* -------------------------------------------------------------- Retrieve */

System_StatusTypeDef ChannelManager_FindLockerByPhone(const char *phone_number, uint8_t *out_locker_index)
{
    if ((phone_number == NULL) || (out_locker_index == NULL))
    {
        return SYS_INVALID_PARAM;
    }

    for (uint8_t i = 0U; i < LOCKER_COUNT; i++)
    {
        if (lockers[i].flags.bits.in_use &&
            (strncmp(lockers[i].phone_number, phone_number, PHONE_NUMBER_DIGIT_COUNT) == 0))
        {
            *out_locker_index = i;
            return SYS_OK;
        }
    }

    return SYS_NOT_FOUND;
}

System_StatusTypeDef ChannelManager_IsLockerLockedOut(uint8_t locker_index, uint32_t unix_time,
                                                       bool *out_locked, uint32_t *out_seconds_remaining)
{
    if ((locker_index >= LOCKER_COUNT) || (out_locked == NULL) || (out_seconds_remaining == NULL))
    {
        return SYS_INVALID_PARAM;
    }

    LockerRecordTypeDef *rec = &lockers[locker_index];

    if (rec->flags.bits.lockout_active && (rec->lockout_until_unix_time > unix_time))
    {
        *out_locked            = true;
        *out_seconds_remaining = rec->lockout_until_unix_time - unix_time;
        return SYS_OK;
    }

    if (rec->flags.bits.lockout_active)
    {
        /* Lockout window has expired: clear it and persist, so a future
         * reboot doesn't come back up still "locked" against a stale timestamp. */
        rec->flags.bits.lockout_active = 0U;
        rec->lockout_until_unix_time   = 0U;
        (void)Storage_SaveLocker(locker_index, rec);
    }

    *out_locked            = false;
    *out_seconds_remaining = 0U;
    return SYS_OK;
}

System_StatusTypeDef ChannelManager_RegisterFailedFingerprintAttempt(uint8_t locker_index, uint32_t unix_time,
                                                                      bool *out_locker_now_locked)
{
    if ((locker_index >= LOCKER_COUNT) || (out_locker_now_locked == NULL))
    {
        return SYS_INVALID_PARAM;
    }

    *out_locker_now_locked = false;
    lockers[locker_index].failed_fp_attempts++;

    if (lockers[locker_index].failed_fp_attempts >= RETRIEVE_MAX_FINGERPRINT_ATTEMPTS)
    {
        LockerRecordTypeDef *rec = &lockers[locker_index];

        rec->flags.bits.lockout_active = 1U;
        rec->lockout_until_unix_time   = unix_time + LOCKER_LOCKOUT_DURATION_SEC;

        if (Storage_SaveLocker(locker_index, rec) != SYS_OK)
        {
            return SYS_ERROR;
        }

        (void)Log_Append(LOG_EVENT_LOCKER_LOCKOUT_30MIN, locker_index, rec->phone_number, unix_time);

        lockers[locker_index].failed_fp_attempts = 0U;
        *out_locker_now_locked = true;
    }

    return SYS_OK;
}

void ChannelManager_ClearFailedAttempts(uint8_t locker_index)
{
    if (locker_index >= LOCKER_COUNT)
    {
        return;
    }
    lockers[locker_index].failed_fp_attempts = 0U;
}

System_StatusTypeDef ChannelManager_StartRetrieve(uint8_t locker_index, uint32_t unix_time)
{
    if (locker_index >= LOCKER_COUNT)
    {
        return SYS_INVALID_PARAM;
    }

    LockerRecordTypeDef *rec = &lockers[locker_index];

    if (rec->state != LOCKER_STATE_OCCUPIED)
    {
        return SYS_ERROR;
    }

    rec->state = LOCKER_STATE_AWAITING_RETRIEVE_CLOSE;

    rec->door_close_deadline_unix_time = unix_time + TIMEOUT_DOOR_CLOSE_AFTER_RETRIEVE_SEC;
    rec->flags.bits.pending_is_deposit = 0U;
    rec->flags.bits.fault_logged       = 0U;

    ChannelHW_SetLock(locker_index, true);
    ChannelHW_SetLED(locker_index, true);

    return SYS_OK;
}

/* ------------------------------------------------------------------ Admin */

System_StatusTypeDef ChannelManager_AdminOpenLocker(uint8_t locker_index, uint32_t unix_time)
{
    if (locker_index >= LOCKER_COUNT)
    {
        return SYS_INVALID_PARAM;
    }

    LockerRecordTypeDef *rec = &lockers[locker_index];

    rec->flags.bits.opened_by_admin = 1U; /* RAM-only flag, not persisted -- purely for the log entry below */

    ChannelHW_SetLock(locker_index, true);
    ChannelHW_SetLED(locker_index, true);

    (void)Log_Append(LOG_EVENT_ADMIN_OPENED_LOCKER, locker_index, rec->phone_number, unix_time);

    return SYS_OK;
}

void ChannelManager_AdminCloseLocker(uint8_t locker_index)
{
    if (locker_index >= LOCKER_COUNT)
    {
        return;
    }

    ChannelHW_SetLock(locker_index, false);
    ChannelHW_SetLED(locker_index, false);
}

/* -------------------------------------------------------------- Background */

System_StatusTypeDef ChannelManager_Poll(uint32_t unix_time, uint32_t *out_reminder_bitmask)
{
    if (out_reminder_bitmask == NULL)
    {
        return SYS_INVALID_PARAM;
    }
    *out_reminder_bitmask = 0U;

    for (uint8_t i = 0U; i < LOCKER_COUNT; i++)
    {
        bool door_closed = ChannelHW_IsDoorClosed(i);

        /* Keep the live door-state cache fresh for EVERY locker, EVERY
         * call -- not just the ones this state machine actively times.
         * Lets the UI/admin layer show real door status for OCCUPIED
         * lockers too (e.g. a door propped open) without touching
         * hardware directly and without this module having to decide
         * what, if anything, to do about it. */
        lockers[i].flags.bits.door_open = door_closed ? 0U : 1U;

        LockerStateTypeDef state = lockers[i].state;

        if ((state != LOCKER_STATE_AWAITING_DEPOSIT_CLOSE) &&
            (state != LOCKER_STATE_AWAITING_RETRIEVE_CLOSE) &&
            (state != LOCKER_STATE_DOOR_LEFT_OPEN_FAULT))
        {
            continue; /* EMPTY / OCCUPIED lockers need no further polling here */
        }

        if (door_closed)
        {
            /* Door closing at ANY point -- normal window or already in
             * fault -- finalizes the operation the same way. */
            FinalizeSuccess(i, unix_time, lockers[i].flags.bits.pending_is_deposit == 1U);
            continue;
        }

        if (state == LOCKER_STATE_DOOR_LEFT_OPEN_FAULT)
        {
            /* Already faulted: re-announce every DOOR_LEFT_OPEN_REMINDER_INTERVAL_SEC. */
            lockers[i].door_open_reminder_counter++;
            if (lockers[i].door_open_reminder_counter >= DOOR_LEFT_OPEN_REMINDER_INTERVAL_SEC)
            {
                lockers[i].door_open_reminder_counter = 0U;
                *out_reminder_bitmask |= (1UL << i);
            }
        }
        else if (unix_time >= lockers[i].door_close_deadline_unix_time)
        {
            /* Door-close window expired without the door closing. */
            EnterDoorLeftOpenFault(i, unix_time, lockers[i].flags.bits.pending_is_deposit == 1U, out_reminder_bitmask);
        }
        else
        {
            /* Still within the normal window, door still open: nothing to do yet. */
        }
    }

    return SYS_OK;
}

/* ------------------------------------------------------------------- Query */

System_StatusTypeDef ChannelManager_GetLockerState(uint8_t locker_index, LockerRecordTypeDef *out_record)
{
    if ((locker_index >= LOCKER_COUNT) || (out_record == NULL))
    {
        return SYS_INVALID_PARAM;
    }

    *out_record = lockers[locker_index];
    return SYS_OK;
}

uint8_t ChannelManager_CountEmptyLockers(void)
{
    uint8_t count = 0U;

    for (uint8_t i = 0U; i < LOCKER_COUNT; i++)
    {
        if (lockers[i].state == LOCKER_STATE_EMPTY)
        {
            count++;
        }
    }

    return count;
}

/* =========================================================================
 *                          Private helpers
 * ========================================================================= */

static void FinalizeSuccess(uint8_t locker_index, uint32_t unix_time, bool was_deposit)
{
    LockerRecordTypeDef *rec = &lockers[locker_index];

    if (was_deposit)
    {
        rec->flags.bits.in_use = 1U;
        rec->state             = LOCKER_STATE_OCCUPIED;
        /* phone_number / phone_type / fingerprint_id / deposit_unix_time were
         * already written into *rec by ChannelManager_StartDeposit(). */

        (void)Storage_SaveLocker(locker_index, rec);
        (void)Log_Append(LOG_EVENT_DEPOSIT_SUCCESS, locker_index, rec->phone_number, unix_time);
    }
    else
    {
        char phone_for_log[PHONE_NUMBER_DIGIT_COUNT + 1U];
        strncpy(phone_for_log, rec->phone_number, sizeof(phone_for_log));

        uint8_t idx = rec->locker_index; /* preserve across the reset below */
        memset(rec, 0, sizeof(*rec));
        rec->locker_index = idx;
        rec->state        = LOCKER_STATE_EMPTY;

        (void)Storage_SaveLocker(locker_index, rec);
        (void)Log_Append(LOG_EVENT_RETRIEVE_SUCCESS, locker_index, phone_for_log, unix_time);
    }

    rec->flags.bits.fault_logged    = 0U;
    rec->door_open_reminder_counter = 0U;

    ChannelHW_SetLock(locker_index, false);
    ChannelHW_SetLED(locker_index, false);
}

static void EnterDoorLeftOpenFault(uint8_t locker_index, uint32_t unix_time, bool was_deposit,
                                    uint32_t *out_reminder_bitmask)
{
    lockers[locker_index].state                         = LOCKER_STATE_DOOR_LEFT_OPEN_FAULT;
    lockers[locker_index].door_open_reminder_counter     = 0U;
    lockers[locker_index].flags.bits.pending_is_deposit  = was_deposit ? 1U : 0U;

    if (!lockers[locker_index].flags.bits.fault_logged)
    {
        LogEventTypeDef event = was_deposit ? LOG_EVENT_DOOR_LEFT_OPEN_AFTER_DEPOSIT
                                             : LOG_EVENT_DOOR_LEFT_OPEN_AFTER_RETRIEVE;
        (void)Log_Append(event, locker_index, lockers[locker_index].phone_number, unix_time);
        lockers[locker_index].flags.bits.fault_logged = 1U;
    }

    /* Fire the reminder immediately on entering the fault too, not just on
     * the next DOOR_LEFT_OPEN_REMINDER_INTERVAL_SEC boundary. */
    *out_reminder_bitmask |= (1UL << locker_index);
}
