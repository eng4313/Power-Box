/**
  ******************************************************************************
  * @file    sdram.c
  * @brief   Standard driver for the external SDRAM (IS42S16400J) on FMC Bank1.
  *
  * Pin mapping (extracted from the Power_Box_V2 / MCU.SchDoc schematic):
  *   A0..A5   -> PF0..PF5      A6..A9  -> PF12..PF15    A10,A11 -> PG0,PG1
  *   BA0,BA1  -> PG4,PG5
  *   D0..D15  -> PD14,PD15,PD0,PD1,PE7..PE15,PD8,PD9,PD10
  *   NWE      -> PC0           NCAS    -> PG15          NRAS    -> PF11
  *   NE0(CS)  -> PC2           CKE0    -> PC3            SDCLK  -> PG8
  *   NBL0,NBL1-> PE0,PE1  (DQML, DQMH)
  *
  * This pinout exactly matches the standard FMC-SDRAM mapping on the
  * STM32F429 (same chip and same wiring as the STM32F429I-Discovery board),
  * so the timings below are set according to the IS42S16400J datasheet and
  * ST's tested reference values for HCLK=180MHz (SDCLK=HCLK/2=90MHz).
  * If the system clock frequency changes, SDRAM_REFRESH_COUNT must be
  * recalculated using the formula below:
  *
  *   REFRESH_COUNT = (SDRAM_Refresh_Period / Rows) * SDCLK_Freq - 20
  *   IS42S16400J : Refresh Period = 64ms , Rows = 4096
  ******************************************************************************
  */

#include "sdram.h"

/* ==================== System-clock-dependent settings ==================== */
/* Assumption: HCLK = 180MHz  =>  SDCLK = HCLK/2 = 90MHz */
/* If SystemClock differs, recompute this value with the formula in the file header. */
#define SDRAM_REFRESH_COUNT             ((uint32_t)1386)

#define SDRAM_TIMEOUT                   ((uint32_t)0xFFFF)

static SDRAM_HandleTypeDef hsdram1;

/* ==================== Private functions ==================== */
static void SDRAM_MspInit(SDRAM_HandleTypeDef *hsdram);
static SDRAM_StatusTypeDef SDRAM_SendCommand(uint32_t CommandMode, uint32_t RefreshNum, uint32_t RegVal);
static SDRAM_StatusTypeDef SDRAM_InitSequence(void);

/* =========================================================================
 *                              Public API
 * ========================================================================= */

SDRAM_StatusTypeDef SDRAM_Init(void)
{
	FMC_SDRAM_TimingTypeDef SdramTiming = {0};

	hsdram1.Instance = FMC_SDRAM_DEVICE;

	/* ---------------- Controller configuration ---------------- */
	hsdram1.Init.SDBank             = FMC_SDRAM_BANK1;
	hsdram1.Init.ColumnBitsNumber   = FMC_SDRAM_COLUMN_BITS_NUM_8;
	hsdram1.Init.RowBitsNumber      = FMC_SDRAM_ROW_BITS_NUM_12;
	hsdram1.Init.MemoryDataWidth    = SDRAM_MEMORY_WIDTH;
	hsdram1.Init.InternalBankNumber = FMC_SDRAM_INTERN_BANKS_NUM_4;
	hsdram1.Init.CASLatency         = FMC_SDRAM_CAS_LATENCY_3;
	hsdram1.Init.WriteProtection    = FMC_SDRAM_WRITE_PROTECTION_DISABLE;
	hsdram1.Init.SDClockPeriod      = FMC_SDRAM_CLOCK_PERIOD_2;
	hsdram1.Init.ReadBurst          = FMC_SDRAM_RBURST_ENABLE;
	hsdram1.Init.ReadPipeDelay      = FMC_SDRAM_RPIPE_DELAY_1;

	/* ---------------- Timing (in SDCLK cycles) ----------------
	 * Reference values for IS42S16400J @ SDCLK=90MHz (HCLK=180MHz) */
	SdramTiming.LoadToActiveDelay    = 2;   /* TMRD */
	SdramTiming.ExitSelfRefreshDelay = 7;   /* TXSR */
	SdramTiming.SelfRefreshTime      = 4;   /* TRAS */
	SdramTiming.RowCycleDelay        = 7;   /* TRC  */
	SdramTiming.WriteRecoveryTime    = 2;   /* TWR  */
	SdramTiming.RPDelay              = 2;   /* TRP  */
	SdramTiming.RCDDelay             = 2;   /* TRCD */

	/* FMC GPIO and clock must be configured before HAL_SDRAM_Init */
	SDRAM_MspInit(&hsdram1);

	if (HAL_SDRAM_Init(&hsdram1, &SdramTiming) != HAL_OK)
	{
		return SDRAM_ERROR;
	}

	if (SDRAM_InitSequence() != SDRAM_OK)
	{
		return SDRAM_ERROR;
	}

	return SDRAM_OK;
}

