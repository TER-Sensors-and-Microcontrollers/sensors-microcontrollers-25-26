/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : BNO085 CAN RX board — receives gyro data from TX board
  *                   and prints diagnostics over USB CDC
  ******************************************************************************
  *
  *  CAN frame layout expected from TX board (ID 0x420, DLC 8):
  *    [0:1] X gyro  (int16_t LE, Q9 — divide by 512.0 for rad/s)
  *    [2:3] Y gyro
  *    [4:5] Z gyro
  *    [6]   Sequence counter (0-255 rolling)
  *    [7]   Reserved
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
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define CAN_RX_ID              0x420
#define PRINT_BUF              512

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
FDCAN_HandleTypeDef hfdcan2;

/* USER CODE BEGIN PV */
static char pBuf[PRINT_BUF];

/* Latest received gyro frame */
static volatile float    rxGyroX   = 0, rxGyroY = 0, rxGyroZ = 0;
static volatile uint8_t  rxSeq     = 0;
static volatile uint32_t rxCount   = 0;
static volatile uint8_t  rxFresh   = 0;   /* set in ISR, cleared after print */

/* Stats */
static uint32_t printCount  = 0;
static uint32_t lastRxCount = 0;
static uint8_t  lastSeq     = 0;
static uint32_t missedFrames = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_FDCAN2_Init(void);
/* USER CODE BEGIN PFP */
static void CDC_Print(const char *fmt, ...);
static void CAN_FilterAndStart(void);
static void PrintVal(char *dest, int maxLen, float v);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* ---- CDC helper (same as TX board) ---- */
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

/* ---- Float formatting without printf %f (same as TX board) ---- */
static void PrintVal(char *dest, int maxLen, float v)
{
    int vi = (int)(v * 1000);
    int va = vi < 0 ? -vi : vi;
    snprintf(dest, maxLen, "%c%d.%03d", v < 0 ? '-' : '+', va / 1000, va % 1000);
}

/* ---- CAN RX setup ---- */
static void CAN_FilterAndStart(void)
{
    /* Accept only standard ID 0x420 into RXFIFO0 */
    FDCAN_FilterTypeDef filter;
    filter.IdType       = FDCAN_STANDARD_ID;
    filter.FilterIndex  = 0;
    filter.FilterType   = FDCAN_FILTER_MASK;
    filter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    filter.FilterID1    = CAN_RX_ID;       /* ID to match     */
    filter.FilterID2    = 0x7FF;           /* mask: exact match */

    HAL_StatusTypeDef st = HAL_FDCAN_ConfigFilter(&hfdcan2, &filter);
    CDC_Print("CAN filter cfg: %s\r\n", HalStr(st));

    /* Reject non-matching standard frames, accept all extended (don't care) */
    HAL_FDCAN_ConfigGlobalFilter(&hfdcan2,
                                  FDCAN_REJECT,    /* non-matching std */
                                  FDCAN_REJECT,    /* non-matching ext */
                                  FDCAN_REJECT_REMOTE,
                                  FDCAN_REJECT_REMOTE);

    /* Start the peripheral */
    st = HAL_FDCAN_Start(&hfdcan2);
    CDC_Print("FDCAN2 Start:   %s\r\n", HalStr(st));

    /* Enable RX FIFO0 new-message interrupt */
    st = HAL_FDCAN_ActivateNotification(&hfdcan2,
                                         FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0);
    CDC_Print("CAN RX IRQ:     %s\r\n", HalStr(st));
}

/* ---- FDCAN RX callback (called from ISR context) ---- */
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs)
{
    if (!(RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE)) return;

    FDCAN_RxHeaderTypeDef rxHeader;
    uint8_t rxData[8];

    if (HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &rxHeader, rxData) != HAL_OK)
        return;

    /* Only process our expected ID */
    if (rxHeader.Identifier != CAN_RX_ID) return;

    /* Decode gyro: int16 LE, Q9 (divide by 512) */
    int16_t rawX = (int16_t)((uint16_t)rxData[0] | ((uint16_t)rxData[1] << 8));
    int16_t rawY = (int16_t)((uint16_t)rxData[2] | ((uint16_t)rxData[3] << 8));
    int16_t rawZ = (int16_t)((uint16_t)rxData[4] | ((uint16_t)rxData[5] << 8));

    rxGyroX = (float)rawX / 512.0f;
    rxGyroY = (float)rawY / 512.0f;
    rxGyroZ = (float)rawZ / 512.0f;
    rxSeq   = rxData[6];
    rxCount++;
    rxFresh = 1;
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
  MX_USB_Device_Init();
  MX_FDCAN2_Init();
  /* USER CODE BEGIN 2 */

  /* Give USB CDC time to enumerate on host */
  HAL_Delay(1500);

  CDC_Print("\r\n=== BNO085 CAN RX Board ===\r\n");
  CDC_Print("Listening for gyro frames on CAN ID 0x%03X\r\n", CAN_RX_ID);


  HAL_Delay(10000);

  /* Configure filter and start FDCAN with RX interrupt */
  CAN_FilterAndStart();

  uint32_t lastPrint   = HAL_GetTick();
  uint32_t lastRxSnap  = 0;
  uint8_t  connected   = 0;

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    uint32_t now = HAL_GetTick();

    /* ---- 1 Hz status print ---- */
    if (now - lastPrint >= 1000) {
        lastPrint = now;
        printCount++;

        uint32_t snap = rxCount;  /* atomic-ish read of ISR counter */

        /* Connection status */
        if (snap > lastRxSnap) {
            if (!connected) {
                CDC_Print(">> CAN link UP — receiving frames from TX board\r\n");
                connected = 1;
            }

            /* Check for missed sequence numbers */
            uint8_t seqNow = rxSeq;
            if (connected && lastRxSnap > 0) {
                uint8_t expected = (uint8_t)(lastSeq + 1);
                if (seqNow != expected && snap != 1) {
                    uint8_t gap = (uint8_t)(seqNow - expected);
                    missedFrames += gap;
                    CDC_Print("   Seq gap: expected %u got %u (missed %u)\r\n",
                              (unsigned)expected, (unsigned)seqNow, (unsigned)gap);
                }
            }
            lastSeq = seqNow;
        } else {
            if (connected) {
                CDC_Print(">> CAN link DOWN — no frames in last 1s\r\n");
                connected = 0;
            }
        }

        /* Gyro readout */
        char sx[12], sy[12], sz[12];
        PrintVal(sx, sizeof(sx), rxGyroX);
        PrintVal(sy, sizeof(sy), rxGyroY);
        PrintVal(sz, sizeof(sz), rxGyroZ);

        CDC_Print("X:%s  Y:%s  Z:%s  |  seq=%u  frames=%lu  missed=%lu\r\n",
                  sx, sy, sz,
                  (unsigned)rxSeq,
                  (unsigned long)snap,
                  (unsigned long)missedFrames);

        lastRxSnap = snap;
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
  hfdcan2.Init.StdFiltersNbr = 1;   /* <-- need 1 filter slot */
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
