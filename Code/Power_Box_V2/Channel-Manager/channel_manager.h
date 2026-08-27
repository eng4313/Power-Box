/**
  ******************************************************************************
  * @file    channel_manager.h
  * @brief   Per-locker state machine implementing the deposit/retrieve
  *          algorithm: locker allocation, door-close timeout supervision,
  *          the 30-minute per-locker retrieve lockout, and admin override.
  *
  * SCOPE: this module starts from "the caller already has a validated
  * phone number, phone type, and a fingerprint template sitting in the
  * ZFM-40 buffer/library" -- collecting those from the touchscreen keypad
  * and driving the fingerprint enrollment/match sequence itself belongs to
  * the UI/Menu layer (next module), which calls into this one once each
  * piece of input is ready. This keeps the state machine here testable
  * independent of any screen.
  *
  * All persistence goes through the Storage module; all history goes
  * through the Log module. This module has no direct RTC dependency --
  * every function that needs "now" takes it as a unix_time parameter, so
  * the caller (UI task) is the only place that calls RTC_GetUnixTime().
  ******************************************************************************
  */

#ifndef __CHANNEL_MANAGER_H
#define __CHANNEL_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "typedef.h"
#include "channel_hw.h"
#include "storage.h"
#include "log.h"
#include <string.h>

/**
  * @brief  Loads all locker records from Storage into the RAM cache this
  *         module works from, and resets in-RAM-only bookkeeping (failed
  *         fingerprint attempt counters, door-close deadlines, reminder
  *         timers -- none of which need to survive a reboot). Call once at
  *         boot, after Storage_Init() and ChannelHW_Init() have succeeded.
  */
System_StatusTypeDef ChannelManager_Init(void);

/* --------------------------------------------------------------- Deposit */

/**
  * @brief  Allocates the first empty locker, opens its lock/LED, and starts
  *         the TIMEOUT_DOOR_CLOSE_AFTER_DEPOSIT_SEC countdown. Does NOT
  *         write the phone/fingerprint record to Storage yet -- that only
  *         happens once ChannelManager_Poll() observes the door actually
  *         close within the timeout (see typedef.h LockerRecordTypeDef:
  *         state becomes LOCKER_STATE_OCCUPIED and Log_Append(
  *         LOG_EVENT_DEPOSIT_SUCCESS, ...) fires at that point).
  * @retval SYS_OK, or SYS_FULL if no locker is currently empty.
  */
System_StatusTypeDef ChannelManager_StartDeposit(const char *phone_number, PhoneTypeTypeDef phone_type,
                                                  uint16_t fingerprint_id, uint32_t unix_time,
                                                  uint8_t *out_locker_index);

/* -------------------------------------------------------------- Retrieve */

/**
  * @brief  Finds the currently-occupied locker whose stored phone number
  *         matches. Does not check lockout -- call
  *         ChannelManager_IsLockerLockedOut() separately once found.
  * @retval SYS_OK, or SYS_NOT_FOUND if no occupied locker matches.
  */
System_StatusTypeDef ChannelManager_FindLockerByPhone(const char *phone_number, uint8_t *out_locker_index);

/**
  * @brief  Checks whether a locker is currently inside its 30-minute
  *         retrieve lockout window.
  * @param  out_seconds_remaining  valid only when the function returns
  *                                 SYS_OK and *out_locked is true.
  */
System_StatusTypeDef ChannelManager_IsLockerLockedOut(uint8_t locker_index, uint32_t unix_time,
                                                       bool *out_locked, uint32_t *out_seconds_remaining);

/**
  * @brief  Call after a failed fingerprint match during a retrieve attempt
  *         (the phone number was already found to own locker_index, but
  *         RETRIEVE flow's fingerprint comparison did not match). Increments
  *         an in-RAM attempt counter; on the RETRIEVE_MAX_FINGERPRINT_ATTEMPTS-th
  *         failure, automatically applies the 30-minute lockout, persists
  *         it, logs LOG_EVENT_LOCKER_LOCKOUT_30MIN, and resets the counter.
  * @param  out_locker_now_locked  true if this call just triggered the lockout.
  */
System_StatusTypeDef ChannelManager_RegisterFailedFingerprintAttempt(uint8_t locker_index, uint32_t unix_time,
                                                                      bool *out_locker_now_locked);