SDRAM_StatusTypeDef SDRAM_WriteBuffer(const uint32_t *pData, uint32_t offset, uint32_t size_words)
{
	if (pData == NULL)
	{
		return SDRAM_ERROR;
	}

	if (HAL_SDRAM_Write_32b(&hsdram1, (uint32_t *)(SDRAM_BANK_ADDR + offset),
													 (uint32_t *)pData, size_words) != HAL_OK)
	{
		return SDRAM_ERROR;
	}

	return SDRAM_OK;
}

SDRAM_StatusTypeDef SDRAM_ReadBuffer(uint32_t *pData, uint32_t offset, uint32_t size_words)
{
	if (pData == NULL)
	{
		return SDRAM_ERROR;
	}

	if (HAL_SDRAM_Read_32b(&hsdram1, (uint32_t *)(SDRAM_BANK_ADDR + offset),
													pData, size_words) != HAL_OK)
	{
		return SDRAM_ERROR;
	}

	return SDRAM_OK;
}

SDRAM_StatusTypeDef SDRAM_Test(uint32_t offset, uint32_t size_words)
{
	uint32_t i;
	uint32_t rd_data;
	volatile uint32_t *pSdram = (volatile uint32_t *)(SDRAM_BANK_ADDR + offset);

	/* Write the test pattern */
	for (i = 0; i < size_words; i++)
	{
		pSdram[i] = (uint32_t)(i ^ 0xA5A5A5A5UL);
	}

	/* Read back and compare */
	for (i = 0; i < size_words; i++)
	{
		rd_data = pSdram[i];
		if (rd_data != (uint32_t)(i ^ 0xA5A5A5A5UL))
		{
			return SDRAM_TEST_FAIL;
		}
	}

	return SDRAM_OK;
}

/* =========================================================================
 *                        Private functions
 * ========================================================================= */

/**
  * @brief  Chip init sequence per the IS42S16400J datasheet:
  *         1) enable clock            2) 100us delay (per datasheet)
  *         3) Precharge All            4) Auto-Refresh x8
  *         5) Load Mode Register       6) set refresh rate
  */
static SDRAM_StatusTypeDef SDRAM_InitSequence(void)
{
	uint32_t tmpmrd;

	/* Step 1: enable clock */
	if (SDRAM_SendCommand(FMC_SDRAM_CMD_CLK_ENABLE, 1, 0) != SDRAM_OK)
	{
		return SDRAM_ERROR;
	}
	HAL_Delay(1); /* datasheet requires >= 100us - rounded up to 1ms for margin */

	/* Step 2: Precharge All */
	if (SDRAM_SendCommand(FMC_SDRAM_CMD_PALL, 1, 0) != SDRAM_OK)
	{
		return SDRAM_ERROR;
	}

	/* Step 3: Auto-Refresh (datasheet requires >= 2, using 8 for margin) */
	if (SDRAM_SendCommand(FMC_SDRAM_CMD_AUTOREFRESH_MODE, 8, 0) != SDRAM_OK)
	{
		return SDRAM_ERROR;
	}

	/* Step 4: Load Mode Register
	 * Burst Length = 1 , Burst Type = Sequential , CAS Latency = 3
	 * Operating Mode = Standard , Write Burst = Single Location Access */
	tmpmrd = (uint32_t)(SDRAM_MODEREG_BURST_LENGTH_1          |
											 SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL   |
											 SDRAM_MODEREG_CAS_LATENCY_3           |
											 SDRAM_MODEREG_OPERATING_MODE_STANDARD |
											 SDRAM_MODEREG_WRITEBURST_MODE_SINGLE);

	if (SDRAM_SendCommand(FMC_SDRAM_CMD_LOAD_MODE, 1, tmpmrd) != SDRAM_OK)
	{
		return SDRAM_ERROR;
	}

	/* Step 5: set refresh rate */
	if (HAL_SDRAM_ProgramRefreshRate(&hsdram1, SDRAM_REFRESH_COUNT) != HAL_OK)
	{
		return SDRAM_ERROR;
	}

	return SDRAM_OK;
}

