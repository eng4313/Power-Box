/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdbool.h>
#include <stdint.h>

#include "Typedef.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
static uint8_t 		Cabinet_Read_Door(uint8_t index);
static void 			Cabinet_Set_Lock(uint8_t index, uint8_t state);
static void 			Cabinet_Set_LED(uint8_t index, uint8_t state);
static void 			Cabinet_Toggle_LED(uint8_t index);
void 							Cabinet_Init(void);
void 							Cabinet_Update_Door_Status(void);
void 							Cabinet_Update_LEDs(void);
void 							Cabinet_Set_Empty(uint8_t index, uint8_t empty_state);
void 							Cabinet_Control_Lock(uint8_t index, uint8_t state);
uint8_t 					Cabinet_Get_Blinker(uint8_t index);
uint8_t 					Cabinet_Get_Status(uint8_t index);
void 							Cabinet_Reset_Blinker(uint8_t index);
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
static Cabinet_Pins_t cabinets[CABINET_COUNT] = 
{
	#if CABINET_COUNT >= 1U
	{LED_1_GPIO_Port, LED_1_Pin, LOCK_1_GPIO_Port, LOCK_1_Pin, DOOR_1_GPIO_Port, DOOR_1_Pin}
	#endif

	#if CABINET_COUNT >= 2U
	,{LED_2_GPIO_Port, LED_2_Pin, LOCK_2_GPIO_Port, LOCK_2_Pin, DOOR_2_GPIO_Port, DOOR_2_Pin}
	#endif

	#if CABINET_COUNT >= 3U
	,{LED_3_GPIO_Port, LED_3_Pin, LOCK_3_GPIO_Port, LOCK_3_Pin, DOOR_3_GPIO_Port, DOOR_3_Pin}
	#endif

	#if CABINET_COUNT >= 4U
	,{LED_4_GPIO_Port, LED_4_Pin, LOCK_4_GPIO_Port, LOCK_4_Pin, DOOR_4_GPIO_Port, DOOR_4_Pin}
	#endif

	#if CABINET_COUNT >= 5U
	{LED_5_GPIO_Port, LED_5_Pin, LOCK_5_GPIO_Port, LOCK_5_Pin, DOOR_5_GPIO_Port, DOOR_5_Pin}
	#endif

	#if CABINET_COUNT >= 6U
	,{LED_6_GPIO_Port, LED_6_Pin, LOCK_6_GPIO_Port, LOCK_6_Pin, DOOR_6_GPIO_Port, DOOR_6_Pin}
	#endif

	#if CABINET_COUNT >= 7U
	,{LED_7_GPIO_Port, LED_7_Pin, LOCK_7_GPIO_Port, LOCK_7_Pin, DOOR_7_GPIO_Port, DOOR_7_Pin}
	#endif

	#if CABINET_COUNT >= 8U
	,{LED_8_GPIO_Port, LED_8_Pin, LOCK_8_GPIO_Port, LOCK_8_Pin, DOOR_8_GPIO_Port, DOOR_8_Pin}
	#endif
	
	#if CABINET_COUNT >= 9U
	{LED_9_GPIO_Port, LED_9_Pin, LOCK_9_GPIO_Port, LOCK_9_Pin, DOOR_9_GPIO_Port, DOOR_9_Pin}
	#endif

	#if CABINET_COUNT >= 10U
	,{LED_10_GPIO_Port, LED_10_Pin, LOCK_10_GPIO_Port, LOCK_10_Pin, DOOR_10_GPIO_Port, DOOR_10_Pin}
	#endif

	#if CABINET_COUNT >= 11U
	,{LED_11_GPIO_Port, LED_11_Pin, LOCK_11_GPIO_Port, LOCK_11_Pin, DOOR_11_GPIO_Port, DOOR_11_Pin}
	#endif

	#if CABINET_COUNT >= 12U
	,{LED_12_GPIO_Port, LED_12_Pin, LOCK_12_GPIO_Port, LOCK_12_Pin, DOOR_12_GPIO_Port, DOOR_12_Pin}
	#endif
	
		#if CABINET_COUNT >= 13U
	{LED_13_GPIO_Port, LED_13_Pin, LOCK_13_GPIO_Port, LOCK_13_Pin, DOOR_13_GPIO_Port, DOOR_13_Pin}
	#endif

	#if CABINET_COUNT >= 14U
	,{LED_14_GPIO_Port, LED_14_Pin, LOCK_14_GPIO_Port, LOCK_14_Pin, DOOR_14_GPIO_Port, DOOR_14_Pin}
	#endif

	#if CABINET_COUNT >= 15U
	,{LED_15_GPIO_Port, LED_15_Pin, LOCK_15_GPIO_Port, LOCK_15_Pin, DOOR_15_GPIO_Port, DOOR_15_Pin}
	#endif

	#if CABINET_COUNT >= 16U
	,{LED_16_GPIO_Port, LED_16_Pin, LOCK_16_GPIO_Port, LOCK_16_Pin, DOOR_16_GPIO_Port, DOOR_16_Pin}
	#endif
	
		#if CABINET_COUNT >= 17U
	{LED_17_GPIO_Port, LED_17_Pin, LOCK_17_GPIO_Port, LOCK_17_Pin, DOOR_17_GPIO_Port, DOOR_17_Pin}
	#endif

	#if CABINET_COUNT >= 18U
	,{LED_18_GPIO_Port, LED_18_Pin, LOCK_18_GPIO_Port, LOCK_18_Pin, DOOR_18_GPIO_Port, DOOR_18_Pin}
	#endif

	#if CABINET_COUNT >= 19U
	,{LED_19_GPIO_Port, LED_19_Pin, LOCK_19_GPIO_Port, LOCK_19_Pin, DOOR_19_GPIO_Port, DOOR_19_Pin}
	#endif

	#if CABINET_COUNT >= 20U
	,{LED_20_GPIO_Port, LED_20_Pin, LOCK_20_GPIO_Port, LOCK_20_Pin, DOOR_20_GPIO_Port, DOOR_20_Pin}
	#endif
	
	#if CABINET_COUNT >= 21U
	{LED_21_GPIO_Port, LED_21_Pin, LOCK_21_GPIO_Port, LOCK_21_Pin, DOOR_21_GPIO_Port, DOOR_21_Pin}
	#endif

	#if CABINET_COUNT >= 22U
	,{LED_22_GPIO_Port, LED_22_Pin, LOCK_22_GPIO_Port, LOCK_22_Pin, DOOR_22_GPIO_Port, DOOR_22_Pin}
	#endif

	#if CABINET_COUNT >= 23U
	,{LED_23_GPIO_Port, LED_23_Pin, LOCK_23_GPIO_Port, LOCK_23_Pin, DOOR_23_GPIO_Port, DOOR_23_Pin}
	#endif

	#if CABINET_COUNT >= 24U
	,{LED_24_GPIO_Port, LED_24_Pin, LOCK_24_GPIO_Port, LOCK_24_Pin, DOOR_24_GPIO_Port, DOOR_24_Pin}
	#endif
	
	#if CABINET_COUNT >= 25U
	{LED_25_GPIO_Port, LED_25_Pin, LOCK_25_GPIO_Port, LOCK_25_Pin, DOOR_25_GPIO_Port, DOOR_25_Pin}
	#endif

	#if CABINET_COUNT >= 26U
	,{LED_26_GPIO_Port, LED_26_Pin, LOCK_26_GPIO_Port, LOCK_26_Pin, DOOR_26_GPIO_Port, DOOR_26_Pin}
	#endif

	#if CABINET_COUNT >= 27U
	,{LED_27_GPIO_Port, LED_27_Pin, LOCK_27_GPIO_Port, LOCK_27_Pin, DOOR_27_GPIO_Port, DOOR_27_Pin}
	#endif

	#if CABINET_COUNT >= 28U
	,{LED_28_GPIO_Port, LED_28_Pin, LOCK_28_GPIO_Port, LOCK_28_Pin, DOOR_28_GPIO_Port, DOOR_28_Pin}
	#endif
	
	#if CABINET_COUNT >= 29U
	{LED_29_GPIO_Port, LED_29_Pin, LOCK_29_GPIO_Port, LOCK_29_Pin, DOOR_29_GPIO_Port, DOOR_29_Pin}
	#endif

	#if CABINET_COUNT >= 30U
	,{LED_30_GPIO_Port, LED_30_Pin, LOCK_30_GPIO_Port, LOCK_30_Pin, DOOR_30_GPIO_Port, DOOR_30_Pin}
	#endif

	#if CABINET_COUNT >= 31U
	,{LED_31_GPIO_Port, LED_31_Pin, LOCK_31_GPIO_Port, LOCK_31_Pin, DOOR_31_GPIO_Port, DOOR_31_Pin}
	#endif

	#if CABINET_COUNT >= 32U
	,{LED_32_GPIO_Port, LED_32_Pin, LOCK_32_GPIO_Port, LOCK_32_Pin, DOOR_32_GPIO_Port, DOOR_32_Pin}
	#endif
	/* Add more cabinets here if needed (up to 32) */
};

