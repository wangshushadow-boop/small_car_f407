#include "encoder.h"

#include "main.h"
#include "tim.h"

typedef struct {
  TIM_HandleTypeDef *timer;
  int32_t count;
  uint16_t last_raw_count;
  int16_t delta;
} EncoderState;

static EncoderState g_encoders[] = {
    [MOTOR_A] = {&htim2, 0, 0U, 0},
    [MOTOR_B] = {&htim3, 0, 0U, 0},
    [MOTOR_C] = {&htim4, 0, 0U, 0},
    [MOTOR_D] = {&htim5, 0, 0U, 0},
};

static uint16_t Encoder_ReadRawCount(const EncoderState *encoder)
{
  return (uint16_t)__HAL_TIM_GET_COUNTER(encoder->timer);
}

void Encoder_Init(void)
{
  for (MotorId motor = MOTOR_A; motor <= MOTOR_D; ++motor)
  {
    EncoderState *encoder = &g_encoders[motor];
    __HAL_TIM_SET_COUNTER(encoder->timer, 0U);
    encoder->count = 0;
    encoder->last_raw_count = 0U;
    encoder->delta = 0;

    if (HAL_TIM_Encoder_Start(encoder->timer, TIM_CHANNEL_ALL) != HAL_OK)
    {
      Error_Handler();
    }
  }
}

void Encoder_TaskStep(void)
{
  for (MotorId motor = MOTOR_A; motor <= MOTOR_D; ++motor)
  {
    EncoderState *encoder = &g_encoders[motor];
    const uint16_t raw_count = Encoder_ReadRawCount(encoder);
    const int16_t delta = (int16_t)(raw_count - encoder->last_raw_count);
    encoder->last_raw_count = raw_count;
    encoder->delta = delta;
    encoder->count += delta;
  }
}

EncoderSample Encoder_GetSample(MotorId motor)
{
  EncoderSample sample = {
      .count = 0,
      .delta = 0,
  };

  if (motor > MOTOR_D)
  {
    return sample;
  }

  sample.count = g_encoders[motor].count;
  sample.delta = g_encoders[motor].delta;
  return sample;
}

void Encoder_Reset(MotorId motor)
{
  if (motor > MOTOR_D)
  {
    return;
  }

  EncoderState *encoder = &g_encoders[motor];
  __HAL_TIM_SET_COUNTER(encoder->timer, 0U);
  encoder->count = 0;
  encoder->last_raw_count = 0U;
  encoder->delta = 0;
}

void Encoder_ResetAll(void)
{
  for (MotorId motor = MOTOR_A; motor <= MOTOR_D; ++motor)
  {
    Encoder_Reset(motor);
  }
}
