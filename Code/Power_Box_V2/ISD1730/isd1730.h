/**
  ******************************************************************************
  * @file    isd1730.h
  * @brief   Driver for ISD1730 voice record/playback IC (ISD1700 family).
  *          Sits on the shared SPI1 bus, CS = PG13.
  *
  * Opcode values below are taken directly from the official Nuvoton
  * ISD1700 Series datasheet/design guide (section 11.1 Direct Access
  * Commands, section 11.1/11.2 Circular Memory & Addressed Commands).
  * SET_REC (0x81) is inferred from the confirmed SET_PLAY(0x80)/
  * SET_ERASE(0x82) bracketing pattern and should be double-checked
  * against your exact datasheet revision before relying on it in
  * production recording flows.
  *
  * IMPORTANT: this chip uses SPI Mode 3 (CPOL=1, CPHA=1) and LSB-first
  * bit order, unlike the flash/touch devices on the same physical bus
  * (Mode 0, MSB-first). This driver calls SPI_Bus_Configure() before
  * every transaction to switch modes safely.
  ******************************************************************************
  */

#ifndef __ISD1730_H
#define __ISD1730_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>
#include "spi_bus.h"

/* ---- Confirmed opcodes (ISD1700 series datasheet, section 11.1) ---- */
#define OP_PU              0x01U
#define OP_STOP            0x02U
#define OP_RESET           0x03U
#define OP_CLR_INT         0x04U
#define OP_RD_STATUS       0x05U
#define OP_PD              0x07U

/* ---- Addressed (SPI-mode) commands, 7-byte frames ----*/
#define OP_PLAY            0x50U
#define OP_REC             0x51U
#define OP_ERASE           0x43U

#define OP_SET_PLAY        0x90U
#define OP_SET_REC         0x91U
#define OP_SET_ERASE       0x92U

#define ISD_CS_GPIO_PORT   GPIOG
#define ISD_CS_GPIO_PIN    GPIO_PIN_13

#define ISD_SPI_PRESCALER  SPI_BAUDRATEPRESCALER_16
#define ISD_SPI_TIMEOUT_MS 200U

typedef enum
{
    FULL_BOX    	= 0x00U,
    OS_SELECT 		= 0x01U,
    ENTER_NUM 		= 0x02U,
    SAVE 					= 0x03U,
    NOT_SAVE 			= 0x04U,
    ENTER_FINGER 	= 0x05U,
    END_TIME 			= 0x06U,
    DOOR_OPENED 	= 0x07U,
    DOOR_IS_OPEN 	= 0x08U,
    WRONG_FINGER 	= 0x09U,
    NOT_FOUND 		= 0x0AU
} ISD_MESSAGE_t;

typedef struct
{
    uint16_t startRow;
    uint16_t endRow;
} ISD_MessageAddress_t;

static const ISD_MessageAddress_t ISD_MessageTable[] =
{
	[FULL_BOX]     = {0U,   20U },
	[OS_SELECT]    = {20U, 	40U },
	[ENTER_NUM]    = {40U, 	60U },
	[SAVE]         = {60U, 	70U },
	[NOT_SAVE]     = {70U, 	80U },
	[ENTER_FINGER] = {80U, 	100U},
	[END_TIME]     = {100U, 115U},
	[DOOR_OPENED]  = {115U, 135U},
	[DOOR_IS_OPEN] = {135U, 155U},
	[WRONG_FINGER] = {155U, 180U},
	[NOT_FOUND]    = {180U, 190U}
};

typedef enum
{
    ISD_OK    = 0x00U,
    ISD_ERROR = 0x01U
} ISD_StatusTypeDef;

/**
  * @brief  Init CS GPIO and issue Power-Up so the chip is ready for
  *         further commands.
  * @note   SPI_Bus_Init() must already have been called before this.
  */
ISD_StatusTypeDef ISD1730_Init(void);

ISD_StatusTypeDef ISD1730_PowerUp(void);
ISD_StatusTypeDef ISD1730_PowerDown(void);
ISD_StatusTypeDef ISD1730_Stop(void);
ISD_StatusTypeDef ISD1730_Reset(void);
ISD_StatusTypeDef ISD1730_ClearInterrupt(void);

ISD_StatusTypeDef ISD1730_Play(void);
ISD_StatusTypeDef ISD1730_Record(void);
ISD_StatusTypeDef ISD1730_Erase(void);

/**
  * @brief  Read status registers SR0 (16-bit) and SR1 (8-bit).
  *         SR0 bit0 = command error, other bits indicate current
  *         operation state (PLAY/REC/ERASE/RDY, etc. per datasheet
  *         Table 10.3/10.4) -- refer to datasheet for full bit meaning.
  */
ISD_StatusTypeDef ISD1730_ReadStatus(uint16_t *pSR0, uint8_t *pSR1);

/**
  * @brief  Play back rows [startRow, endRow] inclusive (SET_PLAY, 0x80).
  *         Row addresses are 11-bit (0-2047).
  */
ISD_StatusTypeDef ISD1730_PlayRange(uint16_t startRow, uint16_t endRow);

/**
  * @brief  Record into rows [startRow, endRow] inclusive (SET_REC, 0x81).
  *         See header note: opcode 0x81 is pattern-inferred, verify
  *         against your datasheet before relying on this in production.
  */
ISD_StatusTypeDef ISD1730_RecordRange(uint16_t startRow, uint16_t endRow);

/**
  * @brief  Erase rows [startRow, endRow] inclusive (SET_ERASE, 0x82).
  */
ISD_StatusTypeDef ISD1730_EraseRange(uint16_t startRow, uint16_t endRow);

ISD_StatusTypeDef ISD1730_PlayMessage(ISD_MESSAGE_t message);
#ifdef __cplusplus
}
#endif

#endif /* __ISD1730_H */
