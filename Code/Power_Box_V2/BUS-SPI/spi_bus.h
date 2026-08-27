/**
  ******************************************************************************
  * @file    spi_bus.h
  * @brief   Shared SPI1 bus driver (SCK=PB3, MISO=PB4, MOSI=PB5, AF5)
  *
  * This bus is physically shared between three devices on the schematic:
  *   - W25Q32 external flash   (CS = PG14)
  *   - ISD1730 audio codec     (CS = PG13)
  *   - XPT2046 touch controller(CS = PD7)
  *
  * Each device driver owns and drives only its own CS pin. This module
  * is only responsible for:
  *   1) One-time GPIO + SPI1 peripheral initialization
  *   2) Providing a thread-unsafe (single master context) transmit/receive
  *      primitive that all device drivers call
  *   3) Allowing the clock prescaler to be changed at runtime, since
  *      different slaves on the bus support different max SPI clocks
  *      (e.g. touch controller is much slower than flash memory)
  *
  * Device drivers must NOT touch hspi1 directly and must NOT re-init
  * the SPI peripheral themselves.
  ******************************************************************************
  */

#ifndef __SPI_BUS_H
#define __SPI_BUS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include <stdint.h>

typedef enum
{
    SPI_BUS_OK      = 0x00U,
    SPI_BUS_ERROR   = 0x01U,
    SPI_BUS_TIMEOUT = 0x02U
} SPI_Bus_StatusTypeDef;

/**
  * @brief  One-time init of SPI1 peripheral and its GPIO pins (SCK/MISO/MOSI).
  *         CS pins are NOT configured here; each device driver configures
  *         and drives its own CS pin independently.
  */
SPI_Bus_StatusTypeDef SPI_Bus_Init(void);

/**
  * @brief  Change SPI clock prescaler before talking to a specific slave.
  *         Call this right after asserting the target device's CS pin
  *         (before) if that device requires a different bus speed than
  *         the currently configured one. Keeps bit order at MSB-first
  *         (the default, used by the flash and touch controller).
  * @param  prescaler: one of SPI_BAUDRATEPRESCALER_x
  */
SPI_Bus_StatusTypeDef SPI_Bus_SetPrescaler(uint32_t prescaler);

/**
  * @brief  Full reconfiguration before talking to a device whose SPI mode
  *         (clock polarity/phase, bit order) differs from the bus default
  *         (Mode 0, MSB-first, used by flash and touch controller). The
  *         ISD1730 audio codec, for example, requires LSB-first with
  *         clock idling high and data sampled on the rising edge (Mode 3).
  * @param  prescaler: one of SPI_BAUDRATEPRESCALER_x
  * @param  firstBit:  SPI_FIRSTBIT_MSB or SPI_FIRSTBIT_LSB
  * @param  polarity:  SPI_POLARITY_LOW or SPI_POLARITY_HIGH
  * @param  phase:     SPI_PHASE_1EDGE or SPI_PHASE_2EDGE
  */
SPI_Bus_StatusTypeDef SPI_Bus_Configure(uint32_t prescaler, uint32_t firstBit,
                                         uint32_t polarity, uint32_t phase);

/**
  * @brief  Full-duplex transmit/receive on the shared bus.
  *         Caller is responsible for asserting/de-asserting its own CS
  *         pin before/after calling this function.
  */
SPI_Bus_StatusTypeDef SPI_Bus_TransmitReceive(uint8_t *pTxData, uint8_t *pRxData,
                                               uint16_t size, uint32_t timeout_ms);

/**
  * @brief  Transmit-only helper (rx byte discarded).
  */
SPI_Bus_StatusTypeDef SPI_Bus_Transmit(uint8_t *pTxData, uint16_t size, uint32_t timeout_ms);

/**
  * @brief  Receive-only helper (dummy 0xFF bytes sent out while receiving).
  */
SPI_Bus_StatusTypeDef SPI_Bus_Receive(uint8_t *pRxData, uint16_t size, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* __SPI_BUS_H */
