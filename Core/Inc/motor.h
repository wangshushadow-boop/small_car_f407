#ifndef MOTOR_H_
#define MOTOR_H_

#include <stdint.h>

typedef enum {
  MOTOR_A = 0,
  MOTOR_B,
  MOTOR_C,
  MOTOR_D,
} MotorId;

typedef enum {
  MOTOR_DIRECTION_STOP = 0,
  MOTOR_DIRECTION_FORWARD,
  MOTOR_DIRECTION_REVERSE,
  MOTOR_DIRECTION_BRAKE,
} MotorDirection;

void Motor_Init(void);
void Motor_SetDirection(MotorId motor, MotorDirection direction);
void Motor_StopAll(void);
void Motor_TestTaskStep(void);

#endif  // MOTOR_H_
