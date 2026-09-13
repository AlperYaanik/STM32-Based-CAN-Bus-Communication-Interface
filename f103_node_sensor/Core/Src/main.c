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
#include "can.h"
#include "i2c.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include "debug.h"
#include "mpu6050.h"
#include "can_protocol.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* CAN_ID_ACCEL / CAN_ID_GYRO / CAN_AXES_DLC now live in can_protocol.h,
   the file this node shares with the receiver (f401_mcp2515_node). */
#define CAN_TIMEOUT_MS    100

/* ---- Rate experiment knobs --------------------------------------------
   Requested sampling period. 100 ms = 10 Hz is the normal setting; drop it
   to 10 or 5 to push the loop until it can no longer keep up. */
#define SAMPLE_PERIOD_MS  0

/* Per-sample UART line. This is the expensive part - about 3.5 ms at 115200
   baud - and it sits directly in the sampling path. Set to 0 to run the same
   rate without it and see how much of the shortfall it was responsible for.
   The once-a-second summary is printed either way. */
#define LOG_EVERY_SAMPLE  0

/* Order in which the two frames of each sample are handed to the controller.
   0 = accel then gyro (normal), 1 = gyro then accel.

   The bus is idle when the first frame is queued, so it starts transmitting
   at once and the order on the wire follows the call order. Under receiver
   saturation one message type survives ~90% of the time; swapping the order
   tells whether that follows a frame's position in the pair (a timing
   effect) or its identifier. */
#define SEND_GYRO_FIRST   0

#define STATS_PERIOD_MS   1000u
/* ----------------------------------------------------------------------- */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
CAN_TxHeaderTypeDef TxHeader = {0};
uint8_t TxData[8];
uint32_t TxMailbox;

/* Rate-experiment counters, reset every STATS_PERIOD_MS. */
static uint32_t samples;
static uint32_t txFailures;
static uint32_t readFailures;
static uint32_t statsTick;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* Returns false when the frame could not be handed to the controller, so the
   caller can count losses instead of only watching them scroll past. */
