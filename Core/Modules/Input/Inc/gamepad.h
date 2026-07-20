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

typedef enum {
  GAMEPAD_BUTTON_SELECT = 0,
  GAMEPAD_BUTTON_LEFT_STICK,
  GAMEPAD_BUTTON_RIGHT_STICK,
  GAMEPAD_BUTTON_START,
  GAMEPAD_BUTTON_UP,
  GAMEPAD_BUTTON_RIGHT,
  GAMEPAD_BUTTON_DOWN,
  GAMEPAD_BUTTON_LEFT,
  GAMEPAD_BUTTON_L2,
  GAMEPAD_BUTTON_R2,
  GAMEPAD_BUTTON_L1,
  GAMEPAD_BUTTON_R1,
  GAMEPAD_BUTTON_GREEN,
  GAMEPAD_BUTTON_RED,
  GAMEPAD_BUTTON_BLUE,
  GAMEPAD_BUTTON_PINK,
} GamepadButton;

void Gamepad_Init(void);
void Gamepad_TaskStep(void);
bool Gamepad_GetState(GamepadState *state);
bool Gamepad_GetControlCommand(ControlCommand *command);
void Gamepad_UpdateFromUsb(bool connected,
                           uint8_t lx,
                           uint8_t ly,
                           uint8_t rx,
                           uint8_t ry,
                           uint16_t buttons);

#endif  // GAMEPAD_H_
