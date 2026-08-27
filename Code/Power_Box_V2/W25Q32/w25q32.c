/**
  ******************************************************************************
  * @file    w25q32.c
  * @brief   W25Q32 SPI NOR flash driver implementation
  *
  * Pinout (extracted from Power_Box_V2 schematic):
  *   CS   -> PG14 (GPIO output, active low)
  *   SCK/MISO/MOSI -> shared SPI1 bus (see spi_bus.h)
  ******************************************************************************
  */

#include "w25q32.h"

/* ---- W25Q32 command set (per Winbond datasheet) ---- */
#define CMD_WRITE_ENABLE          0x06U
#define CMD_WRITE_DISABLE         0x04U
#define CMD_READ_STATUS_REG1      0x05U
#define CMD_PAGE_PROGRAM          0x02U
#define CMD_SECTOR_ERASE_4K       0x20U
#define CMD_BLOCK_ERASE_64K       0xD8U
#define CMD_CHIP_ERASE            0xC7U
#define CMD_READ_DATA             0x03U
#define CMD_JEDEC_ID              0x9FU

static void     W25_CS_Low(void);
static void     W25_CS_High(void);
static W25_StatusTypeDef W25_WriteEnable(void);
static W25_StatusTypeDef W25_WaitBusy(uint32_t timeout_ms);
static W25_StatusTypeDef W25_SelectBus(void);

/* =========================================================================
 *                              Public API
 * ========================================================================= */

W25_StatusTypeDef W25Q32_Init(void)
{
	GPIO_InitTypeDef GPIO_Init = {0};
	uint32_t id = 0;

	__HAL_RCC_GPIOG_CLK_ENABLE();

	GPIO_Init.Pin   = W25_CS_GPIO_PIN;
	GPIO_Init.Mode  = GPIO_MODE_OUTPUT_PP;
	GPIO_Init.Pull  = GPIO_NOPULL;
	GPIO_Init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	HAL_GPIO_Init(W25_CS_GPIO_PORT, &GPIO_Init);

	W25_CS_High();

	if (W25Q32_ReadID(&id) != W25_OK)
	{
		return W25_ERROR;
	}

	if (id != W25Q32_JEDEC_ID)
	{
		return W25_BAD_ID;
	}

	return W25_OK;
}

W25_StatusTypeDef W25Q32_ReadID(uint32_t *pId)
{
	uint8_t tx[4] = { CMD_JEDEC_ID, 0x00, 0x00, 0x00 };
	uint8_t rx[4] = { 0 };

	if (W25_SelectBus() != W25_OK)
	{
		return W25_ERROR;
	}

	W25_CS_Low();
	if (SPI_Bus_TransmitReceive(tx, rx, 4, W25_SPI_TIMEOUT_MS) != SPI_BUS_OK)
	{
		W25_CS_High();
		return W25_ERROR;
	}
	W25_CS_High();

	*pId = ((uint32_t)rx[1] << 16) | ((uint32_t)rx[2] << 8) | (uint32_t)rx[3];
	return W25_OK;
}

W25_StatusTypeDef W25Q32_Read(uint32_t address, uint8_t *pData, uint32_t size)
{
	uint8_t cmd[4];

	if ((pData == NULL) || (size == 0U) || ((address + size) > W25Q32_TOTAL_SIZE))
	{
		return W25_ERROR;
	}

	if (W25_SelectBus() != W25_OK)
	{
		return W25_ERROR;
	}

	cmd[0] = CMD_READ_DATA;
	cmd[1] = (uint8_t)(address >> 16);
	cmd[2] = (uint8_t)(address >> 8);
	cmd[3] = (uint8_t)(address);

	W25_CS_Low();

	if (SPI_Bus_Transmit(cmd, 4, W25_SPI_TIMEOUT_MS) != SPI_BUS_OK)
	{
		W25_CS_High();
		return W25_ERROR;
	}

	if (SPI_Bus_Receive(pData, (uint16_t)size, W25_SPI_TIMEOUT_MS) != SPI_BUS_OK)
	{
		W25_CS_High();
		return W25_ERROR;
	}

	W25_CS_High();
	return W25_OK;
}

W25_StatusTypeDef W25Q32_PageProgram(uint32_t address, const uint8_t *pData, uint16_t size)
{
	uint8_t cmd[4];

	if ((pData == NULL) || (size == 0U) || (size > W25Q32_PAGE_SIZE))
	{
		return W25_ERROR;
	}

	if (W25_SelectBus() != W25_OK)
	{
			return W25_ERROR;
	}

	if (W25_WriteEnable() != W25_OK)
	{
		return W25_ERROR;
	}

	cmd[0] = CMD_PAGE_PROGRAM;
	cmd[1] = (uint8_t)(address >> 16);
	cmd[2] = (uint8_t)(address >> 8);
	cmd[3] = (uint8_t)(address);

	W25_CS_Low();

	if (SPI_Bus_Transmit(cmd, 4, W25_SPI_TIMEOUT_MS) != SPI_BUS_OK)
	{
		W25_CS_High();
		return W25_ERROR;
	}

	if (SPI_Bus_Transmit((uint8_t *)pData, size, W25_SPI_TIMEOUT_MS) != SPI_BUS_OK)
	{
		W25_CS_High();
		return W25_ERROR;
	}

	W25_CS_High();

	return W25_WaitBusy(W25_SPI_TIMEOUT_MS);
}

