#ifndef RASPI_LINK_H_
#define RASPI_LINK_H_

#include <stdbool.h>
#include <stdint.h>

#include "control_types.h"
#include "odometry.h"
#include "usart.h"

typedef enum {
  RASPI_TELEMETRY_CHASSIS = 1U << 0,
  RASPI_TELEMETRY_ENCODER = 1U << 1,
  RASPI_TELEMETRY_IMU = 1U << 2,
  RASPI_TELEMETRY_DEVICE = 1U << 3,
  RASPI_TELEMETRY_ODOMETRY = 1U << 4,
  RASPI_TELEMETRY_ODOMETRY_DEBUG = 1U << 5,
} RaspiTelemetryMask;

void RaspiLink_Init(void);
void RaspiLink_TaskStep(void);
bool RaspiLink_GetControlCommand(ControlCommand *command);
bool RaspiLink_TelemetryEnabled(RaspiTelemetryMask telemetry);
void RaspiLink_SendChassisStatus(const ControlCommand *command, int16_t ultra_mm);
void RaspiLink_SendEncoderDelta(int16_t delta_a,
                                int16_t delta_b,
                                int16_t delta_c,
                                int16_t delta_d);
void RaspiLink_SendImuRaw(int16_t ax,
                          int16_t ay,
                          int16_t az,
                          int16_t gx,
                          int16_t gy,
                          int16_t gz);
void RaspiLink_SendDeviceStatus(bool pad_ok,
                                bool imu_ok,
                                bool ultra_ok,
                                uint8_t error);
void RaspiLink_SendOdometry(const OdometrySample *odometry);
void RaspiLink_SendOdometryDebug(uint32_t odom_time_ms,
                                 int16_t left_speed_mm_s,
                                 int16_t right_speed_mm_s,
                                 int16_t turn_speed_mm_s,
                                 int16_t left_delta_mm,
                                 int16_t right_delta_mm);
void RaspiLink_OnUartRxCpltCallback(UART_HandleTypeDef *huart);
void RaspiLink_OnUartErrorCallback(UART_HandleTypeDef *huart);

#endif  // RASPI_LINK_H_