/* Status array for all cabinets */
static Cabinet_Status_t cabinet_status[CABINET_COUNT];
/* LED state tracking for toggling (current physical state of each LED) */
static uint8_t led_physical_state[CABINET_COUNT];




/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */
	Cabinet_Init();
	HAL_TIM_Base_Start_IT(&htim2);
	HAL_TIM_Base_Start_IT(&htim3);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
		Cabinet_Update_Door_Status();
		
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }

  /** Enables the Clock Security System
  */
  HAL_RCC_EnableCSS();
}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 7199;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 1999;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 7199;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 9999;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, LED_1_Pin|LOCK_1_Pin|LED_2_Pin|LOCK_2_Pin
                          |LED_3_Pin|LOCK_3_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, LED_4_Pin|LOCK_4_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : LED_Pin */
  GPIO_InitStruct.Pin = LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LED_1_Pin LOCK_1_Pin LED_2_Pin LOCK_2_Pin
                           LED_3_Pin LOCK_3_Pin */
  GPIO_InitStruct.Pin = LED_1_Pin|LOCK_1_Pin|LED_2_Pin|LOCK_2_Pin
                          |LED_3_Pin|LOCK_3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : DOOR_1_Pin DOOR_2_Pin */
  GPIO_InitStruct.Pin = DOOR_1_Pin|DOOR_2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : DOOR_3_Pin DOOR_4_Pin */
  GPIO_InitStruct.Pin = DOOR_3_Pin|DOOR_4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : LED_4_Pin LOCK_4_Pin */
  GPIO_InitStruct.Pin = LED_4_Pin|LOCK_4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/*-------------------------------------------------------------------------------
  Function  : Cabinet_Read_Door
  Brief     : Read door status for a specific cabinet
  Param     : index - Cabinet index (0 to CABINET_COUNT-1)
  Return    : 1 if door is open, 0 if closed
-------------------------------------------------------------------------------*/
static uint8_t Cabinet_Read_Door(uint8_t index)
{
	if (index >= CABINET_COUNT)
			return 0;
	
	return (HAL_GPIO_ReadPin(cabinets[index].door_port, 
                           cabinets[index].door_pin) == GPIO_PIN_SET) ? 1 : 0;
}

