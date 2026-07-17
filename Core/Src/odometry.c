#include "odometry.h"

#include <stddef.h>

#define ODOMETRY_YAW_WRAP_MDEG 180000L
#define ODOMETRY_YAW_FULL_MDEG 360000L

static OdometrySample g_sample;
static uint32_t g_last_update_ms = 0U;
static int64_t g_gyro_z_bias_sum = 0;
static uint16_t g_gyro_z_bias_count = 0U;
static int16_t g_gyro_z_bias = 0;

static int32_t NormalizeYawMdeg(int32_t yaw_mdeg)
{
  while (yaw_mdeg > ODOMETRY_YAW_WRAP_MDEG)
  {
    yaw_mdeg -= ODOMETRY_YAW_FULL_MDEG;
  }
  while (yaw_mdeg < -ODOMETRY_YAW_WRAP_MDEG)
  {
    yaw_mdeg += ODOMETRY_YAW_FULL_MDEG;
  }
  return yaw_mdeg;
}

static int16_t ClampI16(int32_t value)
{
  if (value > INT16_MAX)
  {
    return INT16_MAX;
  }
  if (value < INT16_MIN)
  {
    return INT16_MIN;
  }
  return (int16_t)value;
}

static int32_t EncoderDeltaToMm(EncoderSample encoder_a,
                                EncoderSample encoder_b,
                                EncoderSample encoder_c,
                                EncoderSample encoder_d)
{
  /*
   * 四轮普通底盘先取四个轮子的平均位移作为车体前进距离。
   * 如果某个轮子编码器方向反了，修改 odometry.h 中对应 SIGN 即可。
   */
  const int32_t sum_ticks =
      (int32_t)encoder_a.delta * ODOMETRY_MOTOR_A_SIGN +
      (int32_t)encoder_b.delta * ODOMETRY_MOTOR_B_SIGN +
      (int32_t)encoder_c.delta * ODOMETRY_MOTOR_C_SIGN +
      (int32_t)encoder_d.delta * ODOMETRY_MOTOR_D_SIGN;
  const int32_t average_ticks = sum_ticks / 4;
  return (average_ticks * ODOMETRY_MM_PER_TICK_NUM) / ODOMETRY_MM_PER_TICK_DEN;
}

void Odometry_Init(void)
{
  Odometry_Reset();
}

void Odometry_Reset(void)
{
  g_sample.time_ms = 0U;
  g_sample.distance_mm = 0;
  g_sample.speed_mm_s = 0;
  g_sample.yaw_mdeg = 0;
  g_sample.yaw_rate_mdeg_s = 0;
  g_sample.calibrated = false;

  g_last_update_ms = 0U;
  g_gyro_z_bias_sum = 0;
  g_gyro_z_bias_count = 0U;
  g_gyro_z_bias = 0;
}

void Odometry_Update(const Icm20948Sample *imu,
                     EncoderSample encoder_a,
                     EncoderSample encoder_b,
                     EncoderSample encoder_c,
                     EncoderSample encoder_d,
                     uint32_t now_ms)
{
  if (imu == NULL)
  {
    return;
  }

  if (g_last_update_ms == 0U)
  {
    g_last_update_ms = now_ms;
    g_sample.time_ms = now_ms;
    return;
  }

  const uint32_t dt_ms = now_ms - g_last_update_ms;
  g_last_update_ms = now_ms;
  if (dt_ms == 0U)
  {
    return;
  }

  const int32_t delta_mm =
      EncoderDeltaToMm(encoder_a, encoder_b, encoder_c, encoder_d);
  g_sample.distance_mm += delta_mm;
  g_sample.speed_mm_s = ClampI16((delta_mm * 1000L) / (int32_t)dt_ms);

  /*
   * 上电后先用前 100 个陀螺仪 Z 轴样本估算零偏。
   * 这段时间尽量保持小车静止，校准完成后 yaw 才开始积分。
   */
  const int16_t signed_gz = (int16_t)(imu->gyro_z * ODOMETRY_GYRO_Z_SIGN);
  if (g_gyro_z_bias_count < ODOMETRY_GYRO_BIAS_SAMPLE_COUNT)
  {
    g_gyro_z_bias_sum += signed_gz;
    ++g_gyro_z_bias_count;
    if (g_gyro_z_bias_count >= ODOMETRY_GYRO_BIAS_SAMPLE_COUNT)
    {
      g_gyro_z_bias =
          (int16_t)(g_gyro_z_bias_sum / (int32_t)ODOMETRY_GYRO_BIAS_SAMPLE_COUNT);
      g_sample.calibrated = true;
    }
    g_sample.yaw_rate_mdeg_s = 0;
    g_sample.time_ms = now_ms;
    return;
  }

  const int32_t corrected_gz = (int32_t)signed_gz - (int32_t)g_gyro_z_bias;
  const int32_t yaw_rate_mdeg_s =
      (corrected_gz * 10000L) / ODOMETRY_GYRO_LSB_PER_DPS_X10;
  const int32_t delta_yaw_mdeg =
      (int32_t)(((int64_t)yaw_rate_mdeg_s * (int64_t)dt_ms) / 1000);

  g_sample.yaw_rate_mdeg_s = ClampI16(yaw_rate_mdeg_s);
  g_sample.yaw_mdeg = NormalizeYawMdeg(g_sample.yaw_mdeg + delta_yaw_mdeg);
  g_sample.time_ms = now_ms;
}

OdometrySample Odometry_GetSample(void)
{
  return g_sample;
}
