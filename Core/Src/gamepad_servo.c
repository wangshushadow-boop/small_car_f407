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
  return (int16_t)axis - GAMEPAD_SERVO_CENTER_VALUE;
}

static int16_t AxisToStep(uint8_t axis)
{
  int16_t centered = AxisDeviation(axis);
  if ((centered > -GAMEPAD_SERVO_DEADBAND) && (centered < GAMEPAD_SERVO_DEADBAND))
  {
    return 0;
  }

  return (int16_t)(((int32_t)centered * GAMEPAD_SERVO_MAX_STEP_US) /
                   (GAMEPAD_SERVO_MAX_VALUE - GAMEPAD_SERVO_CENTER_VALUE));
}

static uint16_t ClampPulse(int32_t pulse_us)
{
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
  Servo_SetBothPulse(SERVO_MID_PULSE_US, SERVO_MID_PULSE_US);
}

void GamepadServo_TaskStep(void)
{
  static uint16_t left_pulse_us = SERVO_MID_PULSE_US;
  static uint16_t right_pulse_us = SERVO_MID_PULSE_US;
  GamepadState state;

  if (!Gamepad_GetState(&state))
  {
    return;
  }

  const int16_t left_step_us = AxisToStep(state.rx);
  const int16_t right_step_us = AxisToStep(state.ry);
  if ((left_step_us == 0) && (right_step_us == 0))
  {
    return;
  }

  left_pulse_us = ClampPulse((int32_t)left_pulse_us + left_step_us);
  right_pulse_us = ClampPulse((int32_t)right_pulse_us + right_step_us);
  Servo_SetBothPulse(left_pulse_us, right_pulse_us);
}
