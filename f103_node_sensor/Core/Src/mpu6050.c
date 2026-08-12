#include "mpu6050.h"

#define MPU6050_REG_PWR_MGMT_1   0x6B
#define MPU6050_REG_WHO_AM_I     0x75
#define MPU6050_REG_ACCEL_XOUT_H 0x3B
#define MPU6050_WHO_AM_I_VALUE   0x68

static int16_t Combine(uint8_t hi, uint8_t lo)
{
  return (int16_t)((hi << 8) | lo);
}

bool MPU6050_Init(I2C_HandleTypeDef *hi2c)
{
  /* Chip fabrika çıkışında SLEEP bitiyle (PWR_MGMT_1 bit6) gelir; temizlemeden
     hiçbir register güncellenmez, ReadRaw sessizce hep 0 döner. */
  uint8_t wake = 0x00;
  if (HAL_I2C_Mem_Write(hi2c, MPU6050_I2C_ADDR, MPU6050_REG_PWR_MGMT_1,
                         I2C_MEMADD_SIZE_8BIT, &wake, 1, HAL_MAX_DELAY) != HAL_OK)
  {
    return false;
  }

  uint8_t whoAmI = 0;
  if (HAL_I2C_Mem_Read(hi2c, MPU6050_I2C_ADDR, MPU6050_REG_WHO_AM_I,
                        I2C_MEMADD_SIZE_8BIT, &whoAmI, 1, HAL_MAX_DELAY) != HAL_OK)
  {
    return false;
  }

  return whoAmI == MPU6050_WHO_AM_I_VALUE;
}

bool MPU6050_ReadRaw(I2C_HandleTypeDef *hi2c, int16_t accel[3], int16_t gyro[3])
{
  uint8_t raw[14];

  /* ACCEL_XOUT_H'tan başlayan tek burst: accel(6) + temp(2) + gyro(6).
     Üç ayrı okuma yerine tek transaction -> accel ve gyro aynı ana ait. */
  if (HAL_I2C_Mem_Read(hi2c, MPU6050_I2C_ADDR, MPU6050_REG_ACCEL_XOUT_H,
                        I2C_MEMADD_SIZE_8BIT, raw, sizeof(raw), HAL_MAX_DELAY) != HAL_OK)
  {
    return false;
  }

  accel[0] = Combine(raw[0], raw[1]);
  accel[1] = Combine(raw[2], raw[3]);
  accel[2] = Combine(raw[4], raw[5]);
  /* raw[6..7] = sıcaklık, D kararına göre şimdilik CAN'a taşınmıyor */
  gyro[0] = Combine(raw[8], raw[9]);
  gyro[1] = Combine(raw[10], raw[11]);
  gyro[2] = Combine(raw[12], raw[13]);

  return true;
}
