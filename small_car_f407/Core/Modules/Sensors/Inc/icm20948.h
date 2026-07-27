/**
 * @file icm20948.h
 * @brief 声明 ICM20948 六轴惯性传感器初始化和原始采样接口。
 */
#ifndef ICM20948_H_
#define ICM20948_H_

#include <stdint.h>

/** ICM20948 WHO_AM_I 寄存器的期望值。 */
#define ICM20948_WHO_AM_I_VALUE 0xEAU

/** 一次原始六轴与温度采样；单位换算由里程计或上位机完成。 */
typedef struct {
  int16_t accel_x;
  int16_t accel_y;
  int16_t accel_z;
  int16_t gyro_x;
  int16_t gyro_y;
  int16_t gyro_z;
  int16_t temperature;
} Icm20948Sample;

/** 驱动公开状态码，用于区分总线故障和芯片身份不匹配。 */
typedef enum {
  ICM20948_STATUS_OK = 0,
  ICM20948_STATUS_BUS_ERROR,
  ICM20948_STATUS_WHO_AM_I_ERROR,
} Icm20948Status;

/** 复位芯片、验证身份并配置当前加速度计和陀螺仪量程。 */
Icm20948Status Icm20948_Init(void);
/** 读取 WHO_AM_I，参数不能为空。 */
Icm20948Status Icm20948_ReadWhoAmI(uint8_t *who_am_i);
/** 连续读取一帧原始传感器数据，参数不能为空。 */
Icm20948Status Icm20948_ReadSample(Icm20948Sample *sample);

#endif  // ICM20948_H_
