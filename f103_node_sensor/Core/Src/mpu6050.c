#include "mpu6050.h"

#define MPU6050_REG_PWR_MGMT_1   0x6B
#define MPU6050_REG_WHO_AM_I     0x75
#define MPU6050_REG_ACCEL_XOUT_H 0x3B
/* Most boards sold as GY-521 no longer carry a real MPU6050 but an MPU6500,
   which has the same register map - only the WHO_AM_I value differs. */
#define MPU6050_WHO_AM_I_MPU6050 0x68
#define MPU6050_WHO_AM_I_MPU6500 0x70

/* A finite timeout instead of HAL_MAX_DELAY: if the sensor never acknowledges
   (loose wire, SDA/SCL not connected, address 0x69 because of AD0) the call
   blocks forever and the "read FAILED" branch in main.c can never run - the
   board just looks dead. 100 ms is plenty for a 14-byte burst at 400 kHz. */
#define MPU6050_I2C_TIMEOUT_MS 100u

/* After clearing SLEEP the chip needs time to settle its internal oscillator;
   until then the first samples can be garbage. The datasheet suggests about
   100 ms for start-up. */
#define MPU6050_WAKEUP_DELAY_MS 100u

static int16_t Combine(uint8_t hi, uint8_t lo)
{
  return (int16_t)((hi << 8) | lo);
}

bool MPU6050_Init(I2C_HandleTypeDef *hi2c)
{
  /* The chip powers up with the SLEEP bit set (PWR_MGMT_1 bit 6). Until it is
     cleared no register updates, and ReadRaw silently returns all zeros. */
  uint8_t wake = 0x00;
  if (HAL_I2C_Mem_Write(hi2c, MPU6050_I2C_ADDR, MPU6050_REG_PWR_MGMT_1,
                         I2C_MEMADD_SIZE_8BIT, &wake, 1, MPU6050_I2C_TIMEOUT_MS) != HAL_OK)
  {
    return false;
  }

  HAL_Delay(MPU6050_WAKEUP_DELAY_MS);

  uint8_t whoAmI = 0;
  if (HAL_I2C_Mem_Read(hi2c, MPU6050_I2C_ADDR, MPU6050_REG_WHO_AM_I,
                        I2C_MEMADD_SIZE_8BIT, &whoAmI, 1, MPU6050_I2C_TIMEOUT_MS) != HAL_OK)
  {
    return false;
  }

  return (whoAmI == MPU6050_WHO_AM_I_MPU6050) || (whoAmI == MPU6050_WHO_AM_I_MPU6500);
}

bool MPU6050_ReadRaw(I2C_HandleTypeDef *hi2c, int16_t accel[3], int16_t gyro[3])
{
  uint8_t raw[14];

  /* A single burst starting at ACCEL_XOUT_H: accel(6) + temp(2) + gyro(6).
     One transaction instead of three separate reads, so the accel and gyro
     samples belong to the same instant. */
  if (HAL_I2C_Mem_Read(hi2c, MPU6050_I2C_ADDR, MPU6050_REG_ACCEL_XOUT_H,
                        I2C_MEMADD_SIZE_8BIT, raw, sizeof(raw), MPU6050_I2C_TIMEOUT_MS) != HAL_OK)
  {
    return false;
  }

  accel[0] = Combine(raw[0], raw[1]);
  accel[1] = Combine(raw[2], raw[3]);
  accel[2] = Combine(raw[4], raw[5]);
  /* raw[6..7] is temperature; by design it is not carried over CAN. */
  gyro[0] = Combine(raw[8], raw[9]);
  gyro[1] = Combine(raw[10], raw[11]);
  gyro[2] = Combine(raw[12], raw[13]);

  return true;
}
