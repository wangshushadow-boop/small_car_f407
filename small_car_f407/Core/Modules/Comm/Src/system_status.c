/**
 * @file system_status.c
 * @brief 汇总 IMU、编码器、超声和控制状态并按节拍上传树莓派。
 *
 * 本模块只负责采样调度和遥测分发，不参与底盘控制决策。
 */
#include "system_status.h"

/*
 * 系统状态汇总模块。
 *
 * 周期采集各业务模块的最新状态，并按需输出到调试串口、树莓派链路和 OLED。
 * 这样日志输出集中管理，不需要各模块自己决定什么时候刷屏。
 */

#include <stdio.h>

#include "debug_uart.h"
#include "encoder.h"
#include "gamepad.h"
#include "icm20948.h"
#include "main.h"
#include "oled.h"
#include "raspi_link.h"
#include "ultrasonic.h"

#define CHASSIS_REPORT_PERIOD_MS 50U
#define ENCODER_REPORT_PERIOD_MS 20U
#define IMU_REPORT_PERIOD_MS 20U
#define DEVICE_REPORT_PERIOD_MS 1000U
#define OLED_REPORT_PERIOD_MS 1000U
#define IMU_REINIT_PERIOD_MS 1000U

/*
 * 各通道独立记录上次执行时刻。使用无符号减法比较可自然处理
 * HAL_GetTick() 约 49 天一次的回绕。
 */
static uint32_t g_last_chassis_report_tick = 0U;
static uint32_t g_last_encoder_report_tick = 0U;
static uint32_t g_last_imu_report_tick = 0U;
static uint32_t g_last_device_report_tick = 0U;
static uint32_t g_last_oled_report_tick = 0U;
static uint32_t g_last_imu_reinit_tick = 0U;
static bool g_imu_ok = false;

static const char *SourceToText(ControlSource source);
static void ReportChassisToHost(const ControlCommand *command);
static void ReportEncoderToHost(void);
static void ReportImuToHost(void);
static void ReportDeviceToHost(void);
static void ReportToOled(const ControlCommand *command);

void SystemStatus_Init(void)
{
  /* 从当前 tick 起算周期，避免启动阶段一次性发送全部遥测。 */
  uint32_t now = HAL_GetTick();
  g_last_chassis_report_tick = now;
  g_last_encoder_report_tick = now;
  g_last_imu_report_tick = now;
  g_last_device_report_tick = now;
  g_last_oled_report_tick = now;
  g_last_imu_reinit_tick = now;
  g_imu_ok = false;
}

void SystemStatus_TaskStep(const ControlCommand *command)
{
  if (command == NULL)
  {
    return;
  }

  uint32_t now = HAL_GetTick();

  /*
   * 树莓派可按遥测位图选择需要的二进制数据；编码器和 IMU 保持固定采样
   * 节拍，是否上传只影响串口流量，不影响电机闭环和设备健康检测。
   */
  if (RaspiLink_TelemetryEnabled(RASPI_TELEMETRY_CHASSIS) &&
      ((now - g_last_chassis_report_tick) >= CHASSIS_REPORT_PERIOD_MS))
  {
    g_last_chassis_report_tick = now;
    ReportChassisToHost(command);
  }

  if ((now - g_last_encoder_report_tick) >= ENCODER_REPORT_PERIOD_MS)
  {
    g_last_encoder_report_tick = now;
    ReportEncoderToHost();
  }

  if ((now - g_last_imu_report_tick) >= IMU_REPORT_PERIOD_MS)
  {
    g_last_imu_report_tick = now;
    ReportImuToHost();
  }

  if (RaspiLink_TelemetryEnabled(RASPI_TELEMETRY_DEVICE) &&
      ((now - g_last_device_report_tick) >= DEVICE_REPORT_PERIOD_MS))
  {
    g_last_device_report_tick = now;
    ReportDeviceToHost();
  }

  if ((now - g_last_oled_report_tick) >= OLED_REPORT_PERIOD_MS)
  {
    g_last_oled_report_tick = now;
    ReportToOled(command);
  }
}

static const char *SourceToText(ControlSource source)
{
  /* 文本仅供 OLED 摘要显示，不参与协议编码。 */
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

static void ReportChassisToHost(const ControlCommand *command)
{
  UltrasonicSample ultrasonic = {0};
  int16_t ultra_mm = -1;

  /* 无效超声值用 -1 表示，避免与真实的 0 mm 混淆。 */
  if (Ultrasonic_GetSample(&ultrasonic) && ultrasonic.valid)
  {
    ultra_mm = (int16_t)ultrasonic.distance_mm;
  }

  RaspiLink_SendChassisStatus(command, ultra_mm);
}

static void ReportEncoderToHost(void)
{
  /* 一次读取四路快照，保证同一遥测帧中的数据来自同一调度周期。 */
  EncoderSample encoder_a = Encoder_GetSample(MOTOR_A);
  EncoderSample encoder_b = Encoder_GetSample(MOTOR_B);
  EncoderSample encoder_c = Encoder_GetSample(MOTOR_C);
  EncoderSample encoder_d = Encoder_GetSample(MOTOR_D);

  if (RaspiLink_TelemetryEnabled(RASPI_TELEMETRY_ENCODER))
  {
    RaspiLink_SendEncoderCounts(encoder_a.count,
                                encoder_b.count,
                                encoder_c.count,
                                encoder_d.count);
  }

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
}

static void ReportImuToHost(void)
{
  Icm20948Sample imu = {0};
  Icm20948Status imu_status = Icm20948_ReadSample(&imu);
  g_imu_ok = (imu_status == ICM20948_STATUS_OK);

  if (g_imu_ok)
  {
    if (RaspiLink_TelemetryEnabled(RASPI_TELEMETRY_IMU))
    {
      RaspiLink_SendImuRaw(imu.accel_x,
                           imu.accel_y,
                           imu.accel_z,
                           imu.gyro_x,
                           imu.gyro_y,
                           imu.gyro_z);
    }

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
    /* 读取失败时限速重试初始化，避免主任务被连续 I2C 初始化占满。 */
    const uint32_t now = HAL_GetTick();
    if ((now - g_last_imu_reinit_tick) >= IMU_REINIT_PERIOD_MS)
    {
      g_last_imu_reinit_tick = now;
      imu_status = Icm20948_Init();
      g_imu_ok = (imu_status == ICM20948_STATUS_OK);
    }

    DebugUart_PrintfIf(DEBUG_LOG_IMU, "[IMU] read failed, status=%d\r\n", imu_status);
  }
}

static void ReportDeviceToHost(void)
{
  /* 设备状态是低频健康摘要，最后一个保留字节当前固定为零。 */
  GamepadState gamepad = {0};
  UltrasonicSample ultrasonic = {0};

  (void)Gamepad_GetState(&gamepad);
  (void)Ultrasonic_GetSample(&ultrasonic);

  RaspiLink_SendDeviceStatus(gamepad.connected, g_imu_ok, ultrasonic.valid, 0U);
}

static void ReportToOled(const ControlCommand *command)
{
  char line[24];
  GamepadState gamepad = {0};
  UltrasonicSample ultrasonic = {0};

  (void)Gamepad_GetState(&gamepad);
  (void)Ultrasonic_GetSample(&ultrasonic);

  /* OLED 只显示低频摘要，避免占用主循环时间。 */
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
