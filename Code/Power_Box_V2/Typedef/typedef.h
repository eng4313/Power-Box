/**
  ******************************************************************************
  * @file    typedef.h
  * @brief   System-wide configuration defines and shared typedefs for the
  *          Power Box firmware (phone locker/charging station).
  *
  *          This header is the single place where locker count, admin count,
  *          timeouts, default password, log capacity, and every struct/enum
  *          shared between modules (Fingerprint, UI, Channel Manager, Storage,
  *          Audio, Log) are defined. Every other module includes this file
  *          instead of hardcoding any of these values.
  ******************************************************************************
  */

#ifndef __TYPEDEF_H
#define __TYPEDEF_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

/* ========================================================================== 
	0U -> lcd & touch are not installed and UI will work by uart
	1U -> lcd & touch are installed  
*/
#define UI_HARD_WARE_MODE                 0U   
/* ==========================================================================
 *  HARDWARE SCALE (locker count)
 * ==========================================================================
 *  Current assembled board manages 8 channels (see Power_Box_V2 schematic,
 *  Channel.SchDoc instances A..H). The MCU_LOCK/MCU_LED/MCU_MAGNET busses on
 *  the Interface board are wired for up to 28 channels, and the architecture
 *  target is 32. Changing this single define (and the matching GPIO/HAL pin
 *  tables in the Channel module) is enough to re-target the firmware to a
 *  4-channel or 8-channel physical box built from the same board design.
 * ========================================================================== */
#define LOCKER_COUNT                        8U      /* number of physical lockers on THIS assembled board */
#define LOCKER_COUNT_MAX_SUPPORTED          32U     /* upper bound the interface bus/architecture was designed for */

/* ==========================================================================
 *  EEPROM / STORAGE NOTE
 * ==========================================================================
 *  Assembled part is AT24C04 (4Kbit = 512 bytes, see EEPROM.SchDoc). This is
 *  NOT enough to hold LOCKER_COUNT active records + LOG_CAPACITY_TOTAL log
 *  entries at the same time (rough estimate: a few KB needed once logging is
 *  full). AT24C04 and AT24C256 share the same pinout/footprint on this PCB,
 *  so the intended fix is a part swap on next assembly, no rework needed.
 *  TODO(hardware): replace U1 (AT24C04) with AT24C256 (or bigger) before
 *  final assembly. Until then the storage abstraction layer must be written
 *  against a generic address range, not against the 512-byte physical limit,
 *  so firmware development is not blocked by this.
 * ========================================================================== */
#define EEPROM_INSTALLED_SIZE_BYTES         512U    /* AT24C04 actually on the board today */
#define EEPROM_TARGET_SIZE_BYTES            32768U  /* AT24C256 planned replacement, size the storage layer for this */

/* ==========================================================================
 *  ADMIN CONFIGURATION
 * ========================================================================== */
#define ADMIN_MAX_COUNT                     20U     /* total admins the system can hold, including the main admin */
#define ADMIN_PASSWORD_DIGIT_COUNT          6U      /* numeric password length, e.g. "000000" */
#define ADMIN_DEFAULT_PASSWORD              "000000"
#define ADMIN_MAIN_INDEX                    0U      /* main admin always lives at index 0 in the admin table */
#define ADMIN_RESET_BUTTON_HOLD_MS          10000U  /* hold S2 (MCU_KEY) for this long to reset ONLY the main admin password */
#define ADMIN_NAME_MAX_LEN                  16U     /* optional display name length, adjust if UI needs more */

/* ==========================================================================
 *  PHONE / USER INPUT
 * ========================================================================== */
#define PHONE_NUMBER_DIGIT_COUNT            11U     /* fixed length required by the algorithm doc */

/* ==========================================================================
 *  TIMEOUTS (all in seconds unless noted otherwise)
 * ========================================================================== */
