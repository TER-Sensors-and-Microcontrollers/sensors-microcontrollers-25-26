/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : BNO085 Gyro diagnostic — verbose I2C + packet logging
  *                   Stage 1: CAN TX heartbeat alongside gyro data
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
#define RPT_GET_FEATURE_RESP   0xFC
#define RPT_BASE_TS            0xFB
#define RPT_TS_REBASE          0xFA
#define RPT_COMMAND_REQ        0xF2
#define RPT_FLUSH_COMPLETED    0xEF

#define PRINT_BUF              512

#define CAN_TX_ID              0x420

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
FDCAN_HandleTypeDef hfdcan2;

I2C_HandleTypeDef hi2c2;

/* USER CODE BEGIN PV */
static uint8_t rxBuf[SHTP_MAX];
static uint8_t txBuf[SHTP_MAX];
static uint8_t seqOut[6] = {0};
static char    pBuf[PRINT_BUF];

static float lastX = 0, lastY = 0, lastZ = 0;
static uint32_t readOK = 0, readFail = 0, gyroCount = 0;

/* CAN TX stats */
static FDCAN_TxHeaderTypeDef canTxHeader;
static uint8_t  canTxSeq   = 0;
static uint32_t canTxPass  = 0;
static uint32_t canTxFail  = 0;
static uint8_t  canFirstFailPrinted = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C2_Init(void);
static void MX_FDCAN2_Init(void);
/* USER CODE BEGIN PFP */
static void CDC_Print(const char *fmt, ...);
static HAL_StatusTypeDef SHTP_Read(uint16_t *len, uint8_t *ch);
static HAL_StatusTypeDef SHTP_Write(uint8_t ch, const uint8_t *d, uint16_t n);
static void BNO_Boot(void);
static void BNO_EnableGyro(void);
static void BNO_SoftReset(void);
static void CAN_Init(void);
static void CAN_SendHeartbeat(void);
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
    HAL_Delay(2);
}

static const char *HalStr(HAL_StatusTypeDef s)
{
    switch (s) {
        case HAL_OK:      return "OK";
        case HAL_ERROR:   return "ERR";
        case HAL_BUSY:    return "BUSY";
        case HAL_TIMEOUT: return "TOUT";
        default:          return "??";
    }
}

/* ---- CAN helpers ---- */

static void CAN_Init(void)
{
    /* Configure TX header — reused for every send */
    canTxHeader.Identifier          = CAN_TX_ID;
    canTxHeader.IdType              = FDCAN_STANDARD_ID;
    canTxHeader.TxFrameType         = FDCAN_DATA_FRAME;
    canTxHeader.DataLength          = FDCAN_DLC_BYTES_8;
    canTxHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    canTxHeader.BitRateSwitch       = FDCAN_BRS_OFF;
    canTxHeader.FDFormat            = FDCAN_CLASSIC_CAN;
    canTxHeader.TxEventFifoControl  = FDCAN_NO_TX_EVENTS;
    canTxHeader.MessageMarker       = 0;

    /* Start the FDCAN peripheral — moves from Init to Normal mode */
    HAL_StatusTypeDef st = HAL_FDCAN_Start(&hfdcan2);
    CDC_Print("FDCAN2 Start: %s\r\n", HalStr(st));
}

static void CAN_SendHeartbeat(void)
{
    uint8_t payload[8];

    /* Pack raw Q9 gyro values as int16 little-endian */
    int16_t rawX = (int16_t)(lastX * 512.0f);
    int16_t rawY = (int16_t)(lastY * 512.0f);
    int16_t rawZ = (int16_t)(lastZ * 512.0f);

    payload[0] = (uint8_t)( rawX       & 0xFF);
    payload[1] = (uint8_t)((rawX >> 8)  & 0xFF);
    payload[2] = (uint8_t)( rawY       & 0xFF);
    payload[3] = (uint8_t)((rawY >> 8)  & 0xFF);
    payload[4] = (uint8_t)( rawZ       & 0xFF);
    payload[5] = (uint8_t)((rawZ >> 8)  & 0xFF);
    payload[6] = canTxSeq;
    payload[7] = 0x00;  /* reserved */

    HAL_StatusTypeDef st = HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan2, &canTxHeader, payload);

    if (st == HAL_OK) {
        canTxPass++;
        canFirstFailPrinted = 0;
    } else {
        canTxFail++;

        if (!canFirstFailPrinted) {
            canFirstFailPrinted = 1;
            FDCAN_ProtocolStatusTypeDef psr;
            HAL_FDCAN_GetProtocolStatus(&hfdcan2, &psr);
            CDC_Print("  CAN PSR: LEC=%lu DLEC=%lu ACT=%lu EP=%lu EW=%lu BO=%lu\r\n",
                      (unsigned long)psr.LastErrorCode,
                      (unsigned long)psr.DataLastErrorCode,
                      (unsigned long)psr.Activity,
                      (unsigned long)psr.ErrorPassive,
                      (unsigned long)psr.Warning,
                      (unsigned long)psr.BusOff);
        }
    }

    CDC_Print("CAN TX #%u: %s  (pass=%lu fail=%lu)\r\n",
              (unsigned)canTxSeq, HalStr(st),
              (unsigned long)canTxPass,
              (unsigned long)canTxFail);

    canTxSeq++;
}

/* ---- SHTP / BNO085 ---- */

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

