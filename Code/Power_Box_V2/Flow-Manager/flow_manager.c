/**
  ******************************************************************************
  * @file    flow_manager.c
  * @brief   See flow_manager.h.
  ******************************************************************************
  */

#include "flow_manager.h"
#include "ui_interface.h"
#include "channel_manager.h"
#include "storage.h"
#include "log.h"
#include "zfm40.h"
#include "isd1730.h"
#include "rtc.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ==========================================================================
 *  Foreground state
 * ========================================================================== */
typedef enum
{
    FLOW_STATE_IDLE = 0,

    FLOW_STATE_DEPOSIT_SELECT_TYPE,
    FLOW_STATE_DEPOSIT_ENTER_PHONE,
    FLOW_STATE_DEPOSIT_CONFIRM_PHONE,
    FLOW_STATE_DEPOSIT_FINGER_SCAN1,
    FLOW_STATE_DEPOSIT_FINGER_SCAN2,
    FLOW_STATE_DEPOSIT_WAIT_DOOR_CLOSE,     /* Channel Manager owns the timeout from here on */

    FLOW_STATE_RETRIEVE_ENTER_PHONE,
    FLOW_STATE_RETRIEVE_FINGER_SCAN,
    FLOW_STATE_RETRIEVE_WAIT_DOOR_CLOSE,

    FLOW_STATE_ADMIN_ENTER_PASSWORD,
    FLOW_STATE_ADMIN_MENU,
    FLOW_STATE_ADMIN_OPEN_LOCKER_ENTER_NUMBER,
    FLOW_STATE_ADMIN_VIEW_LOGS,
    FLOW_STATE_ADMIN_CHANGE_PW_ENTER_NEW,
    FLOW_STATE_ADMIN_CHANGE_PW_CONFIRM_NEW
} FlowStateTypeDef;

/* ==========================================================================
 *  Audio mapping (see flow_manager.h header note on the 11-clip limit)
 * ========================================================================== */
typedef enum
{
    FAUD_LOCKER_FULL = 0,
    FAUD_SELECT_PHONE_TYPE,
    FAUD_ENTER_PHONE_NUMBER,
    FAUD_CONFIRM_PHONE_NUMBER,
    FAUD_PLACE_FINGER,
    FAUD_PLACE_FINGER_AGAIN,
    FAUD_FINGERPRINT_TIMEOUT,
    FAUD_FINGERPRINT_MISMATCH,
    FAUD_LOCKER_OPENED_DEPOSIT,
    FAUD_DEPOSIT_TIMEOUT,
    FAUD_DEPOSIT_SUCCESS,
    FAUD_RETRIEVE_ENTER_PHONE,
    FAUD_PHONE_NOT_FOUND,
    FAUD_RETRIEVE_PLACE_FINGER,
    FAUD_FINGERPRINT_NOT_MATCH,
    FAUD_LOCKER_LOCKED_30MIN,
    FAUD_LOCKER_OPENED_RETRIEVE,
    FAUD_RETRIEVE_TIMEOUT_DOOR_OPEN,
    FAUD_RETRIEVE_SUCCESS,
    FAUD_DOOR_LEFT_OPEN_REMINDER,
    FAUD_ADMIN_WRONG_PASSWORD,
    FAUD_ADMIN_WELCOME,
    FAUD_COUNT
} FlowAudioEventTypeDef;

static const ISD_MESSAGE_t FlowAudioTable[FAUD_COUNT] =
{
    [FAUD_LOCKER_FULL]                = FULL_BOX,
    [FAUD_SELECT_PHONE_TYPE]          = OS_SELECT,
    [FAUD_ENTER_PHONE_NUMBER]         = ENTER_NUM,
    [FAUD_CONFIRM_PHONE_NUMBER]       = SAVE,             /* TODO(audio): guessed, verify against recording */
    [FAUD_PLACE_FINGER]               = ENTER_FINGER,
    [FAUD_PLACE_FINGER_AGAIN]         = ENTER_FINGER,     /* TODO(audio): reused, no dedicated "again" clip */
    [FAUD_FINGERPRINT_TIMEOUT]        = END_TIME,
    [FAUD_FINGERPRINT_MISMATCH]       = WRONG_FINGER,     /* TODO(audio): reused from retrieve-mismatch clip */
    [FAUD_LOCKER_OPENED_DEPOSIT]      = DOOR_OPENED,
    [FAUD_DEPOSIT_TIMEOUT]            = END_TIME,         /* TODO(audio): reused */
    [FAUD_DEPOSIT_SUCCESS]            = SAVE,             /* TODO(audio): guessed */
    [FAUD_RETRIEVE_ENTER_PHONE]       = ENTER_NUM,
    [FAUD_PHONE_NOT_FOUND]            = NOT_FOUND,
    [FAUD_RETRIEVE_PLACE_FINGER]      = ENTER_FINGER,
    [FAUD_FINGERPRINT_NOT_MATCH]      = WRONG_FINGER,
    [FAUD_LOCKER_LOCKED_30MIN]        = WRONG_FINGER,     /* TODO(audio): no dedicated clip exists -- needs a new recording */
    [FAUD_LOCKER_OPENED_RETRIEVE]     = DOOR_OPENED,
    [FAUD_RETRIEVE_TIMEOUT_DOOR_OPEN] = DOOR_IS_OPEN,
    [FAUD_RETRIEVE_SUCCESS]           = SAVE,             /* TODO(audio): guessed */
    [FAUD_DOOR_LEFT_OPEN_REMINDER]    = DOOR_IS_OPEN,
    [FAUD_ADMIN_WRONG_PASSWORD]       = NOT_FOUND,        /* TODO(audio): guessed, no dedicated clip */
    [FAUD_ADMIN_WELCOME]              = SAVE,             /* TODO(audio): guessed, no dedicated clip */
};

