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
#include "rc522.h"
#include "servo_manager.h"
#include <stdio.h>
#include <string.h>
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
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
static RC522_Handle hrc522;
static ServoManager servo_mgr;

static RC522_Uid last_uid = {.size = 0U};
static uint32_t last_uid_tick = 0U;
static uint32_t scan_led_until = 0U;
static uint32_t last_no_card_log_tick = 0U;

static const RC522_Uid valid_uids[] = {
    {.bytes = {0xDE, 0xAD, 0xBE, 0xEF}, .size = 4},
    {.bytes = {0x12, 0x34, 0x56, 0x78}, .size = 4},
};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */
static void LogUid(const RC522_Uid *uid, bool valid);
static void LogText(const char *msg);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void LogText(const char *msg)
{
  HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)strlen(msg), 100U);
}

static void LogUid(const RC522_Uid *uid, bool valid)
{
  char msg[96];
  int len = snprintf(msg, sizeof(msg), "UID[%lu]:", (unsigned long)uid->size);
  for (uint8_t i = 0; (i < uid->size) && (len > 0) && (len < (int)sizeof(msg) - 5); i++) {
    len += snprintf(&msg[len], sizeof(msg) - (size_t)len, " %02X", uid->bytes[i]);
  }
  if (len > 0 && len < (int)sizeof(msg) - 10) {
    len += snprintf(&msg[len], sizeof(msg) - (size_t)len, " -> %s\r\n", valid ? "VALID" : "INVALID");
  }
  if (len > 0) {
    HAL_UART_Transmit(&huart2, (uint8_t*)msg, (uint16_t)len, 100U);
  }
}
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
  MX_USART2_UART_Init();
    /* USER CODE BEGIN 2 */
  hrc522.sck_port = RC522_SCK_GPIO_Port;
  hrc522.sck_pin = RC522_SCK_Pin;
  hrc522.miso_port = RC522_MISO_GPIO_Port;
  hrc522.miso_pin = RC522_MISO_Pin;
  hrc522.mosi_port = RC522_MOSI_GPIO_Port;
  hrc522.mosi_pin = RC522_MOSI_Pin;
  hrc522.nss_port = RC522_NSS_GPIO_Port;
  hrc522.nss_pin = RC522_NSS_Pin;
  hrc522.rst_port = RC522_RST_GPIO_Port;
  hrc522.rst_pin = RC522_RST_Pin;

  LogText("[BOOT] Starting RC522/UID app...\r\n");
  RC522_Init(&hrc522);
  LogText("[BOOT] RC522 initialized. Waiting for cards...\r\n");

  ServoManager_Init(&servo_mgr, 20000U);
  ServoManager_Add(&servo_mgr, SERVO1_GPIO_Port, SERVO1_Pin, 500U, 2500U);
  ServoManager_Add(&servo_mgr, SERVO2_GPIO_Port, SERVO2_Pin, 500U, 2500U);
  ServoManager_SetAngle(&servo_mgr, 0U, 90U);
  ServoManager_SetAngle(&servo_mgr, 1U, 90U);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    RC522_Uid uid;
    bool valid = false;
    if (HAL_GetTick() < scan_led_until) {
      HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, GPIO_PIN_SET);
    } else {
      HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, GPIO_PIN_RESET);
    }

    if (RC522_ReadUid(&hrc522, &uid)) {
      for (uint32_t i = 0; i < (sizeof(valid_uids) / sizeof(valid_uids[0])); i++) {
        if (uid.size == valid_uids[i].size && memcmp(uid.bytes, valid_uids[i].bytes, uid.size) == 0) {
          valid = true;
          break;
        }
      }

      scan_led_until = HAL_GetTick() + 500U;

      HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, valid ? GPIO_PIN_SET : GPIO_PIN_RESET);
      HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, valid ? GPIO_PIN_RESET : GPIO_PIN_SET);

      if ((uid.size != last_uid.size) || (memcmp(uid.bytes, last_uid.bytes, uid.size) != 0) || ((HAL_GetTick() - last_uid_tick) > 1000U)) {
        LogUid(&uid, valid);
        last_uid = uid;
        last_uid_tick = HAL_GetTick();
      }
    } else {
      HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_RESET);
      HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_RESET);

      if ((HAL_GetTick() - last_no_card_log_tick) > 2000U) {
        LogText("[RFID] No card detected.\r\n");
        last_no_card_log_tick = HAL_GetTick();
      }
    }

    ServoManager_Task(&servo_mgr);
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
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure LSE Drive Capability
  */
  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSE|RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 16;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Enable MSI Auto calibration
  */
  HAL_RCCEx_EnableMSIPLLMode();
}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

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
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, RC522_RST_Pin|LED_GREEN_Pin|LED_RED_Pin|SERVO1_Pin|SERVO2_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(RC522_NSS_GPIO_Port, RC522_NSS_Pin, GPIO_PIN_SET);

  /*Configure GPIO pins : RC522_SCK_Pin RC522_MOSI_Pin RC522_NSS_Pin */
  GPIO_InitStruct.Pin = RC522_SCK_Pin|RC522_MOSI_Pin|RC522_NSS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : RC522_MISO_Pin */
  GPIO_InitStruct.Pin = RC522_MISO_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : RC522_RST_Pin LED_GREEN_Pin LED_RED_Pin SERVO1_Pin SERVO2_Pin */
  GPIO_InitStruct.Pin = RC522_RST_Pin|LED_GREEN_Pin|LED_RED_Pin|SERVO1_Pin|SERVO2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : LD3_Pin */
  GPIO_InitStruct.Pin = LD3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD3_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

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
#ifdef USE_FULL_ASSERT
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
