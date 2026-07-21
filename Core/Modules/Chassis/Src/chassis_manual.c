#include "chassis_manual.h"

/*
 * 手动底盘控制算法。
 *
 * 输入 forward/turn 两个控制量，输出四轮差速。
 * 小转向时先降低一侧轮速，转向量足够大时再允许一侧反转，手感更柔和。
 */

#include <stddef.h>

#include "motor.h"

#define MANUAL_DEADBAND 12
#define MANUAL_PIVOT_FORWARD_THRESHOLD 80

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

static int16_t Abs16(int16_t value)
{
  return (value < 0) ? (int16_t)-value : value;
}

void ChassisManual_Mix(int16_t forward, int16_t turn, int16_t *left, int16_t *right)
{
  if ((left == NULL) || (right == NULL))
  {
    return;
  }

  if ((forward < MANUAL_DEADBAND) && (forward > -MANUAL_DEADBAND))
  {
    forward = 0;
  }

  if ((turn < MANUAL_DEADBAND) && (turn > -MANUAL_DEADBAND))
  {
    turn = 0;
  }

  if (Abs16(forward) < MANUAL_PIVOT_FORWARD_THRESHOLD)
  {
    *left = ClampSpeed(turn);
    *right = ClampSpeed((int16_t)-turn);
    return;
  }

  if (turn > 0)
  {
    *left = ClampSpeed(forward);
    *right = ClampSpeed(forward - turn);
    if (((forward > 0) && (*right < 0)) || ((forward < 0) && (*right > 0)))
    {
      *right = 0;
    }
    return;
  }

  if (turn < 0)
  {
    *left = ClampSpeed(forward + turn);
    *right = ClampSpeed(forward);
    if (((forward > 0) && (*left < 0)) || ((forward < 0) && (*left > 0)))
    {
      *left = 0;
    }
    return;
  }

  *left = ClampSpeed(forward);
  *right = ClampSpeed(forward);
}
