/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body — CAN <-> UART (RFD900) bridge
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
#include "string.h"
#include "stdbool.h"
#include "stdio.h"
#include <stdint.h>



/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum {
  ST_WAIT_SOF = 0,
  ST_WAIT_LEN,
  ST_WAIT_BODY,
  ST_WAIT_CRC
} parse_state_t;

/* Structure for one outgoing UART packet in the TX ring buffer */

#define UART_PKT_MAX   64
typedef struct {
  uint8_t data[UART_PKT_MAX];
  uint8_t len;
} uart_tx_pkt_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define SOF                0x7E
#define PKT_TYPE_CAN       0x01

#define UART_RX_CHUNK      1
#define UART_TX_TIMEOUT_MS 10
#define MAX_BODY_LEN       32

/* TX ring buffer depth — must be power of 2 */
#define TX_RING_SIZE       128
#define TX_RING_MASK       (TX_RING_SIZE - 1)
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
FDCAN_HandleTypeDef hfdcan2;
FDCAN_HandleTypeDef hfdcan3;

UART_HandleTypeDef huart3;

/* USER CODE BEGIN PV */

/* UART RX (interrupt-driven, 1 byte at a time) */
static uint8_t uart_rx_byte;

/* Packet parser state */
static parse_state_t st        = ST_WAIT_SOF;
static uint8_t       pkt_len   = 0;
static uint8_t       body[MAX_BODY_LEN];
static uint8_t       body_idx  = 0;

/* TX ring buffer (written from CAN ISR, drained in main loop) */
static uart_tx_pkt_t tx_ring[TX_RING_SIZE];
static volatile uint8_t tx_head = 0;   /* ISR writes here */
static volatile uint8_t tx_tail = 0;   /* main loop reads here */

/*
 * DLC (0-8) -> HAL FDCAN DataLength constant lookup table.
 * The STM32 FDCAN HAL encodes DLC as (dlc << 16), but using the
 * named constants is safer and more portable across HAL versions.
 */
static const uint32_t dlc_to_hal[9] = {
  FDCAN_DLC_BYTES_0,
  FDCAN_DLC_BYTES_1,
  FDCAN_DLC_BYTES_2,
  FDCAN_DLC_BYTES_3,
  FDCAN_DLC_BYTES_4,
  FDCAN_DLC_BYTES_5,
  FDCAN_DLC_BYTES_6,
  FDCAN_DLC_BYTES_7,
  FDCAN_DLC_BYTES_8,
};

volatile uint32_t can_rx_irq_count = 0;
volatile uint32_t can_getmsg_fail  = 0;
volatile uint32_t uart_tx_pkts     = 0;
volatile uint32_t ring_drops       = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_FDCAN3_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_FDCAN2_Init(void);
/* USER CODE BEGIN PFP */
static uint8_t  crc8_atm(const uint8_t *data, uint32_t len);
static void     u32_to_le(uint32_t v, uint8_t out[4]);
static uint32_t le_to_u32(const uint8_t in[4]);

static void enqueue_can_over_uart(uint32_t id, bool ext, bool rtr, uint8_t dlc, const uint8_t *data);
static void drain_uart_tx(void);
static void parser_feed(uint8_t byte);
static void process_can_packet(const uint8_t *b, uint8_t len);

static void CAN_FilterAcceptAll(FDCAN_HandleTypeDef *hfdcan);
static void CAN_StartAndEnableRxIRQ(FDCAN_HandleTypeDef *hfdcan);

static void CAN_GlobalAcceptAll(FDCAN_HandleTypeDef *hfdcan)
{
  (void)HAL_FDCAN_ConfigGlobalFilter(hfdcan,
        FDCAN_ACCEPT_IN_RX_FIFO0,   // std frames
        FDCAN_ACCEPT_IN_RX_FIFO0,   // ext frames
        FDCAN_REJECT_REMOTE,        // std remote
        FDCAN_REJECT_REMOTE);       // ext remote
}
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void usb_print(const char *s)
{
  if (s == NULL) return;
  (void)CDC_Transmit_FS((uint8_t*)s, (uint16_t)strlen(s));
}
/* -------------------------------------------------------------------------
 * CRC-8/ATM  (poly 0x07, init 0x00, no reflect)
 * ------------------------------------------------------------------------- */
