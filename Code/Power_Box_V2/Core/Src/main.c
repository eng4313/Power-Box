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
#include "channel_hw.h"
#include "channel_manager.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* PA0 blinks once when a scanned finger matches a template already
   stored in the module; not used anywhere else in this project */
#define FP_MATCH_LED_GPIO_Port     GPIOA
#define FP_MATCH_LED_Pin           GPIO_PIN_0

/* Audio message definitions for test (match ISD_MESSAGE_t in isd1730.h) */
#define AUDIO_MSG_WELCOME          FULL_BOX
#define AUDIO_MSG_PLACE_FINGER     ENTER_FINGER
#define AUDIO_MSG_FINGER_AGAIN     ENTER_FINGER
#define AUDIO_MSG_FINGER_MISMATCH  WRONG_FINGER
#define AUDIO_MSG_DOOR_OPENED      DOOR_OPENED
#define AUDIO_MSG_DOOR_IS_OPEN     DOOR_IS_OPEN
#define AUDIO_MSG_SUCCESS          END_TIME
#define AUDIO_MSG_CANCELLED        NOT_SAVE
#define AUDIO_MSG_TIMEOUT          END_TIME
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

/* Test state machine for single locker */
typedef enum
{
    TEST_STATE_IDLE = 0,
    TEST_STATE_WAIT_FINGER_1,      /* First fingerprint scan */
    TEST_STATE_WAIT_FINGER_2,      /* Second fingerprint scan (enroll) */
    TEST_STATE_DOOR_OPEN,          /* Locker opened, waiting for door close */
    TEST_STATE_COMPLETE            /* Test finished successfully */
} TestStateTypeDef;

