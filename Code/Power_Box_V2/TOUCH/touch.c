/**
  ******************************************************************************
  * @file    touch.c
  * @brief   XPT2046 touch controller driver implementation
  ******************************************************************************
  */

#include "touch.h"

static void     Touch_CS_Low(void);
static void     Touch_CS_High(void);
static TOUCH_StatusTypeDef Touch_SelectBus(void);
static uint16_t Touch_ReadChannel(uint8_t command);

TOUCH_StatusTypeDef Touch_Init(void)
{
    GPIO_InitTypeDef GPIO_Init = {0};

    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();

    /* CS: push-pull output, idle high (not selected) */
    GPIO_Init.Pin   = TOUCH_CS_GPIO_PIN;
    GPIO_Init.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_Init.Pull  = GPIO_NOPULL;
    GPIO_Init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(TOUCH_CS_GPIO_PORT, &GPIO_Init);
    Touch_CS_High();

    /* PENIRQ: input, open-drain on the controller side, active low
     * when panel is pressed -- external/internal pull-up needed. */
    GPIO_Init.Pin   = TOUCH_PENIRQ_GPIO_PIN;
    GPIO_Init.Mode  = GPIO_MODE_INPUT;
    GPIO_Init.Pull  = GPIO_PULLUP;
    GPIO_Init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(TOUCH_PENIRQ_GPIO_PORT, &GPIO_Init);

    return TOUCH_OK;
}

bool Touch_IsPressed(void)
{
    return (HAL_GPIO_ReadPin(TOUCH_PENIRQ_GPIO_PORT, TOUCH_PENIRQ_GPIO_PIN) == GPIO_PIN_RESET);
}

void Touch_ForceConversion(void)
{
    (void)Touch_SelectBus();
    (void)Touch_ReadChannel(CMD_READ_X);
}

TOUCH_StatusTypeDef Touch_ReadRaw(TOUCH_RawPointTypeDef *pPoint)
{
    uint16_t x1, x2, y1, y2;

    if (pPoint == NULL)
    {
        return TOUCH_ERROR;
    }

    if (Touch_SelectBus() != TOUCH_OK)
    {
        return TOUCH_ERROR;
    }

    if (!Touch_IsPressed())
    {
        return TOUCH_ERROR;
    }

    x1 = Touch_ReadChannel(CMD_READ_X);
    x2 = Touch_ReadChannel(CMD_READ_X);
    y1 = Touch_ReadChannel(CMD_READ_Y);
    y2 = Touch_ReadChannel(CMD_READ_Y);

    if (!Touch_IsPressed())
    {
        /* panel was released while sampling, discard this reading */
        return TOUCH_ERROR;
    }

    if (((x1 > x2) ? (x1 - x2) : (x2 - x1)) > TOUCH_NOISE_THRESHOLD)
    {
        return TOUCH_ERROR;
    }

    if (((y1 > y2) ? (y1 - y2) : (y2 - y1)) > TOUCH_NOISE_THRESHOLD)
    {
        return TOUCH_ERROR;
    }

    pPoint->x = (x1 + x2) / 2U;
    pPoint->y = (y1 + y2) / 2U;

    return TOUCH_OK;
}

/* =========================================================================
 *                          Private helpers
 * ========================================================================= */

static void Touch_CS_Low(void)
{
    HAL_GPIO_WritePin(TOUCH_CS_GPIO_PORT, TOUCH_CS_GPIO_PIN, GPIO_PIN_RESET);
}

static void Touch_CS_High(void)
{
    HAL_GPIO_WritePin(TOUCH_CS_GPIO_PORT, TOUCH_CS_GPIO_PIN, GPIO_PIN_SET);
}

static TOUCH_StatusTypeDef Touch_SelectBus(void)
{
    if (SPI_Bus_SetPrescaler(TOUCH_SPI_PRESCALER) != SPI_BUS_OK)
    {
        return TOUCH_ERROR;
    }
    return TOUCH_OK;
}

/**
  * @brief  Send one command byte, clock out 2 dummy bytes while reading
  *         back the 12-bit conversion result (result occupies bits
  *         [14:3] of the 16-bit response, per XPT2046 datasheet timing).
  */
static uint16_t Touch_ReadChannel(uint8_t command)
{
    uint8_t tx[3] = { command, 0x00, 0x00 };
    uint8_t rx[3] = { 0 };
    uint16_t raw;

    Touch_CS_Low();
    SPI_Bus_TransmitReceive(tx, rx, 3, TOUCH_SPI_TIMEOUT_MS);
    Touch_CS_High();

    raw = (((uint16_t)rx[1] << 8) | (uint16_t)rx[2]) >> 3;
    return (raw & 0x0FFFU);
}
