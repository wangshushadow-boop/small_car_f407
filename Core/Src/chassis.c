#include "chassis.h"

#include <stddef.h>

#include "chassis_auto.h"
#include "chassis_manual.h"
#include "motor.h"

static void Chassis_ApplyWheelSpeed(int16_t left, int16_t right)
{
  /* 当前硬件约定：A/B 为左侧电机，C/D 为右侧电机。 */
  Motor_SetSpeed(MOTOR_A, left);
  Motor_SetSpeed(MOTOR_B, left);
  Motor_SetSpeed(MOTOR_C, right);
  Motor_SetSpeed(MOTOR_D, right);
}

void Chassis_Init(void)
{
  Chassis_Stop();
}

void Chassis_SetManualVelocity(int16_t forward, int16_t turn)
{
  int16_t left = 0;
  int16_t right = 0;

  ChassisManual_Mix(forward, turn, &left, &right);
  Chassis_ApplyWheelSpeed(left, right);
}

void Chassis_SetAutoVelocity(int16_t forward, int16_t turn)
{
  int16_t left = 0;
  int16_t right = 0;

  ChassisAuto_Mix(forward, turn, &left, &right);
  Chassis_ApplyWheelSpeed(left, right);
}

void Chassis_SetVelocity(int16_t forward, int16_t turn)
{
  Chassis_SetAutoVelocity(forward, turn);
}

void Chassis_ApplyCommand(const ControlCommand *command)
{
  if ((command == NULL) || !command->enabled)
  {
    Chassis_Stop();
    return;
  }

  if (command->source == CONTROL_SOURCE_GAMEPAD)
  {
    Chassis_SetManualVelocity(command->forward, command->turn);
    return;
  }

  if (command->source == CONTROL_SOURCE_HOST)
  {
    Chassis_SetAutoVelocity(command->forward, command->turn);
    return;
  }

  Chassis_Stop();
}

void Chassis_Stop(void)
{
  Motor_StopAll();
}
