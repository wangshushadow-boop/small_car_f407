#ifndef HOST_LINK_H_
#define HOST_LINK_H_

#include <stdbool.h>

#include "control_types.h"

void HostLink_Init(void);
void HostLink_TaskStep(void);
bool HostLink_GetControlCommand(ControlCommand *command);

#endif  // HOST_LINK_H_
