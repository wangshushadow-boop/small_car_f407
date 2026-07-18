#ifndef RASPI_LINK_H_
#define RASPI_LINK_H_

#include <stdbool.h>
#include <stdint.h>

#include "control_types.h"
#include "usart.h"

void RaspiLink_Init(void);
void RaspiLink_TaskStep(void);
bool RaspiLink_GetControlCommand(ControlCommand *command);
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
void RaspiLink_SendOdometry(uint32_t odom_time_ms,
                            int32_t distance_mm,
                            int16_t speed_mm_s,
                            int32_t yaw_mdeg,
                            int16_t yaw_rate_mdeg_s,
                            bool calibrated);
void RaspiLink_SendOdometryDebug(uint32_t odom_time_ms,
                                 int16_t left_speed_mm_s,
                                 int16_t right_speed_mm_s,
                                 int16_t turn_speed_mm_s,
                                 int16_t left_delta_mm,
                                 int16_t right_delta_mm);
void RaspiLink_OnUartRxCpltCallback(UART_HandleTypeDef *huart);

#endif  // RASPI_LINK_H_
