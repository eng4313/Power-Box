/**
  ******************************************************************************
  * @file    lcd.c
  * @brief   LTDC driver implementation
  *
  * Pinout (extracted from Power_Box_V2 schematic, MCU.SchDoc + LCD TFT 50
  * Pin.SchDoc), all LTDC signals on AF14:
  *
  *   R2 -> PC10   R3 -> PB0    R4 -> PA11   R5 -> PA12   R6 -> PB1   R7 -> PG6
  *   G2 -> PA6    G3 -> PG10   G4 -> PB10   G5 -> PB11   G6 -> PC7   G7 -> PD3
  *   B2 -> PD6    B3 -> PG11   B4 -> PG12   B5 -> PA3    B6 -> PB8   B7 -> PB9
  *   CLK -> PG7   DE -> PF10   HSYNC -> PC6   VSYNC -> PA4
  *
  *   LTDC_RST -> PC8  (plain GPIO output, panel hardware reset, NOT an
  *                     LTDC peripheral signal)
  *
  * R0,R1,G0,G1,B0,B1 are tied to GND on the panel FPC connector (18-bit
  * color bus instead of full 24-bit), so this driver uses RGB565 /
  * effectively RGB666-truncated framebuffer content.
  *
  * TIMING ASSUMPTION TO VERIFY: values below are typical figures for a
  * generic 800x480 RGB TFT panel of this connector family. Replace with
  * exact values from your panel's datasheet if display geometry looks
  * wrong (rolling image, shifted colors, torn edges, etc. are symptoms
  * of incorrect timing).
  ******************************************************************************
  */

#include "lcd.h"

static LTDC_HandleTypeDef hltdc;
static uint8_t active_buffer_index = 0; /* 0 = FB0 is active, 1 = FB1 is active */

static void LCD_ConfigPixelClock(void);
static void LCD_MspInit(void);
static void LCD_ResetPanel(void);
static void LCD_GpioAfInit(GPIO_TypeDef *port, uint32_t pins, uint32_t alternate);

LCD_StatusTypeDef LCD_Init(void)
{
	LTDC_LayerCfgTypeDef LayerCfg = {0};

	LCD_MspInit();
	LCD_ResetPanel();

	/* ---- LTDC global timing / sync polarity ----
	 * HSYNC/VSYNC/DE active low, pixel clock rising edge, typical for
	 * this panel family. */
	hltdc.Instance = LTDC;
	hltdc.Init.HSPolarity = LTDC_HSPOLARITY_AL;
	hltdc.Init.VSPolarity = LTDC_VSPOLARITY_AL;
	hltdc.Init.DEPolarity = LTDC_DEPOLARITY_AL;
	hltdc.Init.PCPolarity = LTDC_PCPOLARITY_IPC;

	hltdc.Init.HorizontalSync     = LCD_HSYNC_WIDTH - 1U;
	hltdc.Init.VerticalSync       = LCD_VSYNC_WIDTH - 1U;
	hltdc.Init.AccumulatedHBP     = LCD_HSYNC_WIDTH + LCD_HBP - 1U;
	hltdc.Init.AccumulatedVBP     = LCD_VSYNC_WIDTH + LCD_VBP - 1U;
	hltdc.Init.AccumulatedActiveW = LCD_HSYNC_WIDTH + LCD_HBP + LCD_WIDTH - 1U;
	hltdc.Init.AccumulatedActiveH = LCD_VSYNC_WIDTH + LCD_VBP + LCD_HEIGHT - 1U;
	hltdc.Init.TotalWidth         = LCD_HSYNC_WIDTH + LCD_HBP + LCD_WIDTH + LCD_HFP - 1U;
	hltdc.Init.TotalHeigh         = LCD_VSYNC_WIDTH + LCD_VBP + LCD_HEIGHT + LCD_VFP - 1U;

	/* Background color shown outside the active layer area (black) */
	hltdc.Init.Backcolor.Red   = 0;
	hltdc.Init.Backcolor.Green = 0;
	hltdc.Init.Backcolor.Blue  = 0;

	if (HAL_LTDC_Init(&hltdc) != HAL_OK)
	{
		return LCD_ERROR;
	}

	/* ---- Layer 0: full-screen, points at frame buffer 0 in SDRAM ---- */
	LayerCfg.WindowX0        = 0;
	LayerCfg.WindowX1        = LCD_WIDTH;
	LayerCfg.WindowY0        = 0;
	LayerCfg.WindowY1        = LCD_HEIGHT;
	LayerCfg.PixelFormat     = LTDC_PIXEL_FORMAT_RGB565;
	LayerCfg.Alpha           = 255;
	LayerCfg.Alpha0          = 0;
	LayerCfg.BlendingFactor1 = LTDC_BLENDING_FACTOR1_CA;
	LayerCfg.BlendingFactor2 = LTDC_BLENDING_FACTOR2_CA;
	LayerCfg.FBStartAdress   = SDRAM_GetAbsoluteAddress(LCD_FB0_OFFSET);
	LayerCfg.ImageWidth      = LCD_WIDTH;
	LayerCfg.ImageHeight     = LCD_HEIGHT;
	LayerCfg.Backcolor.Red   = 0;
	LayerCfg.Backcolor.Green = 0;
	LayerCfg.Backcolor.Blue  = 0;

	if (HAL_LTDC_ConfigLayer(&hltdc, &LayerCfg, LTDC_LAYER_1) != HAL_OK)
	{
		return LCD_ERROR;
	}

	active_buffer_index = 0;

	return LCD_OK;
}

