#include "system_status.h"

#include <stdio.h>

#include "debug_uart.h"
#include "encoder.h"
#include "gamepad.h"
#include "icm20948.h"
#include "main.h"
#include "oled.h"
#include "raspi_link.h"
#include "ultrasonic.h"

#define HOST_REPORT_PERIOD_MS 500U
#define OLED_REPORT_PERIOD_MS 1000U

static uint32_t g_last_host_report_tick = 0U;
static uint32_t g_last_oled_report_tick = 0U;

static const char *SourceToText(ControlSource source);
static void ReportToHost(const ControlCommand *command);
static void ReportToOled(const ControlCommand *command);

void SystemStatus_Init(void)
{
  g_last_host_report_tick = HAL_GetTick();
  g_last_oled_report_tick = HAL_GetTick();
}

void SystemStatus_TaskStep(const ControlCommand *command)
{
  if (command == NULL)
  {
    return;
  }

  uint32_t now = HAL_GetTick();

  if ((now - g_last_host_report_tick) >= HOST_REPORT_PERIOD_MS)
  {
    g_last_host_report_tick = now;
    ReportToHost(command);
  }

  if ((now - g_last_oled_report_tick) >= OLED_REPORT_PERIOD_MS)
  {
    g_last_oled_report_tick = now;
    ReportToOled(command);
  }
}

static const char *SourceToText(ControlSource source)
{
  switch (source)
  {
    case CONTROL_SOURCE_HOST:
      return "HOST";
    case CONTROL_SOURCE_GAMEPAD:
      return "PAD";
    case CONTROL_SOURCE_SAFETY:
      return "SAFE";
    case CONTROL_SOURCE_NONE:
    default:
      return "NONE";
  }
}

static void ReportToHost(const ControlCommand *command)
{
  char line[192];
  GamepadState gamepad = {0};
  UltrasonicSample ultrasonic = {0};
  EncoderSample encoder_a = Encoder_GetSample(MOTOR_A);
  EncoderSample encoder_b = Encoder_GetSample(MOTOR_B);
  EncoderSample encoder_c = Encoder_GetSample(MOTOR_C);
  EncoderSample encoder_d = Encoder_GetSample(MOTOR_D);

  (void)Gamepad_GetState(&gamepad);
  (void)Ultrasonic_GetSample(&ultrasonic);

  /*
   * USART3 面向树莓派，保持一行一条结构化状态。
   * 这里只发送当前代码里有可靠来源的数据，不猜测电池、电压、里程等字段。
   */
  (void)snprintf(line,
                 sizeof(line),
                 "STAT src=%s en=%d f=%d t=%d pad=%d ultra=%d enc=%ld,%ld,%ld,%ld\r\n",
                 SourceToText(command->source),
                 command->enabled ? 1 : 0,
                 command->forward,
                 command->turn,
                 gamepad.connected ? 1 : 0,
                 ultrasonic.valid ? ultrasonic.distance_mm : -1,
                 encoder_a.count,
                 encoder_b.count,
                 encoder_c.count,
                 encoder_d.count);
  RaspiLink_WriteString(line);
  DebugUart_PrintfIf(
      DEBUG_LOG_ENCODER,
      "[ENC] A=%ld/%d B=%ld/%d C=%ld/%d D=%ld/%d\r\n",
      encoder_a.count,
      encoder_a.delta,
      encoder_b.count,
      encoder_b.delta,
      encoder_c.count,
      encoder_c.delta,
      encoder_d.count,
      encoder_d.delta);

  Icm20948Sample imu = {0};
  Icm20948Status imu_status = Icm20948_ReadSample(&imu);
  if (imu_status == ICM20948_STATUS_OK)
  {
    (void)snprintf(line,
                   sizeof(line),
                   "IMU ax=%d ay=%d az=%d gx=%d gy=%d gz=%d temp=%d\r\n",
                   imu.accel_x,
                   imu.accel_y,
                   imu.accel_z,
                   imu.gyro_x,
                   imu.gyro_y,
                   imu.gyro_z,
                   imu.temperature);
    RaspiLink_WriteString(line);
    DebugUart_PrintfIf(
        DEBUG_LOG_IMU,
        "[IMU] ax=%d ay=%d az=%d gx=%d gy=%d gz=%d temp=%d\r\n",
        imu.accel_x,
        imu.accel_y,
        imu.accel_z,
        imu.gyro_x,
        imu.gyro_y,
        imu.gyro_z,
        imu.temperature);
  }
  else
  {
    DebugUart_PrintfIf(DEBUG_LOG_IMU, "[IMU] read failed, status=%d\r\n", imu_status);
  }
}

static void ReportToOled(const ControlCommand *command)
{
  char line[24];
  GamepadState gamepad = {0};
  UltrasonicSample ultrasonic = {0};

  (void)Gamepad_GetState(&gamepad);
  (void)Ultrasonic_GetSample(&ultrasonic);

  /*
   * OLED 只做本地仪表盘，不显示高频原始数据。
   * 每秒刷新一次，避免占用过多主循环时间。
   */
  Oled_Clear();

  (void)snprintf(line, sizeof(line), "SRC:%s", SourceToText(command->source));
  Oled_ShowString(0, 0, line);

  (void)snprintf(line, sizeof(line), "F:%d T:%d", command->forward, command->turn);
  Oled_ShowString(0, 10, line);

  (void)snprintf(line, sizeof(line), "PAD:%s", gamepad.connected ? "OK" : "NO");
  Oled_ShowString(0, 20, line);

  if (ultrasonic.valid)
  {
    (void)snprintf(line, sizeof(line), "UL:%umm", ultrasonic.distance_mm);
  }
  else
  {
    (void)snprintf(line, sizeof(line), "UL:--");
  }
  Oled_ShowString(0, 30, line);

  Oled_Refresh();
}
