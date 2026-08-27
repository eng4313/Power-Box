/**
  ******************************************************************************
  * @file    backlight.c
  * @brief   Backlight PWM driver implementation (TIM4_CH1 -> PB6, AF2)
  ******************************************************************************
  */

#include "backlight.h"

static TIM_HandleTypeDef htim4;

static BL_StatusTypeDef Backlight_MspInit(void);

BL_StatusTypeDef Backlight_Init(void)
{
    TIM_OC_InitTypeDef sConfigOC = {0};

    if (Backlight_MspInit() != BL_OK)
    {
        return BL_ERROR;
    }

    htim4.Instance               = BL_TIM_INSTANCE;
    htim4.Init.Prescaler         = BL_TIM_PRESCALER;
    htim4.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim4.Init.Period            = BL_TIM_PERIOD;
    htim4.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

    if (HAL_TIM_PWM_Init(&htim4) != HAL_OK)
    {
        return BL_ERROR;
    }

    sConfigOC.OCMode     = TIM_OCMODE_PWM1;
    sConfigOC.Pulse      = 0; /* start OFF */
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;

    if (HAL_TIM_PWM_ConfigChannel(&htim4, &sConfigOC, BL_TIM_CHANNEL) != HAL_OK)
    {
        return BL_ERROR;
    }

    if (HAL_TIM_PWM_Start(&htim4, BL_TIM_CHANNEL) != HAL_OK)
    {
        return BL_ERROR;
    }

    return BL_OK;
}

BL_StatusTypeDef Backlight_SetBrightness(uint8_t percent)
{
    uint32_t pulse;

    if (percent > 100U)
    {
        percent = 100U;
    }

    pulse = ((uint32_t)(BL_TIM_PERIOD + 1U) * percent) / 100U;

    __HAL_TIM_SET_COMPARE(&htim4, BL_TIM_CHANNEL, pulse);

    return BL_OK;
}

static BL_StatusTypeDef Backlight_MspInit(void)
{
    GPIO_InitTypeDef GPIO_Init = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_TIM4_CLK_ENABLE();

    GPIO_Init.Pin       = BL_PWM_GPIO_PIN;
    GPIO_Init.Mode      = GPIO_MODE_AF_PP;
    GPIO_Init.Pull      = GPIO_NOPULL;
    GPIO_Init.Speed     = GPIO_SPEED_FREQ_LOW;
    GPIO_Init.Alternate  = GPIO_AF2_TIM4;
    HAL_GPIO_Init(BL_PWM_GPIO_PORT, &GPIO_Init);

    return BL_OK;
}
