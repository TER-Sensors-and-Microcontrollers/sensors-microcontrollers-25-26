/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : BNO085 Gyro X/Y/Z serial print
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "usbd_cdc_if.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define BNO085_ADDR            (0x4A << 1)
#define SHTP_MAX               300
#define SHTP_HDR               4
#define I2C_TOUT               150

#define CH_EXEC   1
#define CH_CTRL   2
#define CH_INPUT  3

#define RPT_GYRO               0x02
#define RPT_SET_FEATURE        0xFD
#define RPT_BASE_TS            0xFB
#define RPT_TS_REBASE          0xFA

#define PRINT_BUF              256

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c2;

/* USER CODE BEGIN PV */
static uint8_t rxBuf[SHTP_MAX];
static uint8_t txBuf[SHTP_MAX];
static uint8_t seqOut[6] = {0};
static char    pBuf[PRINT_BUF];

static float lastX = 0, lastY = 0, lastZ = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C2_Init(void);
/* USER CODE BEGIN PFP */
static void CDC_Print(const char *fmt, ...);
static HAL_StatusTypeDef SHTP_Read(uint16_t *len, uint8_t *ch);
static HAL_StatusTypeDef SHTP_Write(uint8_t ch, const uint8_t *d, uint16_t n);
static void BNO_Boot(void);
static void BNO_EnableGyro(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

static void CDC_Print(const char *fmt, ...)
{
    va_list a;
    va_start(a, fmt);
    int n = vsnprintf(pBuf, PRINT_BUF, fmt, a);
    va_end(a);
    if (n <= 0) return;
    uint32_t t0 = HAL_GetTick();
    while (CDC_Transmit_FS((uint8_t *)pBuf, (uint16_t)n) == USBD_BUSY)
        if (HAL_GetTick() - t0 > 50) break;
}

static HAL_StatusTypeDef SHTP_Read(uint16_t *len, uint8_t *ch)
{
    *len = 0; *ch = 0;
    HAL_StatusTypeDef st;
    st = HAL_I2C_Master_Receive(&hi2c2, BNO085_ADDR, rxBuf, 4, I2C_TOUT);
    if (st != HAL_OK) return st;

    uint16_t pktLen = (uint16_t)rxBuf[0] | ((uint16_t)(rxBuf[1] & 0x7F) << 8);
    if (pktLen == 0 || pktLen == 0x7FFF) return HAL_TIMEOUT;
    if (pktLen > SHTP_MAX) pktLen = SHTP_MAX;

    st = HAL_I2C_Master_Receive(&hi2c2, BNO085_ADDR, rxBuf, pktLen, I2C_TOUT);
    if (st != HAL_OK) return st;

    *len = (uint16_t)rxBuf[0] | ((uint16_t)(rxBuf[1] & 0x7F) << 8);
    *ch  = rxBuf[2];
    return HAL_OK;
}

static HAL_StatusTypeDef SHTP_Write(uint8_t ch, const uint8_t *d, uint16_t n)
{
    uint16_t total = SHTP_HDR + n;
    txBuf[0] = (uint8_t)(total & 0xFF);
    txBuf[1] = (uint8_t)((total >> 8) & 0x7F);
    txBuf[2] = ch;
    txBuf[3] = seqOut[ch]++;
    if (n > 0) memcpy(&txBuf[4], d, n);
    return HAL_I2C_Master_Transmit(&hi2c2, BNO085_ADDR, txBuf, total, I2C_TOUT);
}

static void BNO_Boot(void)
{
    HAL_Delay(300);
    for (int i = 0; i < 15; i++) {
        uint16_t len; uint8_t ch;
        if (SHTP_Read(&len, &ch) != HAL_OK) HAL_Delay(50);
        else HAL_Delay(20);
    }
}

static void BNO_EnableGyro(void)
{
    uint8_t cmd[17] = {0};
    cmd[0] = RPT_SET_FEATURE;
    cmd[1] = RPT_GYRO;
    cmd[5] = 0x20; cmd[6] = 0x4E;  /* 20000 us = 50 Hz */
    SHTP_Write(CH_CTRL, cmd, 17);
    HAL_Delay(50);
    for (int i = 0; i < 5; i++) {
        uint16_t len; uint8_t ch;
        SHTP_Read(&len, &ch);
        HAL_Delay(10);
    }
}

static void PrintVal(char *dest, int maxLen, float v)
{
    int vi = (int)(v * 1000);
    int va = vi < 0 ? -vi : vi;
    snprintf(dest, maxLen, "%c%d.%03d", v < 0 ? '-' : '+', va / 1000, va % 1000);
}

/* USER CODE END 0 */

int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_I2C2_Init();
  MX_USB_Device_Init();
  /* USER CODE BEGIN 2 */

  HAL_Delay(1500);
  BNO_Boot();
  BNO_EnableGyro();

  uint32_t lastPrint = 0;

  /* USER CODE END 2 */

  while (1)
  {
    /* USER CODE BEGIN 3 */

    uint16_t pktLen = 0;
    uint8_t  chan   = 0;

    if (SHTP_Read(&pktLen, &chan) == HAL_OK && chan == CH_INPUT
        && pktLen > SHTP_HDR)
    {
        uint16_t cargoLen = pktLen - SHTP_HDR;
        const uint8_t *c  = &rxBuf[SHTP_HDR];
        uint16_t idx = 0;

        while (idx < cargoLen) {
            uint8_t id = c[idx];
            if (id == RPT_BASE_TS || id == RPT_TS_REBASE) {
                idx += 5;
            } else if (id == RPT_GYRO && idx + 10 <= cargoLen) {
                lastX = (float)(int16_t)((uint16_t)c[idx+4] | (uint16_t)c[idx+5]<<8) / 512.0f;
                lastY = (float)(int16_t)((uint16_t)c[idx+6] | (uint16_t)c[idx+7]<<8) / 512.0f;
                lastZ = (float)(int16_t)((uint16_t)c[idx+8] | (uint16_t)c[idx+9]<<8) / 512.0f;
                idx += 10;
            } else {
                break;
            }
        }
    }

    if (chan == CH_EXEC) {
        HAL_Delay(100);
        BNO_EnableGyro();
    }

    /* Print once per second */
    uint32_t now = HAL_GetTick();
    if (now - lastPrint >= 1000) {
        lastPrint = now;

        char sx[12], sy[12], sz[12];
        PrintVal(sx, sizeof(sx), lastX);
        PrintVal(sy, sizeof(sy), lastY);
        PrintVal(sz, sizeof(sz), lastZ);

        CDC_Print("X: %s   Y: %s   Z: %s  rad/s\r\n", sx, sy, sz);
    }

    HAL_Delay(5);

    /* USER CODE END 3 */
  }
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

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
    Error_Handler();

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
    Error_Handler();
}

static void MX_I2C2_Init(void)
{
  hi2c2.Instance = I2C2;
  hi2c2.Init.Timing = 0x00503D58;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK) Error_Handler();
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c2, I2C_ANALOGFILTER_ENABLE) != HAL_OK) Error_Handler();
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c2, 0) != HAL_OK) Error_Handler();
}

static void MX_GPIO_Init(void)
{
  __HAL_RCC_GPIOA_CLK_ENABLE();
}

void Error_Handler(void)
{
  __disable_irq();
  while (1) {}
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) {}
#endif