#define TIMEOUT_FINGERPRINT_FIRST_SCAN_SEC     60U   /* deposit flow: time to place finger the 1st time */
#define TIMEOUT_FINGERPRINT_CONFIRM_SCAN_SEC   60U   /* deposit flow: time to place finger the 2nd time (match/confirm) */
#define TIMEOUT_DOOR_CLOSE_AFTER_DEPOSIT_SEC   150U  /* deposit flow: time to put phone in and close the door */
#define TIMEOUT_DOOR_CLOSE_AFTER_RETRIEVE_SEC  60U   /* retrieve flow: time to take phone out and close the door */
#define DOOR_LEFT_OPEN_REMINDER_INTERVAL_SEC   30U   /* re-announce/re-display "locker N door is open" every N sec */

#define RETRIEVE_MAX_PHONE_ATTEMPTS         3U      /* retrieve flow: wrong phone number tries before returning to idle */
#define RETRIEVE_MAX_FINGERPRINT_ATTEMPTS   3U      /* retrieve flow: wrong fingerprint tries before locker lockout */

#define LOCKER_LOCKOUT_DURATION_SEC         (30U * 60U)  /* 30 minutes: the LOCKER (not phone, not whole device) is
                                                             locked out after RETRIEVE_MAX_FINGERPRINT_ATTEMPTS failures
                                                             for the phone number that owns it */

/* ==========================================================================
 *  LOG SYSTEM
 * ========================================================================== */
#define LOG_CAPACITY_PER_LOCKER             100U
#define LOG_CAPACITY_TOTAL                  (LOG_CAPACITY_PER_LOCKER * LOCKER_COUNT)  /* e.g. 800 for 8 lockers */
#define LOG_PAGE_SIZE                       50U     /* entries shown per admin log page, newest page first */

/* ==========================================================================
 *  AUDIO (ISD1730) MESSAGE ADDRESS TABLE
 * ==========================================================================
 *  No voice messages have been recorded yet, so every address below is a
 *  placeholder (0x000000). Once messages are recorded onto the ISD1730,
 *  replace each value with the real start address returned/used by the
 *  record tool for that slot. Do not remove or renumber entries once other
 *  modules start calling Audio_Play(AUDIO_MSG_xxx) against them.
 * ========================================================================== */
//typedef enum
//{
//    AUDIO_MSG_SELECT_PHONE_TYPE = 0,       /* "select android or iphone" */
//    AUDIO_MSG_ENTER_PHONE_NUMBER,          /* "enter your phone number" */
//    AUDIO_MSG_INVALID_PHONE_NUMBER,        /* "invalid phone number" */
//    AUDIO_MSG_CONFIRM_PHONE_NUMBER,        /* "is this number correct?" */
//    AUDIO_MSG_PLACE_FINGER,                /* "place your finger" (1st scan) */
//    AUDIO_MSG_PLACE_FINGER_AGAIN,          /* "place your finger again" (confirm scan) */
//    AUDIO_MSG_FINGERPRINT_TIMEOUT,         /* 60s expired without a valid scan */
//    AUDIO_MSG_FINGERPRINT_MISMATCH,        /* two scans did not match, deposit flow */
//    AUDIO_MSG_LOCKER_OPENED_PLACE_PHONE,   /* "locker opened, place your phone and close the door" */
//    AUDIO_MSG_DEPOSIT_TIMEOUT_ERROR,       /* 150s expired, door not closed */
//    AUDIO_MSG_DEPOSIT_SUCCESS,             /* deposit completed successfully */

