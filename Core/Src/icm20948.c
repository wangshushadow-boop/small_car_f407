#include "icm20948.h"

#include <stddef.h>

#include "i2c.h"
#include "main.h"

#define ICM20948_I2C_ADDR_PRIMARY 0x68U
#define ICM20948_I2C_ADDR_SECONDARY 0x69U

#define ICM20948_REG_BANK_SEL 0x7FU
#define ICM20948_REG_WHO_AM_I 0x00U
#define ICM20948_REG_PWR_MGMT_1 0x06U
#define ICM20948_REG_PWR_MGMT_2 0x07U
#define ICM20948_REG_ACCEL_XOUT_H 0x2DU
#define ICM20948_REG_GYRO_CONFIG_1 0x01U
#define ICM20948_REG_ACCEL_CONFIG 0x14U

#define ICM20948_USER_BANK_0 0U
#define ICM20948_USER_BANK_2 2U
#define ICM20948_I2C_TIMEOUT_MS 100U

static uint8_t icm20948_address = ICM20948_I2C_ADDR_PRIMARY;

static uint16_t Icm20948_HalAddress(void)
{
  return (uint16_t)(icm20948_address << 1U);
}

static Icm20948Status Icm20948_WriteReg(uint8_t reg, uint8_t value)
{
  if (HAL_I2C_Mem_Write(&hi2c2,
                        Icm20948_HalAddress(),
                        reg,
                        I2C_MEMADD_SIZE_8BIT,
                        &value,
                        1U,
                        ICM20948_I2C_TIMEOUT_MS) != HAL_OK)
  {
    return ICM20948_STATUS_BUS_ERROR;
  }

  return ICM20948_STATUS_OK;
}

static Icm20948Status Icm20948_ReadRegs(uint8_t reg, uint8_t *data, uint8_t length)
{
  if (data == NULL || length == 0U)
  {
    return ICM20948_STATUS_BUS_ERROR;
  }

  if (HAL_I2C_Mem_Read(&hi2c2,
                       Icm20948_HalAddress(),
                       reg,
                       I2C_MEMADD_SIZE_8BIT,
                       data,
                       length,
                       ICM20948_I2C_TIMEOUT_MS) != HAL_OK)
  {
    return ICM20948_STATUS_BUS_ERROR;
  }

  return ICM20948_STATUS_OK;
}

static Icm20948Status Icm20948_SelectBank(uint8_t bank)
{
  return Icm20948_WriteReg(ICM20948_REG_BANK_SEL, (uint8_t)(bank << 4U));
}

static int16_t Icm20948_MakeInt16(uint8_t high, uint8_t low)
{
  return (int16_t)((uint16_t)high << 8U | low);
}

Icm20948Status Icm20948_ReadWhoAmI(uint8_t *who_am_i)
{
  if (who_am_i == NULL)
  {
    return ICM20948_STATUS_BUS_ERROR;
  }

  Icm20948Status status = Icm20948_SelectBank(ICM20948_USER_BANK_0);
  if (status != ICM20948_STATUS_OK)
  {
    return status;
  }

  return Icm20948_ReadRegs(ICM20948_REG_WHO_AM_I, who_am_i, 1U);
}

Icm20948Status Icm20948_Init(void)
{
  uint8_t who_am_i = 0U;

  icm20948_address = ICM20948_I2C_ADDR_PRIMARY;
  Icm20948Status status = Icm20948_ReadWhoAmI(&who_am_i);
  if (status != ICM20948_STATUS_OK || who_am_i != ICM20948_WHO_AM_I_VALUE)
  {
    icm20948_address = ICM20948_I2C_ADDR_SECONDARY;
    status = Icm20948_ReadWhoAmI(&who_am_i);
  }

  if (status != ICM20948_STATUS_OK)
  {
    return status;
  }

  if (who_am_i != ICM20948_WHO_AM_I_VALUE)
  {
    return ICM20948_STATUS_WHO_AM_I_ERROR;
  }

  status = Icm20948_WriteReg(ICM20948_REG_PWR_MGMT_1, 0x80U);
  if (status != ICM20948_STATUS_OK)
  {
    return status;
  }
  HAL_Delay(100);

  status = Icm20948_SelectBank(ICM20948_USER_BANK_0);
  if (status != ICM20948_STATUS_OK)
  {
    return status;
  }
  status = Icm20948_WriteReg(ICM20948_REG_PWR_MGMT_1, 0x01U);
  if (status != ICM20948_STATUS_OK)
  {
    return status;
  }
  status = Icm20948_WriteReg(ICM20948_REG_PWR_MGMT_2, 0x00U);
  if (status != ICM20948_STATUS_OK)
  {
    return status;
  }

  status = Icm20948_SelectBank(ICM20948_USER_BANK_2);
  if (status != ICM20948_STATUS_OK)
  {
    return status;
  }
  status = Icm20948_WriteReg(ICM20948_REG_GYRO_CONFIG_1, 0x06U);
  if (status != ICM20948_STATUS_OK)
  {
    return status;
  }
  status = Icm20948_WriteReg(ICM20948_REG_ACCEL_CONFIG, 0x06U);
  if (status != ICM20948_STATUS_OK)
  {
    return status;
  }

  return Icm20948_SelectBank(ICM20948_USER_BANK_0);
}

Icm20948Status Icm20948_ReadSample(Icm20948Sample *sample)
{
  uint8_t buffer[14] = {0};

  if (sample == NULL)
  {
    return ICM20948_STATUS_BUS_ERROR;
  }

  Icm20948Status status = Icm20948_SelectBank(ICM20948_USER_BANK_0);
  if (status != ICM20948_STATUS_OK)
  {
    return status;
  }

  status = Icm20948_ReadRegs(ICM20948_REG_ACCEL_XOUT_H, buffer, sizeof(buffer));
  if (status != ICM20948_STATUS_OK)
  {
    return status;
  }

  sample->accel_x = Icm20948_MakeInt16(buffer[0], buffer[1]);
  sample->accel_y = Icm20948_MakeInt16(buffer[2], buffer[3]);
  sample->accel_z = Icm20948_MakeInt16(buffer[4], buffer[5]);
  sample->gyro_x = Icm20948_MakeInt16(buffer[6], buffer[7]);
  sample->gyro_y = Icm20948_MakeInt16(buffer[8], buffer[9]);
  sample->gyro_z = Icm20948_MakeInt16(buffer[10], buffer[11]);
  sample->temperature = Icm20948_MakeInt16(buffer[12], buffer[13]);

  return ICM20948_STATUS_OK;
}
