#include "ultrasonic.h"

#include <stddef.h>

#include "chassis_params.h"
#include "debug_uart.h"
#include "main.h"

/* 超声测距触发周期，单位 ms。数值越小测距越频繁，但也更占用任务时间。 */
#define ULTRASONIC_TRIGGER_PERIOD_MS 100U

/* ECHO 最大等待时间，单位 us。超过该时间认为本次测距超时无效。 */
#define ULTRASONIC_ECHO_TIMEOUT_US 40000U

/* TRIG 触发脉冲宽度，单位 us。HC-SR04 类模块通常要求至少 10us。 */
#define ULTRASONIC_TRIGGER_PULSE_US 10U

/*
 * 前方避障阈值。
 *
 * 当超声测距有效且距离小于该值时，control_mux.c 会禁止继续向前，
 * 但仍允许后退和转向。因此如果出现“向后能走、向前不走”，除了检查手柄输出，
 * 也要看 sensor_monitor 的 ultra 距离是否小于这里的阈值。
 *
 * 标定或室内窄空间调试时，如果确认前方安全但频繁误触发，可以临时改小，
 * 例如 200 -> 80。正式运行时再按实际安全距离调回合适值。
 */
#define ULTRASONIC_NEAR_DISTANCE_MM 200U

/* 这些变量会在 EXTI 中断和普通任务里同时访问，所以使用 volatile。 */
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
  /* DWT CYCCNT 是 CPU 周期计数器，除以每微秒周期数即可得到微秒时间。 */
  return DWT->CYCCNT / g_cycles_per_us;
}

static void DelayMicros(uint32_t delay_us)
{
  /* 这里只用于 10us 触发脉冲，短时间忙等比开定时器更简单。 */
  const uint32_t start_us = GetMicros();
  while ((GetMicros() - start_us) < delay_us)
  {
  }
}

static void TriggerMeasurement(void)
{
  /* HC-SR04 类模块需要 TRIG 至少 10us 高电平来启动一次测距。 */
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

  /*
   * 使能 DWT 周期计数器，用来测量 ECHO 高电平宽度。
   * 这样不需要额外占用一个硬件定时器。
   */
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
  /* 等待 ECHO 太久说明本次测距无回波，标记为无效样本。 */
  if ((g_echo_waiting != 0U) && ((GetMicros() - g_echo_rise_us) > ULTRASONIC_ECHO_TIMEOUT_US))
  {
    g_echo_waiting = 0U;
    g_echo_capturing = 0U;
    g_sample_valid = 0U;
    DebugUart_WriteStringIf(DEBUG_LOG_ULTRASONIC, "[ULTRA] timeout\r\n");
  }

  if (g_report_pending != 0U)
  {
    /* 中断里只记录数据，不打印；打印放到任务里，避免中断执行时间过长。 */
    g_report_pending = 0U;
    DebugUart_PrintfIf(DEBUG_LOG_ULTRASONIC,
                       "[ULTRA] distance=%u mm pulse=%lu us\r\n",
                       g_distance_mm,
                       g_echo_pulse_us);
  }

  if ((g_echo_waiting == 0U) && ((now_ms - g_last_trigger_ms) >= ULTRASONIC_TRIGGER_PERIOD_MS))
  {
    /* 没有正在等待的回波时，按固定周期触发下一次测距。 */
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
  /* 没有有效测距时不触发安全停车，避免传感器未接时一直锁死。 */
  if (g_sample_valid == 0U)
  {
    return false;
  }

  const ChassisParams params = ChassisParams_Get();
  return g_distance_mm < (uint16_t)params.ultra_near_distance_mm;
}

void Ultrasonic_OnEchoEdge(uint16_t gpio_pin)
{
  /* 该函数由 HAL_GPIO_EXTI_Callback 调用，只处理超声波 ECHO 引脚。 */
  if (gpio_pin != ULTRASONIC_ECHO_Pin)
  {
    return;
  }

  const uint32_t now_us = GetMicros();
  if (HAL_GPIO_ReadPin(ULTRASONIC_ECHO_GPIO_Port, ULTRASONIC_ECHO_Pin) == GPIO_PIN_SET)
  {
    /* 上升沿：记录 ECHO 开始时间。 */
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
  /* 下降沿：高电平宽度就是声波往返时间。 */
  g_echo_capturing = 0U;
  g_echo_waiting = 0U;

  if (pulse_us > ULTRASONIC_ECHO_TIMEOUT_US)
  {
    g_sample_valid = 0U;
    return;
  }

  /*
   * 距离换算：
   * 声速约 343m/s = 0.343mm/us，ECHO 是往返时间，所以除以 2。
   * distance_mm = pulse_us * 343 / 1000 / 2 = pulse_us * 343 / 2000。
   */
  g_distance_mm = (uint16_t)((pulse_us * 343U) / 2000U);
  g_echo_pulse_us = pulse_us;
  g_sample_valid = 1U;
  g_report_pending = 1U;
}
