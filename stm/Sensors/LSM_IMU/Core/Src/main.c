/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  *                   Reads LSM6DSV32X IMU data over I2C2 and outputs via USB CDC
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

/* ── LSM6DSV32X I2C address ──────────────────────────────────────────────────
 * SDO/SA0 pin to GND → 0x6A (7-bit) → 0xD4 (8-bit for HAL)
 * SDO/SA0 pin to VDD → 0x6B (7-bit) → 0xD6 (8-bit for HAL)
 *
 * Check your breakout board: if SDO is tied to GND (or left floating with
 * internal pull-down), use 0xD4. If tied to VDD, use 0xD6.
 * ──────────────────────────────────────────────────────────────────────────── */
#define LSM6DSV32X_ADDR       0xD4  /* 0x6A << 1 — change to 0xD6 if SDO=VDD */

/* ── Register addresses ──────────────────────────────────────────────────── */
#define LSM6DSV32X_WHO_AM_I   0x0F
#define LSM6DSV32X_CTRL1      0x10  /* Accel ODR + OP_MODE  */
#define LSM6DSV32X_CTRL2      0x11  /* Gyro  ODR + OP_MODE  */
#define LSM6DSV32X_CTRL3      0x12  /* BDU, IF_INC, SW_RESET */
#define LSM6DSV32X_CTRL6      0x15  /* Gyro  full-scale      */
#define LSM6DSV32X_CTRL8      0x17  /* Accel full-scale      */
#define LSM6DSV32X_STATUS_REG 0x1E
#define LSM6DSV32X_OUT_TEMP_L 0x20
#define LSM6DSV32X_OUTX_L_G   0x22  /* Gyro  data start (6 bytes) */
#define LSM6DSV32X_OUTX_L_A   0x28  /* Accel data start (6 bytes) */

/* ── Expected WHO_AM_I value ─────────────────────────────────────────────── */
#define LSM6DSV32X_WHO_AM_I_VALUE  0x70

/* ── CTRL1 — Accel ODR (bits [7:4]), OP_MODE_XL (bits [2:0]) ────────────── */
#define LSM6DSV32X_ODR_XL_OFF      0x00
#define LSM6DSV32X_ODR_XL_1_875HZ  0x10
#define LSM6DSV32X_ODR_XL_7_5HZ   0x20
#define LSM6DSV32X_ODR_XL_15HZ    0x30
#define LSM6DSV32X_ODR_XL_30HZ    0x40
#define LSM6DSV32X_ODR_XL_60HZ    0x50
#define LSM6DSV32X_ODR_XL_120HZ   0x60
#define LSM6DSV32X_ODR_XL_240HZ   0x70
#define LSM6DSV32X_ODR_XL_480HZ   0x80
#define LSM6DSV32X_ODR_XL_960HZ   0x90

/* OP_MODE_XL = 000 → high-performance (default, no extra bits needed) */

/* ── CTRL2 — Gyro ODR (bits [7:4]), OP_MODE_G (bits [2:0]) ──────────────── */
#define LSM6DSV32X_ODR_G_OFF       0x00
#define LSM6DSV32X_ODR_G_7_5HZ    0x20
#define LSM6DSV32X_ODR_G_15HZ     0x30
#define LSM6DSV32X_ODR_G_30HZ     0x40
#define LSM6DSV32X_ODR_G_60HZ     0x50
#define LSM6DSV32X_ODR_G_120HZ    0x60
#define LSM6DSV32X_ODR_G_240HZ    0x70
#define LSM6DSV32X_ODR_G_480HZ    0x80
#define LSM6DSV32X_ODR_G_960HZ    0x90

/* ── CTRL3 bits ──────────────────────────────────────────────────────────── */
#define LSM6DSV32X_BDU             0x40  /* Block Data Update    */
#define LSM6DSV32X_IF_INC          0x04  /* Auto-increment addr  */
#define LSM6DSV32X_SW_RESET        0x01  /* Software reset       */

/* ── CTRL6 — Gyro FS (bits [3:0]) ────────────────────────────────────────── */
#define LSM6DSV32X_FS_G_125DPS     0x00
#define LSM6DSV32X_FS_G_250DPS    0x01
#define LSM6DSV32X_FS_G_500DPS    0x02
#define LSM6DSV32X_FS_G_1000DPS   0x03
#define LSM6DSV32X_FS_G_2000DPS   0x04
#define LSM6DSV32X_FS_G_4000DPS   0x0C

/* ── CTRL8 — Accel FS (bits [1:0]) ───────────────────────────────────────── */
#define LSM6DSV32X_FS_XL_4G       0x00
#define LSM6DSV32X_FS_XL_8G       0x01
#define LSM6DSV32X_FS_XL_16G      0x02
#define LSM6DSV32X_FS_XL_32G      0x03

