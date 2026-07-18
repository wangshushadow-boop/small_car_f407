#ifndef CHASSIS_PARAMS_H_
#define CHASSIS_PARAMS_H_

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  CHASSIS_PARAM_ODOM_MM_PER_TICK_NUM = 1,
  CHASSIS_PARAM_GAMEPAD_FORWARD_START = 2,
  CHASSIS_PARAM_GAMEPAD_REVERSE_START = 3,
  CHASSIS_PARAM_GAMEPAD_DRIVE_MAX = 4,
  CHASSIS_PARAM_GAMEPAD_TURN_START = 5,
  CHASSIS_PARAM_GAMEPAD_TURN_MAX = 6,
  CHASSIS_PARAM_ULTRA_NEAR_DISTANCE = 7,
  CHASSIS_PARAM_GYRO_LSB_PER_DPS_X10 = 8,
  CHASSIS_PARAM_WHEEL_TRACK_MM = 9,
  CHASSIS_PARAM_YAW_GYRO_WEIGHT_PERMILLE = 10,
  CHASSIS_PARAM_ATTITUDE_GYRO_WEIGHT_PERMILLE = 11,
  CHASSIS_PARAM_IMU_ROLL_OFFSET_MDEG = 12,
  CHASSIS_PARAM_IMU_PITCH_OFFSET_MDEG = 13,
} ChassisParamId;

typedef struct {
  int32_t odom_mm_per_tick_num;
  int16_t gamepad_forward_start;
  int16_t gamepad_reverse_start;
  int16_t gamepad_drive_max;
  int16_t gamepad_turn_start;
  int16_t gamepad_turn_max;
  int16_t ultra_near_distance_mm;
  int16_t gyro_lsb_per_dps_x10;
  int16_t wheel_track_mm;
  int16_t yaw_gyro_weight_permille;
  int16_t attitude_gyro_weight_permille;
  int16_t imu_roll_offset_mdeg;
  int16_t imu_pitch_offset_mdeg;
} ChassisParams;

void ChassisParams_Init(void);
bool ChassisParams_Set(ChassisParamId id, int32_t value);
bool ChassisParams_GetValue(ChassisParamId id, int32_t *value);
ChassisParams ChassisParams_Get(void);

#endif  // CHASSIS_PARAMS_H_
