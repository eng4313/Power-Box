/**
  ******************************************************************************
  * @file    ui_interface_uart.c
  * @brief   UI implementation over UART7 (PC debug tool)
  *          Compiled only when UI_HARD_WARE_MODE == 0U
  ******************************************************************************
  */

#include "ui_interface.h"
#include "debug_link.h"
#include <string.h>
#include <stdio.h>

#if (UI_HARD_WARE_MODE == 0U)

/* ==========================================================================
 *  Constants & static data
 * ========================================================================== */
#define UI_EVENT_QUEUE_SIZE   32U
#define UI_DIGIT_BUFFER_SIZE  32U

typedef struct
{
    UI_EventTypeDef event;
    uint8_t         digit;
} UI_QueuedEventTypeDef;

static UI_QueuedEventTypeDef s_event_queue[UI_EVENT_QUEUE_SIZE];
static volatile uint8_t s_event_head = 0;
static volatile uint8_t s_event_tail = 0;

static char s_entry_digits[UI_DIGIT_BUFFER_SIZE] = "";
static char s_message[64] = "";
static char s_clock_str[16] = "";
static char s_date_str[16] = "";
static char s_screen_state[32] = "";

/* Locker states: bit0=open, bit1=led_on, bit2=blinking */
static uint8_t s_locker_states[LOCKER_COUNT];

/* ==========================================================================
 *  Private functions
 * ========================================================================== */

/**
  * @brief  Queue an event for later processing by main loop
  */
static void UI_QueueEvent(UI_EventTypeDef event, uint8_t digit)
{
    uint8_t next = (s_event_tail + 1) % UI_EVENT_QUEUE_SIZE;
    if (next != s_event_head)  /* Queue not full */
    {
        s_event_queue[s_event_tail].event = event;
        s_event_queue[s_event_tail].digit = digit;
        s_event_tail = next;
    }
}

/* ==========================================================================
 *  Callback from DebugLink - processes incoming UART lines
 *  This function is called from DebugLink_Process() when a line is received
 * ========================================================================== */
void UI_ProcessIncomingLine(const char *line)
{
    if (line == NULL)
    {
        return;
    }

    /* Lines from MCU to PC start with "OUT:" - ignore them */
    if (strncmp(line, "OUT:", 4) == 0)
    {
        return;
    }

    /* Only process "IN:" commands from PC */
    if (strncmp(line, "IN:", 3) != 0)
    {
        return;
    }

    const char *cmd = line + 3;  /* Skip "IN:" */

    /* ---- Main buttons ---- */
    if (strcmp(cmd, "BTN_DEPOSIT") == 0)
    {
        UI_QueueEvent(UI_EVENT_BTN_DEPOSIT, 0);
    }
    else if (strcmp(cmd, "BTN_RETRIEVE") == 0)
    {
        UI_QueueEvent(UI_EVENT_BTN_RETRIEVE, 0);
    }
    else if (strcmp(cmd, "BTN_ADMIN") == 0)
    {
        UI_QueueEvent(UI_EVENT_BTN_ADMIN, 0);
    }

    /* ---- Phone type selection ---- */
    else if (strcmp(cmd, "PHONE_ANDROID") == 0)
    {
        UI_QueueEvent(UI_EVENT_PHONE_ANDROID, 0);
    }
    else if (strcmp(cmd, "PHONE_IPHONE") == 0)
    {
        UI_QueueEvent(UI_EVENT_PHONE_IPHONE, 0);
    }

    /* ---- Digit input ---- */
    else if (strncmp(cmd, "DIGIT:", 6) == 0)
    {
        char d = cmd[6];
        if ((d >= '0') && (d <= '9'))
        {
            UI_QueueEvent((UI_EventTypeDef)(UI_EVENT_DIGIT_0 + (d - '0')), (uint8_t)(d - '0'));
        }
    }

    /* ---- Control buttons ---- */
    else if (strcmp(cmd, "BACKSPACE") == 0)
    {
        UI_QueueEvent(UI_EVENT_BACKSPACE, 0);
    }
    else if (strcmp(cmd, "CONFIRM") == 0)
    {
        UI_QueueEvent(UI_EVENT_CONFIRM, 0);
    }
    else if (strcmp(cmd, "CANCEL") == 0)
    {
        UI_QueueEvent(UI_EVENT_CANCEL, 0);
    }
}

