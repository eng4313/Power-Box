/**
  ******************************************************************************
  * @file    spi_bus.c
  * @brief   Shared SPI1 bus driver implementation
  *
  * Pinout (extracted from Power_Box_V2 schematic, MCU.SchDoc):
  *   SCK  -> PB3  (AF5, SPI1_SCK)
  *   MISO -> PB4  (AF5, SPI1_MISO)
  *   MOSI -> PB5  (AF5, SPI1_MOSI)
  ******************************************************************************
  */

#include "spi_bus.h"

static SPI_HandleTypeDef hspi1;
static uint8_t bus_initialized = 0;

static SPI_Bus_StatusTypeDef SPI_Bus_Reinit(uint32_t prescaler, uint32_t firstBit,
                                             uint32_t polarity, uint32_t phase);

SPI_Bus_StatusTypeDef SPI_Bus_Init(void)
{
    GPIO_InitTypeDef GPIO_Init = {0};

    if (bus_initialized)
    {
        return SPI_BUS_OK;
    }

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_SPI1_CLK_ENABLE();

    GPIO_Init.Pin       = GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5;
    GPIO_Init.Mode      = GPIO_MODE_AF_PP;
    GPIO_Init.Pull      = GPIO_PULLUP;
    GPIO_Init.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_Init.Alternate  = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOB, &GPIO_Init);

    /* Default: moderate speed, safe starting point for all slaves on
     * this bus (flash, audio codec, touch controller). Fast devices
     * (like the flash) can request a higher prescaler via
     * SPI_Bus_SetPrescaler() right before their own transaction. */
    if (SPI_Bus_Reinit(SPI_BAUDRATEPRESCALER_8, SPI_FIRSTBIT_MSB,
                        SPI_POLARITY_LOW, SPI_PHASE_1EDGE) != SPI_BUS_OK)
    {
        return SPI_BUS_ERROR;
    }

    bus_initialized = 1;
    return SPI_BUS_OK;
}

SPI_Bus_StatusTypeDef SPI_Bus_SetPrescaler(uint32_t prescaler)
{
    return SPI_Bus_Reinit(prescaler, SPI_FIRSTBIT_MSB, SPI_POLARITY_LOW, SPI_PHASE_1EDGE);
}

SPI_Bus_StatusTypeDef SPI_Bus_Configure(uint32_t prescaler, uint32_t firstBit,
                                         uint32_t polarity, uint32_t phase)
{
    return SPI_Bus_Reinit(prescaler, firstBit, polarity, phase);
}

static SPI_Bus_StatusTypeDef SPI_Bus_Reinit(uint32_t prescaler, uint32_t firstBit,
                                             uint32_t polarity, uint32_t phase)
{
    hspi1.Instance               = SPI1;
    hspi1.Init.Mode              = SPI_MODE_MASTER;
    hspi1.Init.Direction         = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize          = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity       = polarity;
    hspi1.Init.CLKPhase          = phase;
    hspi1.Init.NSS               = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = prescaler;
    hspi1.Init.FirstBit          = firstBit;
    hspi1.Init.TIMode            = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial     = 7;

    if (HAL_SPI_Init(&hspi1) != HAL_OK)
    {
        return SPI_BUS_ERROR;
    }

    return SPI_BUS_OK;
}

SPI_Bus_StatusTypeDef SPI_Bus_TransmitReceive(uint8_t *pTxData, uint8_t *pRxData,
                                               uint16_t size, uint32_t timeout_ms)
{
    if (HAL_SPI_TransmitReceive(&hspi1, pTxData, pRxData, size, timeout_ms) != HAL_OK)
    {
        return SPI_BUS_ERROR;
    }
    return SPI_BUS_OK;
}

SPI_Bus_StatusTypeDef SPI_Bus_Transmit(uint8_t *pTxData, uint16_t size, uint32_t timeout_ms)
{
    if (HAL_SPI_Transmit(&hspi1, pTxData, size, timeout_ms) != HAL_OK)
    {
        return SPI_BUS_ERROR;
    }
    return SPI_BUS_OK;
}

SPI_Bus_StatusTypeDef SPI_Bus_Receive(uint8_t *pRxData, uint16_t size, uint32_t timeout_ms)
{
    if (HAL_SPI_Receive(&hspi1, pRxData, size, timeout_ms) != HAL_OK)
    {
        return SPI_BUS_ERROR;
    }
    return SPI_BUS_OK;
}
