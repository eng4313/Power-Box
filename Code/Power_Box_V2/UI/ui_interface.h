/**
  ******************************************************************************
  * @file    ui_interface.h
  * @brief   Abstract UI layer: switches between UART debug and LCD/Touch
  *          based on UI_HARD_WARE_MODE in typedef.h
  ******************************************************************************
  */

#ifndef __UI_INTERFACE_H
#define __UI_INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "typedef.h"
#include <stdbool.h>

/* ==========================================================================
 *  UI Event Types (identical for both UART and LCD modes)
 * ========================================================================== */
typedef enum
{
    UI_EVENT_NONE = 0,
    UI_EVENT_BTN_DEPOSIT,
    UI_EVENT_BTN_RETRIEVE,
    UI_EVENT_BTN_ADMIN,
    UI_EVENT_PHONE_ANDROID,
    UI_EVENT_PHONE_IPHONE,
    UI_EVENT_DIGIT_0,
    UI_EVENT_DIGIT_1,
    UI_EVENT_DIGIT_2,
    UI_EVENT_DIGIT_3,
    UI_EVENT_DIGIT_4,
    UI_EVENT_DIGIT_5,
    UI_EVENT_DIGIT_6,
    UI_EVENT_DIGIT_7,
    UI_EVENT_DIGIT_8,
    UI_EVENT_DIGIT_9,
    UI_EVENT_BACKSPACE,
    UI_EVENT_CONFIRM,
    UI_EVENT_CANCEL,
    UI_EVENT_TIMEOUT,
} UI_EventTypeDef;

/* ==========================================================================
 *  Public API - same interface for both hardware modes
 * ========================================================================== */

/**
  * @brief  Initialize UI subsystem (UART or LCD/Touch)
  * @retval SYS_OK or SYS_ERROR
  */
System_StatusTypeDef UI_Init(void);

/**
  * @brief  Periodic tick: process incoming events from UART or Touch
  */
void UI_Tick(void);

/**
  * @brief  Get next user event from the queue
  * @param  out_event: received event type
  * @param  out_digit: if event is DIGIT_x, contains the digit (0-9)
  * @retval true if event available, false if queue empty
  */
bool UI_GetNextEvent(UI_EventTypeDef *out_event, uint8_t *out_digit);

/**
  * @brief  Show main message on screen (OUT:MSG in UART mode)
  */
void UI_ShowMessage(const char *text);

/**
  * @brief  Show entered digits (phone number, password, etc.)
  */
void UI_ShowEntry(const char *digits);

/**
  * @brief  Show clock and date (OUT:CLOCK and OUT:DATE)
  */
void UI_ShowClock(const char *time_str, const char *date_str);

/**
  * @brief  Update locker visual state (OUT:LOCKER in UART mode)
  */
void UI_SetLockerState(uint8_t locker_index, bool is_open, bool led_on, bool blinking);

/**
  * @brief  Show current screen state (OUT:SCREEN)
  */
void UI_SetScreenState(const char *state_name);

#if (UI_HARD_WARE_MODE == 0U)
/* UART mode only: callback for incoming lines from DebugLink */
void UI_ProcessIncomingLine(const char *line);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __UI_INTERFACE_H */
