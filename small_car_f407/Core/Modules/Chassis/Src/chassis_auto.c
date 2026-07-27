/**
 * @file chassis_auto.c
 * @brief 实现树莓派物理速度命令的差速运动学逆解和限幅。
 */
#include "chassis_auto.h"

/*
 * 自动底盘控制算法预留模块。
 *
 * 后续 ROS2 导航或树莓派算法控制底盘时可在这里扩展。
 * 将 ROS 底盘线速度、角速度换算为左右轮物理速度目标。
 * 轮速闭环和 PWM 输出由 wheel_speed_controller 模块完成。
 */

#include <stddef.h>

#include "chassis_params.h"

/* 自动控制输入在接近零时直接归零，避免上位机数值噪声驱动电机。 */
#define AUTO_DEADBAND_MM_S 5

/**
 * @brief 将带符号速度限制在对称范围内。
 *
 * @param speed 待限制的速度，单位为 mm/s。
 * @param maximum 允许的最大速度绝对值，单位为 mm/s。
 * @return 限幅后的 16 位速度。
 */
static int16_t ClampSpeed(int32_t speed, int16_t maximum)
{
  if (speed > maximum)
  {
    return maximum;
  }

  if (speed < -maximum)
  {
    return (int16_t)-maximum;
  }

  return (int16_t)speed;
}

void ChassisAuto_Mix(int16_t linear_mm_s,
                     int16_t angular_mrad_s,
                     int16_t *left,
                     int16_t *right)
{
  /* 输出指针无效时不写内存，由调用方继续保持原有安全状态。 */
  if ((left == NULL) || (right == NULL))
  {
    return;
  }

  const ChassisParams params = ChassisParams_Get();
  /* 先限制车体线速度，再进行差速逆解，避免两侧目标同时越界。 */
  int16_t forward =
      ClampSpeed(linear_mm_s, params.max_linear_speed_mm_s);
  /*
   * v_left = v - w * track / 2，v_right = v + w * track / 2。
   * angular_mrad_s * wheel_track_mm / 2000 的结果单位为 mm/s。
   */
  const int32_t turn =
      ((int32_t)angular_mrad_s * params.wheel_track_mm) / 2000;

  if ((forward < AUTO_DEADBAND_MM_S) &&
      (forward > -AUTO_DEADBAND_MM_S))
  {
    forward = 0;
  }

  /* 最后分别限幅，保证转向叠加后仍不超过底盘允许速度。 */
  *left = ClampSpeed((int32_t)forward - turn, params.max_linear_speed_mm_s);
  *right = ClampSpeed((int32_t)forward + turn, params.max_linear_speed_mm_s);
}
