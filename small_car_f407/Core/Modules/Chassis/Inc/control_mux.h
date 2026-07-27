/**
 * @file control_mux.h
 * @brief 声明底盘控制源仲裁器，统一处理手柄、上位机和安全优先级。
 */
#ifndef CONTROL_MUX_H_
#define CONTROL_MUX_H_

#include <stdbool.h>

#include "control_types.h"

/** 清空仲裁器内部状态。 */
void ControlMux_Init(void);
/**
 * @brief 按安全 > 手柄 > 上位机顺序选择一条控制命令。
 * @return 选到有效命令返回 true；无输入时返回停车命令和 false。
 */
bool ControlMux_SelectCommand(ControlCommand *command);

#endif  // CONTROL_MUX_H_