/* ── STATUS_REG bits ─────────────────────────────────────────────────────── */
#define LSM6DSV32X_XLDA            0x01  /* Accel data available */
#define LSM6DSV32X_GDA             0x02  /* Gyro  data available */
#define LSM6DSV32X_TDA             0x04  /* Temp  data available */

/* ── Sensitivity constants ───────────────────────────────────────────────── */
/* Accel: mg per LSB */
#define LSM6DSV32X_SENS_XL_4G     0.122f
#define LSM6DSV32X_SENS_XL_8G     0.244f
#define LSM6DSV32X_SENS_XL_16G    0.488f
#define LSM6DSV32X_SENS_XL_32G    0.976f

/* Gyro: mdps per LSB */
#define LSM6DSV32X_SENS_G_125DPS   4.375f
#define LSM6DSV32X_SENS_G_250DPS   8.750f
#define LSM6DSV32X_SENS_G_500DPS  17.500f
#define LSM6DSV32X_SENS_G_1000DPS 35.000f
#define LSM6DSV32X_SENS_G_2000DPS 70.000f
#define LSM6DSV32X_SENS_G_4000DPS 140.000f

/* ── Chosen configuration (change these to taste) ────────────────────────── */
#define IMU_ACCEL_ODR    LSM6DSV32X_ODR_XL_60HZ
#define IMU_GYRO_ODR     LSM6DSV32X_ODR_G_60HZ
#define IMU_ACCEL_FS     LSM6DSV32X_FS_XL_4G
#define IMU_GYRO_FS      LSM6DSV32X_FS_G_500DPS
#define IMU_ACCEL_SENS   LSM6DSV32X_SENS_XL_4G    /* must match FS above */
#define IMU_GYRO_SENS    LSM6DSV32X_SENS_G_500DPS  /* must match FS above */

#define HAL_TIMEOUT      100
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c2;

/* USER CODE BEGIN PV */
static uint8_t tx_buf[256];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C2_Init(void);
/* USER CODE BEGIN PFP */
static HAL_StatusTypeDef IMU_WriteReg(uint8_t reg, uint8_t val);
static HAL_StatusTypeDef IMU_ReadReg(uint8_t reg, uint8_t *val, uint16_t len);
static void I2C_Scan(void);
static int  IMU_Init(void);
static void IMU_ReadAll(void);
static void USB_Print(const char *msg);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* ──────────────────────────────────────────────────────────────────────────
 * Low-level I2C helpers
 * ────────────────────────────────────────────────────────────────────────── */
static HAL_StatusTypeDef IMU_WriteReg(uint8_t reg, uint8_t val)
{
  return HAL_I2C_Mem_Write(&hi2c2, LSM6DSV32X_ADDR, reg,
                           I2C_MEMADD_SIZE_8BIT, &val, 1, HAL_TIMEOUT);
}

static HAL_StatusTypeDef IMU_ReadReg(uint8_t reg, uint8_t *val, uint16_t len)
{
  return HAL_I2C_Mem_Read(&hi2c2, LSM6DSV32X_ADDR, reg,
                          I2C_MEMADD_SIZE_8BIT, val, len, HAL_TIMEOUT);
}

/* ──────────────────────────────────────────────────────────────────────────
 * USB CDC print helper — retries if the port is not ready yet
 * ────────────────────────────────────────────────────────────────────────── */
static void USB_Print(const char *msg)
{
  uint8_t retries = 5;
  while (retries--)
  {
    if (CDC_Transmit_FS((uint8_t *)msg, (uint16_t)strlen(msg)) == USBD_OK)
      break;
    HAL_Delay(1);
  }
}

/* ──────────────────────────────────────────────────────────────────────────
 * I2C bus scanner — probes every 7-bit address and prints what responds.
 * This tells us exactly what address the IMU is on (or if it's visible
 * on the bus at all).
 * ────────────────────────────────────────────────────────────────────────── */
