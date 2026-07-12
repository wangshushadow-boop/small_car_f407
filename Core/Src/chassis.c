#include "chassis.h"

#include <stddef.h>

#include "motor.h"

#define CHASSIS_DEADBAND 12

/* 将混控后的速度限制在电机驱动允许的范围内，避免 PWM 比例计算越界。 */
static int16_t Chassis_ClampSpeed(int16_t speed)
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

void Chassis_Init(void)
{
  /* 上电先停车，避免 PWM 初始化过程中的未知状态带动电机。 */
  Chassis_Stop();
}

void Chassis_SetVelocity(int16_t forward, int16_t turn)
{
  /*
   * 摇杆或上位机输入可能有轻微抖动。
   * 死区内直接归零，避免小车在“应该静止”时缓慢爬行。
   */
  if (forward < CHASSIS_DEADBAND && forward > -CHASSIS_DEADBAND)
  {
    forward = 0;
  }

  if (turn < CHASSIS_DEADBAND && turn > -CHASSIS_DEADBAND)
  {
    turn = 0;
  }

  /*
   * 差速混控：
   * - forward 同时加到左右两侧，控制前进/后退。
   * - turn 对左右两侧一加一减，控制原地或行进中转向。
   */
  const int16_t left = Chassis_ClampSpeed(forward + turn);
  const int16_t right = Chassis_ClampSpeed(forward - turn);

  /* 当前硬件上 A/B 作为左侧，C/D 作为右侧。 */
  Motor_SetSpeed(MOTOR_A, left);
  Motor_SetSpeed(MOTOR_B, left);
  Motor_SetSpeed(MOTOR_C, right);
  Motor_SetSpeed(MOTOR_D, right);
}

void Chassis_ApplyCommand(const ControlCommand *command)
{
  /* 仲裁层给出空指令或 disabled 时，执行层统一停车。 */
  if (command == NULL || !command->enabled)
  {
    Chassis_Stop();
    return;
  }

  Chassis_SetVelocity(command->forward, command->turn);
}

void Chassis_Stop(void)
{
  Motor_StopAll();
}
