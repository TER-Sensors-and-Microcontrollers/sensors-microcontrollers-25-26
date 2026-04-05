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
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* ---- BNO085 I2C Address ---- */
#define BNO085_I2C_ADDR          (0x4A << 1)   /* 7-bit 0x4A -> HAL 8-bit 0x94 */
#define BNO085_I2C_ADDR_ALT      (0x4B << 1)   /* Alternate if DI pulled high   */

/* ---- SHTP framing ---- */
#define SHTP_MAX_TRANSFER_SIZE   300
#define SHTP_HEADER_SIZE         4

/* SHTP channel numbers */
#define CHAN_SHTP_COMMAND         0   /* SHTP advertisement / command           */
#define CHAN_EXECUTABLE           1   /* Executable channel (reset msgs)        */
#define CHAN_CONTROL              2   /* SH-2 control (set/get feature, prod ID)*/
#define CHAN_INPUT_REPORTS        3   /* Sensor input reports                   */
#define CHAN_WAKE_REPORTS         4   /* Wake sensor input reports              */
#define CHAN_GYRO_RV              5   /* Gyro-integrated rotation vector        */

/* ---- SH-2 sensor report IDs ---- */
#define REPORT_ACCELEROMETER          0x01
#define REPORT_GYROSCOPE              0x02
#define REPORT_ROTATION_VECTOR        0x05

/* ---- SH-2 command / response report IDs ---- */
#define REPORT_PRODUCT_ID_REQUEST     0xF9
#define REPORT_PRODUCT_ID_RESPONSE    0xF8
#define REPORT_SET_FEATURE_CMD        0xFD
#define REPORT_GET_FEATURE_RESP       0xFC
#define REPORT_BASE_TIMESTAMP         0xFB
#define REPORT_TIMESTAMP_REBASE       0xFA
#define REPORT_FORCE_FLUSH            0xF0
#define REPORT_FLUSH_COMPLETED        0xEF

/* ---- Timing ---- */
#define REPORT_INTERVAL_US       20000   /* 50 Hz — safe for 100 kHz I2C bus */
#define I2C_TIMEOUT_MS           150     /* Per-transaction timeout           */
#define USB_STARTUP_DELAY_MS     1500    /* Wait for USB CDC enumeration      */

/* ---- Print buffer ---- */
#define PRINT_BUF_SIZE           512

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c2;

/* USER CODE BEGIN PV */

/* SHTP packet buffers */
static uint8_t shtpRxBuf[SHTP_MAX_TRANSFER_SIZE];
static uint8_t shtpTxBuf[SHTP_MAX_TRANSFER_SIZE];

/* Per-channel SHTP sequence numbers (host -> sensor) */
static uint8_t seqNumOut[6] = {0};

/* USB CDC print buffer */
static char printBuf[PRINT_BUF_SIZE];

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C2_Init(void);
/* USER CODE BEGIN PFP */

/* Utility */
static void        USB_Printf(const char *fmt, ...);
static const char *HAL_StatusStr(HAL_StatusTypeDef s);

/* I2C diagnostics */
static void I2C_ScanBus(void);

/* SHTP layer */
static HAL_StatusTypeDef SHTP_Read(uint16_t *outLen, uint8_t *outChan);
static HAL_StatusTypeDef SHTP_Write(uint8_t channel, const uint8_t *cargo,
                                    uint16_t cargoLen);

/* BNO085 high-level */
static int  BNO085_WaitForBoot(void);
static int  BNO085_RequestProductID(void);
static void BNO085_EnableReport(uint8_t reportId, uint32_t intervalUs);
static void BNO085_EnableAllReports(void);
static void BNO085_ProcessPacket(uint16_t pktLen, uint8_t channel);
static void BNO085_ParseSensorData(const uint8_t *cargo, uint16_t cargoLen);

