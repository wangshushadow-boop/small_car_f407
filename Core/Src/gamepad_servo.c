#include "gamepad_servo.h"

#include <stdint.h>

#include "gamepad.h"
#include "servo.h"

#define GAMEPAD_SERVO_CENTER_VALUE 127
#define GAMEPAD_SERVO_MAX_VALUE 255
#define GAMEPAD_SERVO_DEADBAND 6
#define GAMEPAD_SERVO_MAX_STEP_US 30

static int16_t AxisDeviation(uint8_t axis)
{
  /* 计算摇杆相对中心点的偏移量，右/下方向通常为正。 */
  return (int16_t)axis - GAMEPAD_SERVO_CENTER_VALUE;
}

static int16_t AxisToStep(uint8_t axis)
{
  int16_t centered = AxisDeviation(axis);
  /* 右摇杆回中时不改变舵机位置，实现“松手保持当前位置”。 */
  if ((centered > -GAMEPAD_SERVO_DEADBAND) && (centered < GAMEPAD_SERVO_DEADBAND))
  {
    return 0;
  }

  /*
   * 将摇杆偏移转换为每个调度周期的脉宽增量。
   * 偏移越大，舵机移动越快；最大每次变化 GAMEPAD_SERVO_MAX_STEP_US。
   */
  return (int16_t)(((int32_t)centered * GAMEPAD_SERVO_MAX_STEP_US) /
                   (GAMEPAD_SERVO_MAX_VALUE - GAMEPAD_SERVO_CENTER_VALUE));
}

static uint16_t ClampPulse(int32_t pulse_us)
{
  /* 舵机脉宽限制在安全范围内，避免打到机械限位。 */
  if (pulse_us < (int32_t)SERVO_MIN_PULSE_US)
  {
    return SERVO_MIN_PULSE_US;
  }

  if (pulse_us > (int32_t)SERVO_MAX_PULSE_US)
  {
    return SERVO_MAX_PULSE_US;
  }

  return (uint16_t)pulse_us;
}

void GamepadServo_Init(void)
{
  /* 上电时两路舵机先回到中位。 */
  Servo_SetBothPulse(SERVO_MID_PULSE_US, SERVO_MID_PULSE_US);
}

void GamepadServo_TaskStep(void)
{
  static uint16_t left_pulse_us = SERVO_MID_PULSE_US;
  static uint16_t right_pulse_us = SERVO_MID_PULSE_US;
  GamepadState state;

  if (!Gamepad_GetState(&state))
  {
    /* 手柄未连接时不改舵机位置，保持最后一次有效位置。 */
    return;
  }

  const int16_t left_step_us = AxisToStep(state.rx);
  const int16_t right_step_us = AxisToStep(state.ry);
  if ((left_step_us == 0) && (right_step_us == 0))
  {
    /* 右摇杆在死区内，不更新 PWM，也不产生串口日志。 */
    return;
  }

  /* 使用静态变量保存目标脉宽，所以摇杆回中后舵机不会自动回中。 */
  left_pulse_us = ClampPulse((int32_t)left_pulse_us + left_step_us);
  right_pulse_us = ClampPulse((int32_t)right_pulse_us + right_step_us);
  Servo_SetBothPulse(left_pulse_us, right_pulse_us);
}
