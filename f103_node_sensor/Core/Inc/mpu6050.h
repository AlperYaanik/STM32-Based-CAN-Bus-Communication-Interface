#ifndef __MPU6050_H__
#define __MPU6050_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdbool.h>

/* The 7-bit address is 0x68 with the AD0 pin tied to GND. The HAL_I2C_Mem_*
   functions expect the address already shifted left by one (8-bit form). */
#define MPU6050_I2C_ADDR (0x68 << 1)

bool MPU6050_Init(I2C_HandleTypeDef *hi2c);
bool MPU6050_ReadRaw(I2C_HandleTypeDef *hi2c, int16_t accel[3], int16_t gyro[3]);

#ifdef __cplusplus
}
#endif

#endif /* __MPU6050_H__ */