/* Math helpers */
static float QToFloat(int16_t fixedPoint, uint8_t qPoint);
static uint8_t ReportPayloadSize(uint8_t reportId);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* =========================================================================
 *  USB_Printf  –  printf over USB CDC with busy-wait retry
 * ========================================================================= */
static void USB_Printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(printBuf, PRINT_BUF_SIZE, fmt, args);
    va_end(args);

    if (len <= 0) return;
    if (len >= PRINT_BUF_SIZE) len = PRINT_BUF_SIZE - 1;

    /* Retry for up to 50 ms if the CDC TX endpoint is busy */
    uint32_t t0 = HAL_GetTick();
    while (CDC_Transmit_FS((uint8_t *)printBuf, (uint16_t)len) == USBD_BUSY) {
        if ((HAL_GetTick() - t0) > 50) break;
        HAL_Delay(1);
    }
    /* Small gap so host USB stack can keep up */
    HAL_Delay(2);
}

/* =========================================================================
 *  HAL_StatusStr  –  human-readable HAL status
 * ========================================================================= */
static const char *HAL_StatusStr(HAL_StatusTypeDef s)
{
    switch (s) {
        case HAL_OK:      return "HAL_OK";
        case HAL_ERROR:   return "HAL_ERROR";
        case HAL_BUSY:    return "HAL_BUSY";
        case HAL_TIMEOUT: return "HAL_TIMEOUT";
        default:          return "HAL_UNKNOWN";
    }
}

/* =========================================================================
 *  I2C_ScanBus  –  scan 0x01-0x7F, print every ACK
 * ========================================================================= */
static void I2C_ScanBus(void)
{
    USB_Printf("\r\n--- I2C2 Bus Scan ---\r\n");
    uint8_t found = 0;

    for (uint8_t addr = 1; addr < 128; addr++) {
        HAL_StatusTypeDef r = HAL_I2C_IsDeviceReady(&hi2c2, addr << 1, 2, 25);
        if (r == HAL_OK) {
            USB_Printf("  ACK at 0x%02X\r\n", addr);
            found++;
        }
    }

    if (found == 0) {
        USB_Printf("  No devices found!  Check wiring / pull-ups.\r\n");
    } else {
        USB_Printf("  %u device(s) found.\r\n", found);
    }
    USB_Printf("--- End Scan ---\r\n\r\n");
}

/* =========================================================================
 *  SHTP_Read  –  read one SHTP packet from BNO085 via I2C
 *
 *  Uses the two-read approach:
 *    1) Read 4-byte header  ->  parse packet length
 *    2) Read full packet (header re-sent by sensor)
 *
 *  Returns HAL status.  *outLen = total packet length, *outChan = channel.
 * ========================================================================= */
static HAL_StatusTypeDef SHTP_Read(uint16_t *outLen, uint8_t *outChan)
{
    *outLen  = 0;
    *outChan = 0;

    /* ---- Step 1: read 4-byte header ---- */
    HAL_StatusTypeDef st;
    st = HAL_I2C_Master_Receive(&hi2c2, BNO085_I2C_ADDR, shtpRxBuf, 4,
                                I2C_TIMEOUT_MS);
    if (st != HAL_OK) return st;

    /* Parse length (bits 14:0) and continuation flag (bit 15) */
    uint16_t pktLen = (uint16_t)shtpRxBuf[0] | ((uint16_t)(shtpRxBuf[1] & 0x7F) << 8);

    /* Length == 0 or 0xFFFF means no data available */
    if (pktLen == 0 || pktLen == 0x7FFF) return HAL_TIMEOUT;

    /* Sanity-cap */
    if (pktLen > SHTP_MAX_TRANSFER_SIZE)
        pktLen = SHTP_MAX_TRANSFER_SIZE;

    /* ---- Step 2: re-read full packet ---- */
    st = HAL_I2C_Master_Receive(&hi2c2, BNO085_I2C_ADDR, shtpRxBuf, pktLen,
                                I2C_TIMEOUT_MS);
    if (st != HAL_OK) return st;

    /* Re-parse header from the full read (sensor replays from byte 0) */
    *outLen  = (uint16_t)shtpRxBuf[0] | ((uint16_t)(shtpRxBuf[1] & 0x7F) << 8);
    *outChan = shtpRxBuf[2];

    return HAL_OK;
}

