#include "ultrasonic.h"

#include <stddef.h>

static UltrasonicSample g_ultrasonic_sample = {
    .valid = false,
    .distance_mm = 0U,
};

void Ultrasonic_Init(void)
{
  g_ultrasonic_sample.valid = false;
  g_ultrasonic_sample.distance_mm = 0U;
}

void Ultrasonic_TaskStep(void)
{
}

bool Ultrasonic_GetSample(UltrasonicSample *sample)
{
  if (sample == NULL)
  {
    return false;
  }

  *sample = g_ultrasonic_sample;
  return g_ultrasonic_sample.valid;
}

bool Ultrasonic_IsObstacleNear(void)
{
  if (!g_ultrasonic_sample.valid)
  {
    return false;
  }

  return g_ultrasonic_sample.distance_mm < 200U;
}
