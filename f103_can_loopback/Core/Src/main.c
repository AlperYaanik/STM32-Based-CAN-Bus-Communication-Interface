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
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
#define CAN_TIMEOUT_MS 100
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
  MX_CAN_Init();
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
    canFilterConfig.FilterIdHigh = 0x0000;
    canFilterConfig.FilterIdLow= 0x0000;
    canFilterConfig.FilterMaskIdHigh = 0x0000;
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
    if(HAL_CAN_Start(&hcan)!=HAL_OK)
    {
      Error_Handler();
    }


    //Message Envelope Tx Configuration
    CAN_TxHeaderTypeDef TxHeader ={0};
    uint32_t TxMailbox;
    //We can send 8 bytes of data
    uint8_t TxData[8];
    //Standard ID (11 bits) for the message, 0x123 is a common ID for testing
    TxHeader.StdId = 0x123;
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
    TxData[0]=1;
    TxData[1]=2;
    TxData[2]=3;
    TxData[3]=4;
    TxData[4]=5;
    TxData[5]=6;
    TxData[6]=7;
    TxData[7]=8;


    //Message Envelope Rx Configuration
    CAN_RxHeaderTypeDef RxHeader ={0};
    //Received Bytes
    uint8_t RxData[8];

    //Create Tick Variable for Timeout Detection
    uint32_t tick = HAL_GetTick();
    //Create Message Variable for UART Message
    char msg[64];
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {

	   //Check how many mailboxes are free (returns 0-3), Use to tick for detecting timeout
	   tick= HAL_GetTick();
	   while(HAL_CAN_GetTxMailboxesFreeLevel(&hcan)==0)
	   {
	     if(HAL_GetTick()-tick>CAN_TIMEOUT_MS)
	     {
	       Error_Handler();
	     }
	   }
	   //Send the message
	   if (HAL_CAN_AddTxMessage(&hcan, &TxHeader, TxData, &TxMailbox) != HAL_OK)
	   {
	     Error_Handler();
	   }
	 //Check if the message is still pending in the mailbox
	   while(HAL_CAN_IsTxMessagePending(&hcan, TxMailbox));
	   //Wait until a message is received, Use to tick for detecting timeout
	   tick = HAL_GetTick();
	   while(HAL_CAN_GetRxFifoFillLevel(&hcan, CAN_RX_FIFO0)==0)
	   {
	     if(HAL_GetTick()-tick>CAN_TIMEOUT_MS)
	     {
	       Error_Handler();
	     }
	   }
	   //Read the frame
	   if(HAL_CAN_GetRxMessage(&hcan, CAN_RX_FIFO0, &RxHeader, RxData)!=HAL_OK)
	   {
	     Error_Handler();
	   }
	   //Transmit the received data over UART for debugging purposes
	   snprintf(msg, sizeof(msg),"Received Data: %d %d %d %d %d %d %d %d\r\n",
	    RxData[0], RxData[1], RxData[2], RxData[3], RxData[4], RxData[5], RxData[6], RxData[7]);
	   HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);
	   //Wait 2 seconds for system operability
	   HAL_Delay(2000);
	   //Increase data value
	   TxData[0]++;

	   //Detect If UART is Working
	   //HAL_Delay(1000);
	   //char msg_2[] = "UART OK\r\n";
	   //HAL_UART_Transmit(&huart1, (uint8_t *)msg_2, strlen(msg_2), HAL_MAX_DELAY);

	   // Detect IF Led Is Working
	   //HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
	   //HAL_Delay(2000);
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
