#include "gamepad.h"

#include <string.h>

static GamepadState g_gamepad_state = {
    .connected = false,
    .lx = 127U,
    .ly = 127U,
    .rx = 127U,
    .ry = 127U,
    .buttons = 0U,
};

void Gamepad_Init(void)
{
  g_gamepad_state.connected = false;
}

void Gamepad_TaskStep(void)
{
  // USB Host HID is not enabled yet. Keep this module as a stable placeholder.
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

  const int16_t centered_ly = (int16_t)127 - (int16_t)g_gamepad_state.ly;
  const int16_t centered_rx = (int16_t)g_gamepad_state.rx - (int16_t)127;
  command->enabled = true;
  command->forward = centered_ly;
  command->turn = centered_rx;
  return true;
}
