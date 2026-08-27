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
#include "stm32f1xx_hal.h"

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

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LED_1_Pin GPIO_PIN_13
#define LED_1_GPIO_Port GPIOC
#define LOCK_1_Pin GPIO_PIN_14
#define LOCK_1_GPIO_Port GPIOC
#define DOOR_1_Pin GPIO_PIN_15
#define DOOR_1_GPIO_Port GPIOC
#define BLINK_Pin GPIO_PIN_0
#define BLINK_GPIO_Port GPIOC
#define KEY_1_Pin GPIO_PIN_1
#define KEY_1_GPIO_Port GPIOC
#define KEY_2_Pin GPIO_PIN_2
#define KEY_2_GPIO_Port GPIOC
#define KEY_3_Pin GPIO_PIN_3
#define KEY_3_GPIO_Port GPIOC
#define TFT_XL_Pin GPIO_PIN_0
#define TFT_XL_GPIO_Port GPIOA
#define TFT_YU_Pin GPIO_PIN_1
#define TFT_YU_GPIO_Port GPIOA
#define TFT_XR_Pin GPIO_PIN_2
#define TFT_XR_GPIO_Port GPIOA
#define TFT_YD_Pin GPIO_PIN_3
#define TFT_YD_GPIO_Port GPIOA
#define TFT_CS_Pin GPIO_PIN_4
#define TFT_CS_GPIO_Port GPIOA
#define TFT_DC_Pin GPIO_PIN_4
#define TFT_DC_GPIO_Port GPIOC
#define TFT_RES_Pin GPIO_PIN_5
#define TFT_RES_GPIO_Port GPIOC
#define TFT_BLK_Pin GPIO_PIN_0
#define TFT_BLK_GPIO_Port GPIOB
#define LED_8_Pin GPIO_PIN_1
#define LED_8_GPIO_Port GPIOB
#define LOCK_8_Pin GPIO_PIN_2
#define LOCK_8_GPIO_Port GPIOB
#define DOOR_8_Pin GPIO_PIN_10
#define DOOR_8_GPIO_Port GPIOB
#define LED_7_Pin GPIO_PIN_11
#define LED_7_GPIO_Port GPIOB
#define ISD_SS_Pin GPIO_PIN_12
#define ISD_SS_GPIO_Port GPIOB
#define LOCK_7_Pin GPIO_PIN_6
#define LOCK_7_GPIO_Port GPIOC
#define DOOR_7_Pin GPIO_PIN_7
#define DOOR_7_GPIO_Port GPIOC
#define LED_6_Pin GPIO_PIN_8
#define LED_6_GPIO_Port GPIOC
#define LOCK_6_Pin GPIO_PIN_9
#define LOCK_6_GPIO_Port GPIOC
#define DOOR_6_Pin GPIO_PIN_8
#define DOOR_6_GPIO_Port GPIOA
#define LED_5_Pin GPIO_PIN_11
#define LED_5_GPIO_Port GPIOA
#define LOCK_5_Pin GPIO_PIN_12
#define LOCK_5_GPIO_Port GPIOA
#define DOOR_5_Pin GPIO_PIN_15
#define DOOR_5_GPIO_Port GPIOA
#define LED_4_Pin GPIO_PIN_10
#define LED_4_GPIO_Port GPIOC
#define LOCK_4_Pin GPIO_PIN_11
#define LOCK_4_GPIO_Port GPIOC
#define DOOR_4_Pin GPIO_PIN_12
#define DOOR_4_GPIO_Port GPIOC
#define LED_3_Pin GPIO_PIN_2
#define LED_3_GPIO_Port GPIOD
#define LOCK_3_Pin GPIO_PIN_3
#define LOCK_3_GPIO_Port GPIOB
#define DOOR_3_Pin GPIO_PIN_4
#define DOOR_3_GPIO_Port GPIOB
#define LED_2_Pin GPIO_PIN_5
#define LED_2_GPIO_Port GPIOB
#define LOCK_2_Pin GPIO_PIN_8
#define LOCK_2_GPIO_Port GPIOB
#define DOOR_2_Pin GPIO_PIN_9
#define DOOR_2_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
