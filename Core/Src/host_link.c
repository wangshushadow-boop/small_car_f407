#include "host_link.h"

#include <stddef.h>

void HostLink_Init(void)
{
}

void HostLink_TaskStep(void)
{
}

bool HostLink_GetControlCommand(ControlCommand *command)
{
  if (command == NULL)
  {
    return false;
  }

  command->source = CONTROL_SOURCE_HOST;
  command->enabled = false;
  command->forward = 0;
  command->turn = 0;
  return false;
}
