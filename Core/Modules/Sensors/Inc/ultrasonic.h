#ifndef ULTRASONIC_H_
#define ULTRASONIC_H_

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  bool valid;
  uint16_t distance_mm;
} UltrasonicSample;

void Ultrasonic_Init(void);
void Ultrasonic_TaskStep(void);
bool Ultrasonic_GetSample(UltrasonicSample *sample);
bool Ultrasonic_IsObstacleNear(void);
void Ultrasonic_OnEchoEdge(uint16_t gpio_pin);

#endif  // ULTRASONIC_H_
