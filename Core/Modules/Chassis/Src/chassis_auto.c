#include "chassis_auto.h"

/*
 * 自动底盘控制算法预留模块。
 *
 * 后续 ROS2 导航或树莓派算法控制底盘时可在这里扩展。
 * 当前保持简单透传，避免 MCU 端提前引入复杂策略。
 */

#include <stddef.h>

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

void ChassisAuto_Mix(int16_t forward, int16_t turn, int16_t *left, int16_t *right)
{
  if ((left == NULL) || (right == NULL))
  {
    return;
  }

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
