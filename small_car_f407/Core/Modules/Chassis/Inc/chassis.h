/**
 * @file chassis.h
 * @brief 声明底盘执行层，对外统一提供开环、闭环、停车和周期更新接口。
 */
#ifndef CHASSIS_H_
#define CHASSIS_H_

#include "control_types.h"

/** 初始化电机和轮速控制器，并确保四轮停车。 */
void Chassis_Init(void);
/** 设置手柄开环归一化控制量。 */
void Chassis_SetManualVelocity(int16_t forward, int16_t turn);
/** 设置树莓派物理速度控制量，单位 mm/s 和 mrad/s。 */
void Chassis_SetAutoVelocity(int16_t forward, int16_t turn);
/** 兼容旧调用的手柄开环接口，等价于 Chassis_SetManualVelocity。 */
void Chassis_SetVelocity(int16_t forward, int16_t turn);
/** 根据命令来源和 value_type 选择开环或闭环执行路径。 */
void Chassis_ApplyCommand(const ControlCommand *command);
/** 推进轮速闭环，dt_ms 为本次任务实际周期。 */
void Chassis_TaskStep(uint32_t dt_ms);
/** 立即清除目标并停止所有电机。 */
void Chassis_Stop(void);

#endif  // CHASSIS_H_
