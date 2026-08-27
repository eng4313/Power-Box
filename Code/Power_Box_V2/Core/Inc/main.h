/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define KEY_1_Pin GPIO_PIN_2
#define KEY_1_GPIO_Port GPIOE
#define KEY_2_Pin GPIO_PIN_3
#define KEY_2_GPIO_Port GPIOE
#define LD_Pin GPIO_PIN_6
#define LD_GPIO_Port GPIOE
#define DOOR_1_Pin GPIO_PIN_9
#define DOOR_1_GPIO_Port GPIOF
#define LOCK_1_Pin GPIO_PIN_1
#define LOCK_1_GPIO_Port GPIOC
#define LD_1_Pin GPIO_PIN_0
#define LD_1_GPIO_Port GPIOA
#define DOOR_2_Pin GPIO_PIN_1
#define DOOR_2_GPIO_Port GPIOA
#define LOCK_2_Pin GPIO_PIN_2
#define LOCK_2_GPIO_Port GPIOA
#define LD_2_Pin GPIO_PIN_5
#define LD_2_GPIO_Port GPIOA
#define DOOR_3_Pin GPIO_PIN_7
#define DOOR_3_GPIO_Port GPIOA
#define LOCK_3_Pin GPIO_PIN_4
#define LOCK_3_GPIO_Port GPIOC
#define LD_3_Pin GPIO_PIN_5
#define LD_3_GPIO_Port GPIOC
#define DOOR_4_Pin GPIO_PIN_12
#define DOOR_4_GPIO_Port GPIOB
#define LOCK_4_Pin GPIO_PIN_13
#define LOCK_4_GPIO_Port GPIOB
#define LD_4_Pin GPIO_PIN_14
#define LD_4_GPIO_Port GPIOB
#define DOOR_5_Pin GPIO_PIN_15
#define DOOR_5_GPIO_Port GPIOB
#define LOCK_5_Pin GPIO_PIN_11
#define LOCK_5_GPIO_Port GPIOD
#define LD_5_Pin GPIO_PIN_12
#define LD_5_GPIO_Port GPIOD
#define DOOR_6_Pin GPIO_PIN_13
#define DOOR_6_GPIO_Port GPIOD
#define LOCK_6_Pin GPIO_PIN_2
#define LOCK_6_GPIO_Port GPIOG
#define LD_6_Pin GPIO_PIN_3
#define LD_6_GPIO_Port GPIOG
#define LTDC_RST_Pin GPIO_PIN_8
#define LTDC_RST_GPIO_Port GPIOC
#define DOOR_7_Pin GPIO_PIN_15
#define DOOR_7_GPIO_Port GPIOA
#define LOCK_7_Pin GPIO_PIN_11
#define LOCK_7_GPIO_Port GPIOC
#define LD_7_Pin GPIO_PIN_12
#define LD_7_GPIO_Port GPIOC
#define DOOR_8_Pin GPIO_PIN_2
#define DOOR_8_GPIO_Port GPIOD
#define LOCK_8_Pin GPIO_PIN_4
#define LOCK_8_GPIO_Port GPIOD
#define LD_8_Pin GPIO_PIN_5
#define LD_8_GPIO_Port GPIOD
#define XPT_CS_Pin GPIO_PIN_7
#define XPT_CS_GPIO_Port GPIOD
#define XPT_IRQ_Pin GPIO_PIN_9
#define XPT_IRQ_GPIO_Port GPIOG
#define ISD_SS_Pin GPIO_PIN_13
#define ISD_SS_GPIO_Port GPIOG
#define W25_CS_Pin GPIO_PIN_14
#define W25_CS_GPIO_Port GPIOG

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
