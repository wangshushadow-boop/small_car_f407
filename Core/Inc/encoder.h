#ifndef ENCODER_H_
#define ENCODER_H_

#include <stdint.h>

#include "motor.h"

typedef struct {
  int32_t count;
  int16_t delta;
} EncoderSample;

void Encoder_Init(void);
void Encoder_TaskStep(void);
EncoderSample Encoder_GetSample(MotorId motor);
void Encoder_Reset(MotorId motor);
void Encoder_ResetAll(void);

#endif  // ENCODER_H_