static void I2C_Scan(void)
{
  /* ── Print I2C peripheral and bus state for diagnostics ──────────────── */
  {
    uint32_t isr = hi2c2.Instance->ISR;
    int len = snprintf((char *)tx_buf, sizeof(tx_buf),
        "I2C2 ISR: 0x%08lX  (BUSY=%lu)\r\n",
        isr, (isr >> 15) & 1);
    USB_Print((char *)tx_buf);
    (void)len;

    /* Read the raw pin levels — on a healthy bus both should be HIGH (1)
     * because the external pull-ups hold them up when idle. */
    uint8_t sda_level = (GPIOA->IDR >> 8) & 1;  /* PA8 */
    uint8_t scl_level = (GPIOA->IDR >> 9) & 1;  /* PA9 */
    len = snprintf((char *)tx_buf, sizeof(tx_buf),
        "Pin levels — SDA(PA8): %d  SCL(PA9): %d  (both should be 1)\r\n",
        sda_level, scl_level);
    USB_Print((char *)tx_buf);
    (void)len;

    if (!sda_level || !scl_level)
    {
      USB_Print("  WARNING: Line held LOW — check wiring, pull-ups, power.\r\n");

      if (!sda_level && !scl_level)
        USB_Print("  LIKELY: IMU not powered, or SDA/SCL not connected.\r\n");
      else if (!sda_level)
        USB_Print("  LIKELY: SDA stuck low — bus may be hung or SDA miswired.\r\n");
      else
        USB_Print("  LIKELY: SCL stuck low — SCL miswired or shorted.\r\n");
    }
  }

  USB_Print("Scanning I2C2 bus...\r\n");
  uint8_t found = 0;

  for (uint16_t addr = 0x03; addr <= 0x77; addr++)
  {
    /* HAL_I2C_IsDeviceReady sends address + checks for ACK.
     * The address must be left-shifted by 1 for HAL. */
    if (HAL_I2C_IsDeviceReady(&hi2c2, (uint16_t)(addr << 1), 2, 10) == HAL_OK)
    {
      int len = snprintf((char *)tx_buf, sizeof(tx_buf),
          "  Found device at 0x%02X (8-bit HAL addr: 0x%02X)\r\n",
          addr, addr << 1);
      USB_Print((char *)tx_buf);
      (void)len;
      found++;
    }
  }

  if (found == 0)
    USB_Print("  No I2C devices found! Check wiring, pull-ups, and power.\r\n");
  else
  {
    int len = snprintf((char *)tx_buf, sizeof(tx_buf),
        "  Scan complete: %d device(s) found.\r\n", found);
    USB_Print((char *)tx_buf);
    (void)len;
  }
}

/* ──────────────────────────────────────────────────────────────────────────
 * IMU initialisation
 *  1. Software-reset
 *  2. Check WHO_AM_I
 *  3. Enable BDU + address auto-increment
 *  4. Set accel / gyro ODR, FS, and high-performance mode
 *
 *  Returns 0 on success, -1 on failure.
 * ────────────────────────────────────────────────────────────────────────── */
static int IMU_Init(void)
{
  uint8_t who = 0;
  HAL_StatusTypeDef status;

  /* 1. Software reset ────────────────────────────────────────────────────── */
  IMU_WriteReg(LSM6DSV32X_CTRL3, LSM6DSV32X_SW_RESET);
  HAL_Delay(50);  /* wait for reset to complete */

  /* 2. WHO_AM_I check ────────────────────────────────────────────────────── */
  status = IMU_ReadReg(LSM6DSV32X_WHO_AM_I, &who, 1);
  if (status != HAL_OK || who != LSM6DSV32X_WHO_AM_I_VALUE)
  {
    uint16_t len = snprintf((char *)tx_buf, sizeof(tx_buf),
        "IMU INIT FAIL — HAL status: %d, WHO_AM_I: 0x%02X (expected 0x%02X)\r\n",
        (int)status, who, LSM6DSV32X_WHO_AM_I_VALUE);
    USB_Print((char *)tx_buf);
    (void)len;
    return -1;
  }

  /* 3. Enable BDU + auto-increment ──────────────────────────────────────── */
  IMU_WriteReg(LSM6DSV32X_CTRL3, LSM6DSV32X_BDU | LSM6DSV32X_IF_INC);

  /* 4a. Set accel full-scale in CTRL8 (read-modify-write) ────────────── */
  {
    uint8_t ctrl8 = 0;
    IMU_ReadReg(LSM6DSV32X_CTRL8, &ctrl8, 1);
    ctrl8 = (ctrl8 & 0xFC) | IMU_ACCEL_FS;   /* bits [1:0] = FS_XL */
    IMU_WriteReg(LSM6DSV32X_CTRL8, ctrl8);
  }

  /* 4b. Set gyro full-scale in CTRL6 (read-modify-write) ─────────────── */
  {
    uint8_t ctrl6 = 0;
    IMU_ReadReg(LSM6DSV32X_CTRL6, &ctrl6, 1);
    ctrl6 = (ctrl6 & 0xF0) | IMU_GYRO_FS;    /* bits [3:0] = FS_G */
    IMU_WriteReg(LSM6DSV32X_CTRL6, ctrl6);
  }

  /* 4c. Turn on accel: ODR + high-performance mode (OP_MODE = 000) ─────  */
  IMU_WriteReg(LSM6DSV32X_CTRL1, IMU_ACCEL_ODR);  /* OP_MODE_XL = 000 */

  /* 4d. Turn on gyro:  ODR + high-performance mode (OP_MODE = 000) ─────  */
  IMU_WriteReg(LSM6DSV32X_CTRL2, IMU_GYRO_ODR);   /* OP_MODE_G  = 000 */

  HAL_Delay(100);  /* let sensors stabilise */

  USB_Print("LSM6DSV32X initialised OK\r\n");
  return 0;
}

