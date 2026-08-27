/**
  ******************************************************************************
  * @file    lcd.h
  * @brief   LTDC driver for the 7" 50-pin RGB TFT panel (CN3 on
  *          "LCD TFT 50 Pin.SchDoc"). 18-bit color bus (R2-R7,G2-G7,B2-B7),
  *          LSBs of each color tied to GND on the panel side.
  *
  * ASSUMPTION TO VERIFY: panel resolution assumed 800x480 (most common
  * for this generic 50-pin RGB FPC panel family). If your actual panel
  * datasheet specifies different resolution/timings, update
  * LCD_WIDTH/LCD_HEIGHT and the timing defines in lcd.c accordingly.
  ******************************************************************************
  */

#ifndef __LCD_H
#define __LCD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>
#include "sdram.h"

/* ---- Timing parameters (typical 800x480 RGB TFT, ~30-33MHz pixel clock) ---- */
#define LCD_HSYNC_WIDTH             1U
#define LCD_HBP                     46U
#define LCD_HFP                     210U
#define LCD_VSYNC_WIDTH             1U
#define LCD_VBP                     23U
#define LCD_VFP                     22U

#define LCD_RST_GPIO_PORT           GPIOC
#define LCD_RST_GPIO_PIN            GPIO_PIN_8

#define LCD_WIDTH                  800U
#define LCD_HEIGHT                 480U
#define LCD_PIXEL_SIZE_BYTES       2U   /* RGB565 */

#define LCD_FRAME_BUFFER_SIZE      (LCD_WIDTH * LCD_HEIGHT * LCD_PIXEL_SIZE_BYTES)

/* Two frame buffers reserved at the start of SDRAM for double buffering.
 * Anything an application needs to store in SDRAM beyond the display
 * (e.g. audio/image cache) must start at LCD_SDRAM_RESERVED_SIZE. */
#define LCD_FB0_OFFSET              0U
#define LCD_FB1_OFFSET              (LCD_FB0_OFFSET + LCD_FRAME_BUFFER_SIZE)
#define LCD_SDRAM_RESERVED_SIZE     (LCD_FB1_OFFSET + LCD_FRAME_BUFFER_SIZE)

typedef enum
{
	LCD_OK      = 0x00U,
	LCD_ERROR   = 0x01U
} LCD_StatusTypeDef;

/**
  * @brief  Full LCD bring-up: GPIO (AF14 for LTDC signals + RST as plain
  *         GPIO output), LTDC peripheral clock/timing configuration,
  *         single layer pointing at the frame buffer in SDRAM, panel
  *         reset pulse, and display/layer enable.
  * @note   SDRAM_Init() MUST have been called successfully before this,
  *         since the frame buffer physically lives in external SDRAM.
  */
LCD_StatusTypeDef LCD_Init(void);

/**
  * @brief  Returns the absolute address of the active frame buffer
  *         (the one currently being scanned out by LTDC).
  */
uint32_t LCD_GetActiveFrameBufferAddress(void);

/**
  * @brief  Returns the absolute address of the back buffer (not currently
  *         displayed) so the application can draw into it, then call
  *         LCD_SwapBuffers() to flip.
  */
uint32_t LCD_GetBackFrameBufferAddress(void);

/**
  * @brief  Switch LTDC layer address to the back buffer, making it the
  *         new active buffer (simple double buffering, no v-sync wait).
  */
void LCD_SwapBuffers(void);

void LCD_DisplayOn(void);
void LCD_DisplayOff(void);

/**
  * @brief  Fills the BACK buffer with a single RGB565 color (e.g. for a
  *         quick power/timing sanity check before wiring up real image
  *         data). Call LCD_SwapBuffers() afterwards to make it visible.
  */
void LCD_FillColor(uint16_t color);

/**
  * @brief  Copies a raw RGB565 image (row-major, no header/compression --
  *         e.g. exported with an "image to RGB565 array" converter) into
  *         the BACK buffer at (x,y). Silently clips anything that would
  *         fall outside the panel. Call LCD_SwapBuffers() afterwards to
  *         make it visible.
  * @param  image      Pointer to img_width * img_height uint16_t RGB565 pixels.
  */
void LCD_DrawImageRGB565(const uint16_t *image, uint16_t img_width, uint16_t img_height,
                          uint16_t x, uint16_t y);

#ifdef __cplusplus
}
#endif

#endif /* __LCD_H */
