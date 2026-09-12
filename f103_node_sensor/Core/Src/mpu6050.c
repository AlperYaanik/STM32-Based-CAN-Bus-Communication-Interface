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
   board just looks dead, which is the least useful failure mode possible.
   100 ms is plenty for a 14-byte burst at 400 kHz. */
#define MPU6050_I2C_TIMEOUT_MS 100u

/* After clearing SLEEP the chip needs time to settle its internal oscillator;
   until then the first samples can be garbage. The datasheet suggests about
   100 ms for start-up. */
#define MPU6050_WAKEUP_DELAY_MS 100u

/* Averaging N samples divides the random noise by sqrt(N). At 512 samples the
   noise is reduced roughly 23x, which puts the residual well under 1 LSB for
   this part - far below anything that matters. Spacing them 5 ms apart keeps
   each sample independent and spreads the measurement over ~2.5 s, long enough
   to average out slow disturbances rather than a single quiet instant. */
#define MPU6050_GYRO_CAL_SAMPLES   512u
#define MPU6050_GYRO_CAL_PERIOD_MS 5u

/* Peak-to-peak rejection threshold, in LSB. At 131 LSB per deg/s this is about
   7.6 deg/s of movement across the whole run - loose enough to tolerate the
   vibration of a normal desk, tight enough that a hand touching the board or a
   deliberate rotation is caught and the calibration refused. */
#define MPU6050_GYRO_CAL_MAX_SPREAD 1000

/* Zero-rate offset in raw LSB, subtracted from every gyro reading. */
static int16_t gyroBias[3] = {0, 0, 0};

static int16_t Combine(uint8_t hi, uint8_t lo)
{
  return (int16_t)((hi << 8) | lo);
}

/* Saturating subtraction. Near full scale, value - bias can fall outside the
   int16 range; wrapping there would turn a large positive rate into a large
   negative one, which is worse than clipping. */
static int16_t SubtractBias(int16_t value, int16_t bias)
{
  int32_t corrected = (int32_t)value - (int32_t)bias;

  if (corrected > 32767)
  {
    return 32767;
  }
  if (corrected < -32768)
  {
    return -32768;
  }
  return (int16_t)corrected;
}

/* Integer division truncates toward zero, which biases a negative average
   upward - exactly the quantity we are trying to measure. Round instead. */
static int16_t AverageRounded(int32_t sum, uint32_t count)
{
  int32_t half = (int32_t)(count / 2u);

  if (sum >= 0)
  {
    return (int16_t)((sum + half) / (int32_t)count);
  }
  return (int16_t)((sum - half) / (int32_t)count);
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

bool MPU6050_CalibrateGyro(I2C_HandleTypeDef *hi2c)
{
  int32_t sum[3] = {0, 0, 0};
  int16_t lowest[3];
  int16_t highest[3];

  /* Clear the stored bias first, so the samples collected below are the chip's
     actual output rather than output already corrected by a previous run. */
  for (uint8_t axis = 0; axis < 3; axis++)
  {
    gyroBias[axis] = 0;
  }

  for (uint32_t i = 0; i < MPU6050_GYRO_CAL_SAMPLES; i++)
  {
    int16_t accel[3];
    int16_t gyro[3];

    if (!MPU6050_ReadRaw(hi2c, accel, gyro))
    {
      return false;
    }

    for (uint8_t axis = 0; axis < 3; axis++)
    {
      sum[axis] += gyro[axis];

      if (i == 0u)
      {
        lowest[axis] = gyro[axis];
        highest[axis] = gyro[axis];
      }
      else if (gyro[axis] < lowest[axis])
      {
        lowest[axis] = gyro[axis];
      }
      else if (gyro[axis] > highest[axis])
      {
        highest[axis] = gyro[axis];
      }
    }

    HAL_Delay(MPU6050_GYRO_CAL_PERIOD_MS);
  }

  /* If any axis swung too far, the board was not still and the average is a
     measurement of the disturbance, not of the offset. Refuse it: an
     uncalibrated sensor is honest, a wrongly calibrated one is not. */
  for (uint8_t axis = 0; axis < 3; axis++)
  {
    if ((int32_t)highest[axis] - (int32_t)lowest[axis] > MPU6050_GYRO_CAL_MAX_SPREAD)
    {
      return false;
    }
  }

  for (uint8_t axis = 0; axis < 3; axis++)
  {
    gyroBias[axis] = AverageRounded(sum[axis], MPU6050_GYRO_CAL_SAMPLES);
  }

  return true;
}

void MPU6050_GetGyroBias(int16_t bias[3])
{
  for (uint8_t axis = 0; axis < 3; axis++)
  {
    bias[axis] = gyroBias[axis];
  }
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
  gyro[0] = SubtractBias(Combine(raw[8], raw[9]), gyroBias[0]);
  gyro[1] = SubtractBias(Combine(raw[10], raw[11]), gyroBias[1]);
  gyro[2] = SubtractBias(Combine(raw[12], raw[13]), gyroBias[2]);

  return true;
}
