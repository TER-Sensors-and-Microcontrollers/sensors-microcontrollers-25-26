/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : BNO055 IMU Communication Test
  *                   Lights up LED when accelerometer detects movement
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
// BNO055 I2C Address (ADR pin to GND = 0x28, 7-bit address)
// HAL uses 8-bit address format, so we shift left by 1
#define BNO055_I2C_ADDR         (0x28 << 1)  // = 0x50
// BNO055 Register Addresses
#define BNO055_CHIP_ID_REG      0x00
#define BNO055_OPR_MODE_REG     0x3D
#define BNO055_PWR_MODE_REG     0x3E
#define BNO055_SYS_TRIGGER_REG  0x3F
// Accelerometer data registers (LSB first)
#define BNO055_ACC_DATA_X_LSB   0x08
#define BNO055_ACC_DATA_X_MSB   0x09
#define BNO055_ACC_DATA_Y_LSB   0x0A
#define BNO055_ACC_DATA_Y_MSB   0x0B
#define BNO055_ACC_DATA_Z_LSB   0x0C
#define BNO055_ACC_DATA_Z_MSB   0x0D
// Expected chip ID
#define BNO055_CHIP_ID          0xA0
// Operating modes
#define BNO055_OPR_MODE_CONFIG  0x00
#define BNO055_OPR_MODE_ACCONLY 0x01
#define BNO055_OPR_MODE_NDOF    0x0C
// Power modes
#define BNO055_PWR_MODE_NORMAL  0x00
// Movement detection threshold
// Accelerometer returns data in m/s² * 100 (1 LSB = 0.01 m/s²)
// At rest, Z ≈ 980 (gravity ≈ 9.8 m/s²), X and Y ≈ 0
// We'll detect if X or Y deviates significantly from 0
#define ACCEL_THRESHOLD         50  // About 0.5 m/s² change triggers LED
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

COM_InitTypeDef BspCOMInit;
I2C_HandleTypeDef hi2c4;

/* USER CODE BEGIN PV */
// Store baseline (rest) accelerometer values
int16_t accel_baseline_x = 0;
int16_t accel_baseline_y = 0;
int16_t accel_baseline_z = 0;
uint8_t baseline_captured = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C4_Init(void);
/* USER CODE BEGIN PFP */
// BNO055 helper functions
HAL_StatusTypeDef BNO055_ReadRegister(uint8_t reg, uint8_t *data);
HAL_StatusTypeDef BNO055_WriteRegister(uint8_t reg, uint8_t data);
HAL_StatusTypeDef BNO055_Init(void);
HAL_StatusTypeDef BNO055_ReadAccelerometer(int16_t *x, int16_t *y, int16_t *z);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/**
  * @brief  Read a single register from BNO055
  */
HAL_StatusTypeDef BNO055_ReadRegister(uint8_t reg, uint8_t *data)
{
    return HAL_I2C_Mem_Read(&hi2c4, BNO055_I2C_ADDR, reg,
                            I2C_MEMADD_SIZE_8BIT, data, 1, HAL_MAX_DELAY);
}
/**
  * @brief  Write a single register to BNO055
  */
