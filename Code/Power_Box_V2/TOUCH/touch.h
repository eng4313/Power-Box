/**
  ******************************************************************************
  * @file    touch.h
  * @brief   Driver for XPT2046 resistive touch controller
  *          (Touch XPT2046.SchDoc). Sits on the shared SPI1 bus,
  *          CS = PD7, PENIRQ = PG9 (touch-detect interrupt line, active low).
  ******************************************************************************
  */

#ifndef __TOUCH_H
#define __TOUCH_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>
#include <stdbool.h>
#include "spi_bus.h"

/* ---- XPT2046 control byte commands (12-bit, differential mode) ----
 * Bit7=Start, Bits6-4=Channel, Bit3=Mode(0=12bit), Bit2=SER/DFR(0=diff),
 * Bits1-0=Power-down mode (00 = power down between conversions) */
#define CMD_READ_X                0xD0U  /* channel 5 (X position) */
#define CMD_READ_Y                0x90U  /* channel 1 (Y position) */

#define TOUCH_CS_GPIO_PORT        GPIOD
#define TOUCH_CS_GPIO_PIN         GPIO_PIN_7

#define TOUCH_PENIRQ_GPIO_PORT    GPIOG
#define TOUCH_PENIRQ_GPIO_PIN     GPIO_PIN_9

/* Resistive touch controllers are typically limited to a few MHz;
 * this is a conservative, safe value regardless of which other device
 * previously used the shared bus at a higher speed. */
#define TOUCH_SPI_PRESCALER       SPI_BAUDRATEPRESCALER_64
#define TOUCH_SPI_TIMEOUT_MS      50U

/* Maximum allowed difference between two consecutive raw samples for a
 * reading to be considered stable (simple noise filter for resistive
 * panels, which are electrically noisy compared to capacitive ones). */
#define TOUCH_NOISE_THRESHOLD     40U

typedef enum
{
    TOUCH_OK    = 0x00U,
    TOUCH_ERROR = 0x01U
} TOUCH_StatusTypeDef;

typedef struct
{
    uint16_t x;   /* raw ADC value, 0-4095 (before calibration to screen px) */
    uint16_t y;   /* raw ADC value, 0-4095 */
} TOUCH_RawPointTypeDef;

/**
  * @brief  Init CS + PENIRQ GPIO pins.
  * @note   SPI_Bus_Init() must already have been called before this.
  */
TOUCH_StatusTypeDef Touch_Init(void);

/**
  * @brief  Send one dummy SPI conversion command to the controller.
  *
  *         XPT2046 gotcha: after power-up, the internal Y-switch that
  *         drives PENIRQ is OFF and stays OFF until at least one A/D
  *         conversion has been clocked out with PD1:PD0 = 00 (power-down
  *         between conversions, which is what CMD_READ_X/CMD_READ_Y
  *         already use). Until that happens PENIRQ will never assert,
  *         no matter how hard the panel is pressed.
  *
  *         Call this once right after Touch_Init() to "wake up" PENIRQ
  *         detection. The result is discarded; only the side effect
  *         (enabling the Y-switch) matters here.
  */
void Touch_ForceConversion(void);

/**
  * @brief  Fast check via PENIRQ pin (no SPI transaction) whether the
  *         panel is currently being touched. Safe to poll frequently.
  */
bool Touch_IsPressed(void);

/**
  * @brief  Perform SPI conversion and return raw ADC X/Y (0-4095 each).
  *         Internally reads twice and keeps the reading only if the two
  *         samples are close enough (simple noise/debounce filter),
  *         since resistive touch panels are electrically noisy.
  * @retval TOUCH_OK if a stable reading was obtained, TOUCH_ERROR
  *         otherwise (e.g. panel released mid-read, or noisy samples).
  */
TOUCH_StatusTypeDef Touch_ReadRaw(TOUCH_RawPointTypeDef *pPoint);

#ifdef __cplusplus
}
#endif

#endif /* __TOUCH_H */
