#ifndef CHASSIS_AUTO_H_
#define CHASSIS_AUTO_H_

#include <stdint.h>

void ChassisAuto_Mix(int16_t linear_mm_s,
                     int16_t angular_mrad_s,
                     int16_t *left,
                     int16_t *right);

#endif  // CHASSIS_AUTO_H_
