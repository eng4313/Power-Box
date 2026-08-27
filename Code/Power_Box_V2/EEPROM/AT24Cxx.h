/*
 * AT24Cxx.h
 *
 *  Created on: 20 ????. 2018 ?.
 *      Author: Andriy
 */

#ifndef AT24CXX_H_
#define AT24CXX_H_

#include "stdio.h"
#include "stm32f4xx_hal.h"

#define	EEPROM_I2C			  hi2c3
#define EEPROM_ADDRESS		0x50
#define EEPROM_PAGESIZE		128
#define EEPROM_WRITE		  6					        //time to wait in ms
#define EEPROM_TIMEOUT		5 * EEPROM_WRITE	//timeout while writing

extern I2C_HandleTypeDef EEPROM_I2C;

HAL_StatusTypeDef AT24Cxx_IsConnected(void);
HAL_StatusTypeDef AT24Cxx_ReadEEPROM(unsigned address, const void* src, unsigned len);
HAL_StatusTypeDef AT24Cxx_WriteEEPROM(unsigned address, const void* src, unsigned len);

HAL_StatusTypeDef AT24_Read_8(uint16_t Addr, void *Data);
HAL_StatusTypeDef AT24_Read_16(uint16_t Addr, void *Data);
HAL_StatusTypeDef AT24_Read_32(uint16_t Addr, void *Data);
HAL_StatusTypeDef AT24_Write_8(uint16_t Addr, uint8_t Data);
HAL_StatusTypeDef AT24_Write_16(uint16_t Addr, uint16_t Data);
HAL_StatusTypeDef AT24_Write_32(uint16_t Addr, uint32_t Data);

#endif /* AT24CXX_H_ */
