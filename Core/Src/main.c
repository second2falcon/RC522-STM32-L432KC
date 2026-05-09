/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  */
/* USER CODE END Header */
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "rc522.h"
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
static RC522_Handle hrc522;
static RC522_Uid last_uid = {.size = 0U};
static uint32_t last_uid_tick = 0U;
/* USER CODE END PV */

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);

/* USER CODE BEGIN PFP */
static void LogText(const char *msg);
static void LogHex(const char *label, uint8_t v);
static void LogUid(const RC522_Uid *uid);
/* USER CODE END PFP */

/* USER CODE BEGIN 0 */
static void LogText(const char *msg)
{
  HAL_UART_Transmit(&huart2, (uint8_t *)msg, (uint16_t)strlen(msg), 100U);
}

static void LogHex(const char *label, uint8_t v)
{
  char buf[64];
  int n = snprintf(buf, sizeof(buf), "%s0x%02X\r\n", label, v);
  if (n > 0) {
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, (uint16_t)n, 100U);
  }
}

static void LogUid(const RC522_Uid *uid)
{
  char msg[96];
  int len = snprintf(msg, sizeof(msg), "[RFID] UID[%lu]:", (unsigned long)uid->size);
  for (uint8_t i = 0; (i < uid->size) && (len > 0) && (len < (int)sizeof(msg) - 5); i++) {
    len += snprintf(&msg[len], sizeof(msg) - (size_t)len, " %02X", uid->bytes[i]);
  }
  if (len > 0 && len < (int)sizeof(msg) - 3) {
    len += snprintf(&msg[len], sizeof(msg) - (size_t)len, "\r\n");
  }
  if (len > 0) {
    HAL_UART_Transmit(&huart2, (uint8_t*)msg, (uint16_t)len, 100U);
  }
}
/* USER CODE END 0 */

int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_USART2_UART_Init();

  /* USER CODE BEGIN 2 */
  LogText("\r\n[BOOT] RC522 UID debug firmware\r\n");
  LogText("[BOOT] If this text is corrupted, check UART terminal 115200 8N1 and board reset.\r\n");

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

  RC522_Init(&hrc522);

  uint8_t version = 0U;
  bool comm_ok = RC522_CheckComm(&hrc522, &version);
  LogHex("[BOOT] RC522 VersionReg = ", version);
  LogText(comm_ok ? "[BOOT] RC522 comm OK\r\n" : "[BOOT] RC522 comm FAILED\r\n");
  /* USER CODE END 2 */

  while (1)
  {
    RC522_Uid uid;
    if (RC522_ReadUid(&hrc522, &uid)) {
      if ((uid.size != last_uid.size) || (memcmp(uid.bytes, last_uid.bytes, uid.size) != 0) || ((HAL_GetTick() - last_uid_tick) > 1000U)) {
        LogUid(&uid);
        last_uid = uid;
        last_uid_tick = HAL_GetTick();
      }
    } else {
      static uint32_t last_diag_tick = 0U;
      if ((HAL_GetTick() - last_diag_tick) > 2000U) {
        uint8_t v = 0U;
        bool ok = RC522_CheckComm(&hrc522, &v);
        LogHex("[DBG] VersionReg = ", v);
        LogText(ok ? "[DBG] RC522 link alive, waiting for card\r\n" : "[DBG] RC522 link broken\r\n");
        last_diag_tick = HAL_GetTick();
      }
    }
  }
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

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

  HAL_RCCEx_EnableMSIPLLMode();
}

static void MX_USART2_UART_Init(void)
{
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
}

static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  HAL_GPIO_WritePin(GPIOB, RC522_RST_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(RC522_NSS_GPIO_Port, RC522_NSS_Pin, GPIO_PIN_SET);

  GPIO_InitStruct.Pin = RC522_SCK_Pin|RC522_MOSI_Pin|RC522_NSS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = RC522_MISO_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = RC522_RST_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}
