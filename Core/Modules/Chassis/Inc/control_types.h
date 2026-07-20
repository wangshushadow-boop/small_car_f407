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

typedef struct {
  /* 指令来源，方便调试时判断当前是谁在控制底盘。 */
  ControlSource source;
  /* false 表示本条指令不可执行，底盘层会停车。 */
  bool enabled;
  /* 前进/后退速度，范围约定为 -1000 到 1000。 */
  int16_t forward;
  /* 左右转向量，范围约定为 -1000 到 1000。 */
  int16_t turn;
} ControlCommand;

#endif  // CONTROL_TYPES_H_