HAL_StatusTypeDef BNO055_WriteRegister(uint8_t reg, uint8_t data)
{
    return HAL_I2C_Mem_Write(&hi2c4, BNO055_I2C_ADDR, reg,
                             I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
}
/**
  * @brief  Initialize BNO055 sensor
  * @retval HAL_OK if successful, HAL_ERROR otherwise
  */
HAL_StatusTypeDef BNO055_Init(void)
{
    uint8_t chip_id = 0;
    HAL_StatusTypeDef status;
    // Small delay for BNO055 to boot up after power-on
    HAL_Delay(700);
    // Read and verify chip ID
    status = BNO055_ReadRegister(BNO055_CHIP_ID_REG, &chip_id);
    if (status != HAL_OK)
    {
        return HAL_ERROR;  // I2C communication failed
    }
    if (chip_id != BNO055_CHIP_ID)
    {
        return HAL_ERROR;  // Wrong chip ID - not a BNO055
    }
    // Set to CONFIG mode first (required before changing settings)
    status = BNO055_WriteRegister(BNO055_OPR_MODE_REG, BNO055_OPR_MODE_CONFIG);
    if (status != HAL_OK) return HAL_ERROR;
    HAL_Delay(25);  // Wait for mode switch
    // Reset the sensor (optional but recommended)
    status = BNO055_WriteRegister(BNO055_SYS_TRIGGER_REG, 0x20);
    if (status != HAL_OK) return HAL_ERROR;
    HAL_Delay(700);  // Wait for reset to complete
    // Verify chip ID again after reset
    status = BNO055_ReadRegister(BNO055_CHIP_ID_REG, &chip_id);
    if (status != HAL_OK || chip_id != BNO055_CHIP_ID)
    {
        return HAL_ERROR;
    }
    // Set power mode to Normal
    status = BNO055_WriteRegister(BNO055_PWR_MODE_REG, BNO055_PWR_MODE_NORMAL);
    if (status != HAL_OK) return HAL_ERROR;
    HAL_Delay(10);
    // Set operating mode to ACCONLY (accelerometer only - simplest mode)
    // You could also use NDOF for full sensor fusion
    status = BNO055_WriteRegister(BNO055_OPR_MODE_REG, BNO055_OPR_MODE_ACCONLY);
    if (status != HAL_OK) return HAL_ERROR;
    HAL_Delay(25);  // Wait for mode switch
    return HAL_OK;
}
/**
  * @brief  Read accelerometer X, Y, Z data
  * @Param Upadhyay  x, y, z: pointers to store accelerometer values
  * @retval HAL_OK if successful
  */
HAL_StatusTypeDef BNO055_ReadAccelerometer(int16_t *x, int16_t *y, int16_t *z)
{
    uint8_t buffer[6];
    HAL_StatusTypeDef status;
    // Read all 6 bytes starting from ACC_DATA_X_LSB
    status = HAL_I2C_Mem_Read(&hi2c4, BNO055_I2C_ADDR, BNO055_ACC_DATA_X_LSB,
                              I2C_MEMADD_SIZE_8BIT, buffer, 6, HAL_MAX_DELAY);
    if (status == HAL_OK)
    {
        // Combine LSB and MSB for each axis (little-endian)
        *x = (int16_t)((buffer[1] << 8) | buffer[0]);
        *y = (int16_t)((buffer[3] << 8) | buffer[2]);
        *z = (int16_t)((buffer[5] << 8) | buffer[4]);
    }
    return status;
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
    int16_t accel_x, accel_y, accel_z;
    uint8_t bno055_ready = 0;
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
  MX_I2C4_Init();
  /* USER CODE BEGIN 2 */
    // Initialize BNO055
    if (BNO055_Init() == HAL_OK)
    {
        bno055_ready = 1;
        // Blink LED 3 times to indicate successful initialization
        for (int i = 0; i < 3; i++)
        {
            HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_SET);
            HAL_Delay(200);
            HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_RESET);
            HAL_Delay(200);
        }
        // Wait a moment, then capture baseline accelerometer readings
        HAL_Delay(500);
        if (BNO055_ReadAccelerometer(&accel_baseline_x, &accel_baseline_y, &accel_baseline_z) == HAL_OK)
        {
            baseline_captured = 1;
        }
    }
    else
    {
        // Initialization failed - keep LED on solid to indicate error
        HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);
    }
  /* USER CODE END 2 */

  /* Initialize led */
  BSP_LED_Init(LED_GREEN);

  /* Initialize USER push-button, will be used to trigger an interrupt each time it's pressed.*/
  BSP_PB_Init(BUTTON_USER, BUTTON_MODE_EXTI);

  /* Initialize COM1 port (115200, 8 bits (7-bit data + 1 stop bit), no parity */
  BspCOMInit.BaudRate   = 115200;
  BspCOMInit.WordLength = COM_WORDLENGTH_8B;
  BspCOMInit.StopBits   = COM_STOPBITS_1;
  BspCOMInit.Parity     = COM_PARITY_NONE;
  BspCOMInit.HwFlowCtl  = COM_HWCONTROL_NONE;
  if (BSP_COM_Init(COM1, &BspCOMInit) != BSP_ERROR_NONE)
  {
    Error_Handler();
  }

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
    while (1)
    {
        if (bno055_ready && baseline_captured)
        {
            // Read current accelerometer values
            if (BNO055_ReadAccelerometer(&accel_x, &accel_y, &accel_z) == HAL_OK)
            {
                // Calculate deviation from baseline
                int16_t diff_x = abs(accel_x - accel_baseline_x);
                int16_t diff_y = abs(accel_y - accel_baseline_y);
                int16_t diff_z = abs(accel_z - accel_baseline_z);
                // Check if any axis exceeds threshold (movement detected)
                if (diff_x > ACCEL_THRESHOLD ||
                    diff_y > ACCEL_THRESHOLD ||
                    diff_z > ACCEL_THRESHOLD)
                {
                    // Movement detected! Turn LED ON
                    HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);
                }
                else
                {
                    // No movement - Turn LED OFF
                    HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
                }
            }
        }
        // Small delay to prevent overwhelming the I2C bus
        HAL_Delay(50);
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
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV4;
  RCC_OscInitStruct.PLL.PLLN = 85;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
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

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C4_Init(void)
{

  /* USER CODE BEGIN I2C4_Init 0 */

  /* USER CODE END I2C4_Init 0 */

  /* USER CODE BEGIN I2C4_Init 1 */

  /* USER CODE END I2C4_Init 1 */
  hi2c4.Instance = I2C4;
  hi2c4.Init.Timing = 0x40B285C2;
  hi2c4.Init.OwnAddress1 = 0;
  hi2c4.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c4.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c4.Init.OwnAddress2 = 0;
  hi2c4.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c4.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c4.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c4) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c4, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c4, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C4_Init 2 */

  /* USER CODE END I2C4_Init 2 */

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
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);

  /*Configure GPIO pin : PA5 */
  GPIO_InitStruct.Pin = GPIO_PIN_5;
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
