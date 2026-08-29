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
#include "w25q32.h"
#include "spi_bus.h"
#include "lcd.h"
#include "backlight.h"
#include "sdram.h"
#include "touch.h"
#include "isd1730.h"
#include "zfm40.h"
#include "AT24Cxx.h"
#include "rtc.h"
#include "debug_link.h"
#include "ui_interface.h"  
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c3;

LTDC_HandleTypeDef hltdc;

RTC_HandleTypeDef hrtc;

SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim4;
TIM_HandleTypeDef htim14;

UART_HandleTypeDef huart7;
UART_HandleTypeDef huart1;
DMA_HandleTypeDef hdma_uart7_rx;

SDRAM_HandleTypeDef hsdram1;

/* USER CODE BEGIN PV */
/* Set to 1 by HAL_TIM_PeriodElapsedCallback() every TIM14 tick (~100ms).
   Consumed in the main loop to trigger a non-blocking ZFM40_PollImage()
   call, since the module's touch-detect pin never toggles. */
volatile uint8_t zfm_poll_flag = 0;

/* Drives the identify-or-enroll flow across successive poll ticks so
   the loop never blocks waiting for the finger to be lifted/replaced */
//typedef enum
//{
//    FP_STATE_IDLE = 0,          /* waiting for a finger + running the search    */
//    FP_STATE_WAIT_FINGER_UP,    /* unknown finger: buffer1 captured, wait for lift */
//    FP_STATE_WAIT_SECOND_TOUCH  /* unknown finger: wait for the 2nd placement    */
//} FP_AppStateTypeDef;

//static FP_AppStateTypeDef fp_state = FP_STATE_IDLE;

/* ---- Touch bring-up test (no LCD yet): watch these in Keil's
   Watch window with "Periodic Window Update" enabled while running.
   volatile so the compiler never optimizes the writes away. ---- */
volatile uint16_t g_touch_x       = 0;   /* raw ADC, 0-4095 */
volatile uint16_t g_touch_y       = 0;   /* raw ADC, 0-4095 */
volatile uint32_t g_touch_count   = 0;   /* increments on every valid reading */
volatile uint8_t  g_touch_pressed = 0;   /* 1 while panel is currently pressed */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_LTDC_Init(void);
static void MX_FMC_Init(void);
static void MX_I2C3_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_TIM4_Init(void);
static void MX_RTC_Init(void);
static void MX_TIM14_Init(void);
static void MX_UART7_Init(void);
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
  MX_DMA_Init();
  MX_LTDC_Init();
  MX_FMC_Init();
  MX_I2C3_Init();
  MX_SPI1_Init();
  MX_USART1_UART_Init();
  MX_TIM4_Init();
  MX_RTC_Init();
  MX_TIM14_Init();
  MX_UART7_Init();
  /* USER CODE BEGIN 2 */
	HAL_TIM_Base_Start_IT(&htim14);
///////////////////////////////////////////////////////////////////////////
	Backlight_Init();              
	Backlight_SetBrightness(50);   
///////////////////////////////////////////////////////////////////////////
	RTC_Init();

	RTC_DateTimeTypeDef now;
	RTC_GetDateTime(&now);
///////////////////////////////////////////////////////////////////////////
	SPI_Bus_Init();
	
	ISD1730_Init();
	
	Touch_Init();
	Touch_ForceConversion();   /* wake up PENIRQ detection, see touch.h */
	
	DebugLink_Init(&huart7);
	DebugLink_RegisterCallback(UI_ProcessIncomingLine);
	UI_Init();
///////////////////////////////////////////////////////////////////////////
//ZFM40_Init();
//HAL_Delay(1);
//uint8_t confirm;uint16_t pageId, score;
//ZFM_StatusTypeDef st = ZFM40_VerifyPassword(0x00000000, &confirm);
//HAL_Delay(100);
///////////////////////////////////////////////////////////////////////////
/* Send initial clock/date to PC tool */
	{
			char time_str[16];
			char date_str[16];
			snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", now.hour, now.minute, now.second);
			snprintf(date_str, sizeof(date_str), "%02d/%02d/%04d", now.day, now.month, now.year);
			UI_ShowClock(time_str, date_str);
	}

	UI_ShowMessage("READY SYS");
	UI_SetScreenState("IDLE");