static TestStateTypeDef test_state = TEST_STATE_IDLE;
static uint8_t test_locker_index = 0;
static uint16_t test_fingerprint_id = 0;
static uint8_t test_confirm_code = 0;
static uint32_t test_timer_start = 0;
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
static void Test_HandleFingerprintEvent(void);
static void Test_PlayMessage(ISD_MESSAGE_t msg);
static void Test_Log(const char *msg);
static void Test_ResetState(void);
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

  /* ---- 1. Initialize Backlight ---- */
  Backlight_Init();
  Backlight_SetBrightness(50);

  /* ---- 2. Initialize RTC ---- */
  RTC_Init();
  RTC_DateTimeTypeDef now;
  RTC_GetDateTime(&now);

  /* ---- 3. Initialize SPI bus & peripherals ---- */
  SPI_Bus_Init();

  /* ---- 4. Initialize Audio (ISD1730) ---- */
  Test_Log("Initializing ISD1730...");
  if (ISD1730_Init() == ISD_OK)
  {
      Test_Log("ISD1730 OK - Playing welcome message...");
      Test_PlayMessage(AUDIO_MSG_WELCOME);
  }
  else
  {
      Test_Log("ISD1730 ERROR!");
  }

  /* ---- 5. Initialize Fingerprint (ZFM-40) ---- */
  Test_Log("Initializing ZFM-40...");
  ZFM40_Init();
  HAL_Delay(500);

  /* Verify password (default is 0x00000000) */
  if (ZFM40_VerifyPassword(0x00000000, &test_confirm_code) == ZFM_OK)
  {
      Test_Log("ZFM-40 OK - Password verified!");
  }
  else
  {
      Test_Log("ZFM-40 ERROR - Password verification failed!");
  }

  /* ---- 6. Initialize Channel Hardware ---- */
  Test_Log("Initializing Channel HW...");
  if (ChannelHW_Init() == SYS_OK)
  {
      Test_Log("Channel HW OK - All locks/LEDs/door sensors ready");
  }
  else
  {
      Test_Log("Channel HW ERROR!");
  }

  /* ---- 7. Initialize DebugLink & UI ---- */
  DebugLink_Init(&huart7);
  DebugLink_RegisterCallback(UI_ProcessIncomingLine);
  UI_Init();

  /* ---- 8. Send initial state to PC ---- */
  {
      char time_str[16];
      char date_str[16];
      snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", now.hour, now.minute, now.second);
      snprintf(date_str, sizeof(date_str), "%02d/%02d/%04d", now.day, now.month, now.year);
      UI_ShowClock(time_str, date_str);
  }

  UI_ShowMessage("System ready for test");
  UI_SetScreenState("TEST_MODE");

  /* Show initial locker states */
  for (uint8_t i = 0; i < LOCKER_COUNT; i++)
  {
      UI_SetLockerState(i, false, false, false);
  }

  Test_Log("=== TEST SYSTEM READY ===");
  Test_Log("Press 'Deposit' button on PC tool to start test");

  /* ---- 9. Start test in IDLE state ---- */
  test_state = TEST_STATE_IDLE;
  test_locker_index = 0;  /* Use locker 1 (index 0) */
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    /* Process UART communication */
    DebugLink_Process();

    /* Process UI events (from UART) */
    UI_Tick();

    UI_EventTypeDef event;
    uint8_t digit;
    if (UI_GetNextEvent(&event, &digit))
    {
        /* Map UI events to test actions */
        switch (event)
        {
            case UI_EVENT_BTN_DEPOSIT:
                Test_Log("Starting deposit flow...");
                /* Start the fingerprint enrollment */
                test_state = TEST_STATE_WAIT_FINGER_1;
                Test_PlayMessage(AUDIO_MSG_PLACE_FINGER);
                UI_ShowMessage("Place your finger on sensor");
                test_timer_start = HAL_GetTick();
                break;

            case UI_EVENT_BTN_RETRIEVE:
                Test_Log("Retrieve button pressed - not implemented in test");
                UI_ShowMessage("Retrieve not supported in test mode");
                break;

            case UI_EVENT_BTN_ADMIN:
                Test_Log("Admin button pressed - not implemented in test");
                UI_ShowMessage("Admin not supported in test mode");
                break;

            case UI_EVENT_PHONE_ANDROID:
                Test_Log("Phone type: Android selected");
                UI_ShowMessage("Phone: Android");
                break;

            case UI_EVENT_PHONE_IPHONE:
                Test_Log("Phone type: iPhone selected");
                UI_ShowMessage("Phone: iPhone");
                break;

            case UI_EVENT_CANCEL:
                Test_Log("Test cancelled by user");
                Test_ResetState();
                UI_ShowMessage("Test cancelled");
                Test_PlayMessage(AUDIO_MSG_CANCELLED);
                /* Close locker if it was open */
                ChannelHW_SetLock(test_locker_index, false);
                ChannelHW_SetLED(test_locker_index, false);
                UI_SetLockerState(test_locker_index, false, false, false);
                break;

            default:
                break;
        }
    }

    /* ---- Handle fingerprint polling (every 100ms) ---- */
    if (zfm_poll_flag)
    {
        zfm_poll_flag = 0;
        Test_HandleFingerprintEvent();
    }

    /* ---- Check door sensor for locker 1 ---- */
    if (test_state == TEST_STATE_DOOR_OPEN)
    {
        if (ChannelHW_IsDoorClosed(test_locker_index))
        {
            Test_Log("Door closed! Test completed successfully!");
            UI_SetLockerState(test_locker_index, false, true, false);
            UI_ShowMessage("Test PASSED!");
            Test_PlayMessage(AUDIO_MSG_SUCCESS);
            test_state = TEST_STATE_COMPLETE;

            /* De-energize lock and turn off LED */
            ChannelHW_SetLock(test_locker_index, false);
            ChannelHW_SetLED(test_locker_index, false);
        }
        else
        {
            /* Check timeout (150 seconds) */
            uint32_t elapsed = (HAL_GetTick() - test_timer_start) / 1000;
            if (elapsed > TIMEOUT_DOOR_CLOSE_AFTER_DEPOSIT_SEC)
            {
                Test_Log("ERROR: Door open timeout!");
                UI_SetLockerState(test_locker_index, false, true, true);  /* LED blinking */
                UI_ShowMessage("ERROR: Door left open!");
                Test_PlayMessage(AUDIO_MSG_DOOR_IS_OPEN);
                Test_ResetState();
            }
        }
    }

    /* ---- Toggle LED on development board (status indicator) ---- */
    /* (Already done in HAL_TIM_PeriodElapsedCallback) */

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
  HAL_GPIO_WritePin(FP_MATCH_LED_GPIO_Port, FP_MATCH_LED_Pin, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin = FP_MATCH_LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(FP_MATCH_LED_GPIO_Port, &GPIO_InitStruct);
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/**
  * @brief  Reset test state to IDLE
  */
static void Test_ResetState(void)
{
    test_state = TEST_STATE_IDLE;
    test_timer_start = 0;
    UI_SetScreenState("TEST_MODE");
}

/**
  * @brief  Play a recorded message on ISD1730
  */
static void Test_PlayMessage(ISD_MESSAGE_t msg)
{
    ISD_StatusTypeDef st = ISD1730_PlayMessage(msg);
    if (st == ISD_OK)
    {
        /* Message is playing */
    }
    else
    {
        Test_Log("Audio play error!");
    }
}

/**
  * @brief  Send log message to PC via UART
  */
static void Test_Log(const char *msg)
{
    if (msg != NULL)
    {
        DebugLink_SendLine(msg);
    }
}

/**
  * @brief  Handle fingerprint events (called every 100ms)
  */
static void Test_HandleFingerprintEvent(void)
{
    uint8_t confirm;
    ZFM_StatusTypeDef st;

    switch (test_state)
    {
        case TEST_STATE_IDLE:
            /* Nothing to do - waiting for user to press "Deposit" */
            break;

        case TEST_STATE_WAIT_FINGER_1:
            /* First fingerprint scan (enrollment) */
            st = ZFM40_PollImage(&confirm);
            if ((st == ZFM_OK) && (confirm == ZFM_CONF_OK))
            {
                Test_Log("Finger detected - capturing image 1...");

                /* Generate character file in buffer 1 */
                if (ZFM40_GenChar(1, &confirm) == ZFM_OK)
                {
                    if (confirm == ZFM_CONF_OK)
                    {
                        Test_Log("Character 1 generated successfully");
                        test_state = TEST_STATE_WAIT_FINGER_2;
                        Test_PlayMessage(AUDIO_MSG_FINGER_AGAIN);
                        UI_ShowMessage("Place finger again");
                    }
                    else
                    {
                        Test_Log("Character generation failed!");
                        Test_ResetState();
                        UI_ShowMessage("Fingerprint error!");
                    }
                }
            }
            else if ((st == ZFM_NACK) && (confirm == ZFM_CONF_NO_FINGER))
            {
                /* No finger yet - keep waiting */
                uint32_t elapsed = (HAL_GetTick() - test_timer_start) / 1000;
                if (elapsed > TIMEOUT_FINGERPRINT_FIRST_SCAN_SEC)
                {
                    Test_Log("ERROR: Fingerprint timeout (first scan)!");
                    Test_ResetState();
                    UI_ShowMessage("Fingerprint timeout!");
                    Test_PlayMessage(AUDIO_MSG_TIMEOUT);
                }
            }
            break;

        case TEST_STATE_WAIT_FINGER_2:
            /* Second fingerprint scan (confirmation) */
            st = ZFM40_PollImage(&confirm);
            if ((st == ZFM_OK) && (confirm == ZFM_CONF_OK))
            {
                Test_Log("Finger detected - capturing image 2...");

                /* Generate character file in buffer 2 */
                if (ZFM40_GenChar(2, &confirm) == ZFM_OK)
                {
                    if (confirm == ZFM_CONF_OK)
                    {
                        Test_Log("Character 2 generated successfully");

                        /* Merge the two fingerprints into a template */
                        if (ZFM40_RegModel(&confirm) == ZFM_OK)
                        {
                            if (confirm == ZFM_CONF_OK)
                            {
                                Test_Log("Template merged successfully");

                                /* Get next available page ID */
                                uint16_t template_count;
                                if (ZFM40_GetTemplateCount(&template_count, &confirm) == ZFM_OK)
                                {
                                    if (confirm == ZFM_CONF_OK)
                                    {
                                        test_fingerprint_id = template_count;

                                        /* Store the template */
                                        if (ZFM40_StoreChar(1, test_fingerprint_id, &confirm) == ZFM_OK)
                                        {
                                            if (confirm == ZFM_CONF_OK)
                                            {
                                                Test_Log("Fingerprint stored at page");
                                                Test_Log("Opening locker...");

                                                /* Open the locker */
                                                ChannelHW_SetLock(test_locker_index, true);
                                                ChannelHW_SetLED(test_locker_index, true);
                                                UI_SetLockerState(test_locker_index, true, true, false);

                                                /* Start door-close timer */
                                                test_state = TEST_STATE_DOOR_OPEN;
                                                test_timer_start = HAL_GetTick();

                                                UI_ShowMessage("Locker opened - place phone and close door");
                                                Test_PlayMessage(AUDIO_MSG_DOOR_OPENED);
                                            }
                                            else
                                            {
                                                Test_Log("Store template failed!");
                                                Test_ResetState();
                                                UI_ShowMessage("Store template failed!");
                                            }
                                        }
                                    }
                                    else
                                    {
                                        Test_Log("GetTemplateCount failed!");
                                        Test_ResetState();
                                        UI_ShowMessage("GetTemplateCount failed!");
                                    }
                                }
                            }
                            else
                            {
                                Test_Log("Template merge failed - fingerprints didn't match!");
                                Test_ResetState();
                                UI_ShowMessage("Fingerprints did not match!");
                                Test_PlayMessage(AUDIO_MSG_FINGER_MISMATCH);
                            }
                        }
                    }
                    else
                    {
                        Test_Log("Character 2 generation failed!");
                        Test_ResetState();
                        UI_ShowMessage("Character 2 generation failed!");
                    }
                }
            }
            else if ((st == ZFM_NACK) && (confirm == ZFM_CONF_NO_FINGER))
            {
                /* No finger yet - keep waiting */
                uint32_t elapsed = (HAL_GetTick() - test_timer_start) / 1000;
                if (elapsed > TIMEOUT_FINGERPRINT_CONFIRM_SCAN_SEC)
                {
                    Test_Log("ERROR: Fingerprint timeout (second scan)!");
                    Test_ResetState();
                    UI_ShowMessage("Fingerprint timeout!");
                    Test_PlayMessage(AUDIO_MSG_TIMEOUT);
                }
            }
            break;

        case TEST_STATE_DOOR_OPEN:
        case TEST_STATE_COMPLETE:
        default:
            /* Nothing to do here */
            break;
    }
}

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
