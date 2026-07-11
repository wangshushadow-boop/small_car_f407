#ifndef CHASSIS_H_
#define CHASSIS_H_

#include "control_types.h"

void Chassis_Init(void);
void Chassis_ApplyCommand(const ControlCommand *command);
void Chassis_Stop(void);

#endif  // CHASSIS_H_