static uint8_t crc8_atm(const uint8_t *data, uint32_t len)
{
  uint8_t crc = 0x00;
  for (uint32_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t b = 0; b < 8; b++) {
      crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
    }
  }
  return crc;
}

static void u32_to_le(uint32_t v, uint8_t out[4])
{
  out[0] = (uint8_t)(v);
  out[1] = (uint8_t)(v >> 8);
  out[2] = (uint8_t)(v >> 16);
  out[3] = (uint8_t)(v >> 24);
}

static uint32_t le_to_u32(const uint8_t in[4])
{
  return ((uint32_t)in[0])        |
         ((uint32_t)in[1] <<  8)  |
         ((uint32_t)in[2] << 16)  |
         ((uint32_t)in[3] << 24);
}

/* -------------------------------------------------------------------------
 * Serialize one CAN frame and push it into the TX ring buffer.
 * Safe to call from ISR context — no blocking UART calls here.
 *
 * Packet layout:
 *   SOF(1) | LEN(1) | TYPE(1) | ID_LE(4) | FLAGS(1) | DLC(1) | DATA(dlc) | CRC(1)
 * ------------------------------------------------------------------------- */
static void enqueue_can_over_uart(uint32_t id, bool ext, bool rtr,
                                  uint8_t dlc, const uint8_t *data)
{
  if (dlc > 8) return;
  if (ext) return;   /* Pi parser expects standard IDs only */
  if (rtr) return;   /* skip remote frames */

  /* total length = 2-byte ID + 1-byte DLC + dlc bytes + '\n' */
  uint8_t pkt_len = (uint8_t)(2u + 1u + dlc + 1u);
  if (pkt_len > UART_PKT_MAX) return;

  /* Check ring buffer space (leave one slot as sentinel) */
  uint8_t next_head = (tx_head + 1) & TX_RING_MASK;
  if (next_head == tx_tail) {
    ring_drops++;
    return;
  }

  uart_tx_pkt_t *slot = &tx_ring[tx_head];
  uint8_t *pkt = slot->data;
  uint8_t idx = 0;

  /* standard CAN ID, 11-bit, BIG ENDIAN */
  id &= 0x7FFu;
  pkt[idx++] = (uint8_t)((id >> 8) & 0xFF);
  pkt[idx++] = (uint8_t)(id & 0xFF);

  /* DLC */
  pkt[idx++] = dlc;

  /* data bytes */
  for (uint8_t i = 0; i < dlc; i++) {
    pkt[idx++] = data[i];
  }

  /* newline terminator so Pi readline() returns */
  pkt[idx++] = '\n';

  slot->len = idx;

  /* Commit — advance head after fully writing the slot */
  tx_head = next_head;
}
/* -------------------------------------------------------------------------
 * Drain the TX ring buffer.  Call from the main loop only (blocking TX).
 * ------------------------------------------------------------------------- */

static void drain_uart_tx(void)
{
  while (tx_tail != tx_head) {
    uart_tx_pkt_t *slot = &tx_ring[tx_tail];

    if (HAL_UART_Transmit(&huart3, slot->data, slot->len, UART_TX_TIMEOUT_MS) == HAL_OK) {
    	uart_tx_pkts++;
    	tx_tail = (tx_tail + 1) & TX_RING_MASK;
    } else {
      // UART busy or error: don't drop; retry next loop
      break;
    }
  }
}
/* -------------------------------------------------------------------------
 * Inject a received UART→CAN packet onto the CAN bus.
 * Called from UART RX ISR via parser_feed(); keep it short.
 *
 * Body layout:  TYPE(1) | ID_LE(4) | FLAGS(1) | DLC(1) | DATA(dlc)
 * ------------------------------------------------------------------------- */
