#include "gamepad.h"

#include <string.h>

#include "debug_uart.h"
#include "motor.h"

#define GAMEPAD_CENTER_VALUE 127
#define GAMEPAD_DEADBAND 8
#define GAMEPAD_DEBUG_PERIOD 10U

/* 保存最近一次 USB 解码后的手柄状态；业务层只读这个统一结构。 */
static GamepadState g_gamepad_state = {
    .connected = false,
    .lx = GAMEPAD_CENTER_VALUE,
    .ly = GAMEPAD_CENTER_VALUE,
    .rx = GAMEPAD_CENTER_VALUE,
    .ry = GAMEPAD_CENTER_VALUE,
    .buttons = 0U,
};

static bool g_last_connected = false;
static uint32_t g_debug_counter = 0U;

static int16_t ApplyDeadband(int16_t value)
{
  /* 摇杆回中时常见值会在中心附近跳动，死区用于过滤这种微小抖动。 */
  if ((value > -GAMEPAD_DEADBAND) && (value < GAMEPAD_DEADBAND))
  {
    return 0;
  }
  return value;
}

void Gamepad_Init(void)
{
  /* 初始化为“未连接 + 所有摇杆居中”，避免未插手柄时产生误控制。 */
  Gamepad_UpdateFromUsb(false,
                        GAMEPAD_CENTER_VALUE,
                        GAMEPAD_CENTER_VALUE,
                        GAMEPAD_CENTER_VALUE,
                        GAMEPAD_CENTER_VALUE,
                        0U);
}

void Gamepad_TaskStep(void)
{
  /* 只在连接状态变化时打印一次，避免串口被重复状态刷屏。 */
  if (g_gamepad_state.connected != g_last_connected)
  {
    DebugUart_PrintfIf(DEBUG_LOG_GAMEPAD,
                       "[GAMEPAD] %s\r\n",
                       g_gamepad_state.connected ? "connected" : "disconnected");
    g_last_connected = g_gamepad_state.connected;
    g_debug_counter = 0U;
  }

  /* 手柄数据打印频率较低，打开 pad 日志后便于观察摇杆原始值。 */
  if (g_gamepad_state.connected && ((g_debug_counter % GAMEPAD_DEBUG_PERIOD) == 0U))
  {
    DebugUart_PrintfIf(DEBUG_LOG_GAMEPAD_DATA,
                       "[GAMEPAD] LX=%u LY=%u RX=%u RY=%u BTN=0x%04X\r\n",
                       g_gamepad_state.lx,
                       g_gamepad_state.ly,
                       g_gamepad_state.rx,
                       g_gamepad_state.ry,
                       g_gamepad_state.buttons);
  }
  ++g_debug_counter;
}

bool Gamepad_GetState(GamepadState *state)
{
  if (state == NULL)
  {
    return false;
  }

  /* 复制一份快照给调用者，避免外部直接修改全局手柄状态。 */
  memcpy(state, &g_gamepad_state, sizeof(*state));
  return g_gamepad_state.connected;
}

bool Gamepad_GetControlCommand(ControlCommand *command)
{
  if (command == NULL)
  {
    return false;
  }

  command->source = CONTROL_SOURCE_GAMEPAD;
  command->enabled = false;
  command->forward = 0;
  command->turn = 0;

  if (!g_gamepad_state.connected)
  {
    return false;
  }

  /*
   * 左摇杆控制底盘：
   * - LY 越小表示越向前推，所以使用 center - ly 得到“前进为正”。
   * - LX 越大表示越向右推，所以使用 lx - center 得到“右转为正”。
   */
  const int16_t centered_ly =
      ApplyDeadband((int16_t)GAMEPAD_CENTER_VALUE - (int16_t)g_gamepad_state.ly);
  const int16_t centered_lx =
      ApplyDeadband((int16_t)g_gamepad_state.lx - (int16_t)GAMEPAD_CENTER_VALUE);
  command->enabled = true;
  /* 将 8 位摇杆偏移量按比例映射到电机速度范围 -1000 到 1000。 */
  command->forward = (int16_t)((centered_ly * MOTOR_MAX_SPEED) / GAMEPAD_CENTER_VALUE);
  command->turn = (int16_t)((centered_lx * MOTOR_MAX_SPEED) / GAMEPAD_CENTER_VALUE);
  return true;
}

void Gamepad_UpdateFromUsb(bool connected,
                           uint8_t lx,
                           uint8_t ly,
                           uint8_t rx,
                           uint8_t ry,
                           uint16_t buttons)
{
  /* USB Host 解码完成后调用这里，把不同手柄模式统一成 GamepadState。 */
  g_gamepad_state.connected = connected;
  g_gamepad_state.lx = lx;
  g_gamepad_state.ly = ly;
  g_gamepad_state.rx = rx;
  g_gamepad_state.ry = ry;
  g_gamepad_state.buttons = buttons;
}
