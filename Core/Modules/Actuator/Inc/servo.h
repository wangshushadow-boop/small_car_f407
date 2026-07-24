/**
 * @file servo.h
 * @brief 声明双路 PWM 舵机的脉宽范围、初始位置和控制接口。
 */
#ifndef INC_SERVO_H_
#define INC_SERVO_H_

#include <stdint.h>

/** 两个物理 PWM 舵机通道。 */
typedef enum {
  SERVO_CHANNEL_LEFT = 0,
  SERVO_CHANNEL_RIGHT,
} ServoChannel;

/** 舵机脉宽边界和上电初始值，单位均为 us。 */
#define SERVO_MIN_PULSE_US 800U
#define SERVO_MID_PULSE_US 1500U
#define SERVO_LEFT_MAX_PULSE_US 2300U
#define SERVO_RIGHT_MAX_PULSE_US 1700U
#define SERVO_MAX_PULSE_US SERVO_LEFT_MAX_PULSE_US
#define SERVO_LEFT_INIT_PULSE_US SERVO_MID_PULSE_US
#define SERVO_RIGHT_INIT_PULSE_US SERVO_MIN_PULSE_US

/** 启动 PWM 并把左右舵机移动到各自初始位置。 */
void Servo_Init(void);
/** 设置单通道脉宽，超出边界时自动限幅。 */
void Servo_SetPulse(ServoChannel channel, uint16_t pulse_us);
/** 同时更新两个通道。 */
void Servo_SetBothPulse(uint16_t left_pulse_us, uint16_t right_pulse_us);
/** 可选的非阻塞扫动测试状态机。 */
void Servo_TestTaskStep(void);

#endif /* INC_SERVO_H_ */
