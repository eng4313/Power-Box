/**
  ******************************************************************************
  * @file    channel_manager.c
  * @brief   See channel_manager.h.
  *
  * RAM-only bookkeeping not part of LockerRecordTypeDef (none of it needs to
  * survive a reboot -- a power loss mid-operation simply drops back to
  * whatever was last persisted, i.e. the locker reverts to EMPTY if a
  * deposit was in progress, or stays OCCUPIED if a retrieve was in
  * progress; the customer just has to start that step over):
  *   - deadline_unix_time[i]  : door-close deadline while AWAITING_*_CLOSE
  *   - pending_is_deposit[i]  : which flow to finalize once the fault/await
  *                              state resolves (deposit -> OCCUPIED,
  *                              retrieve -> EMPTY)
  *   - fault_logged[i]        : whether LOG_EVENT_DOOR_LEFT_OPEN_* already
  *                              fired for the CURRENT fault episode
  *   - failed_fp_attempts[i]  : retrieve-flow fingerprint mismatch counter
  ******************************************************************************
  */

#include "channel_manager.h"
#include "channel_hw.h"
#include "storage.h"
#include "log.h"
#include <string.h>

static LockerRecordTypeDef lockers[LOCKER_COUNT];

static uint32_t deadline_unix_time[LOCKER_COUNT];
static bool     pending_is_deposit[LOCKER_COUNT];
static bool     fault_logged[LOCKER_COUNT];
static uint8_t  failed_fp_attempts[LOCKER_COUNT];

static void FinalizeSuccess(uint8_t locker_index, uint32_t unix_time, bool was_deposit);
static void EnterDoorLeftOpenFault(uint8_t locker_index, uint32_t unix_time, bool was_deposit,
                                    uint32_t *out_reminder_bitmask);

System_StatusTypeDef ChannelManager_Init(void)
{
    if (Storage_LoadAllLockers(lockers) != SYS_OK)
    {
        return SYS_ERROR;
    }

    memset(deadline_unix_time, 0, sizeof(deadline_unix_time));
    memset(pending_is_deposit, 0, sizeof(pending_is_deposit));
    memset(fault_logged, 0, sizeof(fault_logged));
    memset(failed_fp_attempts, 0, sizeof(failed_fp_attempts));

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
            lockers[i].phone_type         = phone_type;
            lockers[i].fingerprint_id     = fingerprint_id;
            lockers[i].deposit_unix_time  = unix_time;
            lockers[i].state              = LOCKER_STATE_AWAITING_DEPOSIT_CLOSE;
            /* in_use / lockout fields stay as they were (false / inactive for
             * an EMPTY locker) until FinalizeSuccess() confirms the door closed. */

            deadline_unix_time[i]  = unix_time + TIMEOUT_DOOR_CLOSE_AFTER_DEPOSIT_SEC;
            pending_is_deposit[i]  = true;
            fault_logged[i]        = false;

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
        if (lockers[i].in_use && (strncmp(lockers[i].phone_number, phone_number, PHONE_NUMBER_DIGIT_COUNT) == 0))
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

    if (rec->lockout_active && (rec->lockout_until_unix_time > unix_time))
    {
        *out_locked            = true;
        *out_seconds_remaining = rec->lockout_until_unix_time - unix_time;
        return SYS_OK;
    }

    if (rec->lockout_active)
    {
        /* Lockout window has expired: clear it and persist, so a future
         * reboot doesn't come back up still "locked" against a stale timestamp. */
        rec->lockout_active          = false;
        rec->lockout_until_unix_time = 0U;
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
    failed_fp_attempts[locker_index]++;

    if (failed_fp_attempts[locker_index] >= RETRIEVE_MAX_FINGERPRINT_ATTEMPTS)
    {
        LockerRecordTypeDef *rec = &lockers[locker_index];

        rec->lockout_active          = true;
        rec->lockout_until_unix_time = unix_time + LOCKER_LOCKOUT_DURATION_SEC;

        if (Storage_SaveLocker(locker_index, rec) != SYS_OK)
        {
            return SYS_ERROR;
        }

        (void)Log_Append(LOG_EVENT_LOCKER_LOCKOUT_30MIN, locker_index, rec->phone_number, unix_time);

        failed_fp_attempts[locker_index] = 0U;
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
    failed_fp_attempts[locker_index] = 0U;
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

    deadline_unix_time[locker_index] = unix_time + TIMEOUT_DOOR_CLOSE_AFTER_RETRIEVE_SEC;
    pending_is_deposit[locker_index] = false;
    fault_logged[locker_index]       = false;

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

    rec->opened_by_admin = true; /* RAM-only flag, not persisted -- purely for the log entry below */

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
        LockerStateTypeDef state = lockers[i].state;

        if ((state != LOCKER_STATE_AWAITING_DEPOSIT_CLOSE) &&
            (state != LOCKER_STATE_AWAITING_RETRIEVE_CLOSE) &&
            (state != LOCKER_STATE_DOOR_LEFT_OPEN_FAULT))
        {
            continue; /* EMPTY / OCCUPIED lockers need no polling */
        }

        bool door_closed = ChannelHW_IsDoorClosed(i);

        if (door_closed)
        {
            /* Door closing at ANY point -- normal window or already in
             * fault -- finalizes the operation the same way. */
            FinalizeSuccess(i, unix_time, pending_is_deposit[i]);
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
        else if (unix_time >= deadline_unix_time[i])
        {
            /* Door-close window expired without the door closing. */
            EnterDoorLeftOpenFault(i, unix_time, pending_is_deposit[i], out_reminder_bitmask);
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
        rec->in_use = true;
        rec->state  = LOCKER_STATE_OCCUPIED;
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
        rec->in_use        = false;

        (void)Storage_SaveLocker(locker_index, rec);
        (void)Log_Append(LOG_EVENT_RETRIEVE_SUCCESS, locker_index, phone_for_log, unix_time);
    }

    fault_logged[locker_index]                    = false;
    lockers[locker_index].door_open_reminder_counter = 0U;

    ChannelHW_SetLock(locker_index, false);
    ChannelHW_SetLED(locker_index, false);
}

static void EnterDoorLeftOpenFault(uint8_t locker_index, uint32_t unix_time, bool was_deposit,
                                    uint32_t *out_reminder_bitmask)
{
    lockers[locker_index].state = LOCKER_STATE_DOOR_LEFT_OPEN_FAULT;
    lockers[locker_index].door_open_reminder_counter = 0U;

    if (!fault_logged[locker_index])
    {
        LogEventTypeDef event = was_deposit ? LOG_EVENT_DOOR_LEFT_OPEN_AFTER_DEPOSIT
                                             : LOG_EVENT_DOOR_LEFT_OPEN_AFTER_RETRIEVE;
        (void)Log_Append(event, locker_index, lockers[locker_index].phone_number, unix_time);
        fault_logged[locker_index] = true;
    }

    /* Fire the reminder immediately on entering the fault too, not just on
     * the next DOOR_LEFT_OPEN_REMINDER_INTERVAL_SEC boundary. */
    *out_reminder_bitmask |= (1UL << locker_index);
}
