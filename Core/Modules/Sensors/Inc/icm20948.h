#ifndef ICM20948_H_
#define ICM20948_H_

#include <stdint.h>

#define ICM20948_WHO_AM_I_VALUE 0xEAU

typedef struct {
  int16_t accel_x;
  int16_t accel_y;
  int16_t accel_z;
  int16_t gyro_x;
  int16_t gyro_y;
  int16_t gyro_z;
  int16_t temperature;
} Icm20948Sample;

typedef enum {
  ICM20948_STATUS_OK = 0,
  ICM20948_STATUS_BUS_ERROR,
  ICM20948_STATUS_WHO_AM_I_ERROR,
} Icm20948Status;

Icm20948Status Icm20948_Init(void);
Icm20948Status Icm20948_ReadWhoAmI(uint8_t *who_am_i);
Icm20948Status Icm20948_ReadSample(Icm20948Sample *sample);

#endif  // ICM20948_H_
