#include "chassis.h"

/*
 * 底盘总控模块。
 *
 * 接收仲裁后的 ControlCommand，根据来源选择手动或自动控制算法，
 * 最终把左右侧轮子的目标速度写入 Motor 模块。
 */

#include <stddef.h>

#include "chassis_auto.h"
#include "chassis_manual.h"
#include "motor.h"
#include "wheel_speed_controller.h"

typedef enum {
  CHASSIS_MODE_STOPPED = 0,
  CHASSIS_MODE_MANUAL,
  CHASSIS_MODE_AUTOMATIC,
} ChassisMode;

static ChassisMode g_mode;

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
  WheelSpeedController_Init();
  g_mode = CHASSIS_MODE_STOPPED;
  Chassis_Stop();
}

void Chassis_SetManualVelocity(int16_t forward, int16_t turn)
{
  int16_t left = 0;
  int16_t right = 0;

  if (g_mode != CHASSIS_MODE_MANUAL)
  {
    WheelSpeedController_Stop();
  }
  ChassisManual_Mix(forward, turn, &left, &right);
  Chassis_ApplyWheelSpeed(left, right);
  g_mode = CHASSIS_MODE_MANUAL;
}

void Chassis_SetAutoVelocity(int16_t forward, int16_t turn)
{
  int16_t left = 0;
  int16_t right = 0;

  ChassisAuto_Mix(forward, turn, &left, &right);
  WheelSpeedController_SetTarget(left, right);
  g_mode = CHASSIS_MODE_AUTOMATIC;
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

  if ((command->source == CONTROL_SOURCE_GAMEPAD) &&
      (command->value_type == CONTROL_VALUE_NORMALIZED))
  {
    Chassis_SetManualVelocity(command->forward, command->turn);
    return;
  }

  if ((command->source == CONTROL_SOURCE_HOST) &&
      (command->value_type == CONTROL_VALUE_PHYSICAL_VELOCITY))
  {
    Chassis_SetAutoVelocity(command->forward, command->turn);
    return;
  }

  Chassis_Stop();
}

void Chassis_TaskStep(uint32_t dt_ms)
{
  if (g_mode == CHASSIS_MODE_AUTOMATIC)
  {
    WheelSpeedController_TaskStep(dt_ms);
  }
}

void Chassis_Stop(void)
{
  WheelSpeedController_Stop();
  g_mode = CHASSIS_MODE_STOPPED;
}
