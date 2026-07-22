#include "chassis_auto.h"

/*
 * 自动底盘控制算法预留模块。
 *
 * 后续 ROS2 导航或树莓派算法控制底盘时可在这里扩展。
 * 当前把 ROS 物理速度按标定上限换算为开环电机输出。
 * 后续接入轮速闭环时，只替换本模块，不需要再修改 ROS 或串口协议。
 */

#include <stddef.h>

#include "chassis_params.h"
#include "motor.h"

#define AUTO_DEADBAND 12

static int16_t ClampSpeed(int16_t speed)
{
  if (speed > MOTOR_MAX_SPEED)
  {
    return MOTOR_MAX_SPEED;
  }

  if (speed < -MOTOR_MAX_SPEED)
  {
    return -MOTOR_MAX_SPEED;
  }

  return speed;
}

static int16_t ScaleVelocity(int16_t value, int16_t maximum)
{
  if (maximum <= 0)
  {
    return 0;
  }

  int32_t scaled = ((int32_t)value * MOTOR_MAX_SPEED) / maximum;
  if (scaled > MOTOR_MAX_SPEED)
  {
    scaled = MOTOR_MAX_SPEED;
  }
  else if (scaled < -MOTOR_MAX_SPEED)
  {
    scaled = -MOTOR_MAX_SPEED;
  }
  return (int16_t)scaled;
}

void ChassisAuto_Mix(int16_t linear_mm_s,
                     int16_t angular_mrad_s,
                     int16_t *left,
                     int16_t *right)
{
  if ((left == NULL) || (right == NULL))
  {
    return;
  }

  const ChassisParams params = ChassisParams_Get();
  int16_t forward = ScaleVelocity(linear_mm_s, params.max_linear_speed_mm_s);
  /* 当前底盘正转向输出对应右转，因此对 ROS 正角速度（左转）取反。 */
  int16_t turn = (int16_t)-ScaleVelocity(angular_mrad_s,
                                        params.max_angular_speed_mrad_s);

  if ((forward < AUTO_DEADBAND) && (forward > -AUTO_DEADBAND))
  {
    forward = 0;
  }

  if ((turn < AUTO_DEADBAND) && (turn > -AUTO_DEADBAND))
  {
    turn = 0;
  }

  *left = ClampSpeed(forward + turn);
  *right = ClampSpeed(forward - turn);
}
