/*
 * debug_link.h
 * -----------------------------------------------------------------------
 * Minimal UART7 test module for Power Box V2.
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

/* Call once per main loop iteration */
void DebugLink_Process(void);

/* Sends a single line, appends '\n' automatically */
DebugLinkStatusTypeDef DebugLink_SendLine(const char *text);

/**
  * @brief  Register a callback for incoming lines
  * @param  callback: function to call with each received line
  *         The callback receives a null-terminated string without \n or \r
  */
void DebugLink_RegisterCallback(void (*callback)(const char *line));

/*
 * Must be called from HAL_UARTEx_RxEventCallback()
 */
void DebugLink_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size);

#endif /* DEBUG_LINK_H */
