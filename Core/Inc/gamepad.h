#ifndef GAMEPAD_H_
#define GAMEPAD_H_

#include <stdbool.h>
#include <stdint.h>

#include "control_types.h"

typedef struct {
  bool connected;
  uint8_t lx;
  uint8_t ly;
  uint8_t rx;
  uint8_t ry;
  uint16_t buttons;
} GamepadState;

void Gamepad_Init(void);
void Gamepad_TaskStep(void);
bool Gamepad_GetState(GamepadState *state);
bool Gamepad_GetControlCommand(ControlCommand *command);

#endif  // GAMEPAD_H_