static void PlayAudio(FlowAudioEventTypeDef evt)
{
    if (evt < FAUD_COUNT)
    {
        (void)ISD1730_PlayMessage(FlowAudioTable[evt]);
    }
}

/* ==========================================================================
 *  Session state
 * ========================================================================== */
#define DIGIT_BUF_SIZE   (PHONE_NUMBER_DIGIT_COUNT > ADMIN_PASSWORD_DIGIT_COUNT ? \
                           PHONE_NUMBER_DIGIT_COUNT + 1U : ADMIN_PASSWORD_DIGIT_COUNT + 1U)

static FlowStateTypeDef s_state;

/* Generic digit-entry buffer, reused across phone numbers, admin
 * passwords, and locker numbers -- only one such entry is ever in
 * progress at a time. */
static char    s_digit_buf[DIGIT_BUF_SIZE];
static uint8_t s_digit_len;

static uint8_t  s_phone_type;          /* 0 = Android, 1 = iPhone, set during deposit */
static uint8_t  s_locker_index;        /* LOCKER_COUNT = sentinel "not watching any locker" */
static uint8_t  s_retrieve_phone_attempts;
static bool     s_fp_wait_lift;        /* deposit enrollment: waiting for finger lift between the two scans */

static bool     s_deadline_active;
static uint32_t s_deadline_tick;

static uint32_t s_last_fp_poll_tick;
static uint32_t s_last_periodic_tick;

/* Admin session */
static uint8_t  s_admin_index;                                 /* which admin slot is logged in */
static char     s_admin_new_password[ADMIN_PASSWORD_DIGIT_COUNT + 1U]; /* held between the two change-password entries */
static uint32_t s_log_page_index;
static uint32_t s_log_entry_in_page;

/* ==========================================================================
 *  Forward decls
 * ========================================================================== */
static void GoIdle(void);
static void GoAdminMenu(void);
static void HandleEvent(UI_EventTypeDef event, uint8_t digit);
static void PollFingerprint(void);
static void CheckTimeouts(void);
static void PeriodicTick(void);
static void SetDeadline(uint32_t seconds_from_now);
static void ClearDeadline(void);
static void DigitBufReset(void);
static bool DigitBufAppend(uint8_t digit, uint8_t max_len);
static bool DigitBufBackspace(void);

/* ==========================================================================
 *  Public API
 * ========================================================================== */
System_StatusTypeDef FlowManager_Init(void)
{
    s_last_fp_poll_tick  = HAL_GetTick();
    s_last_periodic_tick = HAL_GetTick();
    GoIdle();
    return SYS_OK;
}

void FlowManager_Process(void)
{
    UI_EventTypeDef event;
    uint8_t digit;

    while (UI_GetNextEvent(&event, &digit))
    {
        HandleEvent(event, digit);
    }

    PollFingerprint();
    CheckTimeouts();
    PeriodicTick();
}

/* ==========================================================================
 *  Small helpers
 * ========================================================================== */
static void SetDeadline(uint32_t seconds_from_now)
{
    s_deadline_active = true;
    s_deadline_tick    = HAL_GetTick() + (seconds_from_now * 1000UL);
}

static void ClearDeadline(void)
{
    s_deadline_active = false;
}

static void DigitBufReset(void)
{
    s_digit_len    = 0U;
    s_digit_buf[0] = '\0';
    UI_ShowEntry(NULL);
}

static bool DigitBufAppend(uint8_t digit, uint8_t max_len)
{
    if (s_digit_len >= max_len)
    {
        return false;
    }
    s_digit_buf[s_digit_len] = (char)('0' + digit);
    s_digit_len++;
    s_digit_buf[s_digit_len] = '\0';
    UI_ShowEntry(s_digit_buf);
    return true;
}

static bool DigitBufBackspace(void)
{
    if (s_digit_len == 0U)
    {
        return false;
    }
    s_digit_len--;
    s_digit_buf[s_digit_len] = '\0';
    UI_ShowEntry(s_digit_buf);
    return true;
}

static int8_t DigitFromEvent(UI_EventTypeDef event)
{
    if ((event >= UI_EVENT_DIGIT_0) && (event <= UI_EVENT_DIGIT_9))
    {
        return (int8_t)(event - UI_EVENT_DIGIT_0);
    }
    return -1;
}

static void GoIdle(void)
{
    char msg[48];
    uint8_t empty_count;

    s_state = FLOW_STATE_IDLE;
    ClearDeadline();
    DigitBufReset();
    s_locker_index = LOCKER_COUNT; /* sentinel: not watching any locker */
    s_fp_wait_lift = false;
    s_retrieve_phone_attempts = 0U;

    UI_SetScreenState("IDLE");

    empty_count = ChannelManager_CountEmptyLockers();
    snprintf(msg, sizeof(msg), "Available lockers: %u/%u", (unsigned int)empty_count, (unsigned int)LOCKER_COUNT);
    UI_ShowMessage(msg);
}

