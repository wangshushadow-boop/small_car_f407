#include "chassis.h"

#include <stddef.h>

#include "motor.h"

#define CHASSIS_DEADBAND 12

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
  Chassis_Stop();
}

void Chassis_SetVelocity(int16_t forward, int16_t turn)
{
  if (forward < CHASSIS_DEADBAND && forward > -CHASSIS_DEADBAND)
  {
    forward = 0;
  }

  if (turn < CHASSIS_DEADBAND && turn > -CHASSIS_DEADBAND)
  {
    turn = 0;
  }

  const int16_t left = Chassis_ClampSpeed(forward + turn);
  const int16_t right = Chassis_ClampSpeed(forward - turn);

  Motor_SetSpeed(MOTOR_A, left);
  Motor_SetSpeed(MOTOR_B, left);
  Motor_SetSpeed(MOTOR_C, right);
  Motor_SetSpeed(MOTOR_D, right);
}

void Chassis_ApplyCommand(const ControlCommand *command)
{
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
