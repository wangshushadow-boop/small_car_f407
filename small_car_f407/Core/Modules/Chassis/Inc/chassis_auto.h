/**
 * @file chassis_auto.h
 * @brief 声明物理线速度和角速度到左右轮目标速度的差速运动学换算。
 */
#ifndef CHASSIS_AUTO_H_
#define CHASSIS_AUTO_H_

#include <stdint.h>

/**
 * @brief 差速运动学逆解。
 * @param linear_mm_s 车体线速度，单位 mm/s。
 * @param angular_mrad_s 车体角速度，单位 mrad/s。
 * @param left 左轮目标线速度输出，单位 mm/s。
 * @param right 右轮目标线速度输出，单位 mm/s。
 */
void ChassisAuto_Mix(int16_t linear_mm_s,
                     int16_t angular_mrad_s,
                     int16_t *left,
                     int16_t *right);

#endif  // CHASSIS_AUTO_H_
