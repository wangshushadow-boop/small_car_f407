/**
 * @file ultrasonic.h
 * @brief 声明前向超声测距模块及其异步 Echo 边沿接口。
 */
#ifndef ULTRASONIC_H_
#define ULTRASONIC_H_

#include <stdbool.h>
#include <stdint.h>

/** 一次测距结果；valid=false 时 distance_mm 不应参与控制。 */
typedef struct {
  bool valid;
  uint16_t distance_mm;
} UltrasonicSample;

/** 初始化 Trig/Echo 状态和首次测量时间。 */
void Ultrasonic_Init(void);
/** 非阻塞测距状态机周期入口，应由 RTOS 任务高频调用。 */
void Ultrasonic_TaskStep(void);
/** 复制最近测距结果，返回当前结果是否有效。 */
bool Ultrasonic_GetSample(UltrasonicSample *sample);
/** 按运行时近障阈值判断是否需要禁止继续前进。 */
bool Ultrasonic_IsObstacleNear(void);
/** GPIO EXTI 回调入口，用 Echo 上升沿/下降沿计算脉宽。 */
void Ultrasonic_OnEchoEdge(uint16_t gpio_pin);

#endif  // ULTRASONIC_H_
