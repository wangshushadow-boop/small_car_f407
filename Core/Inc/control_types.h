#ifndef CONTROL_TYPES_H_
#define CONTROL_TYPES_H_

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  CONTROL_SOURCE_NONE = 0,
  CONTROL_SOURCE_HOST,
  CONTROL_SOURCE_GAMEPAD,
  CONTROL_SOURCE_SAFETY,
} ControlSource;

typedef struct {
  ControlSource source;
  bool enabled;
  int16_t forward;
  int16_t turn;
} ControlCommand;

#endif  // CONTROL_TYPES_H_
