/**
 * @file odometry.h
 * @brief 声明四轮霍尔编码器与 ICM20948 融合里程计接口。
 *
 * 模块输出三维位置、姿态、速度和轮速调试信息；比例与融合权重由底盘参数提供。
 */
#ifndef ODOMETRY_H_
#define ODOMETRY_H_

#include <stdbool.h>
#include <stdint.h>

#include "encoder.h"
#include "icm20948.h"

/*
 * 里程计标定参数。
 *
 * 官方霍尔编码器四轮车参数：
 * - 轮子直径：65 mm
 * - 霍尔编码器线数：13
 * - 电机减速比：30
 * - 编码器 4 倍频
 *
 * 每轮一圈 tick = 4 * 13 * 30 = 1560
 * 每轮一圈距离约 = 65 * PI = 204.2 mm
 * 因此 1 tick 约等于 204.2 / 1560 = 0.1309 mm。
 *
 * 为了避免在 MCU 中使用浮点，使用整数比例：
 * 2042 / 15600 = 0.1309 mm/tick。
 * 实测直行后通过 chassis_params.yaml 调整分子，分母保持 15600。
 */
/*
 * 2026-07-18 first floor calibration:
 * real straight distance 1000 mm, odometry reported 876 mm.
 * 2042 * 1000 / 876 = 2398, so use 2398 / 15600 mm/tick first.
 */
#define ODOMETRY_MM_PER_TICK_DEN 15600

/*
 * 官方 FourWheel_Car 编码器方向：
 * A、B 取正，C、D 取反。
 * 如果实车前进时某一路距离方向不对，再单独调整对应 SIGN。
 */
#define ODOMETRY_MOTOR_A_SIGN 1
#define ODOMETRY_MOTOR_B_SIGN 1
#define ODOMETRY_MOTOR_C_SIGN -1
#define ODOMETRY_MOTOR_D_SIGN -1

/*
 * ICM20948 当前陀螺仪配置为 +/-2000 dps，灵敏度由运行时参数提供。
 */
#define ODOMETRY_GYRO_X_SIGN 1
#define ODOMETRY_GYRO_Y_SIGN 1
#define ODOMETRY_GYRO_Z_SIGN 1
#define ODOMETRY_GYRO_BIAS_SAMPLE_COUNT 100U

/** MCU 对外发布的融合里程计快照，位置 mm、角度 mdeg、时间 ms。 */
typedef struct {
  uint32_t time_ms;
  int32_t x_mm;
  int32_t y_mm;
  int32_t z_mm;
  int32_t distance_mm;
  int16_t speed_mm_s;
  int32_t roll_mdeg;
  int32_t pitch_mdeg;
  int32_t yaw_mdeg;
  int32_t yaw_rate_mdeg_s;
  bool calibrated;
  bool wheel_yaw_fused;
} OdometrySample;

/** 轮速融合调试快照，所有速度和位移单位均为 mm 系列。 */
typedef struct {
  int16_t left_speed_mm_s;
  int16_t right_speed_mm_s;
  int16_t turn_speed_mm_s;
  int16_t left_delta_mm;
  int16_t right_delta_mm;
} OdometryDebug;

/** 清空状态并开始静止陀螺仪零偏标定。 */
void Odometry_Init(void);
/** 保留当前标定结果，清零位置、姿态积分和编码器基线。 */
void Odometry_Reset(void);
/** 使用同一时刻的 IMU 和四轮编码器样本推进一次融合。 */
void Odometry_Update(const Icm20948Sample *imu,
                     EncoderSample encoder_a,
                     EncoderSample encoder_b,
                     EncoderSample encoder_c,
                     EncoderSample encoder_d,
                     uint32_t now_ms);
/** 返回最近一次融合输出和轮速调试数据。 */
OdometrySample Odometry_GetSample(void);
OdometryDebug Odometry_GetDebug(void);

#endif  // ODOMETRY_H_