/**
  * @brief  Call once a retrieve attempt's fingerprint check succeeds, to
  *         zero that locker's failed-attempt counter for next time.
  */
void ChannelManager_ClearFailedAttempts(uint8_t locker_index);

/**
  * @brief  Opens the lock/LED for a retrieve and starts the
  *         TIMEOUT_DOOR_CLOSE_AFTER_RETRIEVE_SEC countdown. Caller must have
  *         already verified phone + fingerprint and that the locker is not
  *         locked out. The locker record is cleared (state -> EMPTY,
  *         in_use -> false) and Log_Append(LOG_EVENT_RETRIEVE_SUCCESS, ...)
  *         fires only once ChannelManager_Poll() observes the door close
  *         within the timeout, same pattern as deposit.
  */
System_StatusTypeDef ChannelManager_StartRetrieve(uint8_t locker_index, uint32_t unix_time);

/* ------------------------------------------------------------------ Admin */

/**
  * @brief  Admin override: opens a locker's lock/LED regardless of its
  *         current state (e.g. maintenance, forgotten phone). Marks
  *         opened_by_admin and logs LOG_EVENT_ADMIN_OPENED_LOCKER
  *         immediately (unlike deposit/retrieve, this does not wait for the
  *         door-close event, since an admin override should be auditable
  *         even if the door never gets closed again e.g. for maintenance).
  *         Does NOT change occupancy: if the locker was OCCUPIED it still
  *         is afterwards (from Storage's point of view), so the same
  *         customer can still retrieve normally later. Use this to inspect
  *         or free a stuck locker, not as a substitute for the retrieve flow.
  */
System_StatusTypeDef ChannelManager_AdminOpenLocker(uint8_t locker_index, uint32_t unix_time);

/**
  * @brief  Pairs with ChannelManager_AdminOpenLocker(): de-energizes the
  *         lock/LED once the admin is done (e.g. UI navigates back from the
  *         "locker opened" screen). No separate log event or door-close
  *         wait -- the open event was already audited at open time.
  */
void ChannelManager_AdminCloseLocker(uint8_t locker_index);

/* -------------------------------------------------------------- Background */

/**
  * @brief  Call once per second (or as close to it as the main loop allows)
  *         for every locker currently in AWAITING_DEPOSIT_CLOSE,
  *         AWAITING_RETRIEVE_CLOSE, or DOOR_LEFT_OPEN_FAULT:
  *           - if the door is now closed: finalizes the operation (saves
  *             the record, logs success, turns off lock/LED).
  *           - else if TIMEOUT_DOOR_CLOSE_AFTER_*_SEC has elapsed without
  *             the door closing: transitions to LOCKER_STATE_DOOR_LEFT_OPEN_FAULT,
  *             logs LOG_EVENT_DOOR_LEFT_OPEN_AFTER_DEPOSIT/RETRIEVE once, and
  *             keeps the LED on to signal the fault.
  *           - while already in DOOR_LEFT_OPEN_FAULT: re-checks the door
  *             every call (closing it at any point still finalizes
  *             normally) and sets that locker's bit in *out_reminder_bitmask
  *             every DOOR_LEFT_OPEN_REMINDER_INTERVAL_SEC so the UI can
  *             re-trigger the audio/visual reminder.
  * @param  out_reminder_bitmask  bit i (0..LOCKER_COUNT-1) set = locker i
  *                                needs a reminder fired THIS call. Caller
  *                                must pass a valid pointer; write 0 if no
  *                                locker needs one.
  */
System_StatusTypeDef ChannelManager_Poll(uint32_t unix_time, uint32_t *out_reminder_bitmask);

/* ------------------------------------------------------------------- Query */

System_StatusTypeDef ChannelManager_GetLockerState(uint8_t locker_index, LockerRecordTypeDef *out_record);

/**
  * @brief  How many lockers currently have state == LOCKER_STATE_EMPTY.
  *         Useful for the idle screen ("X of Y lockers available").
  */
uint8_t ChannelManager_CountEmptyLockers(void);

#ifdef __cplusplus
}
#endif

#endif /* __CHANNEL_MANAGER_H */
