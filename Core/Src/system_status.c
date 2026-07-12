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

#define CHASSIS_REPORT_PERIOD_MS 50U
#define ENCODER_REPORT_PERIOD_MS 20U
#define IMU_REPORT_PERIOD_MS 20U
#define DEVICE_REPORT_PERIOD_MS 1000U
#define OLED_REPORT_PERIOD_MS 1000U

static uint32_t g_last_chassis_report_tick = 0U;
static uint32_t g_last_encoder_report_tick = 0U;
static uint32_t g_last_imu_report_tick = 0U;
static uint32_t g_last_device_report_tick = 0U;
static uint32_t g_last_oled_report_tick = 0U;
static bool g_imu_ok = false;

static const char *SourceToText(ControlSource source);
static void ReportChassisToHost(const ControlCommand *command);
static void ReportEncoderToHost(void);
static void ReportImuToHost(void);
static void ReportDeviceToHost(void);
static void ReportToOled(const ControlCommand *command);

void SystemStatus_Init(void)
{
  uint32_t now = HAL_GetTick();
  g_last_chassis_report_tick = now;
  g_last_encoder_report_tick = now;
  g_last_imu_report_tick = now;
  g_last_device_report_tick = now;
  g_last_oled_report_tick = now;
  g_imu_ok = false;
}

void SystemStatus_TaskStep(const ControlCommand *command)
{
  if (command == NULL)
  {
    return;
  }

  uint32_t now = HAL_GetTick();

  if ((now - g_last_chassis_report_tick) >= CHASSIS_REPORT_PERIOD_MS)
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

  if ((now - g_last_device_report_tick) >= DEVICE_REPORT_PERIOD_MS)
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

  if (Ultrasonic_GetSample(&ultrasonic) && ultrasonic.valid)
  {
    ultra_mm = (int16_t)ultrasonic.distance_mm;
  }

  RaspiLink_SendChassisStatus(command, ultra_mm);
}

static void ReportEncoderToHost(void)
{
  EncoderSample encoder_a = Encoder_GetSample(MOTOR_A);
  EncoderSample encoder_b = Encoder_GetSample(MOTOR_B);
  EncoderSample encoder_c = Encoder_GetSample(MOTOR_C);
  EncoderSample encoder_d = Encoder_GetSample(MOTOR_D);

  RaspiLink_SendEncoderDelta(encoder_a.delta,
                             encoder_b.delta,
                             encoder_c.delta,
                             encoder_d.delta);

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
    RaspiLink_SendImuRaw(imu.accel_x,
                         imu.accel_y,
                         imu.accel_z,
                         imu.gyro_x,
                         imu.gyro_y,
                         imu.gyro_z);

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

static void ReportDeviceToHost(void)
{
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
