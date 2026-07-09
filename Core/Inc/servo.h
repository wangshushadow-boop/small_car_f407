#ifndef INC_SERVO_H_
#define INC_SERVO_H_

#include <stdint.h>

typedef enum {
  SERVO_CHANNEL_LEFT = 0,
  SERVO_CHANNEL_RIGHT,
} ServoChannel;

#define SERVO_MIN_PULSE_US 1000U
#define SERVO_MID_PULSE_US 1500U
#define SERVO_MAX_PULSE_US 2000U

void Servo_Init(void);
void Servo_SetPulse(ServoChannel channel, uint16_t pulse_us);
void Servo_SetBothPulse(uint16_t left_pulse_us, uint16_t right_pulse_us);
void Servo_TestTaskStep(void);

#endif /* INC_SERVO_H_ */
