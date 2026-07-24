/**
 * @file chassis.c
 * @brief 实现底盘执行层，在手柄开环和树莓派轮速闭环之间切换。
 */
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

/* 当前执行模式决定周期任务是否运行轮速闭环。 */
static ChassisMode g_mode;

/**
 * @brief 将左右侧控制量分发给四个实际电机。
 *
 * 同一侧前后轮使用相同命令；各电机安装方向的差异由 Motor 模块处理。
 */
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
  /* 初始化后立即停车，防止外设上电瞬间残留比较值驱动电机。 */
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
    /* 离开自动模式时清除 PI 积分，避免旧误差影响手柄开环输出。 */
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
  /* 自动控制只写目标速度，实际 PWM 在周期任务中闭环计算。 */
  WheelSpeedController_SetTarget(left, right);
  g_mode = CHASSIS_MODE_AUTOMATIC;
}

void Chassis_SetVelocity(int16_t forward, int16_t turn)
{
  Chassis_SetAutoVelocity(forward, turn);
}

void Chassis_ApplyCommand(const ControlCommand *command)
{
  /* 无命令、命令超时或上层明确禁用时，都必须进入停车状态。 */
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

  /* 来源与数值类型不匹配表示协议或调用错误，采用停车而不是猜测含义。 */
  Chassis_Stop();
}

void Chassis_TaskStep(uint32_t dt_ms)
{
  /* 手柄模式已经直接输出 PWM，只有自动模式需要运行轮速 PI。 */
  if (g_mode == CHASSIS_MODE_AUTOMATIC)
  {
    WheelSpeedController_TaskStep(dt_ms);
  }
}

void Chassis_Stop(void)
{
  /* Stop 同时清空闭环状态并关闭所有电机输出。 */
  WheelSpeedController_Stop();
  g_mode = CHASSIS_MODE_STOPPED;
}
