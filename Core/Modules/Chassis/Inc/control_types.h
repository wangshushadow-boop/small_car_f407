/**
 * @file control_types.h
 * @brief 定义手柄、树莓派和安全逻辑之间共享的底盘控制命令类型。
 */
#ifndef CONTROL_TYPES_H_
#define CONTROL_TYPES_H_

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  /* 当前没有任何有效输入，底盘应保持停车。 */
  CONTROL_SOURCE_NONE = 0,
  /* 预留给上位机通信协议的控制源。 */
  CONTROL_SOURCE_HOST,
  /* USB 手柄控制源，当前优先级高于上位机。 */
  CONTROL_SOURCE_GAMEPAD,
  /* 安全保护控制源，例如超声波近距离障碍触发强制停车。 */
  CONTROL_SOURCE_SAFETY,
} ControlSource;

typedef enum {
  /* 手柄使用的电机归一化输出，范围为 -1000 到 1000。 */
  CONTROL_VALUE_NORMALIZED = 0,
  /* ROS/上位机使用的物理速度：forward 为 mm/s，turn 为 mrad/s。 */
  CONTROL_VALUE_PHYSICAL_VELOCITY = 1,
} ControlValueType;

typedef struct {
  /* 指令来源，方便调试时判断当前是谁在控制底盘。 */
  ControlSource source;
  /* 指明 forward/turn 的单位，避免手柄输出与 ROS 物理速度混用。 */
  ControlValueType value_type;
  /* false 表示本条指令不可执行，底盘层会停车。 */
  bool enabled;
  /* 前进量；具体单位由 value_type 指定。 */
  int16_t forward;
  /* 转向量；具体单位由 value_type 指定。 */
  int16_t turn;
} ControlCommand;

#endif  // CONTROL_TYPES_H_
