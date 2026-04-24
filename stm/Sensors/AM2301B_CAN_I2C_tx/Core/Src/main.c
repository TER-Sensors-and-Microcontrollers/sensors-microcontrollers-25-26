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
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "usbd_cdc_if.h"
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define AM2301B_ADDR_7BIT  0x38
#define AM2301B_ADDR_8BIT  (AM2301B_ADDR_7BIT << 1)
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
FDCAN_HandleTypeDef hfdcan2;

I2C_HandleTypeDef hi2c2;

/* USER CODE BEGIN PV */
int counter = 0;
FDCAN_TxHeaderTypeDef TxHeader;
uint8_t TxData[8] = {0};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_FDCAN2_Init(void);
static void MX_I2C2_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// Polynomial 1+X4+X5+X8 corresponds to 0x31
static uint8_t am2301b_crc8(const uint8_t *data, uint8_t len)
{
  uint8_t crc = 0xFF;
  for (uint8_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t b = 0; b < 8; b++) {
      if (crc & 0x80) crc = (uint8_t)((crc << 1) ^ 0x31);
      else           crc = (uint8_t)(crc << 1);
    }
  }
  return crc;
}

// Read one status byte (command 0x71)
static HAL_StatusTypeDef am2301b_read_status(I2C_HandleTypeDef *hi2c, uint8_t *status)
{
  uint8_t cmd = 0x71;
  HAL_StatusTypeDef ok;

  ok = HAL_I2C_Master_Transmit(hi2c, AM2301B_ADDR_8BIT, &cmd, 1, 100);
  if (ok != HAL_OK) return ok;

  return HAL_I2C_Master_Receive(hi2c, AM2301B_ADDR_8BIT, status, 1, 100);
}

// Trigger measurement (0xAC 0x33 0x00)
static HAL_StatusTypeDef am2301b_trigger_measurement(I2C_HandleTypeDef *hi2c)
{
  uint8_t cmd[3] = {0xAC, 0x33, 0x00};
  return HAL_I2C_Master_Transmit(hi2c, AM2301B_ADDR_8BIT, cmd, sizeof(cmd), 100);
}

// Read 6 data bytes
static HAL_StatusTypeDef am2301b_read_data6(I2C_HandleTypeDef *hi2c, uint8_t data6[6])
{
  return HAL_I2C_Master_Receive(hi2c, AM2301B_ADDR_8BIT, data6, 6, 200);
}

// Convert raw bytes to humidity (%) and temperature (°C)
static void am2301b_convert(const uint8_t d[6], float *rh_percent, float *temp_c)
{

  // d0 = status, then 20-bit humidity, then 20-bit temperature in remaining bits.
  // humidity_raw = d1<<12 | d2<<4 | (d3>>4)
  // temp_raw     = (d3&0x0F)<<16 | d4<<8 | d5

  uint32_t humidity_raw = ((uint32_t)d[1] << 12) | ((uint32_t)d[2] << 4) | ((uint32_t)(d[3] >> 4));
  uint32_t temp_raw     = ((uint32_t)(d[3] & 0x0F) << 16) | ((uint32_t)d[4] << 8) | (uint32_t)d[5];

  // RH[%] = (SRH / 2^20) * 100
  // T[°C] = (ST / 2^20) * 200 - 50
  *rh_percent = ((float)humidity_raw * 100.0f) / 1048576.0f; // 2^20
  *temp_c     = ((float)temp_raw     * 200.0f) / 1048576.0f - 50.0f;
}