///////////////////////////////////////////////////////////////////////////
	for (uint8_t i = 0; i < LOCKER_COUNT; i++)
	{
			UI_SetLockerState(i, false, false, false);
	}
		
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
	{
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    /* Process UART communication */
    DebugLink_Process();

    /* NEW: Process UI events (from UART or touch) */
    UI_Tick();

    /* NEW: Handle UI events */
    UI_EventTypeDef event;
    uint8_t digit;
    if (UI_GetNextEvent(&event, &digit))
    {
			switch (event)
			{
				case UI_EVENT_BTN_DEPOSIT:
					UI_ShowMessage("StartDeposit");
					/* TODO: Call ChannelManager_StartDeposit() */
					break;

				case UI_EVENT_BTN_RETRIEVE:
					UI_ShowMessage("StartRetrieve");
					/* TODO: Call ChannelManager_StartRetrieve() */
					break;

				case UI_EVENT_BTN_ADMIN:
					UI_ShowMessage("admin login flow");
					/* TODO: Start admin login flow */
					break;

				case UI_EVENT_PHONE_ANDROID:
					UI_ShowMessage("android");
					break;

				case UI_EVENT_PHONE_IPHONE:
					UI_ShowMessage("ios");
					break;

				case UI_EVENT_DIGIT_0:
				case UI_EVENT_DIGIT_1:
				case UI_EVENT_DIGIT_2:
				case UI_EVENT_DIGIT_3:
				case UI_EVENT_DIGIT_4:
				case UI_EVENT_DIGIT_5:
				case UI_EVENT_DIGIT_6:
				case UI_EVENT_DIGIT_7:
				case UI_EVENT_DIGIT_8:
				case UI_EVENT_DIGIT_9:
					/* TODO: Append digit to current input buffer */
					break;

				case UI_EVENT_BACKSPACE:
					/* TODO: Remove last digit from input buffer */
					break;

				case UI_EVENT_CONFIRM:
					/* TODO: Confirm current input */
					break;

				case UI_EVENT_CANCEL:
					/* TODO: Cancel current operation */
					break;

				default:
						break;
			}
    }	
		
		
		
    /* ---- Touch bring-up test: cheap GPIO check every loop pass,
       SPI transaction only when actually pressed. ---- */
//    if (Touch_IsPressed())
//    {
//        TOUCH_RawPointTypeDef pt;

//        g_touch_pressed = 1;

//        if (Touch_ReadRaw(&pt) == TOUCH_OK)
//        {
//            g_touch_x = pt.x;
//            g_touch_y = pt.y;
//            g_touch_count++;
//        }
//    }
//    else
//    {
//        g_touch_pressed = 0;
//    }
///////////////////////////////////////////////////////////////////////////
//    if (!zfm_poll_flag) continue; /* wait for the next TIM14 (~100ms) tick */
//    zfm_poll_flag = 0;

//    switch (fp_state)
//    {
//        case FP_STATE_IDLE:
//        {
//            /* short-timeout poll: returns immediately with OK or NO_FINGER,
//               never blocks the loop waiting for a finger */
//            if (ZFM40_PollImage(&confirm) != ZFM_OK || confirm != ZFM_CONF_OK) continue;

//            if (ZFM40_GenChar(1, &confirm) != ZFM_OK || confirm != ZFM_CONF_OK) continue;

//            {
//                ZFM_StatusTypeDef searchStatus = ZFM40_Search(1, 0, 1000, &pageId, &score, &confirm);

//                /* ZFM40_Search() returns ZFM_NACK for a clean "not found" reply
//                   (confirm == ZFM_CONF_SEARCH_NOT_FOUND) -- that is a valid,
//                   expected outcome and must still fall through to the confirm
//                   check below. Only bail out on a real transport failure. */
//                if ((searchStatus != ZFM_OK) && (searchStatus != ZFM_NACK)) continue;
//            }

//            if (confirm == ZFM_CONF_OK)
//            {
//                /* known finger: blink PA0 once */
//                HAL_GPIO_WritePin(FP_MATCH_LED_GPIO_Port, FP_MATCH_LED_Pin, GPIO_PIN_SET);
//                HAL_Delay(200);
//                HAL_GPIO_WritePin(FP_MATCH_LED_GPIO_Port, FP_MATCH_LED_Pin, GPIO_PIN_RESET);
//            }
//            else
//            {
//                /* unknown finger: CharBuffer1 already holds this capture,
//                   now wait for it to be lifted before asking for the
//                   confirming second touch (enrollment needs 2 captures) */
//                fp_state = FP_STATE_WAIT_FINGER_UP;
//            }

//            break;
//        }

//        case FP_STATE_WAIT_FINGER_UP:
//        {
//            /* PollImage() returns ZFM_NACK (not ZFM_OK) whenever confirm
//               != ZFM_CONF_OK, so a "finger lifted" reply is ZFM_NACK with
//               confirm == ZFM_CONF_NO_FINGER -- never ZFM_OK */
//            ZFM_StatusTypeDef pollStatus = ZFM40_PollImage(&confirm);

//            if ((pollStatus == ZFM_NACK) && (confirm == ZFM_CONF_NO_FINGER))
//            {
//                fp_state = FP_STATE_WAIT_SECOND_TOUCH;
//            }

//            break;
//        }

//        case FP_STATE_WAIT_SECOND_TOUCH:
//        {
//            uint16_t templateCount;

//            if (ZFM40_PollImage(&confirm) != ZFM_OK || confirm != ZFM_CONF_OK) continue;

//            if (ZFM40_GenChar(2, &confirm) != ZFM_OK || confirm != ZFM_CONF_OK)
//            {
//                fp_state = FP_STATE_IDLE; /* second capture unusable, give up */
//                continue;
//            }

//            if (ZFM40_RegModel(&confirm) != ZFM_OK || confirm != ZFM_CONF_OK)
//            {
//                fp_state = FP_STATE_IDLE; /* the two captures did not merge, e.g. different finger */
//                continue;
//            }

//            /* store at the next free page: this assumes templates are
//               kept compact from page 0 with no gaps left by deletions */
//            if (ZFM40_GetTemplateCount(&templateCount, &confirm) != ZFM_OK || confirm != ZFM_CONF_OK)
//            {
//                fp_state = FP_STATE_IDLE;
//                continue;
//            }

//            ZFM40_StoreChar(1, templateCount, &confirm);

//            fp_state = FP_STATE_IDLE;
//            break;
//        }

//        default:
//        {
//            fp_state = FP_STATE_IDLE;
//            break;
//        }
//    }
///////////////////////////////////////////////////////////////////////////
//		if (!HAL_GPIO_ReadPin(KEY_2_GPIO_Port,KEY_2_Pin))
//		{
//			ISD1730_PlayMessage(i);
//			HAL_Delay(2000);
//		}
//		if (!HAL_GPIO_ReadPin(KEY_1_GPIO_Port,KEY_1_Pin))
//		{
//			if (i == NOT_FOUND)
//				i = FULL_BOX;
//			else
//				i++;
//			
//			HAL_Delay(2000);
//		}
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE|RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 180;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Activate the Over-Drive mode
  */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }

  /** Enables the Clock Security System
  */
  HAL_RCC_EnableCSS();
}

