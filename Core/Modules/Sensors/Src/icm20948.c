/**
 * @file icm20948.c
 * @brief 实现 ICM20948 的 I2C 寄存器访问、初始化和六轴连续采样。
 *
 * 当前配置使用加速度计 ±16 g、陀螺仪 ±2000 dps，换算比例由使用方统一处理。
 */
#include "icm20948.h"

/*
 * ICM20948 IMU 驱动模块。
 *
 * 通过 I2C 读取加速度和陀螺仪原始值。
 * 本模块只负责寄存器初始化和原始数据读取，姿态融合在 odometry.c 中完成。
 */

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
  /* HAL I2C API 需要 8 位地址格式，所以把 7 位地址左移 1 位。 */
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
  /* ICM20948 寄存器分多个 Bank，访问配置寄存器前必须切到对应 Bank。 */
  return Icm20948_WriteReg(ICM20948_REG_BANK_SEL, (uint8_t)(bank << 4U));
}

static int16_t Icm20948_MakeInt16(uint8_t high, uint8_t low)
{
  /* 传感器数据按高字节在前输出，这里拼成有符号 16 位原始值。 */
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

  /*
   * 板子上的 AD0 电平可能导致地址为 0x68 或 0x69。
   * 初始化时两个地址都尝试一次，提高兼容性。
   */
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

  /* 先软复位，保证传感器从已知状态开始配置。 */
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
  /* 选择自动时钟，打开加速度计和陀螺仪。 */
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
  /*
   * 这里先使用厂家示例中常见的基础配置。
   * 后续如果要做姿态融合，可以再根据量程/滤波需求细调这些寄存器。
   */
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
  /* 从 ACCEL_XOUT_H 开始连续读取 14 字节：加速度、陀螺仪、温度。 */
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