//    AUDIO_MSG_RETRIEVE_ENTER_PHONE,        /* retrieve flow: "enter your phone number" */
//    AUDIO_MSG_PHONE_NOT_FOUND,             /* phone number not found / attempts remaining */
//    AUDIO_MSG_RETRIEVE_PLACE_FINGER,       /* retrieve flow: "place your finger" */
//    AUDIO_MSG_FINGERPRINT_NOT_MATCH,       /* retrieve flow: fingerprint mismatch, attempts remaining */
//    AUDIO_MSG_LOCKER_LOCKED_30MIN,         /* locker locked out for 30 minutes */
//    AUDIO_MSG_LOCKER_OPENED_TAKE_PHONE,    /* "locker opened, take your phone and close the door" */
//    AUDIO_MSG_RETRIEVE_TIMEOUT_DOOR_OPEN,  /* 60s expired, door left open */
//    AUDIO_MSG_RETRIEVE_SUCCESS,            /* retrieve completed successfully */

//    AUDIO_MSG_ADMIN_WRONG_PASSWORD,        /* wrong admin password entered */
//    AUDIO_MSG_ADMIN_WELCOME,               /* correct admin password entered */

//    AUDIO_MSG_COUNT                        /* keep last: total number of defined messages */
//} AudioMsgIdTypeDef;

/* TODO(audio): fill in real ISD1730 addresses once messages are recorded.
 * Kept as a separate table (not baked into the enum) so re-recording never
 * requires touching AudioMsgIdTypeDef or any code that references it. */
#define AUDIO_ADDRESS_TABLE_INIT { \
    [AUDIO_MSG_SELECT_PHONE_TYPE]      = 0x000000U, \
    [AUDIO_MSG_ENTER_PHONE_NUMBER]     = 0x000000U, \
    [AUDIO_MSG_INVALID_PHONE_NUMBER]   = 0x000000U, \
    [AUDIO_MSG_CONFIRM_PHONE_NUMBER]   = 0x000000U, \
    [AUDIO_MSG_PLACE_FINGER]           = 0x000000U, \
    [AUDIO_MSG_PLACE_FINGER_AGAIN]     = 0x000000U, \
    [AUDIO_MSG_FINGERPRINT_TIMEOUT]    = 0x000000U, \
    [AUDIO_MSG_FINGERPRINT_MISMATCH]   = 0x000000U, \
    [AUDIO_MSG_LOCKER_OPENED_PLACE_PHONE] = 0x000000U, \
    [AUDIO_MSG_DEPOSIT_TIMEOUT_ERROR]  = 0x000000U, \
    [AUDIO_MSG_DEPOSIT_SUCCESS]        = 0x000000U, \
    [AUDIO_MSG_RETRIEVE_ENTER_PHONE]   = 0x000000U, \
    [AUDIO_MSG_PHONE_NOT_FOUND]        = 0x000000U, \
    [AUDIO_MSG_RETRIEVE_PLACE_FINGER]  = 0x000000U, \
    [AUDIO_MSG_FINGERPRINT_NOT_MATCH]  = 0x000000U, \
    [AUDIO_MSG_LOCKER_LOCKED_30MIN]    = 0x000000U, \
    [AUDIO_MSG_LOCKER_OPENED_TAKE_PHONE] = 0x000000U, \
    [AUDIO_MSG_RETRIEVE_TIMEOUT_DOOR_OPEN] = 0x000000U, \
    [AUDIO_MSG_RETRIEVE_SUCCESS]       = 0x000000U, \
    [AUDIO_MSG_ADMIN_WRONG_PASSWORD]   = 0x000000U, \
    [AUDIO_MSG_ADMIN_WELCOME]          = 0x000000U, \
}

/* ==========================================================================
 *  GENERIC RETURN CODE (shared across new higher-level modules: Storage,
 *  Fingerprint, Log, Channel Manager, UI state machine)
 * ========================================================================== */
typedef enum
{
    SYS_OK = 0x00U,
    SYS_ERROR,
    SYS_TIMEOUT,
    SYS_BUSY,
    SYS_NOT_FOUND,
    SYS_FULL,          /* e.g. no empty locker, EEPROM region full */
    SYS_INVALID_PARAM
} System_StatusTypeDef;

/* ==========================================================================
 *  PHONE TYPE
 * ========================================================================== */
