/*
 * test_system.c
 *
 *  Created on: Jul 27, 2026
 *      Author: alper
 */
#include <string.h>
#include <stdio.h>

#include "test_system.h"

void Test_UART(UART_HandleTypeDef *huart, char *msg)
{
	HAL_UART_Transmit(huart,
			(uint8_t *)msg,
			strlen(msg),
			HAL_MAX_DELAY);
}

void Test_LED(GPIO_TypeDef *GPIOx,
		uint16_t GPIO_Pin,
		uint32_t delay_ms)
{
	HAL_GPIO_TogglePin(GPIOx, GPIO_Pin);
	HAL_Delay(delay_ms);
}
