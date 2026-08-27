/**
  ******************************************************************************
  * @file    sdram.h
  * @brief   درایور استاندارد SDRAM خارجی (IS42S16400J) روی FMC Bank1
  *          پروژه: Power Box V2 - STM32F429ZGT6
  *
  * این ماژول کاملا مستقل است و فقط به HAL وابسته است.
  * بالادست (مثلا درایور LTDC برای فریم‌بافر، یا هر ماژول دیگری که
  * نیاز به حافظه‌ی خارجی زیاد دارد) باید صرفا از طریق توابع این فایل
  * با SDRAM کار کند و هیچ‌گاه مستقیما به HAL_SDRAM_* دسترسی نداشته باشد.
  ******************************************************************************
  */

#ifndef __SDRAM_H
#define __SDRAM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

/* ---- بیت‌های Mode Register طبق دیتاشیت IS42S16400J ----
 * این مقادیر جزو HAL نیستند و مستقیما از دیتاشیت تراشه گرفته شده‌اند */
#define SDRAM_MODEREG_BURST_LENGTH_1             ((uint16_t)0x0000)
#define SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL      ((uint16_t)0x0000)
#define SDRAM_MODEREG_CAS_LATENCY_3              ((uint16_t)0x0030)
#define SDRAM_MODEREG_OPERATING_MODE_STANDARD    ((uint16_t)0x0000)
#define SDRAM_MODEREG_WRITEBURST_MODE_SINGLE     ((uint16_t)0x0200)

/* ==================== آدرس و ظرفیت حافظه ==================== */
/* IS42S16400J : 4M x 16bit x 4 Banks = 64Mbit = 8 MegaByte           */
#define SDRAM_BANK_ADDR                 ((uint32_t)0xC0000000)
#define SDRAM_MEMORY_WIDTH               FMC_SDRAM_MEM_BUS_WIDTH_16
#define SDRAM_SIZE_BYTES                 ((uint32_t)0x800000)   /* 8 MB   */
#define SDRAM_ROWBITS_NUMBER              12
#define SDRAM_COLUMNBITS_NUMBER           8

/* ==================== کدهای بازگشتی ==================== */
typedef enum
{
	SDRAM_OK       = 0x00U,
	SDRAM_ERROR    = 0x01U,
	SDRAM_TIMEOUT  = 0x02U,
	SDRAM_TEST_FAIL = 0x03U
} SDRAM_StatusTypeDef;

/* ==================== API عمومی ==================== */

/**
  * @brief  راه‌اندازی کامل SDRAM (کلاک، GPIO، تایمینگ، دنباله‌ی init، رفرش)
  * @note   قبل از فراخوانی این تابع باید سیستم‌کلاک (SystemClock_Config)
  *         از قبل تنظیم شده باشد، چون محاسبه‌ی نرخ رفرش به HCLK وابسته است.
  * @retval SDRAM_OK در صورت موفقیت
  */
SDRAM_StatusTypeDef SDRAM_Init(void);

/**
  * @brief  تست ساده‌ی حافظه (Write/Read/Compare) روی یک بازه از حافظه
  * @param  offset: افست نسبت به SDRAM_BANK_ADDR (بایت)
  * @param  size_words: تعداد کلمه‌های 32 بیتی برای تست
  * @retval SDRAM_OK اگر تست موفق بود، SDRAM_TEST_FAIL در غیر این صورت
  */
SDRAM_StatusTypeDef SDRAM_Test(uint32_t offset, uint32_t size_words);

/**
  * @brief  نوشتن بافر روی SDRAM
  * @param  pData: اشاره‌گر به داده‌ی مبدا
  * @param  offset: افست نسبت به SDRAM_BANK_ADDR (بایت)
  * @param  size_words: تعداد کلمه‌های 32 بیتی
  */
SDRAM_StatusTypeDef SDRAM_WriteBuffer(const uint32_t *pData, uint32_t offset, uint32_t size_words);

/**
  * @brief  خواندن بافر از SDRAM
  * @param  pData: اشاره‌گر به بافر مقصد
  * @param  offset: افست نسبت به SDRAM_BANK_ADDR (بایت)
  * @param  size_words: تعداد کلمه‌های 32 بیتی
  */
SDRAM_StatusTypeDef SDRAM_ReadBuffer(uint32_t *pData, uint32_t offset, uint32_t size_words);

/**
  * @brief  آدرس مطلق حافظه برای دسترسی مستقیم (مثلا برای فریم‌بافر LTDC)
  * @param  offset: افست نسبت به ابتدای بانک
  * @retval آدرس واقعی حافظه (پوینتر خام)
  */
static inline uint32_t SDRAM_GetAbsoluteAddress(uint32_t offset)
{
	return (SDRAM_BANK_ADDR + offset);
}

#ifdef __cplusplus
}
#endif

#endif /* __SDRAM_H */