/* =========================================================================
 *  SHTP_Write  –  write one SHTP packet to BNO085 via I2C
 * ========================================================================= */
static HAL_StatusTypeDef SHTP_Write(uint8_t channel, const uint8_t *cargo,
                                    uint16_t cargoLen)
{
    uint16_t totalLen = SHTP_HEADER_SIZE + cargoLen;

    /* Build header */
    shtpTxBuf[0] = (uint8_t)(totalLen & 0xFF);          /* Length LSB         */
    shtpTxBuf[1] = (uint8_t)((totalLen >> 8) & 0x7F);   /* Length MSB, no cont*/
    shtpTxBuf[2] = channel;                              /* Channel            */
    shtpTxBuf[3] = seqNumOut[channel]++;                 /* Sequence           */

    /* Copy cargo after header */
    if (cargoLen > 0 && cargo != NULL)
        memcpy(&shtpTxBuf[4], cargo, cargoLen);

    HAL_StatusTypeDef st;
    st = HAL_I2C_Master_Transmit(&hi2c2, BNO085_I2C_ADDR, shtpTxBuf,
                                 totalLen, I2C_TIMEOUT_MS);
    return st;
}

/* =========================================================================
 *  BNO085_WaitForBoot  –  drain init packets after power-on
 *
 *  After reset the BNO085 sends:
 *    1.  SHTP advertisement      (channel 0)
 *    2.  Executable reset msg    (channel 1)
 *    3.  SH-2 init unsolicited   (channel 2)
 *
 *  We attempt up to 10 reads over ~2 seconds to capture them.
 * ========================================================================= */
static int BNO085_WaitForBoot(void)
{
    USB_Printf("[BNO085] Waiting for sensor boot ...\r\n");
    HAL_Delay(200);   /* Give the sensor time after power-on */

    uint8_t gotAdvert    = 0;
    uint8_t gotExecReset = 0;
    uint8_t gotSH2Init   = 0;

    for (int attempt = 0; attempt < 15; attempt++) {
        uint16_t pktLen = 0;
        uint8_t  chan   = 0;
        HAL_StatusTypeDef st = SHTP_Read(&pktLen, &chan);

        if (st == HAL_OK && pktLen > SHTP_HEADER_SIZE) {
            USB_Printf("  Boot pkt #%d: chan=%u  len=%u  status=%s\r\n",
                        attempt, chan, pktLen, HAL_StatusStr(st));

            if (chan == CHAN_SHTP_COMMAND)   gotAdvert    = 1;
            if (chan == CHAN_EXECUTABLE)     gotExecReset = 1;
            if (chan == CHAN_CONTROL)        gotSH2Init   = 1;

            if (gotAdvert && gotExecReset && gotSH2Init) {
                USB_Printf("[BNO085] All boot packets received.\r\n");
                return 1;  /* success */
            }
        }
        HAL_Delay(100);
    }

    USB_Printf("[BNO085] Boot: advert=%u  exec_reset=%u  sh2_init=%u\r\n",
               gotAdvert, gotExecReset, gotSH2Init);

    /* Even if we didn't get all three, try to proceed */
    if (gotAdvert || gotExecReset || gotSH2Init) {
        USB_Printf("[BNO085] Partial boot — proceeding anyway.\r\n");
        return 1;
    }

    USB_Printf("[BNO085] ERROR: No boot packets received!\r\n");
    return 0;
}

/* =========================================================================
 *  BNO085_RequestProductID  –  send Product ID Request, print response
 * ========================================================================= */