/* ---- SH-2 software reset via Initialize command (0x04) ---- */
static void BNO_SoftReset(void)
{
    /* Command Request (0xF2), Seq=0, Command=0x04 (Initialize)
     * Param 0 = 0x01 (reinitialize entire system)
     * Rest = 0
     */
    uint8_t cmd[12];
    memset(cmd, 0, sizeof(cmd));
    cmd[0] = RPT_COMMAND_REQ;  /* Report ID */
    cmd[1] = 0;                /* Sequence   */
    cmd[2] = 0x01;             /* Command: report errors / reset */
    /* bytes 3-11 = 0 */

    SHTP_Write(CH_CTRL, cmd, 12);
    HAL_Delay(300);

    /* Drain post-reset packets */
    for (int i = 0; i < 20; i++) {
        uint16_t len; uint8_t ch;
        if (SHTP_Read(&len, &ch) != HAL_OK) HAL_Delay(30);
        else HAL_Delay(10);
    }
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
    cmd[5] = 0x20; cmd[6] = 0x4E;
    HAL_StatusTypeDef st = SHTP_Write(CH_CTRL, cmd, 17);
    CDC_Print("EnableGyro TX: %s\r\n", HalStr(st));
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
  MX_FDCAN2_Init();
  /* USER CODE BEGIN 2 */

  HAL_Delay(1500);

  CDC_Print("\r\n=== BNO085 Gyro Diagnostic + CAN TX ===\r\n");

  /* Start FDCAN and configure TX header */
  CAN_Init();

  /* Check I2C */
  HAL_StatusTypeDef rdy = HAL_I2C_IsDeviceReady(&hi2c2, BNO085_ADDR, 3, 100);
  CDC_Print("I2C device ready: %s\r\n", HalStr(rdy));

  BNO_Boot();
  BNO_EnableGyro();

  uint32_t lastPrint = 0;
  float prevX = 0, prevY = 0, prevZ = 0;
  uint32_t staleCount = 0;

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    uint16_t pktLen = 0;
    uint8_t  chan   = 0;
    HAL_StatusTypeDef st = SHTP_Read(&pktLen, &chan);

    if (st == HAL_OK && pktLen > SHTP_HDR) {
        readOK++;

        if (chan == CH_INPUT) {
            uint16_t cargoLen = pktLen - SHTP_HDR;
            const uint8_t *c = &rxBuf[SHTP_HDR];
            uint16_t idx = 0;

            while (idx < cargoLen) {
                uint8_t id = c[idx];
                if (id == RPT_BASE_TS || id == RPT_TS_REBASE) {
                    idx += 5;
                } else if (id == RPT_GYRO && idx + 10 <= cargoLen) {
                    lastX = (float)(int16_t)((uint16_t)c[idx+4] | (uint16_t)c[idx+5]<<8) / 512.0f;
                    lastY = (float)(int16_t)((uint16_t)c[idx+6] | (uint16_t)c[idx+7]<<8) / 512.0f;
                    lastZ = (float)(int16_t)((uint16_t)c[idx+8] | (uint16_t)c[idx+9]<<8) / 512.0f;
                    gyroCount++;
                    idx += 10;
                } else {
                    idx++;  /* skip unknown byte, keep parsing */
                }
            }
        }

        if (chan == CH_EXEC) {
            CDC_Print("!! Sensor reset detected on EXEC channel\r\n");
            HAL_Delay(100);
            BNO_EnableGyro();
        }
    } else if (st != HAL_OK && st != HAL_TIMEOUT) {
        readFail++;
    }

    /* Print once per second */
    uint32_t now = HAL_GetTick();
    if (now - lastPrint >= 1000) {
        lastPrint = now;

        char sx[12], sy[12], sz[12];
        PrintVal(sx, sizeof(sx), lastX);
        PrintVal(sy, sizeof(sy), lastY);
        PrintVal(sz, sizeof(sz), lastZ);

        CDC_Print("X:%s  Y:%s  Z:%s  |  ok=%lu fail=%lu gyro=%lu  I2C=%s\r\n",
                  sx, sy, sz,
                  (unsigned long)readOK,
                  (unsigned long)readFail,
                  (unsigned long)gyroCount,
                  hi2c2.State == HAL_I2C_STATE_READY ? "RDY" : "FLT");

        /* Send CAN heartbeat right after gyro line */
        CAN_SendHeartbeat();

        /* Stale detection */
        if (lastX == prevX && lastY == prevY && lastZ == prevZ)
            staleCount++;
        else
            staleCount = 0;

        prevX = lastX; prevY = lastY; prevZ = lastZ;

        if (staleCount >= 5) {
            CDC_Print("!! Stale for %lu sec — sending SH-2 soft reset\r\n",
                      (unsigned long)staleCount);

            /* Full recovery: I2C re-init + SH-2 software reset */
            HAL_I2C_DeInit(&hi2c2);
            HAL_Delay(50);
            MX_I2C2_Init();

            rdy = HAL_I2C_IsDeviceReady(&hi2c2, BNO085_ADDR, 3, 100);
            CDC_Print("   I2C after re-init: %s\r\n", HalStr(rdy));

            BNO_SoftReset();
            CDC_Print("   Soft reset sent, re-enabling gyro\r\n");
            BNO_EnableGyro();

            /* Reset counters */
            readOK = 0; readFail = 0; gyroCount = 0;
            staleCount = 0;
        }
    }

    HAL_Delay(5);

  /* USER CODE END 3 */
}
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