static HAL_StatusTypeDef am2301b_read(float *rh_percent, float *temp_c)
{
  HAL_StatusTypeDef ok;
  uint8_t status = 0;

  // Read status (0x71).
  ok = am2301b_read_status(&hi2c2, &status);
  if (ok != HAL_OK) return ok;

  // Trigger measurement
  ok = am2301b_trigger_measurement(&hi2c2);
  if (ok != HAL_OK) return ok;

  // Wait >=80ms, then poll busy bit (Bit7) until 0
  HAL_Delay(80);

  for (int tries = 0; tries < 10; tries++) {
    ok = am2301b_read_status(&hi2c2, &status);
    if (ok != HAL_OK) return ok;

    if ((status & 0x80) == 0) break; // Bit7 == 0 => ready
    HAL_Delay(10);
  }

  if (status & 0x80) {
    // still busy after polling
    return HAL_TIMEOUT;
  }

  // Read 6 bytes
  uint8_t d[6] = {0};
  ok = am2301b_read_data6(&hi2c2, d);
  if (ok != HAL_OK) return ok;

  // Convert
  am2301b_convert(d, rh_percent, temp_c);

  return HAL_OK;
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
  MX_FDCAN2_Init();
  MX_I2C2_Init();
  MX_USB_Device_Init();
  /* USER CODE BEGIN 2 */
  HAL_Delay(3000);  /* let USB CDC enumerate */

   char initmsg[80];
   int initlen;

   TxHeader.Identifier          = 0x100;
   TxHeader.IdType              = FDCAN_STANDARD_ID;
   TxHeader.TxFrameType         = FDCAN_DATA_FRAME;
   TxHeader.DataLength          = FDCAN_DLC_BYTES_8;
   TxHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
   TxHeader.BitRateSwitch       = FDCAN_BRS_OFF;
   TxHeader.FDFormat            = FDCAN_CLASSIC_CAN;
   TxHeader.TxEventFifoControl  = FDCAN_NO_TX_EVENTS;
   TxHeader.MessageMarker       = 0;

   if (HAL_FDCAN_Start(&hfdcan2) != HAL_OK) {
     initlen = snprintf(initmsg, sizeof(initmsg), "FAIL: FDCAN Start\r\n");
     CDC_Transmit_FS((uint8_t *)initmsg, initlen);
     HAL_Delay(100);
   } else {
     initlen = snprintf(initmsg, sizeof(initmsg), "FDCAN2 started — transmitting...\r\n");
     CDC_Transmit_FS((uint8_t *)initmsg, initlen);
     HAL_Delay(100);
   }
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

		/* USER CODE BEGIN 3 */
	  float rh = 0.0f, tc = 0.0f;
	      char buf[96];

	      if (am2301b_read(&rh, &tc) == HAL_OK)
	      {
	          int rh_x10 = (int)(rh * 10.0f);
	          int tc_x10 = (int)(tc * 10.0f);
	          int tf_x10 = (tc_x10 * 9) / 5 + 320;

	          snprintf(buf, sizeof(buf),
	                   "Humidity: %d.%d %%RH, Temp: %d.%d C / %d.%d F\r\n",
	                   rh_x10 / 10, abs(rh_x10 % 10),
	                   tc_x10 / 10, abs(tc_x10 % 10),
	                   tf_x10 / 10, abs(tf_x10 % 10));
	          CDC_Transmit_FS((uint8_t *)buf, strlen(buf));

	          TxData[0] = (int8_t) (rh_x10/10);
	          TxData[1] = 0;
	          TxData[2] = 0;
	          TxData[3] = (int8_t) (tc_x10/10);
	          TxData[4] = 0;
	          TxData[5] = 0;
	          TxData[6] = 0;
	          TxData[7] = (int8_t) (tf_x10/10);

	          char msg[80];
	          int len;
	          HAL_StatusTypeDef whatsTheError =
	              HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan2, &TxHeader, TxData);

	          if (whatsTheError == HAL_OK)
	              len = snprintf(msg, sizeof(msg), "TX OK  | counter=%d\r\n", counter);
	          else
	              len = snprintf(msg, sizeof(msg), "TX FAIL| counter=%d\r\n", counter);

	          CDC_Transmit_FS((uint8_t *)msg, len);
	      }
	      else
	      {
	          snprintf(buf, sizeof(buf), "AM2301B read error\r\n");
	          CDC_Transmit_FS((uint8_t *)buf, strlen(buf));
	      }

	      counter++;
	      HAL_Delay(2000);
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
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV1;
  RCC_OscInitStruct.PLL.PLLN = 12;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV4;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief FDCAN2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_FDCAN2_Init(void)
{

  /* USER CODE BEGIN FDCAN2_Init 0 */

  /* USER CODE END FDCAN2_Init 0 */

  /* USER CODE BEGIN FDCAN2_Init 1 */

  /* USER CODE END FDCAN2_Init 1 */
  hfdcan2.Instance = FDCAN2;
  hfdcan2.Init.ClockDivider = FDCAN_CLOCK_DIV1;
  hfdcan2.Init.FrameFormat = FDCAN_FRAME_CLASSIC;
  hfdcan2.Init.Mode = FDCAN_MODE_NORMAL;
  hfdcan2.Init.AutoRetransmission = DISABLE;
  hfdcan2.Init.TransmitPause = DISABLE;
  hfdcan2.Init.ProtocolException = DISABLE;
  hfdcan2.Init.NominalPrescaler = 2;
  hfdcan2.Init.NominalSyncJumpWidth = 1;
  hfdcan2.Init.NominalTimeSeg1 = 13;
  hfdcan2.Init.NominalTimeSeg2 = 2;
  hfdcan2.Init.DataPrescaler = 1;
  hfdcan2.Init.DataSyncJumpWidth = 1;
  hfdcan2.Init.DataTimeSeg1 = 1;
  hfdcan2.Init.DataTimeSeg2 = 1;
  hfdcan2.Init.StdFiltersNbr = 0;
  hfdcan2.Init.ExtFiltersNbr = 0;
  hfdcan2.Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;
  if (HAL_FDCAN_Init(&hfdcan2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN FDCAN2_Init 2 */

  /* USER CODE END FDCAN2_Init 2 */

}

/**
  * @brief I2C2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C2_Init(void)
{

  /* USER CODE BEGIN I2C2_Init 0 */

  /* USER CODE END I2C2_Init 0 */

  /* USER CODE BEGIN I2C2_Init 1 */

  /* USER CODE END I2C2_Init 1 */
  hi2c2.Instance = I2C2;
  hi2c2.Init.Timing = 0x00503D58;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c2, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c2, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C2_Init 2 */

  /* USER CODE END I2C2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

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
