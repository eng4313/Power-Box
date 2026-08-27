/**
  ******************************************************************************
  * @file    w25q32.h
  * @brief   Driver for W25Q32 SPI NOR flash (32Mbit = 4MB)
  *          Sits on the shared SPI1 bus, CS = PG14
  ******************************************************************************
  */

#ifndef __W25Q32_H
#define __W25Q32_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>
#include "spi_bus.h"

#define W25Q32_PAGE_SIZE          256U
#define W25Q32_SECTOR_SIZE        4096U
#define W25Q32_BLOCK_SIZE         65536U
#define W25Q32_TOTAL_SIZE         (4U * 1024U * 1024U)  /* 4 MB */
#define W25Q32_JEDEC_ID           0x00EF4016U            /* Winbond, 32Mbit */

#define STATUS_BUSY_BIT           0x01U

#define W25_CS_GPIO_PORT          GPIOG
#define W25_CS_GPIO_PIN           GPIO_PIN_14

#define W25_SPI_PRESCALER         SPI_BAUDRATEPRESCALER_4
#define W25_SPI_TIMEOUT_MS        100U
#define W25_ERASE_TIMEOUT_MS      30000U /* chip erase can take a long time */

typedef enum
{
	W25_OK      = 0x00U,
	W25_ERROR   = 0x01U,
	W25_TIMEOUT = 0x02U,
	W25_BAD_ID  = 0x03U
} W25_StatusTypeDef;

/**
  * @brief  Init CS GPIO pin and verify the flash JEDEC ID.
  * @note   SPI_Bus_Init() must already have been called before this.
  */
W25_StatusTypeDef W25Q32_Init(void);

/**
  * @brief  Read JEDEC ID (manufacturer + memory type + capacity).
  */
W25_StatusTypeDef W25Q32_ReadID(uint32_t *pId);

/**
  * @brief  Sequential read, any address/length, no erase needed.
  */
W25_StatusTypeDef W25Q32_Read(uint32_t address, uint8_t *pData, uint32_t size);

/**
  * @brief  Program up to one page (256 bytes). Target region must already
  *         be erased (0xFF) since NOR flash can only clear bits, not set.
  *         If (address % W25Q32_PAGE_SIZE) + size exceeds the page boundary,
  *         the write wraps within the page per flash datasheet behavior,
  *         so callers should split writes at page boundaries themselves.
  */
W25_StatusTypeDef W25Q32_PageProgram(uint32_t address, const uint8_t *pData, uint16_t size);

/**
  * @brief  Write an arbitrary-length buffer, automatically splitting into
  *         page-aligned Page Program operations. Target region must be
  *         erased beforehand.
  */
W25_StatusTypeDef W25Q32_Write(uint32_t address, const uint8_t *pData, uint32_t size);

/**
  * @brief  Erase one 4KB sector containing the given address.
  */
W25_StatusTypeDef W25Q32_EraseSector(uint32_t address);

/**
  * @brief  Erase one 64KB block containing the given address.
  */
W25_StatusTypeDef W25Q32_EraseBlock(uint32_t address);

/**
  * @brief  Erase the entire chip. This can take several seconds.
  */
W25_StatusTypeDef W25Q32_EraseChip(void);

#ifdef __cplusplus
}
#endif

#endif /* __W25Q32_H */
