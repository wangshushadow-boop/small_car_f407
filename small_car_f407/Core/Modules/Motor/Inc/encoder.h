/**
 * @file encoder.h
 * @brief 声明四路定时器编码器的累计值和周期增量采集接口。
 */
#ifndef ENCODER_H_
#define ENCODER_H_

#include <stdint.h>

#include "motor.h"

/** 单路编码器累计计数和最近采样周期增量。 */
typedef struct {
  int32_t count;
  int16_t delta;
} EncoderSample;

/** 启动四个编码器定时器并记录初值。 */
void Encoder_Init(void);
/** 读取硬件计数器，换算为相对上周期的有符号增量。 */
void Encoder_TaskStep(void);
/** 获取指定电机的最近编码器快照。 */
EncoderSample Encoder_GetSample(MotorId motor);
/** 清零单路或全部编码器的软件累计值。 */
void Encoder_Reset(MotorId motor);
void Encoder_ResetAll(void);

#endif  // ENCODER_H_
