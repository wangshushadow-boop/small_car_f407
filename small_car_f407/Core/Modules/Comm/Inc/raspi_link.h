/**
 * @file raspi_link.h
 * @brief 声明 USART3 树莓派二进制协议的收发、控制和遥测接口。
 *
 * 帧格式、消息编号和字段单位必须与 rpi_host 协议模块保持一致。
 */
#ifndef RASPI_LINK_H_
#define RASPI_LINK_H_

#include <stdbool.h>
#include <stdint.h>

#include "control_types.h"
#include "usart.h"

/** 树莓派可按位选择的 MCU 周期遥测类型。 */
typedef enum {
  RASPI_TELEMETRY_CHASSIS = 1U << 0,
  RASPI_TELEMETRY_ENCODER = 1U << 1,
  RASPI_TELEMETRY_IMU = 1U << 2,
  RASPI_TELEMETRY_DEVICE = 1U << 3,
} RaspiTelemetryMask;

/** 初始化接收状态机、控制超时和 UART 中断接收。 */
void RaspiLink_Init(void);
/** 解析接收环形缓冲区中的完整帧，应由通信任务周期调用。 */
void RaspiLink_TaskStep(void);
/** 获取仍在超时窗口内的最新上位机控制命令。 */
bool RaspiLink_GetControlCommand(ControlCommand *command);
/** 查询某类周期遥测是否被树莓派启用。 */
bool RaspiLink_TelemetryEnabled(RaspiTelemetryMask telemetry);
/** 以下 Send* 接口按协议编码并通过 USART3 阻塞发送一帧。 */
void RaspiLink_SendChassisStatus(const ControlCommand *command, int16_t ultra_mm);
void RaspiLink_SendEncoderCounts(int32_t count_a,
                                 int32_t count_b,
                                 int32_t count_c,
                                 int32_t count_d);
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
/** 转发 HAL UART 接收完成回调，并立即重新挂接下一个字节。 */
void RaspiLink_OnUartRxCpltCallback(UART_HandleTypeDef *huart);
/** 转发 HAL UART 错误回调，清错后恢复中断接收。 */
void RaspiLink_OnUartErrorCallback(UART_HandleTypeDef *huart);

#endif  // RASPI_LINK_H_