static SDRAM_StatusTypeDef SDRAM_SendCommand(uint32_t CommandMode, uint32_t RefreshNum, uint32_t RegVal)
{
	FMC_SDRAM_CommandTypeDef Command;

	Command.CommandMode            = CommandMode;
	Command.CommandTarget          = FMC_SDRAM_CMD_TARGET_BANK1;
	Command.AutoRefreshNumber      = RefreshNum;
	Command.ModeRegisterDefinition = RegVal;

	if (HAL_SDRAM_SendCommand(&hsdram1, &Command, SDRAM_TIMEOUT) != HAL_OK)
	{
		return SDRAM_ERROR;
	}

	return SDRAM_OK;
}

/**
  * @brief  Enables the FMC clock and configures every GPIO pin used by
  *         SDRAM (pinout matches the Power_Box_V2 schematic exactly).
  */
static void SDRAM_MspInit(SDRAM_HandleTypeDef *hsdram)
{
	GPIO_InitTypeDef GPIO_Init = {0};

	/* Enable clocks */
	__HAL_RCC_FMC_CLK_ENABLE();
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOD_CLK_ENABLE();
	__HAL_RCC_GPIOE_CLK_ENABLE();
	__HAL_RCC_GPIOF_CLK_ENABLE();
	__HAL_RCC_GPIOG_CLK_ENABLE();

	GPIO_Init.Mode      = GPIO_MODE_AF_PP;
	GPIO_Init.Pull      = GPIO_PULLUP;
	GPIO_Init.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_Init.Alternate  = GPIO_AF12_FMC;

	/* ---- PORT C: NWE(PC0), NE0/CS(PC2), CKE0(PC3) ---- */
	GPIO_Init.Pin = GPIO_PIN_0 | GPIO_PIN_2 | GPIO_PIN_3;
	HAL_GPIO_Init(GPIOC, &GPIO_Init);

	/* ---- PORT D: D2,D3(PD0,PD1), D13,D14,D15(PD8,PD9,PD10),
	 *              D0,D1(PD14,PD15) ---- */
	GPIO_Init.Pin = GPIO_PIN_0  | GPIO_PIN_1  | GPIO_PIN_8  |
									 GPIO_PIN_9  | GPIO_PIN_10 | GPIO_PIN_14 |
									 GPIO_PIN_15;
	HAL_GPIO_Init(GPIOD, &GPIO_Init);

	/* ---- PORT E: NBL0,NBL1(PE0,PE1), D4..D12(PE7..PE15) ---- */
	GPIO_Init.Pin = GPIO_PIN_0  | GPIO_PIN_1  | GPIO_PIN_7  |
									 GPIO_PIN_8  | GPIO_PIN_9  | GPIO_PIN_10 |
									 GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 |
									 GPIO_PIN_14 | GPIO_PIN_15;
	HAL_GPIO_Init(GPIOE, &GPIO_Init);

	/* ---- PORT F: A0..A5(PF0..PF5), NRAS(PF11), A6..A9(PF12..PF15) ---- */
	GPIO_Init.Pin = GPIO_PIN_0  | GPIO_PIN_1  | GPIO_PIN_2  |
									 GPIO_PIN_3  | GPIO_PIN_4  | GPIO_PIN_5  |
									 GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 |
									 GPIO_PIN_14 | GPIO_PIN_15;
	HAL_GPIO_Init(GPIOF, &GPIO_Init);

	/* ---- PORT G: A10,A11(PG0,PG1), BA0,BA1(PG4,PG5),
	 *              SDCLK(PG8), NCAS(PG15) ---- */
	GPIO_Init.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_4 |
									 GPIO_PIN_5 | GPIO_PIN_8 | GPIO_PIN_15;
	HAL_GPIO_Init(GPIOG, &GPIO_Init);
}