/* ==========================================================================
 *  Public API implementation
 * ========================================================================== */

System_StatusTypeDef UI_Init(void)
{
    /* DebugLink is already initialized in main.c */
    memset(s_locker_states, 0, sizeof(s_locker_states));
    s_event_head = 0;
    s_event_tail = 0;
    s_entry_digits[0] = '\0';

    /* Send welcome message to PC tool */
    DebugLink_SendLine("OUT:MSG:Power Box V2 Ready");
    DebugLink_SendLine("OUT:SCREEN:IDLE");

    return SYS_OK;
}

void UI_Tick(void)
{
    /* DebugLink_Process() is called in main loop separately */
    /* Nothing extra needed for UART mode */
}

bool UI_GetNextEvent(UI_EventTypeDef *out_event, uint8_t *out_digit)
{
    if (s_event_head == s_event_tail)
    {
        return false;
    }

    *out_event = s_event_queue[s_event_head].event;
    *out_digit = s_event_queue[s_event_head].digit;
    s_event_head = (s_event_head + 1) % UI_EVENT_QUEUE_SIZE;

    return true;
}

void UI_ShowMessage(const char *text)
{
    if (text == NULL)
    {
        return;
    }

    strncpy(s_message, text, sizeof(s_message) - 1);
    s_message[sizeof(s_message) - 1] = '\0';

    char buf[128];
    snprintf(buf, sizeof(buf), "OUT:MSG:%s", s_message);
    DebugLink_SendLine(buf);
}

void UI_ShowEntry(const char *digits)
{
    if (digits == NULL)
    {
        s_entry_digits[0] = '\0';
        return;
    }

    strncpy(s_entry_digits, digits, sizeof(s_entry_digits) - 1);
    s_entry_digits[sizeof(s_entry_digits) - 1] = '\0';

    /* PC tool shows digits on its own virtual keypad display */
    /* No need to send anything extra */
}

void UI_ShowClock(const char *time_str, const char *date_str)
{
    if (time_str != NULL)
    {
        strncpy(s_clock_str, time_str, sizeof(s_clock_str) - 1);
        s_clock_str[sizeof(s_clock_str) - 1] = '\0';
        char buf[64];
        snprintf(buf, sizeof(buf), "OUT:CLOCK:%s", time_str);
        DebugLink_SendLine(buf);
    }

    if (date_str != NULL)
    {
        strncpy(s_date_str, date_str, sizeof(s_date_str) - 1);
        s_date_str[sizeof(s_date_str) - 1] = '\0';
        char buf[64];
        snprintf(buf, sizeof(buf), "OUT:DATE:%s", date_str);
        DebugLink_SendLine(buf);
    }
}

void UI_SetLockerState(uint8_t locker_index, bool is_open, bool led_on, bool blinking)
{
    if (locker_index >= LOCKER_COUNT)
    {
        return;
    }

    /* Update internal state */
    uint8_t state = 0;
    if (is_open)    state |= 0x01;
    if (led_on)     state |= 0x02;
    if (blinking)   state |= 0x04;
    s_locker_states[locker_index] = state;

    /* Send to PC tool */
    char buf[32];
    if (blinking)
    {
        snprintf(buf, sizeof(buf), "OUT:LOCKER:%u:LED_BLINK", locker_index + 1);
    }
    else if (is_open)
    {
        snprintf(buf, sizeof(buf), "OUT:LOCKER:%u:OPEN", locker_index + 1);
    }
    else if (led_on)
    {
        snprintf(buf, sizeof(buf), "OUT:LOCKER:%u:LED_ON", locker_index + 1);
    }
    else
    {
        snprintf(buf, sizeof(buf), "OUT:LOCKER:%u:CLOSED", locker_index + 1);
    }
    DebugLink_SendLine(buf);
}

void UI_SetScreenState(const char *state_name)
{
    if (state_name == NULL)
    {
        return;
    }

    strncpy(s_screen_state, state_name, sizeof(s_screen_state) - 1);
    s_screen_state[sizeof(s_screen_state) - 1] = '\0';

    char buf[64];
    snprintf(buf, sizeof(buf), "OUT:SCREEN:%s", state_name);
    DebugLink_SendLine(buf);
}

#endif /* (UI_HARD_WARE_MODE == 0U) */
