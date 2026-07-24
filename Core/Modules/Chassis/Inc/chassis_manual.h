/**
 * @file chassis_manual.h
 * @brief 声明手柄归一化前进量/转向量到左右轮输出的开环混控接口。
 */
#ifndef CHASSIS_MANUAL_H_
#define CHASSIS_MANUAL_H_

#include <stdint.h>

/** 将 -1000..1000 的前进量和转向量混合为左右轮归一化输出。 */
void ChassisManual_Mix(int16_t forward, int16_t turn, int16_t *left, int16_t *right);

#endif  // CHASSIS_MANUAL_H_
