#include "AT24Cxx.h"



static unsigned eeprom_address = EEPROM_ADDRESS << 1;
static unsigned inpage_addr_mask = EEPROM_PAGESIZE - 1;
static HAL_StatusTypeDef AT24Cxx_WriteReadEEPROM(unsigned address, const void* src, unsigned len, uint8_t write);
static unsigned size_to_page_end(unsigned addr);

uint8_t Temp_EE[4];

HAL_StatusTypeDef AT24Cxx_IsConnected(void)
{
	return HAL_I2C_IsDeviceReady(&EEPROM_I2C, eeprom_address, 1, EEPROM_TIMEOUT);
}

HAL_StatusTypeDef AT24Cxx_ReadEEPROM(unsigned address, const void* src, unsigned len)
{
	return AT24Cxx_WriteReadEEPROM(address, src, len, 0);
}

HAL_StatusTypeDef AT24Cxx_WriteEEPROM(unsigned address, const void* src, unsigned len)
{
	return AT24Cxx_WriteReadEEPROM(address, src, len, 1);
}

static HAL_StatusTypeDef AT24Cxx_WriteReadEEPROM(unsigned address, const void* src, unsigned len, uint8_t write)
{
	uint8_t *pdata = (uint8_t*) src;
	HAL_StatusTypeDef result = HAL_OK;
	unsigned max_portion = size_to_page_end(address);
	unsigned portion;

	while (len != 0 && result == HAL_OK)
	{
			portion = len;              

			if (portion > max_portion)
			{
				portion = max_portion;  
			}

			if(write)
			{
				result = HAL_I2C_Mem_Write(&EEPROM_I2C,
										eeprom_address,
										address,
										I2C_MEMADD_SIZE_16BIT,
										pdata,
										portion,
										EEPROM_TIMEOUT);
			}
			else
			{
				result = HAL_I2C_Mem_Read(&EEPROM_I2C,
												eeprom_address,
										address,
										I2C_MEMADD_SIZE_16BIT,
										pdata,
										portion,
										EEPROM_TIMEOUT);
			}

			len     -= portion;
			address += portion;
			pdata   += portion;

			max_portion = EEPROM_PAGESIZE;

			if(write)
			{
				HAL_Delay(EEPROM_WRITE);
			}
			else
			{
				HAL_Delay(EEPROM_WRITE / 2);
			}
	}

	return result;
}


static unsigned size_to_page_end(unsigned addr)
{
    return (~addr & inpage_addr_mask) + 1;
}





HAL_StatusTypeDef AT24_Read_8(uint16_t Addr, void *Data)
{
	uint8_t *pdata = (uint8_t*) Data;
	HAL_StatusTypeDef result;
	
	result = HAL_I2C_Mem_Read(&EEPROM_I2C, eeprom_address, Addr, I2C_MEMADD_SIZE_16BIT, pdata, 1, EEPROM_TIMEOUT);
	HAL_Delay(1);
	
	return result;
}

HAL_StatusTypeDef AT24_Read_16(uint16_t Addr, void *Data)
{
	uint8_t *pdata = (uint8_t*) Data;
	HAL_StatusTypeDef result;
	
	result = HAL_I2C_Mem_Read(&EEPROM_I2C, eeprom_address, Addr, I2C_MEMADD_SIZE_16BIT, pdata, 2, EEPROM_TIMEOUT);
	HAL_Delay(1);
	
	return result;
}

HAL_StatusTypeDef AT24_Read_32(uint16_t Addr, void *Data)
{
	uint8_t *pdata = (uint8_t*) Data;
	HAL_StatusTypeDef result;
	
	result = HAL_I2C_Mem_Read(&EEPROM_I2C, eeprom_address, Addr, I2C_MEMADD_SIZE_16BIT, pdata, 4, EEPROM_TIMEOUT);
	HAL_Delay(1);
	
	return result;
}

HAL_StatusTypeDef AT24_Write_8(uint16_t Addr, uint8_t Data)
{
	HAL_StatusTypeDef result;
	
	result = HAL_I2C_Mem_Write(&EEPROM_I2C, eeprom_address, Addr, I2C_MEMADD_SIZE_16BIT, &Data, 1, EEPROM_TIMEOUT);
	HAL_Delay(EEPROM_WRITE);
	
	return result;
}

HAL_StatusTypeDef AT24_Write_16(uint16_t Addr, uint16_t Data)
{
	HAL_StatusTypeDef result;
	
	Temp_EE[1] = (uint8_t)(Data >> 8);
	Temp_EE[0] = (uint8_t)(Data);
	
	result = HAL_I2C_Mem_Write(&EEPROM_I2C, eeprom_address, Addr, I2C_MEMADD_SIZE_16BIT, Temp_EE, 2, EEPROM_TIMEOUT);
	HAL_Delay(EEPROM_WRITE);
	
	return result;
}

HAL_StatusTypeDef AT24_Write_32(uint16_t Addr, uint32_t Data)
{
	HAL_StatusTypeDef result;
	
	Temp_EE[3] = (uint8_t)(Data >> 24);
	Temp_EE[2] = (uint8_t)(Data >> 16);
	Temp_EE[1] = (uint8_t)(Data >> 8);
	Temp_EE[0] = (uint8_t)(Data);
	
	result = HAL_I2C_Mem_Write(&EEPROM_I2C, eeprom_address, Addr, I2C_MEMADD_SIZE_16BIT, Temp_EE, 4, EEPROM_TIMEOUT);
	HAL_Delay(EEPROM_WRITE);
	
	return result;
}





