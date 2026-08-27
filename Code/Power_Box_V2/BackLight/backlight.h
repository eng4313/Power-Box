/**
  ******************************************************************************
  * @file    backlight.h
  * @brief   Backlight control via MP3202 boost LED driver (MC3202.SchDoc).
  *          EN pin doubles as ON/OFF enable and PWM dimming input.
  *          PWM output confirmed on PB6 (TIM4_CH1) -- wire now connected
  *          per project update.
  ******************************************************************************
  */

#ifndef __BACKLIGHT_H
#define __BACKLIGHT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

#define BL_PWM_GPIO_PORT        GPIOB
#define BL_PWM_GPIO_PIN         GPIO_PIN_6
#define BL_TIM_INSTANCE         TIM4
#define BL_TIM_CHANNEL          TIM_CHANNEL_1

/* Timer runs at APB1 timer clock (typically 90MHz on this project's max
 * clock config, since APB1 timer clock = 2x APB1 = HCLK/2 when APB1
 * prescaler is /4). Prescaler/period chosen for a ~1kHz PWM frequency,
 * matching the "PWM 1KHz" note on the MC3202 schematic sheet. */
#define BL_TIM_PRESCALER         899U   /* 90MHz / (899+1) = 100kHz timer tick */
#define BL_TIM_PERIOD            99U    /* 100kHz / (99+1) = 1kHz PWM frequency */
typedef enum
{
    BL_OK    = 0x00U,
    BL_ERROR = 0x01U
} BL_StatusTypeDef;

/**
  * @brief  Init timer PWM output driving MP3202 EN pin. Backlight starts
  *         OFF (0% duty) after init; call Backlight_SetBrightness() to
  *         turn it on.
  */
BL_StatusTypeDef Backlight_Init(void);

/**
  * @brief  Set brightness level.
  * @param  percent: 0 (off) to 100 (full brightness)
  */
BL_StatusTypeDef Backlight_SetBrightness(uint8_t percent);

#ifdef __cplusplus
}
#endif

#endif /* __BACKLIGHT_H */
