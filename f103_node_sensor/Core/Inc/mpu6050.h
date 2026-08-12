#ifndef __MPU6050_H__
#define __MPU6050_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdbool.h>

/* AD0 pin GND'ye bağlıyken 7-bit adres 0x68'dir. HAL_I2C_Mem_* fonksiyonları
   adresi <<1 kaydırılmış (8-bit) haliyle bekler. */
#define MPU6050_I2C_ADDR (0x68 << 1)

bool MPU6050_Init(I2C_HandleTypeDef *hi2c);
bool MPU6050_ReadRaw(I2C_HandleTypeDef *hi2c, int16_t accel[3], int16_t gyro[3]);

#ifdef __cplusplus
}
#endif

#endif /* __MPU6050_H__ */