W25_StatusTypeDef W25Q32_Write(uint32_t address, const uint8_t *pData, uint32_t size)
{
	uint32_t remaining = size;
	uint32_t addr = address;
	const uint8_t *p = pData;
	uint16_t chunk;

	if ((pData == NULL) || (size == 0U) || ((address + size) > W25Q32_TOTAL_SIZE))
	{
		return W25_ERROR;
	}

	while (remaining > 0U)
	{
		uint32_t offset_in_page = addr % W25Q32_PAGE_SIZE;
		uint32_t space_left_in_page = W25Q32_PAGE_SIZE - offset_in_page;

		chunk = (uint16_t)((remaining < space_left_in_page) ? remaining : space_left_in_page);

		if (W25Q32_PageProgram(addr, p, chunk) != W25_OK)
		{
			return W25_ERROR;
		}

		addr      += chunk;
		p         += chunk;
		remaining -= chunk;
	}

	return W25_OK;
}

W25_StatusTypeDef W25Q32_EraseSector(uint32_t address)
{
	uint8_t cmd[4];

	if (W25_SelectBus() != W25_OK)
	{
		return W25_ERROR;
	}

	if (W25_WriteEnable() != W25_OK)
	{
		return W25_ERROR;
	}

	cmd[0] = CMD_SECTOR_ERASE_4K;
	cmd[1] = (uint8_t)(address >> 16);
	cmd[2] = (uint8_t)(address >> 8);
	cmd[3] = (uint8_t)(address);

	W25_CS_Low();
	if (SPI_Bus_Transmit(cmd, 4, W25_SPI_TIMEOUT_MS) != SPI_BUS_OK)
	{
		W25_CS_High();
		return W25_ERROR;
	}
	W25_CS_High();

	return W25_WaitBusy(W25_ERASE_TIMEOUT_MS);
}

W25_StatusTypeDef W25Q32_EraseBlock(uint32_t address)
{
	uint8_t cmd[4];

	if (W25_SelectBus() != W25_OK)
	{
		return W25_ERROR;
	}

	if (W25_WriteEnable() != W25_OK)
	{
		return W25_ERROR;
	}

	cmd[0] = CMD_BLOCK_ERASE_64K;
	cmd[1] = (uint8_t)(address >> 16);
	cmd[2] = (uint8_t)(address >> 8);
	cmd[3] = (uint8_t)(address);

	W25_CS_Low();
	if (SPI_Bus_Transmit(cmd, 4, W25_SPI_TIMEOUT_MS) != SPI_BUS_OK)
	{
		W25_CS_High();
		return W25_ERROR;
	}
	W25_CS_High();

	return W25_WaitBusy(W25_ERASE_TIMEOUT_MS);
}

W25_StatusTypeDef W25Q32_EraseChip(void)
{
	uint8_t cmd = CMD_CHIP_ERASE;

	if (W25_SelectBus() != W25_OK)
	{
		return W25_ERROR;
	}

	if (W25_WriteEnable() != W25_OK)
	{
		return W25_ERROR;
	}

	W25_CS_Low();
	if (SPI_Bus_Transmit(&cmd, 1, W25_SPI_TIMEOUT_MS) != SPI_BUS_OK)
	{
		W25_CS_High();
		return W25_ERROR;
	}
	W25_CS_High();

	return W25_WaitBusy(W25_ERASE_TIMEOUT_MS);
}

/* =========================================================================
 *                          Private helpers
 * ========================================================================= */

static void W25_CS_Low(void)
{
	HAL_GPIO_WritePin(W25_CS_GPIO_PORT, W25_CS_GPIO_PIN, GPIO_PIN_RESET);
}

static void W25_CS_High(void)
{
	HAL_GPIO_WritePin(W25_CS_GPIO_PORT, W25_CS_GPIO_PIN, GPIO_PIN_SET);
}

/* Flash supports up to 104MHz, but this bus is shared with slower
 * devices (touch, audio codec), so this driver requests its own
 * prescaler right before each transaction. */
static W25_StatusTypeDef W25_SelectBus(void)
{
	if (SPI_Bus_SetPrescaler(W25_SPI_PRESCALER) != SPI_BUS_OK)
	{
		return W25_ERROR;
	}
	return W25_OK;
}

static W25_StatusTypeDef W25_WriteEnable(void)
{
	uint8_t cmd = CMD_WRITE_ENABLE;

	W25_CS_Low();
	if (SPI_Bus_Transmit(&cmd, 1, W25_SPI_TIMEOUT_MS) != SPI_BUS_OK)
	{
		W25_CS_High();
		return W25_ERROR;
	}
	W25_CS_High();

	return W25_OK;
}

static W25_StatusTypeDef W25_WaitBusy(uint32_t timeout_ms)
{
	uint8_t cmd = CMD_READ_STATUS_REG1;
	uint8_t status = 0;
	uint32_t start = HAL_GetTick();

	do
	{
		W25_CS_Low();
		SPI_Bus_Transmit(&cmd, 1, W25_SPI_TIMEOUT_MS);
		SPI_Bus_Receive(&status, 1, W25_SPI_TIMEOUT_MS);
		W25_CS_High();

		if ((HAL_GetTick() - start) > timeout_ms)
		{
			return W25_TIMEOUT;
		}
	} while ((status & STATUS_BUSY_BIT) != 0U);

	return W25_OK;
}
