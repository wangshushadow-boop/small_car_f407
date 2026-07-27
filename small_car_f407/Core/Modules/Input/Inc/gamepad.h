/**
 * @file gamepad.h
 * @brief 声明 USB 无线手柄状态缓存、按键定义和底盘控制命令生成接口。
 */
#ifndef GAMEPAD_H_
#define GAMEPAD_H_

#include <stdbool.h>
#include <stdint.h>

#include "control_types.h"

/** 归一化后的手柄输入快照；摇杆中心约为 128。 */
typedef struct {
  bool connected;
  uint8_t lx;
  uint8_t ly;
  uint8_t rx;
  uint8_t ry;
  uint16_t buttons;
} GamepadState;

/** 按统一位序定义的 16 个手柄按钮。 */
typedef enum {
  GAMEPAD_BUTTON_SELECT = 0,
  GAMEPAD_BUTTON_LEFT_STICK,
  GAMEPAD_BUTTON_RIGHT_STICK,
  GAMEPAD_BUTTON_START,
  GAMEPAD_BUTTON_UP,
  GAMEPAD_BUTTON_RIGHT,
  GAMEPAD_BUTTON_DOWN,
  GAMEPAD_BUTTON_LEFT,
  GAMEPAD_BUTTON_L2,
  GAMEPAD_BUTTON_R2,
  GAMEPAD_BUTTON_L1,
  GAMEPAD_BUTTON_R1,
  GAMEPAD_BUTTON_GREEN,
  GAMEPAD_BUTTON_RED,
  GAMEPAD_BUTTON_BLUE,
  GAMEPAD_BUTTON_PINK,
} GamepadButton;

/** 初始化断连状态和摇杆中心值。 */
void Gamepad_Init(void);
/** 周期打印调试状态并执行连接超时检查。 */
void Gamepad_TaskStep(void);
/** 复制最近手柄状态，返回是否已经连接。 */
bool Gamepad_GetState(GamepadState *state);
/** 将左摇杆转换为高优先级手柄底盘命令。 */
bool Gamepad_GetControlCommand(ControlCommand *command);
/** USB 类驱动完成解码后调用，用新报告更新状态缓存。 */
void Gamepad_UpdateFromUsb(bool connected,
                           uint8_t lx,
                           uint8_t ly,
                           uint8_t rx,
                           uint8_t ry,
                           uint16_t buttons);

#endif  // GAMEPAD_H_
