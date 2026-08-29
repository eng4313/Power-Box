/**
  ******************************************************************************
  * @file    sdram.h
  * @brief   Standard driver for the external SDRAM (IS42S16400J) on FMC Bank1.
  *          Project: Power Box V2 - STM32F429ZGT6
  *
  * This module is fully self-contained and depends only on HAL. Upstream
  * consumers (e.g. the LTDC driver for the framebuffer, or any other module
  * that needs large external memory) must go through this file's functions
  * only, and must never call HAL_SDRAM_* directly.
  ******************************************************************************
  */

#ifndef __SDRAM_H
#define __SDRAM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

/* ---- Mode Register bits per the IS42S16400J datasheet ----
 * These values are not part of HAL; they come directly from the chip datasheet. */
#define SDRAM_MODEREG_BURST_LENGTH_1             ((uint16_t)0x0000)
#define SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL      ((uint16_t)0x0000)
#define SDRAM_MODEREG_CAS_LATENCY_3              ((uint16_t)0x0030)
#define SDRAM_MODEREG_OPERATING_MODE_STANDARD    ((uint16_t)0x0000)
#define SDRAM_MODEREG_WRITEBURST_MODE_SINGLE     ((uint16_t)0x0200)

/* ==================== Address and capacity ==================== */
/* IS42S16400J : 4M x 16bit x 4 Banks = 64Mbit = 8 MegaByte */
#define SDRAM_BANK_ADDR                 ((uint32_t)0xC0000000)
#define SDRAM_MEMORY_WIDTH               FMC_SDRAM_MEM_BUS_WIDTH_16

/* ==================== Return codes ==================== */
typedef enum
{
	SDRAM_OK       = 0x00U,
	SDRAM_ERROR    = 0x01U,
	SDRAM_TIMEOUT  = 0x02U,
	SDRAM_TEST_FAIL = 0x03U
} SDRAM_StatusTypeDef;

/* ==================== Public API ==================== */

/**
  * @brief  Full SDRAM bring-up (clocks, GPIO, timing, init sequence, refresh).
  * @note   SystemClock_Config() must already have run before calling this,
  *         since the refresh rate calculation depends on HCLK.
  * @retval SDRAM_OK on success.
  */
SDRAM_StatusTypeDef SDRAM_Init(void);

/**
  * @brief  Simple memory test (Write/Read/Compare) over a memory range.
  * @param  offset: offset relative to SDRAM_BANK_ADDR (bytes)
  * @param  size_words: number of 32-bit words to test
  * @retval SDRAM_OK if the test passed, SDRAM_TEST_FAIL otherwise.
  */
SDRAM_StatusTypeDef SDRAM_Test(uint32_t offset, uint32_t size_words);

/**
  * @brief  Write a buffer to SDRAM.
  * @param  pData: pointer to the source data
  * @param  offset: offset relative to SDRAM_BANK_ADDR (bytes)
  * @param  size_words: number of 32-bit words
  */
SDRAM_StatusTypeDef SDRAM_WriteBuffer(const uint32_t *pData, uint32_t offset, uint32_t size_words);

/**
  * @brief  Read a buffer from SDRAM.
  * @param  pData: pointer to the destination buffer
  * @param  offset: offset relative to SDRAM_BANK_ADDR (bytes)
  * @param  size_words: number of 32-bit words
  */
SDRAM_StatusTypeDef SDRAM_ReadBuffer(uint32_t *pData, uint32_t offset, uint32_t size_words);

/**
  * @brief  Absolute memory address for direct access (e.g. for the LTDC framebuffer).
  * @param  offset: offset relative to the start of the bank
  * @retval Real memory address (raw pointer).
  */
static inline uint32_t SDRAM_GetAbsoluteAddress(uint32_t offset)
{
	return (SDRAM_BANK_ADDR + offset);
}

#ifdef __cplusplus
}
#endif

#endif /* __SDRAM_H */