static void GoAdminMenu(void)
{
    s_state = FLOW_STATE_ADMIN_MENU;
    DigitBufReset();
    UI_SetScreenState("ADMIN_MENU");
    UI_ShowMessage("Admin: 1=open locker 2=logs 3=change pw 4=add admin 5=remove admin 0=logout");
}

/* ==========================================================================
 *  Discrete input-event handling
 * ========================================================================== */
static void HandleEvent(UI_EventTypeDef event, uint8_t digit)
{
    switch (s_state)
    {
        /* ---------------------------------------------------------- IDLE */
        case FLOW_STATE_IDLE:
        {
            if (event == UI_EVENT_BTN_DEPOSIT)
            {
                if (ChannelManager_CountEmptyLockers() == 0U)
                {
                    UI_ShowMessage("No empty locker available");
                    PlayAudio(FAUD_LOCKER_FULL);
                }
                else
                {
                    s_state = FLOW_STATE_DEPOSIT_SELECT_TYPE;
                    UI_SetScreenState("DEPOSIT_SELECT_TYPE");
                    UI_ShowMessage("Phone type? Press 1=Android  2=iPhone");
                    PlayAudio(FAUD_SELECT_PHONE_TYPE);
                }
            }
            else if (event == UI_EVENT_BTN_RETRIEVE)
            {
                s_state = FLOW_STATE_RETRIEVE_ENTER_PHONE;
                DigitBufReset();
                s_retrieve_phone_attempts = 0U;
                UI_SetScreenState("RETRIEVE_ENTER_PHONE");
                UI_ShowMessage("Enter your phone number");
                PlayAudio(FAUD_RETRIEVE_ENTER_PHONE);
            }
            else if (event == UI_EVENT_BTN_ADMIN)
            {
                s_state = FLOW_STATE_ADMIN_ENTER_PASSWORD;
                DigitBufReset();
                UI_SetScreenState("ADMIN_ENTER_PASSWORD");
                UI_ShowMessage("Enter admin password");
            }
            break;
        }

        /* ------------------------------------------------ DEPOSIT: type */
        case FLOW_STATE_DEPOSIT_SELECT_TYPE:
        {
            int8_t d = DigitFromEvent(event);
            if (d == 1)
            {
                s_phone_type = 0U; /* Android */
            }
            else if (d == 2)
            {
                s_phone_type = 1U; /* iPhone */
            }
            else if (event == UI_EVENT_CANCEL)
            {
                GoIdle();
                break;
            }
            else
            {
                break; /* ignore any other key here */
            }

            DigitBufReset();
            s_state = FLOW_STATE_DEPOSIT_ENTER_PHONE;
            UI_SetScreenState("DEPOSIT_ENTER_PHONE");
            UI_ShowMessage("Enter your phone number");
            PlayAudio(FAUD_ENTER_PHONE_NUMBER);
            break;
        }

        /* ----------------------------------------------- DEPOSIT: phone */
        case FLOW_STATE_DEPOSIT_ENTER_PHONE:
        {
            int8_t d = DigitFromEvent(event);
            if (d >= 0)
            {
                (void)DigitBufAppend((uint8_t)d, PHONE_NUMBER_DIGIT_COUNT);
            }
            else if (event == UI_EVENT_BACKSPACE)
            {
                (void)DigitBufBackspace();
            }
            else if (event == UI_EVENT_CONFIRM)
            {
                if (s_digit_len != PHONE_NUMBER_DIGIT_COUNT)
                {
                    UI_ShowMessage("Phone number must be 11 digits");
                    PlayAudio(FAUD_ENTER_PHONE_NUMBER); /* TODO(audio): no dedicated "invalid" clip */
                }
                else
                {
                    char msg[64];
                    s_state = FLOW_STATE_DEPOSIT_CONFIRM_PHONE;
                    UI_SetScreenState("DEPOSIT_CONFIRM_PHONE");
                    snprintf(msg, sizeof(msg), "Confirm number %s ? (Confirm/Cancel)", s_digit_buf);
                    UI_ShowMessage(msg);
                    PlayAudio(FAUD_CONFIRM_PHONE_NUMBER);
                }
            }
            else if (event == UI_EVENT_CANCEL)
            {
                GoIdle();
            }
            break;
        }

        /* --------------------------------------------- DEPOSIT: confirm */
        case FLOW_STATE_DEPOSIT_CONFIRM_PHONE:
        {
            if (event == UI_EVENT_CONFIRM)
            {
                s_state = FLOW_STATE_DEPOSIT_FINGER_SCAN1;
                SetDeadline(TIMEOUT_FINGERPRINT_FIRST_SCAN_SEC);
                UI_SetScreenState("DEPOSIT_FINGER_SCAN1");
                UI_ShowMessage("Place your finger on the sensor");
                PlayAudio(FAUD_PLACE_FINGER);
            }
            else if (event == UI_EVENT_CANCEL)
            {
                /* Per algorithm doc: "No" returns to the phone-number
                 * screen to edit, keeping the digits already entered. */
                s_state = FLOW_STATE_DEPOSIT_ENTER_PHONE;
                UI_SetScreenState("DEPOSIT_ENTER_PHONE");
                UI_ShowMessage("Edit the number");
                UI_ShowEntry(s_digit_buf);
            }
            break;
        }

        /* ---- DEPOSIT: fingerprint capture (sensor polling happens in
         * PollFingerprint(); only CANCEL is handled here) ---- */
        case FLOW_STATE_DEPOSIT_FINGER_SCAN1:
        case FLOW_STATE_DEPOSIT_FINGER_SCAN2:
        {
            if (event == UI_EVENT_CANCEL)
            {
                GoIdle();
            }
            break;
        }

        case FLOW_STATE_DEPOSIT_WAIT_DOOR_CLOSE:
        {
            break; /* physical door-close event, no touchscreen input meaningful here */
        }

        /* ---------------------------------------------- RETRIEVE: phone */
        case FLOW_STATE_RETRIEVE_ENTER_PHONE:
        {
            int8_t d = DigitFromEvent(event);
            if (d >= 0)
            {
                (void)DigitBufAppend((uint8_t)d, PHONE_NUMBER_DIGIT_COUNT);
            }
            else if (event == UI_EVENT_BACKSPACE)
            {
                (void)DigitBufBackspace();
            }
            else if (event == UI_EVENT_CONFIRM)
            {
                if (s_digit_len != PHONE_NUMBER_DIGIT_COUNT)
                {
                    UI_ShowMessage("Phone number must be 11 digits");
                    PlayAudio(FAUD_RETRIEVE_ENTER_PHONE);
                    break;
                }

                {
                    uint8_t found_index;
                    System_StatusTypeDef find_st = ChannelManager_FindLockerByPhone(s_digit_buf, &found_index);

                    if (find_st != SYS_OK)
                    {
                        s_retrieve_phone_attempts++;
                        if (s_retrieve_phone_attempts >= RETRIEVE_MAX_PHONE_ATTEMPTS)
                        {
                            UI_ShowMessage("Number not found. Returning to main menu");
                            PlayAudio(FAUD_PHONE_NOT_FOUND);
                            GoIdle();
                        }
                        else
                        {
                            char msg[64];
                            snprintf(msg, sizeof(msg), "Number not found (%u/%u) - try again",
                                     (unsigned int)s_retrieve_phone_attempts, (unsigned int)RETRIEVE_MAX_PHONE_ATTEMPTS);
                            UI_ShowMessage(msg);
                            PlayAudio(FAUD_PHONE_NOT_FOUND);
                            DigitBufReset();
                        }
                        break;
                    }

                    {
                        bool locked_out;
                        uint32_t seconds_remaining;
                        ChannelManager_IsLockerLockedOut(found_index, RTC_GetUnixTime(), &locked_out, &seconds_remaining);

                        if (locked_out)
                        {
                            char msg[64];
                            snprintf(msg, sizeof(msg), "This locker is locked for %u more minute(s)",
                                     (unsigned int)((seconds_remaining / 60U) + 1U));
                            UI_ShowMessage(msg);
                            PlayAudio(FAUD_LOCKER_LOCKED_30MIN);
                            GoIdle();
                        }
                        else
                        {
                            s_locker_index = found_index;
                            s_state = FLOW_STATE_RETRIEVE_FINGER_SCAN;
                            /* NOTE: typedef.h has no dedicated retrieve-fingerprint
                             * timeout; reusing TIMEOUT_FINGERPRINT_FIRST_SCAN_SEC. */
                            SetDeadline(TIMEOUT_FINGERPRINT_FIRST_SCAN_SEC);
                            UI_SetScreenState("RETRIEVE_FINGER_SCAN");
                            UI_ShowMessage("Place your finger on the sensor");
                            PlayAudio(FAUD_RETRIEVE_PLACE_FINGER);
                        }
                    }
                }
            }
            else if (event == UI_EVENT_CANCEL)
            {
                GoIdle();
            }
            break;
        }

        case FLOW_STATE_RETRIEVE_FINGER_SCAN:
        {
            if (event == UI_EVENT_CANCEL)
            {
                GoIdle();
            }
            break;
        }

        case FLOW_STATE_RETRIEVE_WAIT_DOOR_CLOSE:
        {
            break;
        }

        /* ------------------------------------------------------- ADMIN */
        case FLOW_STATE_ADMIN_ENTER_PASSWORD:
        {
            int8_t d = DigitFromEvent(event);
            if (d >= 0)
            {
                (void)DigitBufAppend((uint8_t)d, ADMIN_PASSWORD_DIGIT_COUNT);
            }
            else if (event == UI_EVENT_BACKSPACE)
            {
                (void)DigitBufBackspace();
            }
            else if (event == UI_EVENT_CONFIRM)
            {
                AdminRecordTypeDef admins[ADMIN_MAX_COUNT];
                bool found = false;
                uint8_t i;

                if (Storage_LoadAllAdmins(admins) == SYS_OK)
                {
                    for (i = 0U; i < ADMIN_MAX_COUNT; i++)
                    {
                        if (admins[i].in_use && (strcmp(admins[i].password, s_digit_buf) == 0))
                        {
                            s_admin_index = i;
                            found = true;
                            break;
                        }
                    }
                }

                if (found)
                {
                    PlayAudio(FAUD_ADMIN_WELCOME);
                    GoAdminMenu();
                }
                else
                {
                    UI_ShowMessage("Wrong admin password");
                    PlayAudio(FAUD_ADMIN_WRONG_PASSWORD);
                    DigitBufReset();
                }
            }
            else if (event == UI_EVENT_CANCEL)
            {
                GoIdle();
            }
            break;
        }

        case FLOW_STATE_ADMIN_MENU:
        {
            int8_t d = DigitFromEvent(event);

            if (d == 1)
            {
                s_state = FLOW_STATE_ADMIN_OPEN_LOCKER_ENTER_NUMBER;
                DigitBufReset();
                UI_SetScreenState("ADMIN_OPEN_LOCKER");
                UI_ShowMessage("Enter locker number to open");
            }
            else if (d == 2)
            {
                s_state = FLOW_STATE_ADMIN_VIEW_LOGS;
                s_log_page_index    = 0U;
                s_log_entry_in_page = 0U;
                UI_SetScreenState("ADMIN_VIEW_LOGS");
                UI_ShowMessage("Viewing logs. Confirm=next, Cancel=back");
            }
            else if (d == 3)
            {
                s_state = FLOW_STATE_ADMIN_CHANGE_PW_ENTER_NEW;
                DigitBufReset();
                UI_SetScreenState("ADMIN_CHANGE_PW");
                UI_ShowMessage("Enter new 6-digit password");
            }
            else if ((d == 4) || (d == 5))
            {
                if (s_admin_index != ADMIN_MAIN_INDEX)
                {
                    UI_ShowMessage("Only the main admin can do that");
                }
                else
                {
                    /* TODO: add-admin / remove-admin UI flow not implemented yet.
                     * Storage_SaveAdmin()/Storage_LoadAllAdmins() already support
                     * everything needed; this just needs the same enter-password
                     * (+ optional name) digit-entry pattern used elsewhere here. */
                    UI_ShowMessage("Not implemented yet");
                }
            }
            else if (d == 0)
            {
                GoIdle();
            }
            break;
        }

        case FLOW_STATE_ADMIN_OPEN_LOCKER_ENTER_NUMBER:
        {
            int8_t d = DigitFromEvent(event);
            if (d >= 0)
            {
                (void)DigitBufAppend((uint8_t)d, 2U); /* up to 2 digits, supports LOCKER_COUNT_MAX_SUPPORTED(32) */
            }
            else if (event == UI_EVENT_BACKSPACE)
            {
                (void)DigitBufBackspace();
            }
            else if (event == UI_EVENT_CONFIRM)
            {
                int locker_number = (s_digit_len > 0U) ? atoi(s_digit_buf) : 0;

                if ((locker_number < 1) || (locker_number > (int)LOCKER_COUNT))
                {
                    char msg[48];
                    snprintf(msg, sizeof(msg), "Enter a number 1-%u", (unsigned int)LOCKER_COUNT);
                    UI_ShowMessage(msg);
                }
                else
                {
                    uint8_t idx = (uint8_t)(locker_number - 1);
                    ChannelManager_AdminOpenLocker(idx, RTC_GetUnixTime());

                    char msg[48];
                    snprintf(msg, sizeof(msg), "Locker %d opened", locker_number);
                    UI_ShowMessage(msg);
                    GoAdminMenu();
                }
            }
            else if (event == UI_EVENT_CANCEL)
            {
                GoAdminMenu();
            }
            break;
        }

        case FLOW_STATE_ADMIN_VIEW_LOGS:
        {
            if (event == UI_EVENT_CONFIRM)
            {
                uint32_t page_count = 0U;
                LogEntryTypeDef entries[LOG_PAGE_SIZE];
                uint8_t entry_count = 0U;

                (void)Log_GetPageCount(&page_count);

                if (Log_ReadPage(s_log_page_index, entries, &entry_count) != SYS_OK || entry_count == 0U)
                {
                    UI_ShowMessage("No more log entries");
                    break;
                }

                if (s_log_entry_in_page >= entry_count)
                {
                    /* Move to next page */
                    s_log_page_index++;
                    s_log_entry_in_page = 0U;
                    if (s_log_page_index >= page_count)
                    {
                        UI_ShowMessage("End of log");
                        s_log_page_index = (page_count > 0U) ? (page_count - 1U) : 0U;
                        break;
                    }
                    if (Log_ReadPage(s_log_page_index, entries, &entry_count) != SYS_OK || entry_count == 0U)
                    {
                        UI_ShowMessage("No more log entries");
                        break;
                    }
                }

                {
                    LogEntryTypeDef *e = &entries[s_log_entry_in_page];
                    char msg[80];
                    /* NOTE: unix_time shown raw (no calendar formatter exposed
                     * by rtc.h yet for an arbitrary past timestamp) -- TODO:
                     * add RTC_UnixTimeToDateTime() and format this properly. */
                    snprintf(msg, sizeof(msg), "[p%lu #%lu] evt=%u locker=%u phone=%s t=%lu",
                             (unsigned long)(s_log_page_index + 1U), (unsigned long)(s_log_entry_in_page + 1U),
                             (unsigned int)e->event, (unsigned int)(e->locker_index + 1U),
                             e->phone_number, (unsigned long)e->unix_time);
                    UI_ShowMessage(msg);
                    s_log_entry_in_page++;
                }
            }
            else if (event == UI_EVENT_CANCEL)
            {
                GoAdminMenu();
            }
            break;
        }

        case FLOW_STATE_ADMIN_CHANGE_PW_ENTER_NEW:
        {
            int8_t d = DigitFromEvent(event);
            if (d >= 0)
            {
                (void)DigitBufAppend((uint8_t)d, ADMIN_PASSWORD_DIGIT_COUNT);
            }
            else if (event == UI_EVENT_BACKSPACE)
            {
                (void)DigitBufBackspace();
            }
            else if (event == UI_EVENT_CONFIRM)
            {
                if (s_digit_len != ADMIN_PASSWORD_DIGIT_COUNT)
                {
                    UI_ShowMessage("Password must be 6 digits");
                }
                else
                {
                    strncpy(s_admin_new_password, s_digit_buf, sizeof(s_admin_new_password));
                    s_state = FLOW_STATE_ADMIN_CHANGE_PW_CONFIRM_NEW;
                    DigitBufReset();
                    UI_ShowMessage("Re-enter the new password to confirm");
                }
            }
            else if (event == UI_EVENT_CANCEL)
            {
                GoAdminMenu();
            }
            break;
        }

        case FLOW_STATE_ADMIN_CHANGE_PW_CONFIRM_NEW:
        {
            int8_t d = DigitFromEvent(event);
            if (d >= 0)
            {
                (void)DigitBufAppend((uint8_t)d, ADMIN_PASSWORD_DIGIT_COUNT);
            }
            else if (event == UI_EVENT_BACKSPACE)
            {
                (void)DigitBufBackspace();
            }
            else if (event == UI_EVENT_CONFIRM)
            {
                if (strcmp(s_digit_buf, s_admin_new_password) != 0)
                {
                    UI_ShowMessage("Passwords did not match, try again");
                    s_state = FLOW_STATE_ADMIN_CHANGE_PW_ENTER_NEW;
                    DigitBufReset();
                }
                else
                {
                    AdminRecordTypeDef rec;
                    if (Storage_LoadAdmin(s_admin_index, &rec) == SYS_OK)
                    {
                        strncpy(rec.password, s_admin_new_password, sizeof(rec.password) - 1U);
                        rec.password[sizeof(rec.password) - 1U] = '\0';
                        (void)Storage_SaveAdmin(s_admin_index, &rec);
                        UI_ShowMessage("Password changed");
                    }
                    else
                    {
                        UI_ShowMessage("Failed to save new password");
                    }
                    GoAdminMenu();
                }
            }
            else if (event == UI_EVENT_CANCEL)
            {
                GoAdminMenu();
            }
            break;
        }

        default:
        {
            break;
        }
    }

    (void)digit; /* digit is only used through DigitFromEvent(event) above */
}