/**
  * @brief I2C3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C3_Init(void)
{

  /* USER CODE BEGIN I2C3_Init 0 */

  /* USER CODE END I2C3_Init 0 */

  /* USER CODE BEGIN I2C3_Init 1 */

  /* USER CODE END I2C3_Init 1 */
  hi2c3.Instance = I2C3;
  hi2c3.Init.ClockSpeed = 100000;
  hi2c3.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c3.Init.OwnAddress1 = 0;
  hi2c3.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c3.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c3.Init.OwnAddress2 = 0;
  hi2c3.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c3.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c3) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c3, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c3, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C3_Init 2 */

  /* USER CODE END I2C3_Init 2 */

}

/**
  * @brief LTDC Initialization Function
  * @param None
  * @retval None
  */
static void MX_LTDC_Init(void)
{

  /* USER CODE BEGIN LTDC_Init 0 */

  /* USER CODE END LTDC_Init 0 */

  LTDC_LayerCfgTypeDef pLayerCfg = {0};
  LTDC_LayerCfgTypeDef pLayerCfg1 = {0};

  /* USER CODE BEGIN LTDC_Init 1 */

  /* USER CODE END LTDC_Init 1 */
  hltdc.Instance = LTDC;
  hltdc.Init.HSPolarity = LTDC_HSPOLARITY_AL;
  hltdc.Init.VSPolarity = LTDC_VSPOLARITY_AL;
  hltdc.Init.DEPolarity = LTDC_DEPOLARITY_AL;
  hltdc.Init.PCPolarity = LTDC_PCPOLARITY_IPC;
  hltdc.Init.HorizontalSync = 7;
  hltdc.Init.VerticalSync = 3;
  hltdc.Init.AccumulatedHBP = 14;
  hltdc.Init.AccumulatedVBP = 5;
  hltdc.Init.AccumulatedActiveW = 654;
  hltdc.Init.AccumulatedActiveH = 485;
  hltdc.Init.TotalWidth = 660;
  hltdc.Init.TotalHeigh = 487;
  hltdc.Init.Backcolor.Blue = 0;
  hltdc.Init.Backcolor.Green = 0;
  hltdc.Init.Backcolor.Red = 0;
  if (HAL_LTDC_Init(&hltdc) != HAL_OK)
  {
    Error_Handler();
  }
  pLayerCfg.WindowX0 = 0;
  pLayerCfg.WindowX1 = 0;
  pLayerCfg.WindowY0 = 0;
  pLayerCfg.WindowY1 = 0;
  pLayerCfg.PixelFormat = LTDC_PIXEL_FORMAT_ARGB8888;
  pLayerCfg.Alpha = 0;
  pLayerCfg.Alpha0 = 0;
  pLayerCfg.BlendingFactor1 = LTDC_BLENDING_FACTOR1_CA;
  pLayerCfg.BlendingFactor2 = LTDC_BLENDING_FACTOR2_CA;
  pLayerCfg.FBStartAdress = 0;
  pLayerCfg.ImageWidth = 0;
  pLayerCfg.ImageHeight = 0;
  pLayerCfg.Backcolor.Blue = 0;
  pLayerCfg.Backcolor.Green = 0;
  pLayerCfg.Backcolor.Red = 0;
  if (HAL_LTDC_ConfigLayer(&hltdc, &pLayerCfg, 0) != HAL_OK)
  {
    Error_Handler();
  }
  pLayerCfg1.WindowX0 = 0;
  pLayerCfg1.WindowX1 = 0;
  pLayerCfg1.WindowY0 = 0;
  pLayerCfg1.WindowY1 = 0;
  pLayerCfg1.PixelFormat = LTDC_PIXEL_FORMAT_ARGB8888;
  pLayerCfg1.Alpha = 0;
  pLayerCfg1.Alpha0 = 0;
  pLayerCfg1.BlendingFactor1 = LTDC_BLENDING_FACTOR1_CA;
  pLayerCfg1.BlendingFactor2 = LTDC_BLENDING_FACTOR2_CA;
  pLayerCfg1.FBStartAdress = 0;
  pLayerCfg1.ImageWidth = 0;
  pLayerCfg1.ImageHeight = 0;
  pLayerCfg1.Backcolor.Blue = 0;
  pLayerCfg1.Backcolor.Green = 0;
  pLayerCfg1.Backcolor.Red = 0;
  if (HAL_LTDC_ConfigLayer(&hltdc, &pLayerCfg1, 1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN LTDC_Init 2 */

  /* USER CODE END LTDC_Init 2 */

}

/**
  * @brief RTC Initialization Function
  * @param None
  * @retval None
  */
static void MX_RTC_Init(void)
{

  /* USER CODE BEGIN RTC_Init 0 */

  /* USER CODE END RTC_Init 0 */

  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef sDate = {0};

  /* USER CODE BEGIN RTC_Init 1 */

  /* USER CODE END RTC_Init 1 */

  /** Initialize RTC Only
  */
  hrtc.Instance = RTC;
  hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
  hrtc.Init.AsynchPrediv = 127;
  hrtc.Init.SynchPrediv = 255;
  hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
  hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE BEGIN Check_RTC_BKUP */
  if (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR0) == RTC_COLD_START_MAGIC)
  {
    /* VBAT has kept a real calendar running since a previous boot --
     * skip CubeMX's unconditional default below, don't stomp it. */
    return;
  }
  /* USER CODE END Check_RTC_BKUP */

  /** Initialize RTC and set the Time and Date
  */
  sTime.Hours = 0x0;
  sTime.Minutes = 0x0;
  sTime.Seconds = 0x0;
  sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sTime.StoreOperation = RTC_STOREOPERATION_RESET;
  if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }
  sDate.WeekDay = RTC_WEEKDAY_MONDAY;
  sDate.Month = RTC_MONTH_JANUARY;
  sDate.Date = 0x1;
  sDate.Year = 0x0;

  if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN RTC_Init 2 */

  /* USER CODE END RTC_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief TIM4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM4_Init(void)
{

  /* USER CODE BEGIN TIM4_Init 0 */

  /* USER CODE END TIM4_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM4_Init 1 */

  /* USER CODE END TIM4_Init 1 */
  htim4.Instance = TIM4;
  htim4.Init.Prescaler = 0;
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.Period = 65535;
  htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim4) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim4, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM4_Init 2 */

  /* USER CODE END TIM4_Init 2 */
  HAL_TIM_MspPostInit(&htim4);

}