static int BNO085_RequestProductID(void)
{
    USB_Printf("[BNO085] Requesting Product ID ...\r\n");

    /* Build Product ID Request (report 0xF9) */
    uint8_t req[2] = { REPORT_PRODUCT_ID_REQUEST, 0x00 };
    HAL_StatusTypeDef st = SHTP_Write(CHAN_CONTROL, req, 2);
    USB_Printf("  TX Product ID Request: %s\r\n", HAL_StatusStr(st));
    if (st != HAL_OK) return 0;

    HAL_Delay(50);

    /* Read responses — may take a few reads */
    for (int i = 0; i < 10; i++) {
        uint16_t pktLen = 0;
        uint8_t  chan   = 0;
        st = SHTP_Read(&pktLen, &chan);
        if (st != HAL_OK) { HAL_Delay(20); continue; }

        if (chan == CHAN_CONTROL && pktLen > SHTP_HEADER_SIZE) {
            uint8_t reportId = shtpRxBuf[SHTP_HEADER_SIZE];

            if (reportId == REPORT_PRODUCT_ID_RESPONSE) {
                /* Parse Product ID Response (SH-2 ref Figure 36)
                 *  Byte 0: 0xF8
                 *  Byte 1: Reset cause
                 *  Byte 2: SW version major
                 *  Byte 3: SW version minor
                 *  Byte 4-7: SW part number  (uint32 LE)
                 *  Byte 8-11: SW build number (uint32 LE)
                 *  Byte 12-13: SW version patch (uint16 LE)
                 */
                const uint8_t *d = &shtpRxBuf[SHTP_HEADER_SIZE];
                uint8_t  resetCause = d[1];
                uint8_t  swMajor   = d[2];
                uint8_t  swMinor   = d[3];
                uint32_t partNum   = (uint32_t)d[4]  | (uint32_t)d[5] << 8
                                   | (uint32_t)d[6] << 16 | (uint32_t)d[7] << 24;
                uint32_t buildNum  = (uint32_t)d[8]  | (uint32_t)d[9] << 8
                                   | (uint32_t)d[10] << 16 | (uint32_t)d[11] << 24;
                uint16_t swPatch   = (uint16_t)d[12] | (uint16_t)d[13] << 8;

                USB_Printf("  Product ID Response:\r\n");
                USB_Printf("    Reset cause : %u\r\n", resetCause);
                USB_Printf("    SW Version  : %u.%u.%u\r\n", swMajor, swMinor, swPatch);
                USB_Printf("    Part Number : %lu\r\n", (unsigned long)partNum);
                USB_Printf("    Build Number: %lu\r\n", (unsigned long)buildNum);
                return 1;  /* success */
            }
        }
        HAL_Delay(20);
    }

    USB_Printf("  ERROR: No Product ID Response received.\r\n");
    return 0;
}

/* =========================================================================
 *  BNO085_EnableReport  –  send Set Feature Command (0xFD) for one report
 *
 *  Set Feature Command layout (17 bytes cargo, SH-2 ref Figure 68):
 *    Byte  0     : 0xFD  (report ID)
 *    Byte  1     : Feature Report ID
 *    Bytes 2-3   : Change sensitivity (0 = default)
 *    Byte  4     : Reserved
 *    Bytes 5-8   : Report interval (µs, uint32 LE)
 *    Bytes 9-12  : Batch interval  (0 = no batching)
 *    Bytes 13-16 : Sensor-specific config (0)
 * ========================================================================= */