uint32_t LCD_GetActiveFrameBufferAddress(void)
{
	return SDRAM_GetAbsoluteAddress((active_buffer_index == 0) ? LCD_FB0_OFFSET : LCD_FB1_OFFSET);
}

uint32_t LCD_GetBackFrameBufferAddress(void)
{
	return SDRAM_GetAbsoluteAddress((active_buffer_index == 0) ? LCD_FB1_OFFSET : LCD_FB0_OFFSET);
}

void LCD_SwapBuffers(void)
{
	uint32_t new_active_addr = LCD_GetBackFrameBufferAddress();

	HAL_LTDC_SetAddress(&hltdc, new_active_addr, LTDC_LAYER_1);
	__HAL_LTDC_RELOAD_CONFIG(&hltdc); /* force immediate reload, belt-and-suspenders */
	active_buffer_index = (active_buffer_index == 0) ? 1 : 0;
}

void LCD_DisplayOn(void)
{
	__HAL_LTDC_ENABLE(&hltdc);
	HAL_GPIO_WritePin(LCD_RST_GPIO_PORT, LCD_RST_GPIO_PIN, GPIO_PIN_SET);
}

void LCD_DisplayOff(void)
{
	__HAL_LTDC_DISABLE(&hltdc);
}

void LCD_FillColor(uint16_t color)
{
	uint16_t *fb = (uint16_t *)LCD_GetBackFrameBufferAddress();
	uint32_t pixel_count = (uint32_t)LCD_WIDTH * LCD_HEIGHT;

	for (uint32_t i = 0; i < pixel_count; i++)
	{
		fb[i] = color;
	}
}

void LCD_DrawImageRGB565(const uint16_t *image, uint16_t img_width, uint16_t img_height,
                          uint16_t x, uint16_t y)
{
	uint16_t *fb = (uint16_t *)LCD_GetBackFrameBufferAddress();
	uint16_t rows_to_copy;
	uint16_t cols_to_copy;

	if ((image == NULL) || (x >= LCD_WIDTH) || (y >= LCD_HEIGHT))
	{
		return;
	}

	rows_to_copy = ((uint32_t)y + img_height > LCD_HEIGHT) ? (LCD_HEIGHT - y) : img_height;
	cols_to_copy = ((uint32_t)x + img_width  > LCD_WIDTH)  ? (LCD_WIDTH  - x) : img_width;

	for (uint16_t row = 0; row < rows_to_copy; row++)
	{
		const uint16_t *src_row = &image[(uint32_t)row * img_width];
		uint16_t *dst_row = &fb[(uint32_t)(y + row) * LCD_WIDTH + x];

		for (uint16_t col = 0; col < cols_to_copy; col++)
		{
			dst_row[col] = src_row[col];
		}
	}
}

/* =========================================================================
 *                          Private helpers
 * ========================================================================= */

/**
  * @brief  Configures PLLSAI as the LTDC pixel clock source (RCC_PERIPHCLK_LTDC).
  *         Without this, LTDC's pixel clock (PG7/CLK) never comes up, and
  *         the panel free-runs into its default state -- which for most
  *         panels shows as a blank/white screen no matter what the
  *         framebuffer contains.
  *
  *         Assumes HSE=8MHz, PLLM=4 (matches SystemClock_Config() in
  *         main.c -> VCO input = 2MHz, ST's recommended sweet spot, shared
  *         with the main PLL's M divider).
  *           VCO       = 2MHz * PLLSAIN(180) = 360MHz
  *           PLLSAI_R  = 360MHz / PLLSAIR(6) = 60MHz
  *           LCD_CLK   = 60MHz / PLLSAIDIVR(2) = 30MHz
  *
  *         TODO(hardware, verify): 30MHz is a typical figure for this
  *         panel family, not a value taken from your exact panel's
  *         datasheet. If the image rolls, tears, or is shifted once pixel
  *         data is visible, recompute PLLSAIN/PLLSAIR/PLLSAIDivR for your
  *         panel's actual rated pixel clock using the formula above.
  */
