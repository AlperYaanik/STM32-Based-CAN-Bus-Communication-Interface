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
#include "spi.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include "mcp2515.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

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
  MX_USART1_UART_Init();
  MX_SPI1_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE
   * BEGIN WHILE */
  while (1)
  {
	  /* Successful -> Output = CANCTRL = 0X87
	  //Mode Reading Test
	  uint8_t mode;
	  char buf[20];
	  uint8_t len;
	  mode = MCP2515_Read(MCP_CANCTRL);
	  len = (uint8_t)snprintf(buf, sizeof(buf), "CANCTRL=0x%02X\r\n", mode);
	  HAL_UART_Transmit(&huart1, (uint8_t*)buf, len, HAL_MAX_DELAY);
	  HAL_Delay(1000);
	  */
	  /*Successful  -> Output = CANCTRL =0X80
	  //Write Test
	  MCP2515_Reset();

	  MCP2515_Write(MCP_CANCTRL,0x80);

	  uint8_t value = MCP2515_Read(MCP_CANCTRL);
	  char buf[20];
	  uint8_t len = (uint8_t)snprintf(buf, sizeof(buf), "CANCTRL=0x%02X\r\n", value);
	  HAL_UART_Transmit(&huart1, (uint8_t*)buf, len, HAL_MAX_DELAY);
	  HAL_Delay(2000);
	   */
	  /* Successful -> Output  = Status = 0x00
	   //Status Test
	   uint8_t status;

	   status = MCP2515_ReadStatus();

	   char buf[20];
	   uint8_t len = (uint8_t)snprintf(buf, sizeof(buf), "Status=0x%02X\r\n", status);
	   HAL_UART_Transmit(&huart1, (uint8_t*)buf, len, HAL_MAX_DELAY);
	   HAL_Delay(2000);
	  	  */
	  /*
	   //Configuration Mode Test
	  uint8_t status = MCP2515_Read(MCP_CANSTAT);

	  char buf[30];
	  uint8_t len;
	  len = (uint8_t)snprintf(buf, sizeof(buf), "Status=0x%02X\r\n",status);
	  HAL_UART_Transmit(&huart1, (uint8_t*)buf, len, HAL_MAX_DELAY);

	  if(MCP2515_SetConfigurationMode())
	  {
	      len = (uint8_t)snprintf(buf, sizeof(buf), "Config Mode OK\r\n");
	  }
	  else
	  {
	      len = (uint8_t)snprintf(buf, sizeof(buf), "Config Mode FAIL\r\n");
	  }
	  HAL_UART_Transmit(&huart1, (uint8_t*)buf, len, HAL_MAX_DELAY);
	  HAL_Delay(2000);
	  */
	  /* Successful -> Output = Bitrate OK CNF1=0x00 CNF2=0x90 CNF3=0x02
	  //Set Bitrate Test
	  char buf[30];
	  uint8_t len;
	  if(MCP2515_SetBitrate(MCP2515_BITRATE_500KBPS))
	  {
	      len = (uint8_t)snprintf(buf, sizeof(buf), "Bitrate OK\r\n");
	  }
	  else
	  {
	      len = (uint8_t)snprintf(buf, sizeof(buf), "Bitrate FAILED\r\n");
	  }
	  HAL_UART_Transmit(&huart1, (uint8_t*)buf, len, HAL_MAX_DELAY);
	  HAL_Delay(1000);
	  len = (uint8_t)snprintf(buf, sizeof(buf), "CNF1=0x%02X\r\n", MCP2515_Read(MCP_CNF1));
	  HAL_UART_Transmit(&huart1, (uint8_t*)buf, len, HAL_MAX_DELAY);
	  HAL_Delay(1000);
	  len = (uint8_t)snprintf(buf, sizeof(buf), "CNF2=0x%02X\r\n", MCP2515_Read(MCP_CNF2));
	  HAL_UART_Transmit(&huart1, (uint8_t*)buf, len, HAL_MAX_DELAY);
	  HAL_Delay(1000);
	  len = (uint8_t)snprintf(buf, sizeof(buf), "CNF3=0x%02X\r\n", MCP2515_Read(MCP_CNF3));
	  HAL_UART_Transmit(&huart1, (uint8_t*)buf, len, HAL_MAX_DELAY);
	  HAL_Delay(1000);
	  */

	  /*
	   //Successful -> Output = 24 60 08
		// Tx Buffer Test
	  	  uint8_t txData[8] =
	  	  {
	  			  1,2,3,4,5,6,7,8
	  	  };

	  	  MCP2515_LoadTXBuffer(
	  			  0x123,
				  8,
				  txData
	  	  );

	  	  char buf[30];
	  	  uint8_t len;

	  	  len = (uint8_t)snprintf(buf, sizeof(buf), "%02X\r\n", MCP2515_Read(MCP_TXB0SIDH));
	  	  HAL_UART_Transmit(&huart1, (uint8_t*)buf, len, HAL_MAX_DELAY);
	  	  HAL_Delay(1000);
	  	  len= (uint8_t)snprintf(buf, sizeof(buf),"%02X\r\n",MCP2515_Read(MCP_TXB0SIDL));
	  	  HAL_UART_Transmit(&huart1, (uint8_t*)buf, len, HAL_MAX_DELAY);
	  	  HAL_Delay(1000);
	  	  len = (uint8_t)snprintf(buf, sizeof(buf), "%02X\r\n", MCP2515_Read(MCP_TXB0DLC));
	  	  HAL_UART_Transmit(&huart1, (uint8_t*)buf, len, HAL_MAX_DELAY);
	  	  HAL_Delay(1000);
	  	  */
		 // RST Test
	  MCP2515_Reset();
	  MCP2515_SetConfigurationMode();
	  MCP2515_SetBitrate(MCP2515_BITRATE_500KBPS);
	  MCP2515_SetLoopbackMode();

	  uint8_t txData[8] = {1, 2, 3, 4, 5, 6, 7, 8};
	  MCP2515_LoadTXBuffer(0x123, 8, txData);
	  MCP2515_RequestToSend();

	  HAL_Delay(10);  //Wait to  MCP2515 frame processing

	  MCP2515_Frame_t rxFrame;
	  if(MCP2515_ReadRXBuffer(&rxFrame))
	  {
	      char buf[30];
	      uint8_t len;

	      len = (uint8_t)snprintf(buf, sizeof(buf), "ID=0x%03X\r\n", rxFrame.id);
	      HAL_UART_Transmit(&huart1, (uint8_t*)buf, len, HAL_MAX_DELAY);
	      HAL_Delay(1000);
	      len = (uint8_t)snprintf(buf, sizeof(buf), "DLC=%d\r\n", rxFrame.dlc);
	      HAL_UART_Transmit(&huart1, (uint8_t*)buf, len, HAL_MAX_DELAY);
	      HAL_Delay(1000);
	      for(uint8_t i = 0; i < rxFrame.dlc; i++)
	      {
	          len = (uint8_t)snprintf(buf, sizeof(buf), "D%d=%d\r\n", i, rxFrame.data[i]);
	          HAL_UART_Transmit(&huart1, (uint8_t*)buf, len, HAL_MAX_DELAY);
	          HAL_Delay(1000);
	      }
	  }
	  else
	  {
	      char buf[30];
	      uint8_t len = (uint8_t)snprintf(buf, sizeof(buf), "RX FAILED\r\n");
	      HAL_UART_Transmit(&huart1, (uint8_t*)buf, len, HAL_MAX_DELAY);
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
