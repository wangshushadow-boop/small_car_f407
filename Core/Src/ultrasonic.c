#include "ultrasonic.h"

#include <stddef.h>

#include "debug_uart.h"
#include "main.h"

#define ULTRASONIC_TRIGGER_PERIOD_MS 100U
#define ULTRASONIC_ECHO_TIMEOUT_US 40000U
#define ULTRASONIC_TRIGGER_PULSE_US 10U
#define ULTRASONIC_NEAR_DISTANCE_MM 200U

static volatile uint8_t g_sample_valid = 0U;
static volatile uint16_t g_distance_mm = 0U;
static volatile uint32_t g_echo_pulse_us = 0U;
static volatile uint32_t g_echo_rise_us = 0U;
static volatile uint8_t g_echo_capturing = 0U;
static volatile uint8_t g_echo_waiting = 0U;
static volatile uint8_t g_report_pending = 0U;
static uint32_t g_last_trigger_ms = 0U;
static uint32_t g_cycles_per_us = 1U;

static uint32_t GetMicros(void)
{
  return DWT->CYCCNT / g_cycles_per_us;
}

static void DelayMicros(uint32_t delay_us)
{
  const uint32_t start_us = GetMicros();
  while ((GetMicros() - start_us) < delay_us)
  {
  }
}

static void TriggerMeasurement(void)
{
  g_echo_waiting = 1U;
  g_echo_capturing = 0U;
  HAL_GPIO_WritePin(ULTRASONIC_TRIG_GPIO_Port, ULTRASONIC_TRIG_Pin, GPIO_PIN_RESET);
  DelayMicros(2U);
  HAL_GPIO_WritePin(ULTRASONIC_TRIG_GPIO_Port, ULTRASONIC_TRIG_Pin, GPIO_PIN_SET);
  DelayMicros(ULTRASONIC_TRIGGER_PULSE_US);
  HAL_GPIO_WritePin(ULTRASONIC_TRIG_GPIO_Port, ULTRASONIC_TRIG_Pin, GPIO_PIN_RESET);
}

void Ultrasonic_Init(void)
{
  g_sample_valid = 0U;
  g_distance_mm = 0U;
  g_echo_pulse_us = 0U;
  g_echo_rise_us = 0U;
  g_echo_capturing = 0U;
  g_echo_waiting = 0U;
  g_report_pending = 0U;
  g_last_trigger_ms = 0U;

  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

  g_cycles_per_us = HAL_RCC_GetHCLKFreq() / 1000000U;
  if (g_cycles_per_us == 0U)
  {
    g_cycles_per_us = 1U;
  }
}

void Ultrasonic_TaskStep(void)
{
  const uint32_t now_ms = HAL_GetTick();
  if ((g_echo_waiting != 0U) && ((GetMicros() - g_echo_rise_us) > ULTRASONIC_ECHO_TIMEOUT_US))
  {
    g_echo_waiting = 0U;
    g_echo_capturing = 0U;
    g_sample_valid = 0U;
    DebugUart_WriteStringIf(DEBUG_LOG_ULTRASONIC, "[ULTRA] timeout\r\n");
  }

  if (g_report_pending != 0U)
  {
    g_report_pending = 0U;
    DebugUart_PrintfIf(DEBUG_LOG_ULTRASONIC,
                       "[ULTRA] distance=%u mm pulse=%lu us\r\n",
                       g_distance_mm,
                       g_echo_pulse_us);
  }

  if ((g_echo_waiting == 0U) && ((now_ms - g_last_trigger_ms) >= ULTRASONIC_TRIGGER_PERIOD_MS))
  {
    g_last_trigger_ms = now_ms;
    TriggerMeasurement();
  }
}

bool Ultrasonic_GetSample(UltrasonicSample *sample)
{
  if (sample == NULL)
  {
    return false;
  }

  sample->valid = g_sample_valid != 0U;
  sample->distance_mm = g_distance_mm;
  return sample->valid;
}

bool Ultrasonic_IsObstacleNear(void)
{
  if (g_sample_valid == 0U)
  {
    return false;
  }

  return g_distance_mm < ULTRASONIC_NEAR_DISTANCE_MM;
}

void Ultrasonic_OnEchoEdge(uint16_t gpio_pin)
{
  if (gpio_pin != ULTRASONIC_ECHO_Pin)
  {
    return;
  }

  const uint32_t now_us = GetMicros();
  if (HAL_GPIO_ReadPin(ULTRASONIC_ECHO_GPIO_Port, ULTRASONIC_ECHO_Pin) == GPIO_PIN_SET)
  {
    g_echo_rise_us = now_us;
    g_echo_capturing = 1U;
    g_echo_waiting = 1U;
    return;
  }

  if (g_echo_capturing == 0U)
  {
    return;
  }

  const uint32_t pulse_us = now_us - g_echo_rise_us;
  g_echo_capturing = 0U;
  g_echo_waiting = 0U;

  if (pulse_us > ULTRASONIC_ECHO_TIMEOUT_US)
  {
    g_sample_valid = 0U;
    return;
  }

  g_distance_mm = (uint16_t)((pulse_us * 343U) / 2000U);
  g_echo_pulse_us = pulse_us;
  g_sample_valid = 1U;
  g_report_pending = 1U;
}
