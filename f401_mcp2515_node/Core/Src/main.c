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
#include "i2c.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "mcp2515.h"
#include "can_protocol.h"
#include "debug.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* If no frame arrives for this long, dump the bus state (error counters).
   The F103 sends at 10 Hz, so a healthy link delivers a frame every 100 ms. */
#define LINK_SILENCE_MS   1000u

/* ---- Rate experiment knobs --------------------------------------------
   Per-frame UART line, about 3.5 ms at 115200 baud. While it is in flight
   this loop is not draining the MCP2515, whose receive buffers hold only two
   frames. Set to 0 to keep the same traffic without the printing and see
   whether the overflows disappear. The once-a-second summary is printed
   either way. */
#define LOG_EVERY_FRAME   1

#define STATS_PERIOD_MS   1000u
/* ----------------------------------------------------------------------- */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void PrintFrame(const MCP2515_Frame_t *frame)
{
  int16_t axes[3];

  /* Does the frame match the contract? A known ID carrying an unexpected DLC
     means the sender changed; print that distinguishably instead of silently
     decoding it wrong. */
  if (frame->dlc == CAN_AXES_DLC &&
      (frame->id == CAN_ID_ACCEL || frame->id == CAN_ID_GYRO))
  {
    CAN_UnpackAxes(frame->data, axes);

    LOG("%s  x=%6d  y=%6d  z=%6d\r\n",
        (frame->id == CAN_ID_ACCEL) ? "ACCEL" : "GYRO ",
        axes[0], axes[1], axes[2]);
  }
  else
  {
    /* Unknown frame: dump it raw so a protocol mismatch is visible. */
    LOG("RAW   ID=0x%03X DLC=%u  %02X %02X %02X %02X %02X %02X %02X %02X\r\n",
        (unsigned int)frame->id, (unsigned int)frame->dlc,
        frame->data[0], frame->data[1], frame->data[2], frame->data[3],
        frame->data[4], frame->data[5], frame->data[6], frame->data[7]);
  }
}

static void PrintBusDiagnostics(void)
{
  /* EFLG carries the RX overflow / error-passive / bus-off flags, TEC and REC
     are the error counters. If all of them read zero and still no frame shows
     up, the bus itself is fine and the sender simply is not transmitting. */
  LOG("waiting... CANSTAT=0x%02X EFLG=0x%02X TEC=%u REC=%u\r\n",
      (unsigned int)MCP2515_Read(MCP_CANSTAT),
      (unsigned int)MCP2515_Read(MCP_EFLG),
      (unsigned int)MCP2515_Read(MCP_TEC),
      (unsigned int)MCP2515_Read(MCP_REC));
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
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  MX_SPI1_Init();
  /* USER CODE BEGIN 2 */
  LOG("\r\n=== f401 MCP2515 receiver node ===\r\n");

  /* The bring-up sequence runs exactly once. It used to sit inside while(1),
     which meant resetting and reconfiguring the chip on every pass: frames
     arriving at that moment were lost, and for ~10 ms the node did not
     acknowledge anything on the bus. */
  if (!MCP2515_Init(MCP2515_BITRATE_500KBPS))
  {
    LOG("MCP2515 init FAILED - CANSTAT=0x%02X\r\n",
        (unsigned int)MCP2515_Read(MCP_CANSTAT));
    LOG("check: SPI wiring (PA4/5/6/7), supply, 8MHz crystal, CAN bus\r\n");
    Error_Handler();
  }

  LOG("MCP2515 ready - 500 kbit/s, normal mode, listening for 0x%03X/0x%03X\r\n",
      (unsigned int)CAN_ID_ACCEL, (unsigned int)CAN_ID_GYRO);

  uint32_t lastFrameTick = HAL_GetTick();
  uint32_t statsTick = HAL_GetTick();
  uint32_t framesReceived = 0;
  uint32_t overflows = 0;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* Only dlc bytes of data[] get filled in, so zero the struct to keep the
       RAW dump from showing garbage in the untouched bytes. */
    MCP2515_Frame_t frame = {0};

    if (MCP2515_Receive(&frame))
    {
      lastFrameTick = HAL_GetTick();

      /* Toggle the LED on every frame: the link can then be seen to be alive
         even when nothing is watching the UART. */
      HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);

      framesReceived++;

#if LOG_EVERY_FRAME
      PrintFrame(&frame);
#endif
    }
    else if ((HAL_GetTick() - lastFrameTick) > LINK_SILENCE_MS)
    {
      PrintBusDiagnostics();
      lastFrameTick = HAL_GetTick();
    }

    /* Ask the controller whether it had to throw anything away. This is not
       an estimate: EFLG_RXnOVR is set by the hardware itself when a frame
       arrives and both receive buffers are already full, which is exactly the
       failure this experiment is looking for. */
    if (MCP2515_ReadAndClearOverflow() != 0u)
    {
      overflows++;
    }

    uint32_t elapsed = HAL_GetTick() - statsTick;
    if (elapsed >= STATS_PERIOD_MS)
    {
      /* Two frames per sample, so the sample rate is half the frame rate. */
      uint32_t frameTenths = (framesReceived * 10000u) / elapsed;

      LOG("[rx] %u.%u frames/s (%u.%u samples/s), overflow=%u\r\n",
          (unsigned int)(frameTenths / 10u), (unsigned int)(frameTenths % 10u),
          (unsigned int)(frameTenths / 20u), (unsigned int)((frameTenths / 2u) % 10u),
          (unsigned int)overflows);

      framesReceived = 0;
      overflows = 0;
      statsTick = HAL_GetTick();
    }
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
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
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
