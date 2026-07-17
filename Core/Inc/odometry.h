#ifndef ODOMETRY_H_
#define ODOMETRY_H_

#include <stdbool.h>
#include <stdint.h>

#include "encoder.h"
#include "icm20948.h"

/*
 * 里程计标定参数。
 *
 * 当前缺少电机编码器线数、减速比和轮径的准确值，所以先使用 1 tick = 1 mm
 * 作为占位比例。后续实测“直行 1000 mm 的编码器 tick 数”后，只需要调整
 * ODOMETRY_MM_PER_TICK_NUM / ODOMETRY_MM_PER_TICK_DEN。
 */
#define ODOMETRY_MM_PER_TICK_NUM 1
#define ODOMETRY_MM_PER_TICK_DEN 1

/* 前进时某个轮子的编码器如果为负，就把对应符号改成 -1。 */
#define ODOMETRY_MOTOR_A_SIGN 1
#define ODOMETRY_MOTOR_B_SIGN 1
#define ODOMETRY_MOTOR_C_SIGN 1
#define ODOMETRY_MOTOR_D_SIGN 1

/*
 * ICM20948 当前陀螺仪配置为 +/-2000 dps，灵敏度约 16.4 LSB/(deg/s)。
 * 用 x10 避免浮点：16.4 -> 164。
 */
#define ODOMETRY_GYRO_LSB_PER_DPS_X10 164
#define ODOMETRY_GYRO_Z_SIGN 1
#define ODOMETRY_GYRO_BIAS_SAMPLE_COUNT 100U

typedef struct {
  uint32_t time_ms;
  int32_t distance_mm;
  int16_t speed_mm_s;
  int32_t yaw_mdeg;
  int16_t yaw_rate_mdeg_s;
  bool calibrated;
} OdometrySample;

void Odometry_Init(void);
void Odometry_Reset(void);
void Odometry_Update(const Icm20948Sample *imu,
                     EncoderSample encoder_a,
                     EncoderSample encoder_b,
                     EncoderSample encoder_c,
                     EncoderSample encoder_d,
                     uint32_t now_ms);
OdometrySample Odometry_GetSample(void);

#endif  // ODOMETRY_H_
