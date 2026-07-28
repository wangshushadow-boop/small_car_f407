/**
 * @file battery_monitor.h
 * @brief 声明三串锂电池电压采样、滤波和电量估算接口。
 */
#ifndef BATTERY_MONITOR_H_
#define BATTERY_MONITOR_H_

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  BATTERY_LEVEL_UNKNOWN = 0,
  BATTERY_LEVEL_NORMAL,
  BATTERY_LEVEL_LOW,
  BATTERY_LEVEL_CRITICAL,
} BatteryLevel;

typedef struct {
  uint16_t raw_adc;
  uint16_t voltage_mv;
  uint8_t percent;
  BatteryLevel level;
  bool valid;
} BatterySample;

/** 清空滤波器和采样状态；ADC1 外设应已由 MX_ADC1_Init() 初始化。 */
void BatteryMonitor_Init(void);
/** 以 10Hz 节拍执行一次软件触发采样，并更新滤波后的电池状态。 */
void BatteryMonitor_TaskStep(void);
/** 返回最近一次有效电池状态；尚未取得有效样本时返回 false。 */
bool BatteryMonitor_GetSample(BatterySample *sample);

#endif  // BATTERY_MONITOR_H_