static void process_can_packet(const uint8_t *b, uint8_t len)
{
  if (len < 1) return;
  if (b[0] != PKT_TYPE_CAN) return;

  /* Minimum: TYPE + ID(4) + FLAGS + DLC = 7 bytes */
  if (len < 7u) return;

  uint32_t id   = le_to_u32(&b[1]);
  uint8_t flags = b[5];
  uint8_t dlc   = b[6];

  if (dlc > 8) return;
  if (len != (uint8_t)(7u + dlc)) return;   /* exact length check */

  bool ext = (flags & 0x01) != 0;
  bool rtr = (flags & 0x02) != 0;

  /* Mask standard IDs to 11 bits to prevent HAL assertion */
  if (!ext) id &= 0x7FFu;

  FDCAN_TxHeaderTypeDef txHeader = {0};
  txHeader.Identifier            = id;
  txHeader.IdType                = ext ? FDCAN_EXTENDED_ID : FDCAN_STANDARD_ID;
  txHeader.TxFrameType           = rtr ? FDCAN_REMOTE_FRAME : FDCAN_DATA_FRAME;
  txHeader.DataLength            = dlc_to_hal[dlc];   /* use named HAL constant */
  txHeader.ErrorStateIndicator   = FDCAN_ESI_ACTIVE;
  txHeader.BitRateSwitch         = FDCAN_BRS_OFF;
  txHeader.FDFormat              = FDCAN_CLASSIC_CAN;
  txHeader.TxEventFifoControl    = FDCAN_NO_TX_EVENTS;
  txHeader.MessageMarker         = 0;

  uint8_t txdata[8] = {0};
  if (!rtr && dlc > 0) {
    memcpy(txdata, &b[7], dlc);
  }

  (void)HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan3, &txHeader, txdata);
}

/* -------------------------------------------------------------------------
 * Byte-by-byte UART packet parser.
 * Called from HAL_UART_RxCpltCallback (ISR context).
 * ------------------------------------------------------------------------- */
static void parser_feed(uint8_t byte)
{
  switch (st) {
    case ST_WAIT_SOF:
      if (byte == SOF) {
        st = ST_WAIT_LEN;
      }
      break;

    case ST_WAIT_LEN:
      pkt_len = byte;
      if (pkt_len == 0 || pkt_len > MAX_BODY_LEN) {
        st = ST_WAIT_SOF;
      } else {
        body_idx = 0;
        st = ST_WAIT_BODY;
      }
      break;

    case ST_WAIT_BODY:
      body[body_idx++] = byte;
      if (body_idx >= pkt_len) {
        st = ST_WAIT_CRC;
      }
      break;

    case ST_WAIT_CRC:
    {
      /* CRC input = LEN byte + body bytes */
      uint8_t tmp[1u + MAX_BODY_LEN];
      tmp[0] = pkt_len;
      memcpy(&tmp[1], body, pkt_len);
      uint8_t expected = crc8_atm(tmp, (uint32_t)(1u + pkt_len));

      if (expected == byte) {
        process_can_packet(body, pkt_len);
      }
      st = ST_WAIT_SOF;
      break;
    }

    default:
      st = ST_WAIT_SOF;
      break;
  }
}

/* -------------------------------------------------------------------------
 * FDCAN filter: accept ALL frames (std + ext) into RX FIFO0.
 *
 * StdFiltersNbr = 1 and ExtFiltersNbr = 1 must be set in MX_FDCAN3_Init
 * (already done) so HAL allocates the filter RAM slots before this runs.
 * ------------------------------------------------------------------------- */
