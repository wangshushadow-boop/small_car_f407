#ifndef WHEEL_SPEED_CONTROLLER_H_
#define WHEEL_SPEED_CONTROLLER_H_

#include <stdint.h>

/*
 * 树莓派发送的是底盘物理速度，轮速控制器负责把左右轮目标速度转换为 PWM。
 * 手柄仍走原有开环控制，避免闭环参数尚未标定时影响人工接管。
 */
void WheelSpeedController_Init(void);
void WheelSpeedController_SetTarget(int16_t left_mm_s, int16_t right_mm_s);
void WheelSpeedController_TaskStep(uint32_t dt_ms);
void WheelSpeedController_Stop(void);

#endif  // WHEEL_SPEED_CONTROLLER_H_
