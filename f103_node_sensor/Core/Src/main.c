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
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define IMU_CAN_ID    0x101
#define NODE2_CAN_ID  0x200
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
//Message Envelope Tx and Rx Configuration
CAN_TxHeaderTypeDef TxHeader = {0};
CAN_RxHeaderTypeDef RxHeader = {0};
//Set 8 bytes data variables
uint8_t TxData[8];
uint8_t RxData[8];
//Set Tx Mailbox Variable
uint32_t TxMailbox;
//Create Message Variable for UART Message
char msg[64];
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
  MX_CAN_Init();
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  //Can Filter Configuration
  //Loopback Mode Filter Configuration and Selecting Filter Bank (0-13)
  CAN_FilterTypeDef canFilterConfig;
  canFilterConfig.FilterBank = 0;
  //ID Mask Mode (for easy usages , ID list for betwwen specified intervals) and 32-bit scale configuration (for standard and extended IDs)
  canFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
  canFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
  //Accept All Id (ie. 0x123<<5 for IMU sensor)
  canFilterConfig.FilterIdHigh = (IMU_CAN_ID<<5);
  canFilterConfig.FilterIdLow= 0x0000;
  canFilterConfig.FilterMaskIdHigh = (0x7FF<<5);
  canFilterConfig.FilterMaskIdLow = 0x0000;
  //Convention, one FIFO is enough for loopback mode, so we can use FIFO0
  canFilterConfig.FilterFIFOAssignment = CAN_FILTER_FIFO0;
  //Must to ENABLE Filter Activation, otherwise the filter will not work
  canFilterConfig.FilterActivation = ENABLE;
  //Have to configure for Dual CAN board. STM32F103 has only one, but have to filled because of the structure.
  //So, we can use 14 for SlaveStartFilterBank (0-13 for Master, 14-27 for Slave)
  canFilterConfig.SlaveStartFilterBank = 14;
  //Error Handling (HAL_OK = 0, HAL_ERROR = 1, HAL_BUSY = 2, HAL_TIMEOUT = 3), important for debugging
  if (HAL_CAN_ConfigFilter(&hcan, &canFilterConfig) != HAL_OK)
  	  {
	  	  Error_Handler();
  	  }
  //Start CAN Peripheral
  if(HAL_CAN_Start(&hcan) != HAL_OK)
  	  {
      	  Error_Handler();
  	  }
  //If RX Fifo Fill level is equals to zero get the message
  if(HAL_CAN_ActivateNotification(&hcan,CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK)
  	  {
	  	  Error_Handler();
  	  }
  //Standard ID (11 bits) for the message
  TxHeader.StdId = NODE2_CAN_ID;
  //Extended ID (29 bits) is not used, so we can set it to 0
  TxHeader.ExtId = 0x00;
  //Standard ID is used, so we can set the IDE to CAN_ID_STD
  TxHeader.IDE = CAN_ID_STD;
  //Data Frame is used, so we can set the RTR to CAN_RTR_DATA
  TxHeader.RTR = CAN_RTR_DATA;
  //Selecting Bytes (Data Length Code), we send 8 bytes of data so we can set the DLC to 8
  TxHeader.DLC = 8;
  //Disable the Transmit Global Time, we don't need it for this example
  TxHeader.TransmitGlobalTime = DISABLE;
  //Data to be sent, we can fill the TxData array with some values


  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
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
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
	if(HAL_CAN_GetRxMessage(
			hcan,
			CAN_RX_FIFO0,
			&RxHeader,
			RxData) != HAL_OK)
		{
			Error_Handler();
		}

	if(RxHeader.StdId != IMU_CAN_ID)
		{
		return;
		}
	// IMU Packet
	memcpy(TxData, RxData, sizeof(TxData));
	if(HAL_CAN_GetTxMailboxesFreeLevel(hcan)>0)
	    {
	    			if(HAL_CAN_AddTxMessage(hcan, &TxHeader, TxData, &TxMailbox)!=HAL_OK)
	    			{
						  	  	  Error_Handler();
					  	  	  }
	    	 	  	  }
	    	 	 else
	    	 	 	 {
	    	 	     	 droppedPackets++;
	    	 	 	 }
	      	 }

	  }
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