/* ──────────────────────────────────────────────────────────────────────────
 * Read accel + gyro + temp, convert, and print over USB CDC
 * ────────────────────────────────────────────────────────────────────────── */
static void IMU_ReadAll(void)
{
  uint8_t status_reg = 0;
  uint8_t raw[12];                      /* 6 bytes gyro + 6 bytes accel */
  int16_t gx_raw, gy_raw, gz_raw;
  int16_t ax_raw, ay_raw, az_raw;
  float   gx, gy, gz;                  /* degrees per second    */
  float   ax, ay, az;                  /* milli-g               */

  /* Poll STATUS_REG until both accel and gyro data are ready */
  IMU_ReadReg(LSM6DSV32X_STATUS_REG, &status_reg, 1);
  if (!(status_reg & (LSM6DSV32X_XLDA | LSM6DSV32X_GDA)))
    return;  /* no new data yet */

  /* Burst-read gyro (0x22-0x27) + accel (0x28-0x2D) = 12 bytes
   * Thanks to IF_INC, a single read from 0x22 gets all 12 bytes. */
  if (IMU_ReadReg(LSM6DSV32X_OUTX_L_G, raw, 12) != HAL_OK)
    return;

  /* Reconstruct signed 16-bit values (little-endian on wire) */
  gx_raw = (int16_t)(raw[1]  << 8 | raw[0]);
  gy_raw = (int16_t)(raw[3]  << 8 | raw[2]);
  gz_raw = (int16_t)(raw[5]  << 8 | raw[4]);

  ax_raw = (int16_t)(raw[7]  << 8 | raw[6]);
  ay_raw = (int16_t)(raw[9]  << 8 | raw[8]);
  az_raw = (int16_t)(raw[11] << 8 | raw[10]);

  /* Convert to physical units */
  gx = (float)gx_raw * IMU_GYRO_SENS  / 1000.0f;  /* mdps → dps  */
  gy = (float)gy_raw * IMU_GYRO_SENS  / 1000.0f;
  gz = (float)gz_raw * IMU_GYRO_SENS  / 1000.0f;

  ax = (float)ax_raw * IMU_ACCEL_SENS / 1000.0f;   /* mg   → g    */
  ay = (float)ay_raw * IMU_ACCEL_SENS / 1000.0f;
  az = (float)az_raw * IMU_ACCEL_SENS / 1000.0f;

  /* Format and send over USB CDC */
  int len = snprintf((char *)tx_buf, sizeof(tx_buf),
      "A: %+8.3f %+8.3f %+8.3f g  |  G: %+8.2f %+8.2f %+8.2f dps\r\n",
      ax, ay, az, gx, gy, gz);

  if (len > 0)
    USB_Print((char *)tx_buf);
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
  MX_I2C2_Init();
  MX_USB_Device_Init();
  /* USER CODE BEGIN 2 */

  /* Give the USB host a moment to enumerate the CDC device */
  HAL_Delay(1500);

  /* Scan the I2C bus first to see what's connected */
  I2C_Scan();

  /* Initialise the IMU — retries until it succeeds */
  while (IMU_Init() != 0)
  {
    USB_Print("Retrying IMU init in 1 s...\r\n");
    HAL_Delay(1000);
  }

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    IMU_ReadAll();
    HAL_Delay(20);  /* ~50 Hz print rate (adjust to taste) */
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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
  * @brief I2C2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C2_Init(void)
{

  /* USER CODE BEGIN I2C2_Init 0 */

  /* ── Manual GPIO + clock setup for I2C2 ────────────────────────────────
   * This ensures the pins are properly configured even if the
   * HAL_I2C_MspInit() in stm32g4xx_hal_msp.c is missing or incomplete.
   *
   *   PA8  →  I2C2_SDA  (AF4, open-drain)
   *   PA9  →  I2C2_SCL  (AF4, open-drain)
   * ──────────────────────────────────────────────────────────────────────── */
  {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_I2C2_CLK_ENABLE();

    GPIO_InitStruct.Pin       = GPIO_PIN_8 | GPIO_PIN_9;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;   /* external pull-ups on breakout */
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF4_I2C2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  }

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
