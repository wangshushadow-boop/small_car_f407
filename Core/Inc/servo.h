#ifndef INC_SERVO_H_
#define INC_SERVO_H_

#include <stdint.h>

typedef enum {
  SERVO_CHANNEL_LEFT = 0,
  SERVO_CHANNEL_RIGHT,
} ServoChannel;

#define SERVO_MIN_PULSE_US 800U
#define SERVO_MID_PULSE_US 1500U
#define SERVO_LEFT_MAX_PULSE_US 2300U
#define SERVO_RIGHT_MAX_PULSE_US 1700U
#define SERVO_MAX_PULSE_US SERVO_LEFT_MAX_PULSE_US
#define SERVO_LEFT_INIT_PULSE_US SERVO_MID_PULSE_US
#define SERVO_RIGHT_INIT_PULSE_US SERVO_MIN_PULSE_US

void Servo_Init(void);
void Servo_SetPulse(ServoChannel channel, uint16_t pulse_us);
void Servo_SetBothPulse(uint16_t left_pulse_us, uint16_t right_pulse_us);
void Servo_TestTaskStep(void);

#endif /* INC_SERVO_H_ */
