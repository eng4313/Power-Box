/**
  ******************************************************************************
  * @file    isd1730.c
  * @brief   ISD1730 driver implementation
  ******************************************************************************
  */

#include "isd1730.h"

static void ISD_CS_Low(void);
static void ISD_CS_High(void);
static ISD_StatusTypeDef ISD_SelectBus(void);
static ISD_StatusTypeDef ISD_SimpleCommand(uint8_t opcode);
static ISD_StatusTypeDef ISD_AddressedCommand(uint8_t opcode, uint16_t startRow, uint16_t endRow);

ISD_StatusTypeDef ISD1730_Init(void)
{
    GPIO_InitTypeDef GPIO_Init = {0};

    __HAL_RCC_GPIOG_CLK_ENABLE();

    GPIO_Init.Pin   = ISD_CS_GPIO_PIN;
    GPIO_Init.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_Init.Pull  = GPIO_NOPULL;
    GPIO_Init.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(ISD_CS_GPIO_PORT, &GPIO_Init);

    ISD_CS_High();

    return ISD1730_PowerUp();
}

ISD_StatusTypeDef ISD1730_PowerUp(void)
{
    ISD_StatusTypeDef status = ISD_SimpleCommand(OP_PU);
    HAL_Delay(50); /* datasheet-recommended power-up settling delay */
    return status;
}

ISD_StatusTypeDef ISD1730_PowerDown(void)
{
    return ISD_SimpleCommand(OP_PD);
}

ISD_StatusTypeDef ISD1730_Stop(void)
{
    return ISD_SimpleCommand(OP_STOP);
}

ISD_StatusTypeDef ISD1730_Reset(void)
{
    return ISD_SimpleCommand(OP_RESET);
}

ISD_StatusTypeDef ISD1730_ClearInterrupt(void)
{
    return ISD_SimpleCommand(OP_CLR_INT);
}

ISD_StatusTypeDef ISD1730_ReadStatus(uint16_t *pSR0, uint8_t *pSR1)
{
    uint8_t tx[3] = { OP_RD_STATUS, 0x00, 0x00 };
    uint8_t rx[3] = { 0 };

    if (ISD_SelectBus() != ISD_OK)
    {
        return ISD_ERROR;
    }

    ISD_CS_Low();
    if (SPI_Bus_TransmitReceive(tx, rx, 3, ISD_SPI_TIMEOUT_MS) != SPI_BUS_OK)
    {
        ISD_CS_High();
        return ISD_ERROR;
    }
    ISD_CS_High();

    /* SR0 is returned across the first 2 bytes of the transaction, SR1
     * across the 3rd (per datasheet Figure 10.1 / confirmed community
     * usage notes for this exact command). */
    if (pSR0 != NULL)
    {
        *pSR0 = ((uint16_t)rx[0] << 8) | (uint16_t)rx[1];
    }
    if (pSR1 != NULL)
    {
        *pSR1 = rx[2];
    }

    return ISD_OK;
}

ISD_StatusTypeDef ISD1730_PlayRange(uint16_t startRow, uint16_t endRow)
{
    return ISD_AddressedCommand(OP_SET_PLAY, startRow, endRow);
}

ISD_StatusTypeDef ISD1730_RecordRange(uint16_t startRow, uint16_t endRow)
{
    return ISD_AddressedCommand(OP_SET_REC, startRow, endRow);
}

ISD_StatusTypeDef ISD1730_EraseRange(uint16_t startRow, uint16_t endRow)
{
    return ISD_AddressedCommand(OP_SET_ERASE, startRow, endRow);
}

ISD_StatusTypeDef ISD1730_Play(void)
{
    return ISD_SimpleCommand(OP_PLAY);
}

ISD_StatusTypeDef ISD1730_Record(void)
{
    return ISD_SimpleCommand(OP_REC);
}