static void BNO085_EnableReport(uint8_t reportId, uint32_t intervalUs)
{
    uint8_t cmd[17];
    memset(cmd, 0, sizeof(cmd));

    cmd[0] = REPORT_SET_FEATURE_CMD;
    cmd[1] = reportId;
    /* bytes 2-4 = 0 */
    cmd[5] = (uint8_t)(intervalUs        & 0xFF);
    cmd[6] = (uint8_t)((intervalUs >> 8)  & 0xFF);
    cmd[7] = (uint8_t)((intervalUs >> 16) & 0xFF);
    cmd[8] = (uint8_t)((intervalUs >> 24) & 0xFF);
    /* bytes 9-16 = 0 */

    HAL_StatusTypeDef st = SHTP_Write(CHAN_CONTROL, cmd, 17);
    USB_Printf("  Enable report 0x%02X (%luus): %s\r\n",
               reportId, (unsigned long)intervalUs, HAL_StatusStr(st));

    /* Small delay between enabling reports to let the sensor process */
    HAL_Delay(30);
}

/* =========================================================================
 *  BNO085_EnableAllReports  –  enable accel, gyro, rotation vector at 50 Hz
 * ========================================================================= */
static void BNO085_EnableAllReports(void)
{
    USB_Printf("\r\n[BNO085] Enabling sensor reports at %lu us interval ...\r\n",
               (unsigned long)REPORT_INTERVAL_US);

    BNO085_EnableReport(REPORT_ACCELEROMETER,   REPORT_INTERVAL_US);
    BNO085_EnableReport(REPORT_GYROSCOPE,       REPORT_INTERVAL_US);
    BNO085_EnableReport(REPORT_ROTATION_VECTOR, REPORT_INTERVAL_US);

    USB_Printf("[BNO085] All reports enabled.\r\n\r\n");

    /* Drain any Get Feature Response packets the sensor sends back */
    for (int i = 0; i < 15; i++) {
        uint16_t pktLen = 0;
        uint8_t  chan   = 0;
        HAL_StatusTypeDef st = SHTP_Read(&pktLen, &chan);
        if (st != HAL_OK) break;
        if (chan == CHAN_CONTROL && pktLen > SHTP_HEADER_SIZE) {
            uint8_t rid = shtpRxBuf[SHTP_HEADER_SIZE];
            if (rid == REPORT_GET_FEATURE_RESP) {
                uint8_t  featId   = shtpRxBuf[SHTP_HEADER_SIZE + 1];
                uint32_t actualUs = (uint32_t)shtpRxBuf[SHTP_HEADER_SIZE + 5]
                                  | (uint32_t)shtpRxBuf[SHTP_HEADER_SIZE + 6] << 8
                                  | (uint32_t)shtpRxBuf[SHTP_HEADER_SIZE + 7] << 16
                                  | (uint32_t)shtpRxBuf[SHTP_HEADER_SIZE + 8] << 24;
                USB_Printf("  Feature 0x%02X confirmed: actual interval = %lu us\r\n",
                            featId, (unsigned long)actualUs);
            }
        }
        HAL_Delay(10);
    }
}

/* =========================================================================
 *  Sensor name lookup
 * ========================================================================= */
static const char *SensorName(uint8_t id)
{
    switch (id) {
        case REPORT_ACCELEROMETER:        return "Accel";
        case REPORT_GYROSCOPE:            return "Gyro";
        case REPORT_ROTATION_VECTOR:      return "RotVec";
        default:                          return "Unknown";
    }
}

/* =========================================================================
 *  Accuracy string
 * ========================================================================= */
static const char *AccuracyStr(uint8_t acc)
{
    switch (acc & 0x03) {
        case 0:  return "Unreliable";
        case 1:  return "Low";
        case 2:  return "Medium";
        case 3:  return "High";
        default: return "?";
    }
}

/* =========================================================================
 *  QToFloat  –  convert SH-2 Q-point fixed-point to float
 * ========================================================================= */
static float QToFloat(int16_t fixedPoint, uint8_t qPoint)
{
    return (float)fixedPoint / (float)(1 << qPoint);
}

/* =========================================================================
 *  ReportPayloadSize  –  total bytes for a single sensor input report
 *                         (header + data, NOT including SHTP header)
 *
 *  Each sensor input report:  [ReportID, Seq, Status, Delay, ...data...]
 * ========================================================================= */