//static void CAN_FilterAcceptAll(void)
//{
//  FDCAN_FilterTypeDef sFilter = {0};
//
//  /* Standard IDs: mask = 0 accepts everything */
//  sFilter.IdType       = FDCAN_STANDARD_ID;
//  sFilter.FilterIndex  = 0;
//  sFilter.FilterType   = FDCAN_FILTER_MASK;
//  sFilter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
//  sFilter.FilterID1    = 0x000;
//  sFilter.FilterID2    = 0x000;
//  (void)HAL_FDCAN_ConfigFilter(&hfdcan3, &sFilter);
//
//  /* Extended IDs: mask = 0 accepts everything */
//  sFilter.IdType       = FDCAN_EXTENDED_ID;
//  sFilter.FilterIndex  = 0;
//  sFilter.FilterType   = FDCAN_FILTER_MASK;
//  sFilter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
//  sFilter.FilterID1    = 0x00000000;
//  sFilter.FilterID2    = 0x00000000;
//  (void)HAL_FDCAN_ConfigFilter(&hfdcan3, &sFilter);
//}

static void CAN_FilterAcceptAll(FDCAN_HandleTypeDef *hfdcan)
{
  FDCAN_FilterTypeDef sFilter = {0};

  /* Standard IDs: mask = 0 accepts everything */
  sFilter.IdType       = FDCAN_STANDARD_ID;
  sFilter.FilterIndex  = 0;
  sFilter.FilterType   = FDCAN_FILTER_MASK;
  sFilter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
  sFilter.FilterID1    = 0x000;
  sFilter.FilterID2    = 0x000;
  (void)HAL_FDCAN_ConfigFilter(hfdcan, &sFilter);

  /* Extended IDs: mask = 0 accepts everything */
  sFilter.IdType       = FDCAN_EXTENDED_ID;
  sFilter.FilterIndex  = 0;
  sFilter.FilterType   = FDCAN_FILTER_MASK;
  sFilter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
  sFilter.FilterID1    = 0x00000000;
  sFilter.FilterID2    = 0x00000000;
  (void)HAL_FDCAN_ConfigFilter(hfdcan, &sFilter);
}

/* Start FDCAN and enable RX FIFO0 new-message interrupt */
//static void CAN_StartAndEnableRxIRQ(void)
//{
//  (void)HAL_FDCAN_Start(&hfdcan3);
//  (void)HAL_FDCAN_ActivateNotification(&hfdcan3, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0);
//}

static void CAN_StartAndEnableRxIRQ(FDCAN_HandleTypeDef *hfdcan)
{
  HAL_FDCAN_Start(hfdcan);
  HAL_FDCAN_ActivateNotification(
      hfdcan,
      FDCAN_IT_RX_FIFO0_NEW_MESSAGE,
      0);
}

/* -------------------------------------------------------------------------
 * HAL callback: new CAN frame in RX FIFO0.
 * Enqueues a UART packet into the ring buffer — no blocking TX here.
 * ------------------------------------------------------------------------- */
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs)
{   HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_0);

  if ((RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) == 0) return;



  FDCAN_RxHeaderTypeDef rxHeader;
  uint8_t rxData[8];

  can_rx_irq_count++;
  if (HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &rxHeader, rxData) != HAL_OK) {  can_getmsg_fail++;
  return;};

  bool ext = (rxHeader.IdType      == FDCAN_EXTENDED_ID);
  bool rtr = (rxHeader.RxFrameType == FDCAN_REMOTE_FRAME);

  /* Reverse-map HAL DataLength constant back to raw DLC (0-8) */
  uint8_t dlc = 0;
  for (uint8_t i = 0; i <= 8; i++) {
    if (dlc_to_hal[i] == rxHeader.DataLength) { dlc = i; break; }
  }

  uint32_t id = rxHeader.Identifier;
  if (!ext) id &= 0x7FFu;
  {
      char msg[128];
      int len = snprintf(msg, sizeof(msg),
                         "RX id=0x%08lX ext=%u rtr=%u dlc=%u data=%02X %02X %02X %02X %02X %02X %02X %02X\r\n",
                         id,
                         (unsigned)ext,
                         (unsigned)rtr,
                         (unsigned)dlc,
                         rxData[0], rxData[1], rxData[2], rxData[3],
                         rxData[4], rxData[5], rxData[6], rxData[7]);

      if (len > 0) {
        (void)CDC_Transmit_FS((uint8_t*)msg, (uint16_t)len);
      }
    }

  enqueue_can_over_uart(id, ext, rtr, dlc, rxData);
}