ISD_StatusTypeDef ISD1730_Erase(void)
{
    return ISD_SimpleCommand(OP_ERASE);
}
ISD_StatusTypeDef ISD1730_PlayMessage(ISD_MESSAGE_t message)
{
    if (message > NOT_FOUND)
    {
        return ISD_ERROR;
    }

    return ISD1730_PlayRange(
        ISD_MessageTable[message].startRow,
        ISD_MessageTable[message].endRow
    );
}
/* =========================================================================
 *                          Private helpers
 * ========================================================================= */

static void ISD_CS_Low(void)
{
    HAL_GPIO_WritePin(ISD_CS_GPIO_PORT, ISD_CS_GPIO_PIN, GPIO_PIN_RESET);
}

static void ISD_CS_High(void)
{
    HAL_GPIO_WritePin(ISD_CS_GPIO_PORT, ISD_CS_GPIO_PIN, GPIO_PIN_SET);
}

/* ISD1700 series: SPI Mode 3 (CPOL=1, CPHA=1), LSB-first -- different
 * from the flash/touch devices sharing this bus (Mode 0, MSB-first). */
static ISD_StatusTypeDef ISD_SelectBus(void)
{
    if (SPI_Bus_Configure(ISD_SPI_PRESCALER, SPI_FIRSTBIT_LSB,
                           SPI_POLARITY_HIGH, SPI_PHASE_1EDGE) != SPI_BUS_OK)
    {
        return ISD_ERROR;
    }
    return ISD_OK;
}

/**
  * @brief  Simple 2-byte command (opcode + 1 dummy byte), used by PU,
  *         PD, STOP, RESET, CLR_INT. Response (status) is discarded;
  *         call ISD1730_ReadStatus() separately if needed.
  */
static ISD_StatusTypeDef ISD_SimpleCommand(uint8_t opcode)
{
    uint8_t tx[2] = { opcode, 0x00 };
    uint8_t rx[2] = { 0 };

    if (ISD_SelectBus() != ISD_OK)
    {
        return ISD_ERROR;
    }

    ISD_CS_Low();
    if (SPI_Bus_TransmitReceive(tx, rx, 2, ISD_SPI_TIMEOUT_MS) != SPI_BUS_OK)
    {
        ISD_CS_High();
        return ISD_ERROR;
    }
    ISD_CS_High();

    return ISD_OK;
}

/**
  * @brief  7-byte addressed command frame used by SET_PLAY/SET_REC/
  *         SET_ERASE:
  *           byte0: opcode
  *           byte1: data byte 1 (control flags, 0x00 = defaults)
  *           byte2: start row, low byte  (bits 7:0)
  *           byte3: start row, high bits (bits 10:8 in low 3 bits)
  *           byte4: end row, low byte    (bits 7:0)
  *           byte5: end row, high bits   (bits 10:8 in low 3 bits)
  *           byte6: 0x00 (unused padding byte)
  */
static ISD_StatusTypeDef ISD_AddressedCommand(uint8_t opcode, uint16_t startRow, uint16_t endRow)
{
    uint8_t tx[7];
    uint8_t rx[7] = { 0 };

    tx[0] = opcode;
    tx[1] = 0x00;
    tx[2] = (uint8_t)(startRow & 0xFFU);
    tx[3] = (uint8_t)((startRow >> 8) & 0x07U);
    tx[4] = (uint8_t)(endRow & 0xFFU);
    tx[5] = (uint8_t)((endRow >> 8) & 0x07U);
    tx[6] = 0x00;

    if (ISD_SelectBus() != ISD_OK)
    {
        return ISD_ERROR;
    }

    ISD_CS_Low();
    if (SPI_Bus_TransmitReceive(tx, rx, 7, ISD_SPI_TIMEOUT_MS) != SPI_BUS_OK)
    {
        ISD_CS_High();
        return ISD_ERROR;
    }
    ISD_CS_High();

    return ISD_OK;
}
