#include "servo.h"

#include "debug_uart.h"
#include "main.h"
#include "tim.h"

#define SERVO_TEST_STEP_US 25

static uint16_t g_servo_pulse_us[2] = {0U, 0U};
static uint8_t g_servo_pulse_valid[2] = {0U, 0U};

static uint16_t Servo_ClampPulse(uint16_t pulse_us)
{
  if (pulse_us < SERVO_MIN_PULSE_US)
  {
    return SERVO_MIN_PULSE_US;
  }

  if (pulse_us > SERVO_MAX_PULSE_US)
  {
    return SERVO_MAX_PULSE_US;
  }

  return pulse_us;
}

void Servo_Init(void)
{
  if (HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_3) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_4) != HAL_OK)
  {
    Error_Handler();
  }

  Servo_SetBothPulse(SERVO_MID_PULSE_US, SERVO_MID_PULSE_US);
}

void Servo_SetPulse(ServoChannel channel, uint16_t pulse_us)
{
  uint32_t tim_channel = TIM_CHANNEL_3;
  const char *channel_name = "left";

  if (channel == SERVO_CHANNEL_RIGHT)
  {
    tim_channel = TIM_CHANNEL_4;
    channel_name = "right";
  }
  else if (channel != SERVO_CHANNEL_LEFT)
  {
    return;
  }

  const uint16_t clamped_pulse_us = Servo_ClampPulse(pulse_us);
  __HAL_TIM_SET_COMPARE(&htim8, tim_channel, clamped_pulse_us);

  if ((g_servo_pulse_valid[channel] == 0U) || (g_servo_pulse_us[channel] != clamped_pulse_us))
  {
    g_servo_pulse_valid[channel] = 1U;
    g_servo_pulse_us[channel] = clamped_pulse_us;
    DebugUart_PrintfIf(DEBUG_LOG_SERVO, "[SERVO] %s=%u us\r\n", channel_name, clamped_pulse_us);
  }
}

void Servo_SetBothPulse(uint16_t left_pulse_us, uint16_t right_pulse_us)
{
  Servo_SetPulse(SERVO_CHANNEL_LEFT, left_pulse_us);
  Servo_SetPulse(SERVO_CHANNEL_RIGHT, right_pulse_us);
}

void Servo_TestTaskStep(void)
{
  static uint16_t left_pulse_us = SERVO_MID_PULSE_US;
  static int16_t step_us = SERVO_TEST_STEP_US;

  int32_t next_pulse_us = (int32_t)left_pulse_us + step_us;

  if (next_pulse_us >= SERVO_MAX_PULSE_US)
  {
    next_pulse_us = SERVO_MAX_PULSE_US;
    step_us = (int16_t)-SERVO_TEST_STEP_US;
  }
  else if (next_pulse_us <= SERVO_MIN_PULSE_US)
  {
    next_pulse_us = SERVO_MIN_PULSE_US;
    step_us = SERVO_TEST_STEP_US;
  }

  left_pulse_us = (uint16_t)next_pulse_us;
  Servo_SetBothPulse(left_pulse_us, (uint16_t)(SERVO_MIN_PULSE_US + SERVO_MAX_PULSE_US - left_pulse_us));
}
