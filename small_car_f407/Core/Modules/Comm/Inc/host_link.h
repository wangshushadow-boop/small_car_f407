/**
 * @file host_link.h
 * @brief 声明 USART1 文本调试命令接收和上位机控制接口。
 *
 * USART1 面向人工调试；正式 ROS 二进制通信使用 USART3 的 RaspiLink。
 */
#ifndef HOST_LINK_H_
#define HOST_LINK_H_

#include <stdbool.h>

#include "control_types.h"
#include "usart.h"

/** 初始化 USART1 单字节中断接收和命令缓存。 */
void HostLink_Init(void);
/** 解析一行文本命令并维护控制超时。 */
void HostLink_TaskStep(void);
/** 返回 USART1 文本命令产生的最新有效控制命令。 */
bool HostLink_GetControlCommand(ControlCommand *command);
/** 转发 HAL UART 接收完成回调。 */
void HostLink_OnUartRxCpltCallback(UART_HandleTypeDef *huart);

#endif  // HOST_LINK_H_
