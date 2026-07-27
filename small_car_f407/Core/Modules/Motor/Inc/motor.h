/**
 * @file motor.h
 * @brief 声明四路电机方向、PWM 输出和测试接口。
 */
#ifndef MOTOR_H_
#define MOTOR_H_

#include <stdint.h>

/** 主板四个电机接口编号。 */
typedef enum {
  MOTOR_A = 0,
  MOTOR_B,
  MOTOR_C,
  MOTOR_D,
} MotorId;

/** H 桥四种输出状态。 */
typedef enum {
  MOTOR_DIRECTION_STOP = 0,
  MOTOR_DIRECTION_FORWARD,
  MOTOR_DIRECTION_REVERSE,
  MOTOR_DIRECTION_BRAKE,
} MotorDirection;

/** 归一化 PWM 最大值。 */
#define MOTOR_MAX_SPEED 1000

/** 启动四个 PWM 定时器通道并停车。 */
void Motor_Init(void);
/** 设置单个 H 桥方向，不改变当前 PWM 占空比。 */
void Motor_SetDirection(MotorId motor, MotorDirection direction);
/** 设置带符号归一化速度，正负号决定方向。 */
void Motor_SetSpeed(MotorId motor, int16_t speed);
/** 向四个电机发送同一个带符号速度。 */
void Motor_SetAllSpeed(int16_t speed);
/** 将四个电机置为停止输出。 */
void Motor_StopAll(void);
/** 可选的正反转测试状态机。 */
void Motor_TestTaskStep(void);

#endif  // MOTOR_H_
