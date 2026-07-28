/**
 * @file battery_monitor.c
 * @brief 读取 C30D V2.2 板载电池分压，输出滤波电压和估算电量。
 */
#include "battery_monitor.h"

#include <stddef.h>
#include <string.h>

#include "adc.h"
#include "main.h"

#define BATTERY_SAMPLE_PERIOD_MS 100U
#define BATTERY_FILTER_SIZE 8U
#define BATTERY_ADC_MAX_VALUE 4095U
#define BATTERY_ADC_REFERENCE_MV 3300U
#define BATTERY_DIVIDER_RATIO 11U
/* 用万用表标定后可调整，例如实测偏低 1% 时改为 1010。 */
#define BATTERY_CALIBRATION_PERMILLE 1000U
#define BATTERY_MIN_VALID_MV 6000U
#define BATTERY_MAX_VALID_MV 15000U
#define BATTERY_LOW_MV 10500U
#define BATTERY_CRITICAL_MV 9900U
#define BATTERY_FAILURE_LIMIT 3U

typedef struct {
  uint16_t voltage_mv;
  uint8_t percent;
} BatteryCurvePoint;

/*
 * E326S 是三串锂电池。电压会随电机负载和电池老化变化，因此这里只提供
 * 显示用途的近似电量，不把百分比当作精密剩余容量。
 */
static const BatteryCurvePoint k_battery_curve[] = {
    {12600U, 100U},
    {12300U, 90U},
    {12000U, 75U},
    {11700U, 60U},
    {11400U, 45U},
    {11100U, 30U},
    {10800U, 20U},
    {10500U, 10U},
    {9900U, 5U},
    {9600U, 0U},
};

static BatterySample g_sample;
static uint16_t g_voltage_history[BATTERY_FILTER_SIZE];
static uint32_t g_voltage_sum = 0U;
static uint32_t g_last_sample_tick = 0U;
static uint8_t g_history_count = 0U;
static uint8_t g_history_index = 0U;
static uint8_t g_consecutive_failures = 0U;

static uint16_t RawAdcToBatteryMv(uint16_t raw_adc);
static uint8_t VoltageToPercent(uint16_t voltage_mv);
static BatteryLevel VoltageToLevel(uint16_t voltage_mv);
static void RecordSample(uint16_t raw_adc);
static void RecordFailure(void);

void BatteryMonitor_Init(void)
{
  (void)memset(&g_sample, 0, sizeof(g_sample));
  (void)memset(g_voltage_history, 0, sizeof(g_voltage_history));
  g_sample.level = BATTERY_LEVEL_UNKNOWN;
  g_voltage_sum = 0U;
  g_last_sample_tick = HAL_GetTick();
  g_history_count = 0U;
  g_history_index = 0U;
  g_consecutive_failures = 0U;
}

void BatteryMonitor_TaskStep(void)
{
  const uint32_t now = HAL_GetTick();
  if ((now - g_last_sample_tick) < BATTERY_SAMPLE_PERIOD_MS)
  {
    return;
  }
  g_last_sample_tick = now;

  if (HAL_ADC_Start(&hadc1) != HAL_OK)
  {
    RecordFailure();
    return;
  }

  /*
   * 480-cycle 采样在当前约 30MHz ADC 时钟下只需约 16us。
   * 1ms 超时只用于故障兜底，不会在正常情况下影响 20ms 控制周期。
   */
  if (HAL_ADC_PollForConversion(&hadc1, 1U) != HAL_OK)
  {
    (void)HAL_ADC_Stop(&hadc1);
    RecordFailure();
    return;
  }

  const uint16_t raw_adc = (uint16_t)HAL_ADC_GetValue(&hadc1);
  (void)HAL_ADC_Stop(&hadc1);
  RecordSample(raw_adc);
}

bool BatteryMonitor_GetSample(BatterySample *sample)
{
  if (sample == NULL)
  {
    return false;
  }

  *sample = g_sample;
  return g_sample.valid;
}

static uint16_t RawAdcToBatteryMv(uint16_t raw_adc)
{
  /*
   * 板载 100k/10k 分压比为 11。先计算标称电压，再应用整数校准系数，
   * 避免在 MCU 上引入浮点运算。
   */
  const uint32_t nominal_mv =
      ((uint32_t)raw_adc * BATTERY_ADC_REFERENCE_MV * BATTERY_DIVIDER_RATIO +
       (BATTERY_ADC_MAX_VALUE / 2U)) /
      BATTERY_ADC_MAX_VALUE;
  return (uint16_t)((nominal_mv * BATTERY_CALIBRATION_PERMILLE + 500U) / 1000U);
}

static uint8_t VoltageToPercent(uint16_t voltage_mv)
{
  const size_t point_count = sizeof(k_battery_curve) / sizeof(k_battery_curve[0]);
  if (voltage_mv >= k_battery_curve[0].voltage_mv)
  {
    return k_battery_curve[0].percent;
  }

  for (size_t i = 0U; i < (point_count - 1U); ++i)
  {
    const BatteryCurvePoint high = k_battery_curve[i];
    const BatteryCurvePoint low = k_battery_curve[i + 1U];
    if (voltage_mv >= low.voltage_mv)
    {
      const uint32_t voltage_span = (uint32_t)high.voltage_mv - low.voltage_mv;
      const uint32_t percent_span = (uint32_t)high.percent - low.percent;
      const uint32_t offset = (uint32_t)voltage_mv - low.voltage_mv;
      return (uint8_t)(low.percent +
                       ((offset * percent_span + (voltage_span / 2U)) / voltage_span));
    }
  }

  return 0U;
}

static BatteryLevel VoltageToLevel(uint16_t voltage_mv)
{
  if (voltage_mv <= BATTERY_CRITICAL_MV)
  {
    return BATTERY_LEVEL_CRITICAL;
  }
  if (voltage_mv <= BATTERY_LOW_MV)
  {
    return BATTERY_LEVEL_LOW;
  }
  return BATTERY_LEVEL_NORMAL;
}

static void RecordSample(uint16_t raw_adc)
{
  const uint16_t voltage_mv = RawAdcToBatteryMv(raw_adc);
  if ((voltage_mv < BATTERY_MIN_VALID_MV) || (voltage_mv > BATTERY_MAX_VALID_MV))
  {
    RecordFailure();
    return;
  }

  g_consecutive_failures = 0U;
  if (g_history_count < BATTERY_FILTER_SIZE)
  {
    g_voltage_history[g_history_index] = voltage_mv;
    g_voltage_sum += voltage_mv;
    ++g_history_count;
  }
  else
  {
    g_voltage_sum -= g_voltage_history[g_history_index];
    g_voltage_history[g_history_index] = voltage_mv;
    g_voltage_sum += voltage_mv;
  }
  g_history_index = (uint8_t)((g_history_index + 1U) % BATTERY_FILTER_SIZE);

  g_sample.raw_adc = raw_adc;
  g_sample.voltage_mv =
      (uint16_t)((g_voltage_sum + (g_history_count / 2U)) / g_history_count);
  g_sample.percent = VoltageToPercent(g_sample.voltage_mv);
  g_sample.level = VoltageToLevel(g_sample.voltage_mv);
  g_sample.valid = true;
}

static void RecordFailure(void)
{
  if (g_consecutive_failures < BATTERY_FAILURE_LIMIT)
  {
    ++g_consecutive_failures;
  }
  if (g_consecutive_failures >= BATTERY_FAILURE_LIMIT)
  {
    g_sample.valid = false;
    g_sample.level = BATTERY_LEVEL_UNKNOWN;
  }
}
