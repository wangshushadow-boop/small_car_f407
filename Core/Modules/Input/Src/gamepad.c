#include "gamepad.h"

/*
 * 手柄业务层。
 *
 * gamepad_usb 负责解析 USB HID 原始数据，本文件只关心统一后的 GamepadState。
 * 左摇杆映射为底盘前进和转向，起步力、最大输出等手感参数来自 ChassisParams。
 */

#include <string.h>

#include "chassis_params.h"
#include "debug_uart.h"
#include "motor.h"

/* 手柄摇杆中心值。原始数据约为 0-255，中间位置约为 127。 */
#define GAMEPAD_CENTER_VALUE 127

/*
 * 摇杆回中死区。
 * 值越大，中心附近越不容易误触发；值太大会导致小幅拨动没反应。
 * 如果摇杆回中时小车自己动，适当增大；如果轻拨没反应，适当减小。
 */
#define GAMEPAD_DEADBAND 8

/* 手柄原始数据调试打印周期，数值越大打印越少。 */
#define GAMEPAD_DEBUG_PERIOD 10U

/*
 * 向前拨动时的最低起步输出。
 * 向前完全不动时，优先增大这个值，推荐按 380 -> 500 -> 650 逐步试。
 */
#define GAMEPAD_DRIVE_FORWARD_START_SPEED 550

/*
 * 向后拨动时的最低起步输出。
 * 向后没力或轻拨不动时增大它；向后太冲时适当减小。
 */
#define GAMEPAD_DRIVE_REVERSE_START_SPEED 320

/*
 * 前进/后退最大输出。
 * 大幅拨动摇杆也没力时增大；能动但太冲时减小。
 */
#define GAMEPAD_DRIVE_MAX_SPEED 800

/*
 * 转向最低起步输出。
 * 原地转向或低速转向没反应时增大；转向一碰就很猛时减小。
 */
#define GAMEPAD_TURN_START_SPEED 260

/*
 * 转向最大输出。
 * 转向太猛时减小；转向没力或转不过来时增大。
 */
#define GAMEPAD_TURN_MAX_SPEED 550

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

static int16_t MapStickToSpeed(int16_t stick, int16_t start_speed, int16_t max_speed)
{
  if (stick == 0)
  {
    return 0;
  }

  const int16_t direction = (stick < 0) ? -1 : 1;
  const int32_t abs_stick = (stick < 0) ? -(int32_t)stick : (int32_t)stick;
  const int32_t speed_range = (int32_t)max_speed - (int32_t)start_speed;
  const int32_t speed = (int32_t)start_speed +
                        (abs_stick * abs_stick * speed_range) /
                            ((int32_t)GAMEPAD_CENTER_VALUE * GAMEPAD_CENTER_VALUE);
  return (int16_t)(direction * speed);
}

static int16_t MapDriveStickToSpeed(int16_t stick)
{
  /*
   * 实车测试发现前进方向起转阻力更大：向后能动，但向前不动。
   * 因此前进和后退使用不同起步力，最大速度仍保持较低，方便里程计标定。
   */
  const ChassisParams params = ChassisParams_Get();
  if (stick > 0)
  {
    return MapStickToSpeed(stick, params.gamepad_forward_start, params.gamepad_drive_max);
  }

  return MapStickToSpeed(stick, params.gamepad_reverse_start, params.gamepad_drive_max);
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
  const ChassisParams params = ChassisParams_Get();
  command->forward = MapDriveStickToSpeed(centered_ly);
  command->turn =
      MapStickToSpeed(centered_lx, params.gamepad_turn_start, params.gamepad_turn_max);
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
