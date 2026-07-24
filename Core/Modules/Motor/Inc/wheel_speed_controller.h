/**
 * @file wheel_speed_controller.h
 * @brief 声明左右轮物理速度闭环控制器。
 *
 * 控制器仅处理树莓派物理速度命令；手柄命令保留开环输出以保证人工接管可靠。
 */
#ifndef WHEEL_SPEED_CONTROLLER_H_
#define WHEEL_SPEED_CONTROLLER_H_

#include <stdint.h>

/** 清空目标、斜坡、积分项和输出。 */
void WheelSpeedController_Init(void);
/** 设置左右轮目标线速度，单位 mm/s。 */
void WheelSpeedController_SetTarget(int16_t left_mm_s, int16_t right_mm_s);
/** 使用编码器反馈推进一次 PI 控制，dt_ms 为实际周期。 */
void WheelSpeedController_TaskStep(uint32_t dt_ms);
/** 清零控制器状态并立即停止四个电机。 */
void WheelSpeedController_Stop(void);

#endif  // WHEEL_SPEED_CONTROLLER_H_