/*-------------------------------------------------------------------------------
  Function  : Cabinet_Set_Lock
  Brief     : Set lock output for a specific cabinet
  Param     : index - Cabinet index (0 to CABINET_COUNT-1)
              state - 1 = unlock, 0 = lock
-------------------------------------------------------------------------------*/
static void Cabinet_Set_Lock(uint8_t index, uint8_t state)
{
	if (index >= CABINET_COUNT)
		 return;
    
	GPIO_PinState pin_state = (state == 1) ? GPIO_PIN_SET : GPIO_PIN_RESET;
	HAL_GPIO_WritePin(cabinets[index].lock_port,cabinets[index].lock_pin,pin_state);
}

/*-------------------------------------------------------------------------------
  Function  : Cabinet_Set_LED
  Brief     : Set LED output for a specific cabinet
  Param     : index - Cabinet index (0 to CABINET_COUNT-1)
              state - 1 = LED on, 0 = LED off
-------------------------------------------------------------------------------*/
static void Cabinet_Set_LED(uint8_t index, uint8_t state)
{
	if (index >= CABINET_COUNT)
			return;
	
	GPIO_PinState pin_state = (state == 1) ? GPIO_PIN_SET : GPIO_PIN_RESET;
	HAL_GPIO_WritePin(cabinets[index].led_port, cabinets[index].led_pin, pin_state);
	
	/* Update physical state tracker */
	led_physical_state[index] = state;
}

/*-------------------------------------------------------------------------------
  Function  : Cabinet_Toggle_LED
  Brief     : Toggle LED output for a specific cabinet
  Param     : index - Cabinet index (0 to CABINET_COUNT-1)
-------------------------------------------------------------------------------*/
static void Cabinet_Toggle_LED(uint8_t index)
{
	if (index >= CABINET_COUNT)
			return;
    
	uint8_t new_state = (led_physical_state[index] == 1) ? 0 : 1;
	Cabinet_Set_LED(index, new_state);
}

/*-------------------------------------------------------------------------------
  Function  : Cabinet_Init
  Brief     : Initialize all cabinet status to default (all zeros)
-------------------------------------------------------------------------------*/
void Cabinet_Init(void)
{
	for (uint8_t i = 0; i < CABINET_COUNT; i++)
	{
		cabinet_status[i].all = 0;   /* Clear all bits (blinker=0, empty=0, reserved=0) */
		led_physical_state[i] = 0;   /* All LEDs initially off */
		Cabinet_Set_LED(i, 0);       /* Ensure physical LED is off */
	}
}