static uint8_t ReportPayloadSize(uint8_t reportId)
{
    switch (reportId) {
        /* Timestamps */
        case REPORT_BASE_TIMESTAMP:      return 5;   /* 1 + 4 bytes delta  */
        case REPORT_TIMESTAMP_REBASE:    return 5;

        /* 3-axis sensors: 4 header + 6 data = 10 */
        case REPORT_ACCELEROMETER:       return 10;
        case REPORT_GYROSCOPE:           return 10;

        /* Quaternion + accuracy: 4 header + 8 quat + 2 acc = 14 */
        case REPORT_ROTATION_VECTOR:     return 14;

        /* Control responses */
        case REPORT_GET_FEATURE_RESP:    return 17;
        case REPORT_PRODUCT_ID_RESPONSE: return 16;
        case REPORT_FLUSH_COMPLETED:     return 2;

        default:                         return 0;  /* unknown — stop parse */
    }
}

/* =========================================================================
 *  BNO085_ParseSensorData  –  walk through cargo bytes on channel 3,
 *                              parsing concatenated reports and printing
 *
 *  Channel 3 cargo typically looks like:
 *    [BaseTimestamp(5 bytes)][SensorReport(N bytes)][SensorReport...]
 * ========================================================================= */
static void BNO085_ParseSensorData(const uint8_t *cargo, uint16_t cargoLen)
{
    uint16_t idx = 0;

    while (idx < cargoLen) {
        uint8_t reportId = cargo[idx];
        uint8_t rptSize  = ReportPayloadSize(reportId);

        /* Unknown or zero-size report — can't continue parsing */
        if (rptSize == 0 || (idx + rptSize) > cargoLen) break;

        const uint8_t *rpt = &cargo[idx];

        switch (reportId) {

        /* ---- Base Timestamp Reference ---- */
        case REPORT_BASE_TIMESTAMP: {
            int32_t baseDelta = (int32_t)((uint32_t)rpt[1]
                              | (uint32_t)rpt[2] << 8
                              | (uint32_t)rpt[3] << 16
                              | (uint32_t)rpt[4] << 24);
            (void)baseDelta;  /* Suppress unused warning; available for debugging */
            break;
        }

        /* ---- Timestamp Rebase ---- */
        case REPORT_TIMESTAMP_REBASE:
            break;  /* Acknowledged, not printed */

        /* ---- 3-axis sensors (accel, gyro) ---- */
        case REPORT_ACCELEROMETER:
        case REPORT_GYROSCOPE: {
            uint8_t status = rpt[2] & 0x03;
            int16_t rawX = (int16_t)((uint16_t)rpt[4] | (uint16_t)rpt[5] << 8);
            int16_t rawY = (int16_t)((uint16_t)rpt[6] | (uint16_t)rpt[7] << 8);
            int16_t rawZ = (int16_t)((uint16_t)rpt[8] | (uint16_t)rpt[9] << 8);

            /* Determine Q-point and units */
            uint8_t qPt;
            const char *unit;
            if (reportId == REPORT_GYROSCOPE) {
                qPt = 9;  unit = "rad/s";
            } else {
                qPt = 8;  unit = "m/s2";
            }

            float x = QToFloat(rawX, qPt);
            float y = QToFloat(rawY, qPt);
            float z = QToFloat(rawZ, qPt);

            /* Format with 4 decimal places using integer math (no %f on some
             * toolchains).  Most arm-none-eabi printf do NOT support %f,
             * so we do manual formatting. */
            int xi = (int)(x * 10000); int xa = xi < 0 ? -xi : xi;
            int yi = (int)(y * 10000); int ya = yi < 0 ? -yi : yi;
            int zi = (int)(z * 10000); int za = zi < 0 ? -zi : zi;

            USB_Printf("[%-8s] X:%c%d.%04d  Y:%c%d.%04d  Z:%c%d.%04d %s  "
                       "acc=%s\r\n",
                       SensorName(reportId),
                       (x < 0 ? '-' : '+'), xa / 10000, xa % 10000,
                       (y < 0 ? '-' : '+'), ya / 10000, ya % 10000,
                       (z < 0 ? '-' : '+'), za / 10000, za % 10000,
                       unit, AccuracyStr(status));
            break;
        }

        /* ---- Rotation Vector (quaternion + accuracy) ---- */
        case REPORT_ROTATION_VECTOR: {
            uint8_t status = rpt[2] & 0x03;
            int16_t rawI    = (int16_t)((uint16_t)rpt[4]  | (uint16_t)rpt[5]  << 8);
            int16_t rawJ    = (int16_t)((uint16_t)rpt[6]  | (uint16_t)rpt[7]  << 8);
            int16_t rawK    = (int16_t)((uint16_t)rpt[8]  | (uint16_t)rpt[9]  << 8);
            int16_t rawReal = (int16_t)((uint16_t)rpt[10] | (uint16_t)rpt[11] << 8);
            int16_t rawAcc  = (int16_t)((uint16_t)rpt[12] | (uint16_t)rpt[13] << 8);

            float qi   = QToFloat(rawI,    14);
            float qj   = QToFloat(rawJ,    14);
            float qk   = QToFloat(rawK,    14);
            float qr   = QToFloat(rawReal, 14);
            float accr = QToFloat(rawAcc,  12);  /* accuracy in radians */

            int ii = (int)(qi*10000); int ia = ii<0?-ii:ii;
            int ji = (int)(qj*10000); int ja = ji<0?-ji:ji;
            int ki = (int)(qk*10000); int ka = ki<0?-ki:ki;
            int ri = (int)(qr*10000); int ra = ri<0?-ri:ri;
            int ai = (int)(accr*10000); int aa = ai<0?-ai:ai;

            USB_Printf("[RotVec  ] I:%c%d.%04d J:%c%d.%04d K:%c%d.%04d "
                       "R:%c%d.%04d  accRad:%c%d.%04d  acc=%s\r\n",
                       (qi<0?'-':'+'),   ia/10000, ia%10000,
                       (qj<0?'-':'+'),   ja/10000, ja%10000,
                       (qk<0?'-':'+'),   ka/10000, ka%10000,
                       (qr<0?'-':'+'),   ra/10000, ra%10000,
                       (accr<0?'-':'+'), aa/10000, aa%10000,
                       AccuracyStr(status));
            break;
        }

        default:
            break;
        }

        idx += rptSize;
    }
}

