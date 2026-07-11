#include "chassis.h"

#include <stddef.h>

#include "motor.h"

#define CHASSIS_DEADBAND 12

static MotorDirection Chassis_ToDirection(int16_t value)
{
  if (value > CHASSIS_DEADBAND)
  {
    return MOTOR_DIRECTION_FORWARD;
  }

  if (value < -CHASSIS_DEADBAND)
  {
    return MOTOR_DIRECTION_REVERSE;
  }

  return MOTOR_DIRECTION_STOP;
}

void Chassis_Init(void)
{
  Chassis_Stop();
}

void Chassis_ApplyCommand(const ControlCommand *command)
{
  if (command == NULL || !command->enabled)
  {
    Chassis_Stop();
    return;
  }

  const int16_t left = command->forward + command->turn;
  const int16_t right = command->forward - command->turn;

  Motor_SetDirection(MOTOR_A, Chassis_ToDirection(left));
  Motor_SetDirection(MOTOR_B, Chassis_ToDirection(left));
  Motor_SetDirection(MOTOR_C, Chassis_ToDirection(right));
  Motor_SetDirection(MOTOR_D, Chassis_ToDirection(right));
}

void Chassis_Stop(void)
{
  Motor_StopAll();
}
