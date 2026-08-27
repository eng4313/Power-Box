/**
  ******************************************************************************
  * @file    channel_hw.c
  * @brief   See channel_hw.h. Pin table transcribed from Power_Box_V2.ioc
  *          (GPIO_Label entries LOCK_1..8 / LD_1..8 / DOOR_1..8).
  ******************************************************************************
  */

#include "channel_hw.h"

typedef struct
{
    GPIO_TypeDef *port;
    uint16_t      pin;
} ChannelHW_PinTypeDef;

/* Index i (0..LOCKER_COUNT-1) = locker (i+1) = CubeMX label suffix (i+1). */
static const ChannelHW_PinTypeDef lock_pins[LOCKER_COUNT] =
{
    { GPIOC, GPIO_PIN_1  },  /* LOCK_1 */
    { GPIOA, GPIO_PIN_2  },  /* LOCK_2 */
    { GPIOC, GPIO_PIN_4  },  /* LOCK_3 */
    { GPIOB, GPIO_PIN_13 },  /* LOCK_4 */
    { GPIOD, GPIO_PIN_11 },  /* LOCK_5 */
    { GPIOG, GPIO_PIN_2  },  /* LOCK_6 */
    { GPIOC, GPIO_PIN_11 },  /* LOCK_7 */
    { GPIOD, GPIO_PIN_4  },  /* LOCK_8 */
};

static const ChannelHW_PinTypeDef led_pins[LOCKER_COUNT] =
{
    { GPIOA, GPIO_PIN_0  },  /* LD_1 */
    { GPIOA, GPIO_PIN_5  },  /* LD_2 */
    { GPIOC, GPIO_PIN_5  },  /* LD_3 */
    { GPIOB, GPIO_PIN_14 },  /* LD_4 */
    { GPIOD, GPIO_PIN_12 },  /* LD_5 */
    { GPIOG, GPIO_PIN_3  },  /* LD_6 */
    { GPIOC, GPIO_PIN_12 },  /* LD_7 */
    { GPIOD, GPIO_PIN_5  },  /* LD_8 */
};

static const ChannelHW_PinTypeDef door_pins[LOCKER_COUNT] =
{
    { GPIOF, GPIO_PIN_9  },  /* DOOR_1 */
    { GPIOA, GPIO_PIN_1  },  /* DOOR_2 */
    { GPIOA, GPIO_PIN_7  },  /* DOOR_3 */
    { GPIOB, GPIO_PIN_12 },  /* DOOR_4 */
    { GPIOB, GPIO_PIN_15 },  /* DOOR_5 */
    { GPIOD, GPIO_PIN_13 },  /* DOOR_6 */
    { GPIOA, GPIO_PIN_15 },  /* DOOR_7 */
    { GPIOD, GPIO_PIN_2  },  /* DOOR_8 */
};

static void ChannelHW_EnableAllPortClocks(void)
{
    /* Safe to call even if a port is already clocked by another module. */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
}

System_StatusTypeDef ChannelHW_Init(void)
{
    GPIO_InitTypeDef GPIO_Init = {0};

    ChannelHW_EnableAllPortClocks();

    for (uint8_t i = 0U; i < LOCKER_COUNT; i++)
    {
        /* Lock output -- start de-energized (locked/safe). */
        HAL_GPIO_WritePin(lock_pins[i].port, lock_pins[i].pin,
                           (CHANNEL_HW_LOCK_ENERGIZED_LEVEL == GPIO_PIN_SET) ? GPIO_PIN_RESET : GPIO_PIN_SET);
        GPIO_Init.Pin   = lock_pins[i].pin;
        GPIO_Init.Mode  = GPIO_MODE_OUTPUT_PP;
        GPIO_Init.Pull  = GPIO_NOPULL;
        GPIO_Init.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(lock_pins[i].port, &GPIO_Init);

        /* LED output -- start off. */
        HAL_GPIO_WritePin(led_pins[i].port, led_pins[i].pin,
                           (CHANNEL_HW_LED_ON_LEVEL == GPIO_PIN_SET) ? GPIO_PIN_RESET : GPIO_PIN_SET);
        GPIO_Init.Pin   = led_pins[i].pin;
        GPIO_Init.Mode  = GPIO_MODE_OUTPUT_PP;
        GPIO_Init.Pull  = GPIO_NOPULL;
        GPIO_Init.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(led_pins[i].port, &GPIO_Init);

        /* Door input -- see channel_hw.h TODO: pull applied here since the
         * .ioc leaves DOOR_x at its default (no external pull confirmed). */
        GPIO_Init.Pin   = door_pins[i].pin;
        GPIO_Init.Mode  = GPIO_MODE_INPUT;
        GPIO_Init.Pull  = GPIO_PULLUP;
        GPIO_Init.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(door_pins[i].port, &GPIO_Init);
    }

    return SYS_OK;
}

System_StatusTypeDef ChannelHW_SetLock(uint8_t locker_index, bool energize)
{
    GPIO_PinState level;

    if (locker_index >= LOCKER_COUNT)
    {
        return SYS_INVALID_PARAM;
    }

    level = energize ? CHANNEL_HW_LOCK_ENERGIZED_LEVEL
                      : ((CHANNEL_HW_LOCK_ENERGIZED_LEVEL == GPIO_PIN_SET) ? GPIO_PIN_RESET : GPIO_PIN_SET);

    HAL_GPIO_WritePin(lock_pins[locker_index].port, lock_pins[locker_index].pin, level);

    return SYS_OK;
}

System_StatusTypeDef ChannelHW_SetLED(uint8_t locker_index, bool on)
{
    GPIO_PinState level;

    if (locker_index >= LOCKER_COUNT)
    {
        return SYS_INVALID_PARAM;
    }

    level = on ? CHANNEL_HW_LED_ON_LEVEL
               : ((CHANNEL_HW_LED_ON_LEVEL == GPIO_PIN_SET) ? GPIO_PIN_RESET : GPIO_PIN_SET);

    HAL_GPIO_WritePin(led_pins[locker_index].port, led_pins[locker_index].pin, level);

    return SYS_OK;
}

bool ChannelHW_IsDoorClosed(uint8_t locker_index)
{
    if (locker_index >= LOCKER_COUNT)
    {
        return false; /* fail-safe: report as open */
    }

    return (HAL_GPIO_ReadPin(door_pins[locker_index].port, door_pins[locker_index].pin)
            == CHANNEL_HW_DOOR_CLOSED_LEVEL);
}