/* ==========================================================================
 *  Fingerprint sensor polling (throttled, non-blocking)
 * ========================================================================== */
static void PollFingerprint(void)
{
    uint32_t now = HAL_GetTick();
    uint8_t confirm;

    if ((now - s_last_fp_poll_tick) < 100U)
    {
        return; /* respect ZFM_POLL_TIMEOUT_MS-ish cadence */
    }
    s_last_fp_poll_tick = now;

    switch (s_state)
    {
        /* ------------------------------------------- deposit: 1st scan */
        case FLOW_STATE_DEPOSIT_FINGER_SCAN1:
        {
            if ((ZFM40_PollImage(&confirm) == ZFM_OK) && (confirm == ZFM_CONF_OK))
            {
                if ((ZFM40_GenChar(1, &confirm) == ZFM_OK) && (confirm == ZFM_CONF_OK))
                {
                    s_state = FLOW_STATE_DEPOSIT_FINGER_SCAN2;
                    s_fp_wait_lift = true;
                    SetDeadline(TIMEOUT_FINGERPRINT_CONFIRM_SCAN_SEC);
                    UI_SetScreenState("DEPOSIT_FINGER_SCAN2");
                    UI_ShowMessage("Lift your finger, then place it again");
                    PlayAudio(FAUD_PLACE_FINGER_AGAIN);
                }
                /* GenChar failure: stay put, user retries within the timeout. */
            }
            break;
        }

        /* ------------------------------------- deposit: confirm scan */
        case FLOW_STATE_DEPOSIT_FINGER_SCAN2:
        {
            if (s_fp_wait_lift)
            {
                ZFM_StatusTypeDef st = ZFM40_PollImage(&confirm);
                if ((st == ZFM_NACK) && (confirm == ZFM_CONF_NO_FINGER))
                {
                    s_fp_wait_lift = false; /* sensor now ready for the second real touch */
                }
                break;
            }

            if ((ZFM40_PollImage(&confirm) != ZFM_OK) || (confirm != ZFM_CONF_OK))
            {
                break; /* still waiting for the second touch */
            }

            {
                uint8_t confirm2;

                if ((ZFM40_GenChar(2, &confirm2) != ZFM_OK) || (confirm2 != ZFM_CONF_OK))
                {
                    break; /* bad capture, allow retry within the remaining timeout */
                }

                if ((ZFM40_RegModel(&confirm2) != ZFM_OK) || (confirm2 != ZFM_CONF_OK))
                {
                    UI_ShowMessage("The two fingerprint scans did not match");
                    PlayAudio(FAUD_FINGERPRINT_MISMATCH);
                    GoIdle();
                    break;
                }

                {
                    uint16_t template_count;

                    if ((ZFM40_GetTemplateCount(&template_count, &confirm2) != ZFM_OK) || (confirm2 != ZFM_CONF_OK))
                    {
                        UI_ShowMessage("Fingerprint sensor internal error");
                        GoIdle();
                        break;
                    }

                    if ((ZFM40_StoreChar(1, template_count, &confirm2) != ZFM_OK) || (confirm2 != ZFM_CONF_OK))
                    {
                        UI_ShowMessage("Failed to store fingerprint");
                        GoIdle();
                        break;
                    }

                    {
                        uint8_t new_locker;
                        PhoneTypeTypeDef ptype = (s_phone_type == 0U) ? PHONE_TYPE_ANDROID : PHONE_TYPE_IPHONE;
                        System_StatusTypeDef dep_st = ChannelManager_StartDeposit(
                            s_digit_buf, ptype, template_count, RTC_GetUnixTime(), &new_locker);

                        if (dep_st != SYS_OK)
                        {
                            /* Race: another user took the last empty locker
                             * between the earlier CountEmptyLockers() check and now. */
                            UI_ShowMessage("No empty locker available");
                            PlayAudio(FAUD_LOCKER_FULL);
                            GoIdle();
                            break;
                        }

                        {
                            char msg[64];
                            s_locker_index = new_locker;
                            s_state = FLOW_STATE_DEPOSIT_WAIT_DOOR_CLOSE;
                            ClearDeadline(); /* Channel Manager owns the door-close timeout now */

                            snprintf(msg, sizeof(msg), "Locker %u opened. Place phone and close the door",
                                     (unsigned int)(new_locker + 1U));
                            UI_SetScreenState("DEPOSIT_WAIT_DOOR_CLOSE");
                            UI_ShowMessage(msg);
                        }
                        PlayAudio(FAUD_LOCKER_OPENED_DEPOSIT);
                    }
                }
            }
            break;
        }

        /* ---------------------------------------------- retrieve scan */
        case FLOW_STATE_RETRIEVE_FINGER_SCAN:
        {
            LockerRecordTypeDef rec;

            if (ChannelManager_GetLockerState(s_locker_index, &rec) != SYS_OK)
            {
                GoIdle();
                break;
            }

            if ((ZFM40_PollImage(&confirm) != ZFM_OK) || (confirm != ZFM_CONF_OK))
            {
                break; /* no finger yet */
            }

            {
                uint8_t confirm2;

                if ((ZFM40_LoadChar(1, rec.fingerprint_id, &confirm2) != ZFM_OK) || (confirm2 != ZFM_CONF_OK))
                {
                    UI_ShowMessage("Fingerprint sensor internal error");
                    GoIdle();
                    break;
                }

                if ((ZFM40_GenChar(2, &confirm2) != ZFM_OK) || (confirm2 != ZFM_CONF_OK))
                {
                    break; /* bad capture, retry within the timeout */
                }

                {
                    uint16_t score = 0U;
                    (void)ZFM40_Match(&score, &confirm2);

                    if ((confirm2 == ZFM_CONF_OK) && (score >= ZFM_MATCH_SCORE_THRESHOLD))
                    {
                        char msg[64];

                        ChannelManager_ClearFailedAttempts(s_locker_index);
                        (void)ChannelManager_StartRetrieve(s_locker_index, RTC_GetUnixTime());

                        s_state = FLOW_STATE_RETRIEVE_WAIT_DOOR_CLOSE;
                        ClearDeadline();

                        snprintf(msg, sizeof(msg), "Locker %u opened. Take your phone and close the door",
                                 (unsigned int)(s_locker_index + 1U));
                        UI_SetScreenState("RETRIEVE_WAIT_DOOR_CLOSE");
                        UI_ShowMessage(msg);
                        PlayAudio(FAUD_LOCKER_OPENED_RETRIEVE);
                    }
                    else
                    {
                        bool now_locked = false;
                        ChannelManager_RegisterFailedFingerprintAttempt(s_locker_index, RTC_GetUnixTime(), &now_locked);

                        if (now_locked)
                        {
                            UI_ShowMessage("This locker is locked for 30 minutes");
                            PlayAudio(FAUD_LOCKER_LOCKED_30MIN);
                            GoIdle();
                        }
                        else
                        {
                            UI_ShowMessage("Fingerprint did not match, try again");
                            PlayAudio(FAUD_FINGERPRINT_NOT_MATCH);
                            SetDeadline(TIMEOUT_FINGERPRINT_FIRST_SCAN_SEC); /* fresh window for the retry */
                        }
                    }
                }
            }
            break;
        }

        default:
        {
            break; /* no fingerprint activity relevant in other states */
        }
    }
}