/*-------------------------------------------------------------------------------
  Function  : Cabinet_Update_Door_Status
  Brief     : Check door status for all cabinets and update blinker bits
              Logic: If door == 1 (open), set blinker = 1
              This function should be called in main loop
-------------------------------------------------------------------------------*/
void Cabinet_Update_Door_Status(void)
{
	for (uint8_t i = 0; i < CABINET_COUNT; i++)
	{
		uint8_t door_status = Cabinet_Read_Door(i);
		/* Update blinker state based on door status */
		if (door_status == 1)   /* Door is open */
			cabinet_status[i].bits.blinker = 1;
		/* If door is closed (0), blinker state does not change */
	}
}

/*-------------------------------------------------------------------------------
  Function  : Cabinet_Update_LEDs
  Brief     : Update LED states based on blinker and empty status
              This function should be called in Timer3 interrupt (every 1 second)
              Logic:
              - If blinker == 1: Toggle LED (blinking effect)
              - If blinker == 0: Check empty state
                * If empty == 1: LED off
                * If empty == 0: LED on (steady)
-------------------------------------------------------------------------------*/
void Cabinet_Update_LEDs(void)
{
	for (uint8_t i = 0; i < CABINET_COUNT; i++)
	{
		if (cabinet_status[i].bits.blinker == 1)
		{
			/* Blinker active: Toggle LED */
			Cabinet_Toggle_LED(i);
		}
		else
		{
			/* Blinker inactive: Set LED based on empty state */
			if (cabinet_status[i].bits.empty == 1)
				Cabinet_Set_LED(i, 0);   /* Empty: LED off */
			else
				Cabinet_Set_LED(i, 1);   /* Not empty: LED on (steady) */
		}
	}
}

/*-------------------------------------------------------------------------------
  Function  : Cabinet_Set_Empty
  Brief     : Set empty state for a specific cabinet
  Param     : index       - Cabinet index (0 to CABINET_COUNT-1)
              empty_state - 1 = cabinet is empty, 0 = cabinet is occupied
-------------------------------------------------------------------------------*/
void Cabinet_Set_Empty(uint8_t index, uint8_t empty_state)
{
	if (index < CABINET_COUNT)
	{
		cabinet_status[index].bits.empty = (empty_state == 1) ? 1 : 0;
	}
}

/*-------------------------------------------------------------------------------
  Function  : Cabinet_Control_Lock
  Brief     : Manually control lock for a specific cabinet
  Param     : index - Cabinet index (0 to CABINET_COUNT-1)
              state - 1 = unlock, 0 = lock
-------------------------------------------------------------------------------*/
void Cabinet_Control_Lock(uint8_t index, uint8_t state)
{
    Cabinet_Set_Lock(index, state);
}

/*-------------------------------------------------------------------------------
  Function  : Cabinet_Get_Blinker
  Brief     : Get current blinker state of a specific cabinet
  Param     : index - Cabinet index (0 to CABINET_COUNT-1)
  Return    : 1 if blinker is active, 0 otherwise
-------------------------------------------------------------------------------*/
uint8_t Cabinet_Get_Blinker(uint8_t index)
{
	if (index < CABINET_COUNT)
	{
		return cabinet_status[index].bits.blinker;
	}
	return 0;
}

/*-------------------------------------------------------------------------------
  Function  : Cabinet_Get_Status
  Brief     : Get full status byte of a specific cabinet
  Param     : index - Cabinet index (0 to CABINET_COUNT-1)
  Return    : 8-bit status (bit0=blinker, bit1=empty, bit2-7=reserved)
-------------------------------------------------------------------------------*/
uint8_t Cabinet_Get_Status(uint8_t index)
{
	if (index < CABINET_COUNT)
	{
		return cabinet_status[index].all;
	}
	return 0;
}

/*-------------------------------------------------------------------------------
  Function  : Cabinet_Reset_Blinker
  Brief     : Manually reset blinker state for a specific cabinet
  Param     : index - Cabinet index (0 to CABINET_COUNT-1)
-------------------------------------------------------------------------------*/
void Cabinet_Reset_Blinker(uint8_t index)
{
	if (index < CABINET_COUNT)
	{
		cabinet_status[index].bits.blinker = 0;
	}
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	if (htim->Instance == TIM2)
	{
		HAL_GPIO_TogglePin(LED_GPIO_Port,LED_Pin);
	}
	else if (htim->Instance == TIM3)
	{
		Cabinet_Update_LEDs();
	}
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