static void LCD_ConfigPixelClock(void)
{
	RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

	PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_LTDC;
	PeriphClkInitStruct.PLLSAI.PLLSAIN       = 180;
	PeriphClkInitStruct.PLLSAI.PLLSAIR       = 6;
	PeriphClkInitStruct.PLLSAIDivR           = RCC_PLLSAIDIVR_2;

	if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
	{
		Error_Handler();
	}
}

static void LCD_GpioAfInit(GPIO_TypeDef *port, uint32_t pins, uint32_t alternate)
{
	GPIO_InitTypeDef GPIO_Init = {0};

	GPIO_Init.Pin       = pins;
	GPIO_Init.Mode      = GPIO_MODE_AF_PP;
	GPIO_Init.Pull      = GPIO_NOPULL;
	GPIO_Init.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_Init.Alternate = alternate;
	HAL_GPIO_Init(port, &GPIO_Init);
}

static void LCD_MspInit(void)
{
	GPIO_InitTypeDef GPIO_Init = {0};

	LCD_ConfigPixelClock();

	__HAL_RCC_LTDC_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOD_CLK_ENABLE();
	__HAL_RCC_GPIOF_CLK_ENABLE();
	__HAL_RCC_GPIOG_CLK_ENABLE();

	/* Port A: B5(PA3), VSYNC(PA4), G2(PA6), R4(PA11), R5(PA12) -- all AF14 */
	LCD_GpioAfInit(GPIOA, GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_6 | GPIO_PIN_11 | GPIO_PIN_12, GPIO_AF14_LTDC);

	/* Port B: G4(PB10), G5(PB11), B6(PB8), B7(PB9) -- AF14 */
	LCD_GpioAfInit(GPIOB, GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11, GPIO_AF14_LTDC);
	/* Port B: R3(PB0), R6(PB1) -- these two are AF9 on STM32F429, NOT AF14 */
	LCD_GpioAfInit(GPIOB, GPIO_PIN_0 | GPIO_PIN_1, GPIO_AF9_LTDC);

	/* Port C: HSYNC(PC6), G6(PC7), R2(PC10) -- AF14. PC8 handled separately as plain GPIO (RST) */
	LCD_GpioAfInit(GPIOC, GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_10, GPIO_AF14_LTDC);

	/* Port D: G7(PD3), B2(PD6) -- AF14 */
	LCD_GpioAfInit(GPIOD, GPIO_PIN_3 | GPIO_PIN_6, GPIO_AF14_LTDC);

	/* Port F: DE(PF10) -- AF14 */
	LCD_GpioAfInit(GPIOF, GPIO_PIN_10, GPIO_AF14_LTDC);

	/* Port G: R7(PG6), CLK(PG7), B3(PG11) -- AF14 */
	LCD_GpioAfInit(GPIOG, GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_11, GPIO_AF14_LTDC);
	/* Port G: G3(PG10), B4(PG12) -- these two are AF9 on STM32F429, NOT AF14 */
	LCD_GpioAfInit(GPIOG, GPIO_PIN_10 | GPIO_PIN_12, GPIO_AF9_LTDC);

	/* LCD_RST: plain push-pull output, held low during init */
	GPIO_Init.Pin   = LCD_RST_GPIO_PIN;
	GPIO_Init.Mode  = GPIO_MODE_OUTPUT_PP;
	GPIO_Init.Pull  = GPIO_NOPULL;
	GPIO_Init.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(LCD_RST_GPIO_PORT, &GPIO_Init);
}

static void LCD_ResetPanel(void)
{
	HAL_GPIO_WritePin(LCD_RST_GPIO_PORT, LCD_RST_GPIO_PIN, GPIO_PIN_RESET);
	HAL_Delay(10);
	HAL_GPIO_WritePin(LCD_RST_GPIO_PORT, LCD_RST_GPIO_PIN, GPIO_PIN_SET);
	HAL_Delay(10);
}
