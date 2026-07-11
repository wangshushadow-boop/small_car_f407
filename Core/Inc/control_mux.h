#ifndef CONTROL_MUX_H_
#define CONTROL_MUX_H_

#include <stdbool.h>

#include "control_types.h"

void ControlMux_Init(void);
bool ControlMux_SelectCommand(ControlCommand *command);

#endif  // CONTROL_MUX_H_