/* =========================================================================
 *  BNO085_ProcessPacket  –  route a received SHTP packet by channel
 * ========================================================================= */
static void BNO085_ProcessPacket(uint16_t pktLen, uint8_t channel)
{
    if (pktLen <= SHTP_HEADER_SIZE) return;

    uint16_t cargoLen = pktLen - SHTP_HEADER_SIZE;
    const uint8_t *cargo = &shtpRxBuf[SHTP_HEADER_SIZE];

    switch (channel) {
        case CHAN_INPUT_REPORTS:
        case CHAN_WAKE_REPORTS:
            BNO085_ParseSensorData(cargo, cargoLen);
            break;

        case CHAN_CONTROL: {
            uint8_t rptId = cargo[0];
            if (rptId == REPORT_GET_FEATURE_RESP) {
                /* A late Get Feature Response — just acknowledge */
            }
            break;
        }

        case CHAN_EXECUTABLE:
            /* Executable channel messages (e.g., sensor reset detected) */
            USB_Printf("[BNO085] Executable msg received (possible reset).\r\n");
            break;

        default:
            break;
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
  MX_I2C2_Init();
  MX_USB_Device_Init();
  /* USER CODE BEGIN 2 */

  /* ---- Wait for USB CDC to enumerate on host ---- */
  HAL_Delay(USB_STARTUP_DELAY_MS);

  USB_Printf("\r\n");
  USB_Printf("========================================\r\n");
  USB_Printf("  BNO085 IMU Driver — STM32G474CEU6\r\n");
  USB_Printf("  I2C2 (PA8=SDA, PA9=SCL)  USB CDC\r\n");
  USB_Printf("========================================\r\n\r\n");

  /* ---- Report I2C peripheral status ---- */
  USB_Printf("[I2C] Peripheral state: %s\r\n",
             hi2c2.State == HAL_I2C_STATE_READY ? "READY" : "NOT READY");
  USB_Printf("[I2C] Clock speed: ~100 kHz (timing reg = 0x%08lX)\r\n",
             (unsigned long)hi2c2.Init.Timing);

  /* ---- Scan I2C bus for connected devices ---- */
  I2C_ScanBus();

  /* ---- Check if BNO085 ACKs at expected address ---- */
  HAL_StatusTypeDef devReady;
  devReady = HAL_I2C_IsDeviceReady(&hi2c2, BNO085_I2C_ADDR, 3, 100);
  USB_Printf("[I2C] BNO085 at 0x4A: %s\r\n", HAL_StatusStr(devReady));

  if (devReady != HAL_OK) {
      /* Try alternate address */
      devReady = HAL_I2C_IsDeviceReady(&hi2c2, BNO085_I2C_ADDR_ALT, 3, 100);
      USB_Printf("[I2C] BNO085 at 0x4B: %s\r\n", HAL_StatusStr(devReady));

      if (devReady != HAL_OK) {
          USB_Printf("\r\n*** FATAL: BNO085 not detected on I2C bus! ***\r\n");
          USB_Printf("    Check wiring, pull-ups, and power supply.\r\n");
          USB_Printf("    Halting.\r\n");
          while (1) { HAL_Delay(1000); }
      }
  }

  USB_Printf("[I2C] BNO085 ACK confirmed.\r\n\r\n");

  /* ---- Initialize BNO085 (drain boot packets) ---- */
  if (!BNO085_WaitForBoot()) {
      USB_Printf("*** FATAL: BNO085 did not complete boot sequence. ***\r\n");
      USB_Printf("    Try power-cycling the sensor.\r\n");
      while (1) { HAL_Delay(1000); }
  }

  /* ---- Request and display Product ID ---- */
  if (!BNO085_RequestProductID()) {
      USB_Printf("*** WARNING: Could not read Product ID. Continuing ...\r\n");
  }

  /* ---- Enable all desired sensor reports ---- */
  BNO085_EnableAllReports();

  USB_Printf("[BNO085] Initialization complete. Streaming data ...\r\n");
  USB_Printf("--------------------------------------------------\r\n\r\n");

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    /* ---- Poll for SHTP packets ---- */
    uint16_t pktLen = 0;
    uint8_t  chan   = 0;
    HAL_StatusTypeDef st = SHTP_Read(&pktLen, &chan);

    if (st == HAL_OK && pktLen > SHTP_HEADER_SIZE) {
        BNO085_ProcessPacket(pktLen, chan);

        /* Check for sensor reset (executable channel reset message) */
        if (chan == CHAN_EXECUTABLE) {
            USB_Printf("[BNO085] Sensor was reset — re-enabling reports.\r\n");
            HAL_Delay(100);
            BNO085_EnableAllReports();
        }
    } else if (st == HAL_BUSY) {
        USB_Printf("[I2C] WARNING: I2C peripheral BUSY. Resetting ...\r\n");

        /* Attempt I2C recovery */
        HAL_I2C_DeInit(&hi2c2);
        HAL_Delay(10);
        MX_I2C2_Init();
    }

    /* Small delay to avoid hammering the bus when no data is ready.
     * BNO085 sends data every ~20 ms (50 Hz), so 5 ms poll is fine. */
    HAL_Delay(1500);
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