typedef enum
{
    PHONE_TYPE_ANDROID = 0,
    PHONE_TYPE_IPHONE
} PhoneTypeTypeDef;

/* ==========================================================================
 *  LOCKER (CHANNEL) RUNTIME STATE
 * ==========================================================================
 *  One instance per physical locker. Lives in RAM, mirrored to EEPROM by the
 *  storage layer whenever occupancy/lockout state changes. Fingerprint ID is
 *  the ZFM-40 internal template ID (independent from locker_index, see
 *  fingerprint mapping rationale) so a locker can be freed and reused with a
 *  different fingerprint template without renumbering anything.
 * ========================================================================== */
typedef enum
{
    LOCKER_STATE_EMPTY = 0,     /* free, available for a new deposit */
    LOCKER_STATE_OCCUPIED,      /* holds a phone, door closed and confirmed */
    LOCKER_STATE_AWAITING_DEPOSIT_CLOSE,   /* door opened for deposit, waiting for TIMEOUT_DOOR_CLOSE_AFTER_DEPOSIT_SEC */
    LOCKER_STATE_AWAITING_RETRIEVE_CLOSE,  /* door opened for retrieve, waiting for TIMEOUT_DOOR_CLOSE_AFTER_RETRIEVE_SEC */
    LOCKER_STATE_DOOR_LEFT_OPEN_FAULT      /* timeout expired with door still open, LED blinking, periodic reminder */
} LockerStateTypeDef;

typedef struct
{
    uint8_t              locker_index;                             /* 0..LOCKER_COUNT-1, matches Channel module wiring */
    LockerStateTypeDef   state;
    bool                 in_use;                                   /* true once state != LOCKER_STATE_EMPTY */

    char                 phone_number[PHONE_NUMBER_DIGIT_COUNT + 1U]; /* '\0' terminated, valid only if in_use */
    PhoneTypeTypeDef      phone_type;
    uint16_t             fingerprint_id;                            /* ZFM-40 template ID, independent from locker_index */

    uint32_t             deposit_unix_time;                         /* RTC timestamp phone was deposited */

    bool                 opened_by_admin;                           /* true if last door-open was an admin override, for logging */

    bool                 lockout_active;                            /* true while this locker is in its 30-min retrieve lockout */
    uint32_t             lockout_until_unix_time;                   /* valid only if lockout_active == true */

    uint8_t              door_open_reminder_counter;                /* internal use: tracks DOOR_LEFT_OPEN_REMINDER_INTERVAL_SEC */
} LockerRecordTypeDef;

/* ==========================================================================
 *  ADMIN RECORD
 * ========================================================================== */
typedef struct
{
    bool     in_use;                                       /* false = free admin slot */
    char     name[ADMIN_NAME_MAX_LEN + 1U];                 /* optional, may be empty string */
    char     password[ADMIN_PASSWORD_DIGIT_COUNT + 1U];     /* '\0' terminated numeric password */
} AdminRecordTypeDef;

/* ==========================================================================
 *  LOG SYSTEM
 * ========================================================================== */
typedef enum
{
    LOG_EVENT_DEPOSIT_SUCCESS = 0,
    LOG_EVENT_RETRIEVE_SUCCESS,
    LOG_EVENT_ADMIN_OPENED_LOCKER,
    LOG_EVENT_LOCKER_LOCKOUT_30MIN,
    LOG_EVENT_DOOR_LEFT_OPEN_AFTER_DEPOSIT,
    LOG_EVENT_DOOR_LEFT_OPEN_AFTER_RETRIEVE
} LogEventTypeDef;

typedef struct
{
    LogEventTypeDef  event;
    uint8_t          locker_index;
    char             phone_number[PHONE_NUMBER_DIGIT_COUNT + 1U];  /* empty string if not applicable (e.g. admin-only event) */
    uint32_t         unix_time;
} LogEntryTypeDef;

#ifdef __cplusplus
}
#endif

#endif /* __TYPEDEF_H */