/* ==========================================================================
 *  State timeouts (phone/fingerprint entry windows -- NOT the door-close
 *  timeout, which Channel Manager tracks internally once a locker is open)
 * ========================================================================== */
static void CheckTimeouts(void)
{
    if (!s_deadline_active)
    {
        return;
    }
    if (HAL_GetTick() < s_deadline_tick)
    {
        return;
    }

    switch (s_state)
    {
        case FLOW_STATE_DEPOSIT_FINGER_SCAN1:
        case FLOW_STATE_DEPOSIT_FINGER_SCAN2:
        case FLOW_STATE_RETRIEVE_FINGER_SCAN:
        {
            UI_ShowMessage("Fingerprint entry timed out");
            PlayAudio(FAUD_FINGERPRINT_TIMEOUT);
            GoIdle();
            break;
        }
        default:
        {
            ClearDeadline();
            break;
        }
    }
}

/* ==========================================================================
 *  Once-per-second background tick: Channel Manager polling, locker-state
 *  mirror to the UI, clock/date display, and the door-left-open reminder.
 * ========================================================================== */
static void PeriodicTick(void)
{
    uint32_t now = HAL_GetTick();
    uint32_t unix_now;
    uint32_t reminder_bitmask = 0U;
    RTC_DateTimeTypeDef dt;
    uint8_t i;

    if ((now - s_last_periodic_tick) < 1000U)
    {
        return;
    }
    s_last_periodic_tick = now;

    unix_now = RTC_GetUnixTime();

    if (RTC_GetDateTime(&dt) == SYS_OK)
    {
        char time_str[16];
        char date_str[16];
        RTC_ShamsiDateTypeDef shamsi;

        snprintf(time_str, sizeof(time_str), "%02u:%02u:%02u", dt.hour, dt.minute, dt.second);
        RTC_GregorianToShamsi(dt.year, dt.month, dt.day, &shamsi);
        snprintf(date_str, sizeof(date_str), "%04u/%02u/%02u", shamsi.year, shamsi.month, shamsi.day);
        UI_ShowClock(time_str, date_str);
    }

    (void)ChannelManager_Poll(unix_now, &reminder_bitmask);

    for (i = 0U; i < LOCKER_COUNT; i++)
    {
        LockerRecordTypeDef rec;

        if (ChannelManager_GetLockerState(i, &rec) != SYS_OK)
        {
            continue;
        }

        {
            bool is_open    = (rec.state == LOCKER_STATE_AWAITING_DEPOSIT_CLOSE) ||
                               (rec.state == LOCKER_STATE_AWAITING_RETRIEVE_CLOSE);
            bool led_on     = is_open || (rec.state == LOCKER_STATE_DOOR_LEFT_OPEN_FAULT);
            bool blinking   = (rec.state == LOCKER_STATE_DOOR_LEFT_OPEN_FAULT) ||
                               ((rec.state == LOCKER_STATE_OCCUPIED) && (rec.flags.bits.door_open == 1U));
            UI_SetLockerState(i, is_open, led_on, blinking);
        }

        /* Periodic "door left open" reminder -- only surfaced on the idle
         * screen so it doesn't interrupt a different user's active session
         * on the single shared touchscreen. */
        if (((reminder_bitmask & (1UL << i)) != 0UL) && (s_state == FLOW_STATE_IDLE))
        {
            char msg[48];
            snprintf(msg, sizeof(msg), "Warning: locker %u door is open", (unsigned int)(i + 1U));
            UI_ShowMessage(msg);
            PlayAudio(FAUD_DOOR_LEFT_OPEN_REMINDER);
        }

        /* An OCCUPIED locker whose door reads open is an anomaly Channel
         * Manager deliberately does not act on itself (see channel_manager.c) --
         * surface it here instead, idle-screen only, same reasoning as above. */
        if ((rec.state == LOCKER_STATE_OCCUPIED) && (rec.flags.bits.door_open == 1U) && (s_state == FLOW_STATE_IDLE))
        {
            char msg[48];
            snprintf(msg, sizeof(msg), "Warning: locker %u door is open unexpectedly", (unsigned int)(i + 1U));
            UI_ShowMessage(msg);
        }

        /* Finalize / fault detection for the locker THIS session is actively watching. */
        if ((i == s_locker_index) &&
            ((s_state == FLOW_STATE_DEPOSIT_WAIT_DOOR_CLOSE) || (s_state == FLOW_STATE_RETRIEVE_WAIT_DOOR_CLOSE)))
        {
            if ((rec.state == LOCKER_STATE_OCCUPIED) && (s_state == FLOW_STATE_DEPOSIT_WAIT_DOOR_CLOSE))
            {
                UI_ShowMessage("Deposit completed successfully");
                PlayAudio(FAUD_DEPOSIT_SUCCESS);
                GoIdle();
            }
            else if ((rec.state == LOCKER_STATE_EMPTY) && (s_state == FLOW_STATE_RETRIEVE_WAIT_DOOR_CLOSE))
            {
                UI_ShowMessage("Retrieve completed successfully");
                PlayAudio(FAUD_RETRIEVE_SUCCESS);
                GoIdle();
            }
            else if (rec.state == LOCKER_STATE_DOOR_LEFT_OPEN_FAULT)
            {
                char msg[64];
                bool was_deposit = (s_state == FLOW_STATE_DEPOSIT_WAIT_DOOR_CLOSE);

                snprintf(msg, sizeof(msg), "Time to close locker %u door ran out", (unsigned int)(i + 1U));
                UI_ShowMessage(msg);
                PlayAudio(was_deposit ? FAUD_DEPOSIT_TIMEOUT : FAUD_RETRIEVE_TIMEOUT_DOOR_OPEN);

                /* Return the shared screen to idle for other users; Channel
                 * Manager keeps monitoring this locker in the background and
                 * will finalize it whenever the door is eventually closed,
                 * even after this session has moved on. */
                GoIdle();
            }
        }
    }
}