static bool CAN_SendFrame(uint32_t stdId, const uint8_t *data, uint8_t dlc)
{
  uint32_t tick = HAL_GetTick();

  while (HAL_CAN_GetTxMailboxesFreeLevel(&hcan) == 0)
  {
    if (HAL_GetTick() - tick > CAN_TIMEOUT_MS)
    {
      LOG("CAN TX timeout (id=0x%X)\r\n", (unsigned int)stdId);
      return false;
    }
  }

  TxHeader.StdId = stdId;
  TxHeader.DLC = dlc;
  memcpy(TxData, data, dlc);

  if (HAL_CAN_AddTxMessage(&hcan, &TxHeader, TxData, &TxMailbox) != HAL_OK)
  {
    LOG("CAN TX failed (id=0x%X)\r\n", (unsigned int)stdId);
    return false;
  }

  return true;
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
  MX_CAN_Init();
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  // CAN filter setup. This node only transmits, so a filter is not needed
  // for reception; it is harmless and follows the CubeMX convention.
  CAN_FilterTypeDef canFilterConfig;
  canFilterConfig.FilterBank = 0;
  canFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
  canFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
  canFilterConfig.FilterIdHigh = 0x0000;
  canFilterConfig.FilterIdLow = 0x0000;
  canFilterConfig.FilterMaskIdHigh = 0x0000;
  canFilterConfig.FilterMaskIdLow = 0x0000;
  canFilterConfig.FilterFIFOAssignment = CAN_FILTER_FIFO0;
  canFilterConfig.FilterActivation = ENABLE;
  canFilterConfig.SlaveStartFilterBank = 14;
  if (HAL_CAN_ConfigFilter(&hcan, &canFilterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_CAN_Start(&hcan) != HAL_OK)
  {
    Error_Handler();
  }

  TxHeader.ExtId = 0x00;
  TxHeader.IDE = CAN_ID_STD;
  TxHeader.RTR = CAN_RTR_DATA;
  TxHeader.TransmitGlobalTime = DISABLE;

  if (!MPU6050_Init(&hi2c1))
  {
    LOG("MPU6050 init FAILED (WHO_AM_I mismatch or I2C error)\r\n");
    Error_Handler();
  }
  LOG("MPU6050 OK\r\n");

  /* Calibration failure is not fatal: the bias stays at zero, the node keeps
     streaming, and the log says the data is uncorrected. Halting here would
     trade a known accuracy loss for a dead node. */
  LOG("calibrating gyro - keep the board still...\r\n");

  bool calibrated = MPU6050_CalibrateGyro(&hi2c1);
  int16_t bias[3];
  int16_t spread[3];
  MPU6050_GetGyroBias(bias);
  MPU6050_GetGyroCalSpread(spread);

  /* The spread is printed either way. It says how still the board actually
     was, which is the thing that decides whether the bias below is worth
     anything - a hand-held or wire-tugged board cannot be calibrated well no
     matter what the software does. */
  LOG("  movement (peak-to-peak) = %d,%d,%d LSB\r\n", spread[0], spread[1], spread[2]);

  if (calibrated)
  {
    LOG("  gyro bias = %d,%d,%d LSB\r\n", bias[0], bias[1], bias[2]);
  }
  else
  {
    LOG("  calibration REFUSED - board not still, bias left at 0\r\n");
  }
  /* USER CODE END 2 */

  statsTick = HAL_GetTick();

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    int16_t accel[3];
    int16_t gyro[3];

    if (MPU6050_ReadRaw(&hi2c1, accel, gyro))
    {
      uint8_t accelData[CAN_AXES_DLC];
      uint8_t gyroData[CAN_AXES_DLC];

      /* Packing lives in the shared header now; CAN_UnpackAxes on the
         receiving side is its exact inverse, and the two change together. */
      CAN_PackAxes(accel, accelData);
      CAN_PackAxes(gyro, gyroData);

#if SEND_GYRO_FIRST
      if (!CAN_SendFrame(CAN_ID_GYRO, gyroData, sizeof(gyroData)))
      {
        txFailures++;
      }
      if (!CAN_SendFrame(CAN_ID_ACCEL, accelData, sizeof(accelData)))
      {
        txFailures++;
      }
#else
      if (!CAN_SendFrame(CAN_ID_ACCEL, accelData, sizeof(accelData)))
      {
        txFailures++;
      }
      if (!CAN_SendFrame(CAN_ID_GYRO, gyroData, sizeof(gyroData)))
      {
        txFailures++;
      }
#endif

      samples++;

#if LOG_EVERY_SAMPLE
      LOG("accel=%d,%d,%d gyro=%d,%d,%d\r\n",
          accel[0], accel[1], accel[2], gyro[0], gyro[1], gyro[2]);
#endif
    }
    else
    {
      readFailures++;
      LOG("MPU6050 read FAILED\r\n");
    }

    /* Once a second, report what the loop ACTUALLY achieved rather than what
       it was asked for. The gap between the two is the point of the whole
       experiment: HAL_Delay waits SAMPLE_PERIOD_MS *on top of* however long
       sampling, transmitting and logging took, so the real period is always
       longer than requested - and it varies, because a longer line of digits
       takes longer to push out of the UART. */
    uint32_t elapsed = HAL_GetTick() - statsTick;
    if (elapsed >= STATS_PERIOD_MS)
    {
      /* Tenths of Hz in integer arithmetic: newlib-nano has no %f by default
         and pulling in floating-point printf for one diagnostic line is not
         a trade worth making. */
      uint32_t achievedTenths = (samples * 10000u) / elapsed;
#if SAMPLE_PERIOD_MS > 0
      uint32_t requestedTenths = 10000u / SAMPLE_PERIOD_MS;

      LOG("[tx] achieved %u.%u Hz of %u.%u Hz, txfail=%u rdfail=%u\r\n",
          (unsigned int)(achievedTenths / 10u), (unsigned int)(achievedTenths % 10u),
          (unsigned int)(requestedTenths / 10u), (unsigned int)(requestedTenths % 10u),
          (unsigned int)txFailures, (unsigned int)readFailures);
#else
      /* A zero period means "run flat out", so there is no requested rate to
         compare against - and computing one would divide by zero, which is
         undefined behaviour in C (on this core it silently yields 0). */
      LOG("[tx] achieved %u.%u Hz free-running, txfail=%u rdfail=%u\r\n",
          (unsigned int)(achievedTenths / 10u), (unsigned int)(achievedTenths % 10u),
          (unsigned int)txFailures, (unsigned int)readFailures);
#endif

      samples = 0;
      txFailures = 0;
      readFailures = 0;
      statsTick = HAL_GetTick();
    }

    HAL_Delay(SAMPLE_PERIOD_MS);
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

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI_DIV2;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL16;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
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
