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

/* Measures the gyroscope zero-rate offset and stores it, so that every later
   MPU6050_ReadRaw call returns bias-corrected gyro counts.

   THE BOARD MUST BE COMPLETELY STILL for the ~2.5 s this takes. Whatever the
   sensor reports while it runs is taken to be the definition of "not moving",
   so calibrating during movement bakes that movement in as a permanent error.
   To guard against that, the spread of the collected samples is checked and
   the measurement is rejected if any axis moved too much.

   Returns false if the I2C read failed or the board was not still enough; the
   stored bias is then left at zero, which is the same behaviour as before
   calibration rather than a wrong correction. */
bool MPU6050_CalibrateGyro(I2C_HandleTypeDef *hi2c);

/* The currently stored gyro bias in raw LSB, for logging. All zero until
   MPU6050_CalibrateGyro has succeeded. */
void MPU6050_GetGyroBias(int16_t bias[3]);

/* Reads all six axes in a single burst.

   "Raw" refers to the units: these are sensor counts, not engineering units.
   The accelerometer values are exactly what the chip reported. The gyroscope
   values have the calibrated bias subtracted, and are therefore identical to
   the chip's output until MPU6050_CalibrateGyro has run. */
bool MPU6050_ReadRaw(I2C_HandleTypeDef *hi2c, int16_t accel[3], int16_t gyro[3]);

#ifdef __cplusplus
}
#endif

#endif /* __MPU6050_H__ */
