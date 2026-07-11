#include "control_mux.h"

#include <stddef.h>

#include "gamepad.h"
#include "host_link.h"
#include "ultrasonic.h"

void ControlMux_Init(void)
{
}

bool ControlMux_SelectCommand(ControlCommand *command)
{
  if (command == NULL)
  {
    return false;
  }

  if (Ultrasonic_IsObstacleNear())
  {
    command->source = CONTROL_SOURCE_SAFETY;
    command->enabled = false;
    command->forward = 0;
    command->turn = 0;
    return true;
  }

  if (Gamepad_GetControlCommand(command))
  {
    return true;
  }

  if (HostLink_GetControlCommand(command))
  {
    return true;
  }

  command->source = CONTROL_SOURCE_NONE;
  command->enabled = false;
  command->forward = 0;
  command->turn = 0;
  return false;
}
