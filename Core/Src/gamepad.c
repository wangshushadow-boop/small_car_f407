#include "gamepad.h"

#include <string.h>

#include "debug_uart.h"
#include "motor.h"

#define GAMEPAD_CENTER_VALUE 127
#define GAMEPAD_DEADBAND 8
#define GAMEPAD_DEBUG_PERIOD 10U

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
  if ((value > -GAMEPAD_DEADBAND) && (value < GAMEPAD_DEADBAND))
  {
    return 0;
  }
  return value;
}

void Gamepad_Init(void)
{
  Gamepad_UpdateFromUsb(false,
                        GAMEPAD_CENTER_VALUE,
                        GAMEPAD_CENTER_VALUE,
                        GAMEPAD_CENTER_VALUE,
                        GAMEPAD_CENTER_VALUE,
                        0U);
}

void Gamepad_TaskStep(void)
{
  if (g_gamepad_state.connected != g_last_connected)
  {
    DebugUart_PrintfIf(DEBUG_LOG_GAMEPAD,
                       "[GAMEPAD] %s\r\n",
                       g_gamepad_state.connected ? "connected" : "disconnected");
    g_last_connected = g_gamepad_state.connected;
    g_debug_counter = 0U;
  }

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

  const int16_t centered_ly =
      ApplyDeadband((int16_t)GAMEPAD_CENTER_VALUE - (int16_t)g_gamepad_state.ly);
  const int16_t centered_rx =
      ApplyDeadband((int16_t)g_gamepad_state.rx - (int16_t)GAMEPAD_CENTER_VALUE);
  command->enabled = true;
  command->forward = (int16_t)((centered_ly * MOTOR_MAX_SPEED) / GAMEPAD_CENTER_VALUE);
  command->turn = (int16_t)((centered_rx * MOTOR_MAX_SPEED) / GAMEPAD_CENTER_VALUE);
  return true;
}

void Gamepad_UpdateFromUsb(bool connected,
                           uint8_t lx,
                           uint8_t ly,
                           uint8_t rx,
                           uint8_t ry,
                           uint16_t buttons)
{
  g_gamepad_state.connected = connected;
  g_gamepad_state.lx = lx;
  g_gamepad_state.ly = ly;
  g_gamepad_state.rx = rx;
  g_gamepad_state.ry = ry;
  g_gamepad_state.buttons = buttons;
}
