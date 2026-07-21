#include "chassis_params.h"

/*
 * 底盘运行参数模块。
 *
 * MCU 内置一套默认参数，树莓派启动后可以通过串口协议批量下发覆盖。
 * 这样标定里程计、手柄起步力、轮距和 IMU 偏置时，不需要频繁重新烧录。
 */

#include <stddef.h>

/*
 * 底盘运行时参数。
 *
 * 这些值只保存在 RAM 中，树莓派下发后立即生效；MCU 复位后会恢复这里的默认值。
 * 这样调试阶段不需要频繁烧录，也避免过早引入 Flash 存储和磨损问题。
 */
static ChassisParams g_params;

static int16_t ClampI16Param(int32_t value, int16_t min_value, int16_t max_value)
{
  if (value < min_value)
  {
    return min_value;
  }
  if (value > max_value)
  {
    return max_value;
  }
  return (int16_t)value;
}

void ChassisParams_Init(void)
{
  g_params.odom_mm_per_tick_num = 2410;
  g_params.gamepad_forward_start = 550;
  g_params.gamepad_reverse_start = 320;
  g_params.gamepad_drive_max = 800;
  g_params.gamepad_turn_start = 620;
  g_params.gamepad_turn_max = 850;
  g_params.ultra_near_distance_mm = 200;
  g_params.gyro_lsb_per_dps_x10 = 164;
  g_params.wheel_track_mm = 0;
  g_params.yaw_gyro_weight_permille = 900;
  g_params.attitude_gyro_weight_permille = 980;
  g_params.imu_roll_offset_mdeg = 0;
  g_params.imu_pitch_offset_mdeg = 0;
}

bool ChassisParams_Set(ChassisParamId id, int32_t value)
{
  switch (id)
  {
    case CHASSIS_PARAM_ODOM_MM_PER_TICK_NUM:
      if ((value < 1000) || (value > 5000))
      {
        return false;
      }
      g_params.odom_mm_per_tick_num = value;
      return true;

    case CHASSIS_PARAM_GAMEPAD_FORWARD_START:
      g_params.gamepad_forward_start = ClampI16Param(value, 0, 1000);
      return true;

    case CHASSIS_PARAM_GAMEPAD_REVERSE_START:
      g_params.gamepad_reverse_start = ClampI16Param(value, 0, 1000);
      return true;

    case CHASSIS_PARAM_GAMEPAD_DRIVE_MAX:
      g_params.gamepad_drive_max = ClampI16Param(value, 0, 1000);
      return true;

    case CHASSIS_PARAM_GAMEPAD_TURN_START:
      g_params.gamepad_turn_start = ClampI16Param(value, 0, 1000);
      return true;

    case CHASSIS_PARAM_GAMEPAD_TURN_MAX:
      g_params.gamepad_turn_max = ClampI16Param(value, 0, 1000);
      return true;

    case CHASSIS_PARAM_ULTRA_NEAR_DISTANCE:
      g_params.ultra_near_distance_mm = ClampI16Param(value, 0, 5000);
      return true;

    case CHASSIS_PARAM_GYRO_LSB_PER_DPS_X10:
      g_params.gyro_lsb_per_dps_x10 = ClampI16Param(value, 100, 300);
      return true;

    case CHASSIS_PARAM_WHEEL_TRACK_MM:
      g_params.wheel_track_mm = ClampI16Param(value, 0, 1000);
      return true;

    case CHASSIS_PARAM_YAW_GYRO_WEIGHT_PERMILLE:
      g_params.yaw_gyro_weight_permille = ClampI16Param(value, 0, 1000);
      return true;

    case CHASSIS_PARAM_ATTITUDE_GYRO_WEIGHT_PERMILLE:
      g_params.attitude_gyro_weight_permille = ClampI16Param(value, 0, 1000);
      return true;

    case CHASSIS_PARAM_IMU_ROLL_OFFSET_MDEG:
      g_params.imu_roll_offset_mdeg = ClampI16Param(value, -30000, 30000);
      return true;

    case CHASSIS_PARAM_IMU_PITCH_OFFSET_MDEG:
      g_params.imu_pitch_offset_mdeg = ClampI16Param(value, -30000, 30000);
      return true;

    default:
      return false;
  }
}

bool ChassisParams_GetValue(ChassisParamId id, int32_t *value)
{
  if (value == NULL)
  {
    return false;
  }

  switch (id)
  {
    case CHASSIS_PARAM_ODOM_MM_PER_TICK_NUM:
      *value = g_params.odom_mm_per_tick_num;
      return true;

    case CHASSIS_PARAM_GAMEPAD_FORWARD_START:
      *value = g_params.gamepad_forward_start;
      return true;

    case CHASSIS_PARAM_GAMEPAD_REVERSE_START:
      *value = g_params.gamepad_reverse_start;
      return true;

    case CHASSIS_PARAM_GAMEPAD_DRIVE_MAX:
      *value = g_params.gamepad_drive_max;
      return true;

    case CHASSIS_PARAM_GAMEPAD_TURN_START:
      *value = g_params.gamepad_turn_start;
      return true;

    case CHASSIS_PARAM_GAMEPAD_TURN_MAX:
      *value = g_params.gamepad_turn_max;
      return true;

    case CHASSIS_PARAM_ULTRA_NEAR_DISTANCE:
      *value = g_params.ultra_near_distance_mm;
      return true;

    case CHASSIS_PARAM_GYRO_LSB_PER_DPS_X10:
      *value = g_params.gyro_lsb_per_dps_x10;
      return true;

    case CHASSIS_PARAM_WHEEL_TRACK_MM:
      *value = g_params.wheel_track_mm;
      return true;

    case CHASSIS_PARAM_YAW_GYRO_WEIGHT_PERMILLE:
      *value = g_params.yaw_gyro_weight_permille;
      return true;

    case CHASSIS_PARAM_ATTITUDE_GYRO_WEIGHT_PERMILLE:
      *value = g_params.attitude_gyro_weight_permille;
      return true;

    case CHASSIS_PARAM_IMU_ROLL_OFFSET_MDEG:
      *value = g_params.imu_roll_offset_mdeg;
      return true;

    case CHASSIS_PARAM_IMU_PITCH_OFFSET_MDEG:
      *value = g_params.imu_pitch_offset_mdeg;
      return true;

    default:
      return false;
  }
}

ChassisParams ChassisParams_Get(void)
{
  return g_params;
}
