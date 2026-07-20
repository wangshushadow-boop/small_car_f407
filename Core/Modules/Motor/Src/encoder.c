#include "encoder.h"

#include "main.h"
#include "tim.h"

typedef struct {
  /* 每路编码器对应一个定时器的 Encoder Mode。 */
  TIM_HandleTypeDef *timer;
  /* 软件累计计数，解决 16 位硬件计数器溢出后只能看短范围的问题。 */
  int32_t count;
  /* 上一次读取的硬件原始计数值。 */
  uint16_t last_raw_count;
  /* 最近一个调度周期内的增量，可用于后续速度估算。 */
  int16_t delta;
} EncoderState;

static EncoderState g_encoders[] = {
    /* 编码器定时器映射：A/B/C/D 分别使用 TIM2/TIM3/TIM4/TIM5。 */
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
  /* 初始化时清零硬件计数器和软件累计值，然后启动编码器模式。 */
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
  /*
   * 周期性读取 16 位计数器差值。
   * 使用 int16_t 强制转换可以自然处理 16 位回绕：例如 65535 -> 0 的差值为 +1。
   */
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
  /* 返回最近一次 TaskStep 更新后的快照。 */
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
  /* 软件和硬件计数同时清零，避免下一次 delta 出现突变。 */
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
