/**
 * @file chassis_params.c
 * @brief 实现底盘运行参数默认值、编号映射和逐项范围校验。
 */
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

/**
 * @brief 将树莓派下发的 32 位参数安全收窄到指定的 16 位范围。
 *
 * 多数运行参数最终参与 16 位控制计算，集中限幅可避免各 case 重复代码，
 * 同时防止异常配置在类型转换时发生回绕。
 */
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
  /*
   * 默认值是 MCU 的独立运行基线。树莓派连接后会从 YAML 批量下发并校验，
   * 因此这里必须始终保留一组即使没有上位机也能安全工作的参数。
   */
  g_params.odom_mm_per_tick_num = 2410;
  g_params.gamepad_forward_start = 550;
  g_params.gamepad_reverse_start = 320;
  g_params.gamepad_drive_max = 800;
  g_params.gamepad_turn_start = 620;
  g_params.gamepad_turn_max = 850;
  g_params.ultra_near_distance_mm = 200;
  g_params.gyro_lsb_per_dps_x10 = 164;
  g_params.wheel_track_mm = 115;
  g_params.yaw_gyro_weight_permille = 900;
  g_params.attitude_gyro_weight_permille = 980;
  g_params.imu_roll_offset_mdeg = 0;
  g_params.imu_pitch_offset_mdeg = 0;
  g_params.max_linear_speed_mm_s = 600;
  g_params.max_angular_speed_mrad_s = 2000;
  g_params.wheel_speed_closed_loop_enabled = true;
  g_params.wheel_speed_kp_x100 = 50;
  g_params.wheel_speed_ki_x100 = 20;
  g_params.wheel_speed_integral_limit = 1000;
  g_params.wheel_accel_limit_mm_s2 = 500;
  g_params.wheel_pwm_min = 550;
  g_params.wheel_turn_start_pwm = 750;
  g_params.wheel_left_output_permille = 1000;
  g_params.wheel_right_output_permille = 1000;
}

bool ChassisParams_Set(ChassisParamId id, int32_t value)
{
  /*
   * 每个协议参数编号只在这里映射到运行结构体。
   * 返回 false 表示编号未知或值不满足硬性约束，调用方不得回报设置成功。
   */
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

    case CHASSIS_PARAM_MAX_LINEAR_SPEED_MM_S:
      g_params.max_linear_speed_mm_s = ClampI16Param(value, 100, 3000);
      return true;

    case CHASSIS_PARAM_MAX_ANGULAR_SPEED_MRAD_S:
      g_params.max_angular_speed_mrad_s = ClampI16Param(value, 100, 10000);
      return true;

    case CHASSIS_PARAM_WHEEL_SPEED_CLOSED_LOOP_ENABLED:
      if ((value != 0) && (value != 1))
      {
        return false;
      }
      g_params.wheel_speed_closed_loop_enabled = value != 0;
      return true;

    case CHASSIS_PARAM_WHEEL_SPEED_KP_X100:
      g_params.wheel_speed_kp_x100 = ClampI16Param(value, 0, 1000);
      return true;

    case CHASSIS_PARAM_WHEEL_SPEED_KI_X100:
      g_params.wheel_speed_ki_x100 = ClampI16Param(value, 0, 1000);
      return true;

    case CHASSIS_PARAM_WHEEL_SPEED_INTEGRAL_LIMIT:
      g_params.wheel_speed_integral_limit = ClampI16Param(value, 0, 10000);
      return true;

    case CHASSIS_PARAM_WHEEL_ACCEL_LIMIT_MM_S2:
      g_params.wheel_accel_limit_mm_s2 = ClampI16Param(value, 50, 5000);
      return true;

    case CHASSIS_PARAM_WHEEL_PWM_MIN:
      g_params.wheel_pwm_min = ClampI16Param(value, 0, 1000);
      return true;

    case CHASSIS_PARAM_WHEEL_LEFT_OUTPUT_PERMILLE:
      g_params.wheel_left_output_permille = ClampI16Param(value, 500, 1500);
      return true;

    case CHASSIS_PARAM_WHEEL_RIGHT_OUTPUT_PERMILLE:
      g_params.wheel_right_output_permille = ClampI16Param(value, 500, 1500);
      return true;

    case CHASSIS_PARAM_WHEEL_TURN_START_PWM:
      g_params.wheel_turn_start_pwm = ClampI16Param(value, 0, 1000);
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

  /* 与 Set 使用同一编号表，供树莓派在下发后逐项读取验证。 */
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

    case CHASSIS_PARAM_MAX_LINEAR_SPEED_MM_S:
      *value = g_params.max_linear_speed_mm_s;
      return true;

    case CHASSIS_PARAM_MAX_ANGULAR_SPEED_MRAD_S:
      *value = g_params.max_angular_speed_mrad_s;
      return true;

    case CHASSIS_PARAM_WHEEL_SPEED_CLOSED_LOOP_ENABLED:
      *value = g_params.wheel_speed_closed_loop_enabled ? 1 : 0;
      return true;

    case CHASSIS_PARAM_WHEEL_SPEED_KP_X100:
      *value = g_params.wheel_speed_kp_x100;
      return true;

    case CHASSIS_PARAM_WHEEL_SPEED_KI_X100:
      *value = g_params.wheel_speed_ki_x100;
      return true;

    case CHASSIS_PARAM_WHEEL_SPEED_INTEGRAL_LIMIT:
      *value = g_params.wheel_speed_integral_limit;
      return true;

    case CHASSIS_PARAM_WHEEL_ACCEL_LIMIT_MM_S2:
      *value = g_params.wheel_accel_limit_mm_s2;
      return true;

    case CHASSIS_PARAM_WHEEL_PWM_MIN:
      *value = g_params.wheel_pwm_min;
      return true;

    case CHASSIS_PARAM_WHEEL_LEFT_OUTPUT_PERMILLE:
      *value = g_params.wheel_left_output_permille;
      return true;

    case CHASSIS_PARAM_WHEEL_RIGHT_OUTPUT_PERMILLE:
      *value = g_params.wheel_right_output_permille;
      return true;

    case CHASSIS_PARAM_WHEEL_TURN_START_PWM:
      *value = g_params.wheel_turn_start_pwm;
      return true;

    default:
      return false;
  }
}

ChassisParams ChassisParams_Get(void)
{
  /* 返回快照而不是暴露全局变量，业务模块不能绕过校验直接改参数。 */
  return g_params;
}
