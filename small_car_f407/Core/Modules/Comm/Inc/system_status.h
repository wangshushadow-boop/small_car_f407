/**
 * @file system_status.h
 * @brief 声明系统遥测汇总任务，将传感器和控制状态周期上传给树莓派。
 */
#ifndef SYSTEM_STATUS_H_
#define SYSTEM_STATUS_H_

#include "control_types.h"

/** 初始化遥测节拍和传感器缓存。 */
void SystemStatus_Init(void);
/** 采集并按配置周期发送控制、传感器、里程计和诊断消息。 */
void SystemStatus_TaskStep(const ControlCommand *command);

#endif  // SYSTEM_STATUS_H_