/**
  * @brief TIM14 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM14_Init(void)
{

  /* USER CODE BEGIN TIM14_Init 0 */

  /* USER CODE END TIM14_Init 0 */

  /* USER CODE BEGIN TIM14_Init 1 */

  /* USER CODE END TIM14_Init 1 */
  htim14.Instance = TIM14;
  htim14.Init.Prescaler = 8999;
  htim14.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim14.Init.Period = 999;
  htim14.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim14.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim14) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM14_Init 2 */

  /* USER CODE END TIM14_Init 2 */

}

/**
  * @brief UART7 Initialization Function
  * @param None
  * @retval None
  */
static void MX_UART7_Init(void)
{

  /* USER CODE BEGIN UART7_Init 0 */

  /* USER CODE END UART7_Init 0 */

  /* USER CODE BEGIN UART7_Init 1 */

  /* USER CODE END UART7_Init 1 */
  huart7.Instance = UART7;
  huart7.Init.BaudRate = 115200;
  huart7.Init.WordLength = UART_WORDLENGTH_8B;
  huart7.Init.StopBits = UART_STOPBITS_1;
  huart7.Init.Parity = UART_PARITY_NONE;
  huart7.Init.Mode = UART_MODE_TX_RX;
  huart7.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart7.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart7) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN UART7_Init 2 */

  /* USER CODE END UART7_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 57600;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Stream3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream3_IRQn);

}

/* FMC initialization function */
static void MX_FMC_Init(void)
{

  /* USER CODE BEGIN FMC_Init 0 */

  /* USER CODE END FMC_Init 0 */

  FMC_SDRAM_TimingTypeDef SdramTiming = {0};

  /* USER CODE BEGIN FMC_Init 1 */

  /* USER CODE END FMC_Init 1 */

  /** Perform the SDRAM1 memory initialization sequence
  */
  hsdram1.Instance = FMC_SDRAM_DEVICE;
  /* hsdram1.Init */
  hsdram1.Init.SDBank = FMC_SDRAM_BANK1;
  hsdram1.Init.ColumnBitsNumber = FMC_SDRAM_COLUMN_BITS_NUM_8;
  hsdram1.Init.RowBitsNumber = FMC_SDRAM_ROW_BITS_NUM_12;
  hsdram1.Init.MemoryDataWidth = FMC_SDRAM_MEM_BUS_WIDTH_16;
  hsdram1.Init.InternalBankNumber = FMC_SDRAM_INTERN_BANKS_NUM_4;
  hsdram1.Init.CASLatency = FMC_SDRAM_CAS_LATENCY_1;
  hsdram1.Init.WriteProtection = FMC_SDRAM_WRITE_PROTECTION_DISABLE;
  hsdram1.Init.SDClockPeriod = FMC_SDRAM_CLOCK_DISABLE;
  hsdram1.Init.ReadBurst = FMC_SDRAM_RBURST_DISABLE;
  hsdram1.Init.ReadPipeDelay = FMC_SDRAM_RPIPE_DELAY_0;
  /* SdramTiming */
  SdramTiming.LoadToActiveDelay = 16;
  SdramTiming.ExitSelfRefreshDelay = 16;
  SdramTiming.SelfRefreshTime = 16;
  SdramTiming.RowCycleDelay = 16;
  SdramTiming.WriteRecoveryTime = 16;
  SdramTiming.RPDelay = 16;
  SdramTiming.RCDDelay = 16;

  if (HAL_SDRAM_Init(&hsdram1, &SdramTiming) != HAL_OK)
  {
    Error_Handler( );
  }

  /* USER CODE BEGIN FMC_Init 2 */

  /* USER CODE END FMC_Init 2 */
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
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD_GPIO_Port, LD_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, LOCK_1_Pin|LOCK_3_Pin|LD_3_Pin|LTDC_RST_Pin
                          |LOCK_7_Pin|LD_7_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, LD_1_Pin|LOCK_2_Pin|LD_2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, LOCK_4_Pin|LD_4_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, LOCK_5_Pin|LD_5_Pin|LOCK_8_Pin|LD_8_Pin
                          |XPT_CS_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOG, LOCK_6_Pin|LD_6_Pin|ISD_SS_Pin|W25_CS_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : KEY_1_Pin KEY_2_Pin */
  GPIO_InitStruct.Pin = KEY_1_Pin|KEY_2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pin : LD_Pin */
  GPIO_InitStruct.Pin = LD_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : DOOR_1_Pin */
  GPIO_InitStruct.Pin = DOOR_1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(DOOR_1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LOCK_1_Pin LOCK_3_Pin LD_3_Pin LTDC_RST_Pin
                           LOCK_7_Pin LD_7_Pin */
  GPIO_InitStruct.Pin = LOCK_1_Pin|LOCK_3_Pin|LD_3_Pin|LTDC_RST_Pin
                          |LOCK_7_Pin|LD_7_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : LD_1_Pin LOCK_2_Pin LD_2_Pin */
  GPIO_InitStruct.Pin = LD_1_Pin|LOCK_2_Pin|LD_2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : DOOR_2_Pin DOOR_3_Pin DOOR_7_Pin */
  GPIO_InitStruct.Pin = DOOR_2_Pin|DOOR_3_Pin|DOOR_7_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : DOOR_4_Pin DOOR_5_Pin */
  GPIO_InitStruct.Pin = DOOR_4_Pin|DOOR_5_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : LOCK_4_Pin LD_4_Pin */
  GPIO_InitStruct.Pin = LOCK_4_Pin|LD_4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : LOCK_5_Pin LD_5_Pin LOCK_8_Pin LD_8_Pin
                           XPT_CS_Pin */
  GPIO_InitStruct.Pin = LOCK_5_Pin|LD_5_Pin|LOCK_8_Pin|LD_8_Pin
                          |XPT_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pins : DOOR_6_Pin DOOR_8_Pin */
  GPIO_InitStruct.Pin = DOOR_6_Pin|DOOR_8_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pins : LOCK_6_Pin LD_6_Pin ISD_SS_Pin W25_CS_Pin */
  GPIO_InitStruct.Pin = LOCK_6_Pin|LD_6_Pin|ISD_SS_Pin|W25_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  /*Configure GPIO pin : XPT_IRQ_Pin */
  GPIO_InitStruct.Pin = XPT_IRQ_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(XPT_IRQ_GPIO_Port, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */



/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	if (htim->Instance == TIM14)
	{
		HAL_GPIO_TogglePin(LD_GPIO_Port, LD_Pin);
		zfm_poll_flag = 1;
	}
}
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    DebugLink_RxEventCallback(huart, Size);
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
