/**
 * @file gamepad_servo.h
 * @brief 声明右摇杆控制两自由度云台舵机的周期任务。
 */
#ifndef GAMEPAD_SERVO_H_
#define GAMEPAD_SERVO_H_

/** 初始化摇杆中心、累计脉宽和打印状态。 */
void GamepadServo_Init(void);
/** 周期读取右摇杆，以增量方式调整两个舵机脉宽。 */
void GamepadServo_TaskStep(void);

#endif  // GAMEPAD_SERVO_H_
