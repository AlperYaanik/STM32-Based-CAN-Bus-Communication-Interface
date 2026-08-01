/*
 * test_system.h
 *
 *  Created on: Jul 27, 2026
 *      Author: alper
 */

#ifndef APPLICATION_USER_CORE_TEST_SYSTEM_H_
#define APPLICATION_USER_CORE_TEST_SYSTEM_H_

#include "main.h"

void TEST_UART(UART_HandleTypeDef *huart, char *msg);
void TEST_LED(GPIO_TypeDef *GPIOx, uint16_t pin, uint32_t delay_ms);

#endif /* APPLICATION_USER_CORE_TEST_SYSTEM_H_ */