/* -------------------------------------------------------------------------
 * HAL callback: UART RX interrupt (1 byte received).
 * Re-arms the interrupt immediately so no byte is lost.
 * ------------------------------------------------------------------------- */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART3) {
    parser_feed(uart_rx_byte);
    (void)HAL_UART_Receive_IT(&huart3, &uart_rx_byte, UART_RX_CHUNK);
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
  MX_FDCAN3_Init();
  MX_USART3_UART_Init();
  MX_USB_Device_Init();
  MX_FDCAN2_Init();
  /* USER CODE BEGIN 2 */

  const uint8_t hello[] = "UART OK\r\n";
  HAL_UART_Transmit(&huart3, (uint8_t*)hello, sizeof(hello)-1, 100);
  CAN_FilterAcceptAll(&hfdcan3);
  CAN_FilterAcceptAll(&hfdcan2);
  CAN_GlobalAcceptAll(&hfdcan3);
  CAN_GlobalAcceptAll(&hfdcan2);
  CAN_StartAndEnableRxIRQ(&hfdcan3);
  CAN_StartAndEnableRxIRQ(&hfdcan2);




  /* Arm UART RX interrupt */
  (void)HAL_UART_Receive_IT(&huart3, &uart_rx_byte, UART_RX_CHUNK);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  uint32_t last_print = 0;
  while (1)
  {
	  drain_uart_tx();

	  if (HAL_GetTick() - last_print >= 1000) {
	        last_print = HAL_GetTick();

	        char msg[128];
	        int len = snprintf(msg, sizeof(msg),
	                           "CAN irq=%lu  getmsg_fail=%lu  uart_tx_pkts=%lu  ring_drops=%lu\r\n",
	                           can_rx_irq_count, can_getmsg_fail, uart_tx_pkts, ring_drops);
	        if (len > 0) {
	          (void)CDC_Transmit_FS((uint8_t*)msg, (uint16_t)len);
	        }}
	  HAL_Delay(1);
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
  hfdcan2.Init.NominalPrescaler = 4;
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
  * @brief FDCAN3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_FDCAN3_Init(void)
{

  /* USER CODE BEGIN FDCAN3_Init 0 */

  /* USER CODE END FDCAN3_Init 0 */

  /* USER CODE BEGIN FDCAN3_Init 1 */

  /* USER CODE END FDCAN3_Init 1 */
  hfdcan3.Instance = FDCAN3;
  hfdcan3.Init.ClockDivider = FDCAN_CLOCK_DIV1;
  hfdcan3.Init.FrameFormat = FDCAN_FRAME_CLASSIC;
  hfdcan3.Init.Mode = FDCAN_MODE_NORMAL;
  hfdcan3.Init.AutoRetransmission = DISABLE;
  hfdcan3.Init.TransmitPause = DISABLE;
  hfdcan3.Init.ProtocolException = DISABLE;
  hfdcan3.Init.NominalPrescaler = 4;
  hfdcan3.Init.NominalSyncJumpWidth = 1;
  hfdcan3.Init.NominalTimeSeg1 = 13;
  hfdcan3.Init.NominalTimeSeg2 = 2;
  hfdcan3.Init.DataPrescaler = 1;
  hfdcan3.Init.DataSyncJumpWidth = 1;
  hfdcan3.Init.DataTimeSeg1 = 1;
  hfdcan3.Init.DataTimeSeg2 = 1;
  hfdcan3.Init.StdFiltersNbr = 0;
  hfdcan3.Init.ExtFiltersNbr = 0;
  hfdcan3.Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;
  if (HAL_FDCAN_Init(&hfdcan3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN FDCAN3_Init 2 */

  /* USER CODE END FDCAN3_Init 2 */

}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 460800;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart3.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart3, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart3, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

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
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET);

  /*Configure GPIO pin : PA0 */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

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
