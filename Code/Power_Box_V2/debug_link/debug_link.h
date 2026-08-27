/*
 * debug_link.h
 * -----------------------------------------------------------------------
 * Minimal UART7 test module for Power Box V2.
 * Purpose: verify PC <-> MCU line-based communication over UART7 before
 * building the real deposit/retrieve state machine on top of it.
 *
 * Protocol (see power_box_debug_ui.py for the PC-side implementation):
 *   MCU -> PC : "OUT:<TYPE>:<PARAMS>\n"
 *   PC  -> MCU: "IN:<TYPE>:<PARAMS>\n"
 *
 * This module only implements:
 *   - Periodic test transmissions (CLOCK / MSG / LOCKER cycling)
 *   - Reception of any incoming line and echoing it back as OUT:LOG
 *
 * No real system logic here. This is purely a communication sanity check.
 * -----------------------------------------------------------------------
 */

#ifndef DEBUG_LINK_H
#define DEBUG_LINK_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

typedef enum
{
    DEBUG_LINK_OK = 0,
    DEBUG_LINK_ERROR,
    DEBUG_LINK_BUSY
} DebugLinkStatusTypeDef;

/* Must be called once after MX_USART7_UART_Init() in main.c */
DebugLinkStatusTypeDef DebugLink_Init(UART_HandleTypeDef *huart);

/* Call once per main loop iteration. Handles periodic TX and processes
 * any line received since the last call. */
void DebugLink_Process(void);

/* Sends a single line, appends '\n' automatically. Blocking (HAL_UART_Transmit). */
DebugLinkStatusTypeDef DebugLink_SendLine(const char *text);

/*
 * Must be called from HAL_UARTEx_RxEventCallback() in main.c / stm32f4xx_it.c
 * (or wherever HAL callbacks are implemented), forwarding the same
 * parameters HAL provides. This module checks huart against its own
 * handle internally, so it is safe to call unconditionally even if other
 * UARTs also use this callback.
 */
void DebugLink_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size);

#endif /* DEBUG_LINK_H */
