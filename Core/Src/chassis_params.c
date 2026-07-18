#include "chassis_params.h"

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
  g_params.gamepad_turn_start = 260;
  g_params.gamepad_turn_max = 550;
  g_params.ultra_near_distance_mm = 200;
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

    default:
      return false;
  }
}

ChassisParams ChassisParams_Get(void)
{
  return g_params;
}
