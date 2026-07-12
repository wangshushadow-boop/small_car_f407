#ifndef SYSTEM_STATUS_H_
#define SYSTEM_STATUS_H_

#include "control_types.h"

void SystemStatus_Init(void);
void SystemStatus_TaskStep(const ControlCommand *command);

#endif  // SYSTEM_STATUS_H_
