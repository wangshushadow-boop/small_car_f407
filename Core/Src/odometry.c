#include "odometry.h"

#include <math.h>
#include <stddef.h>

#include "chassis_params.h"

#define ODOMETRY_PI 3.14159265358979323846f
#define ODOMETRY_RAD_TO_MDEG (180000.0f / ODOMETRY_PI)
#define ODOMETRY_DEG_TO_RAD (ODOMETRY_PI / 180.0f)
#define ODOMETRY_YAW_WRAP_RAD ODOMETRY_PI
#define ODOMETRY_YAW_FULL_RAD (2.0f * ODOMETRY_PI)
#define ODOMETRY_STATIONARY_GYRO_RAD_S (5.0f * ODOMETRY_DEG_TO_RAD)

static OdometrySample g_sample;
static OdometryDebug g_debug;
static uint32_t g_last_update_ms;
static int64_t g_gyro_bias_sum_x;
static int64_t g_gyro_bias_sum_y;
static int64_t g_gyro_bias_sum_z;
static uint16_t g_gyro_bias_count;
static int16_t g_gyro_bias_x;
static int16_t g_gyro_bias_y;
static int16_t g_gyro_bias_z;
static float g_x_mm;
static float g_y_mm;
static float g_z_mm;
static float g_distance_mm;
static float g_roll_rad;
static float g_pitch_rad;
static float g_yaw_rad;

static float NormalizeYawRad(float yaw_rad)
{
  while (yaw_rad > ODOMETRY_YAW_WRAP_RAD)
  {
    yaw_rad -= ODOMETRY_YAW_FULL_RAD;
  }
  while (yaw_rad < -ODOMETRY_YAW_WRAP_RAD)
  {
    yaw_rad += ODOMETRY_YAW_FULL_RAD;
  }
  return yaw_rad;
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

static float TicksToMm(float ticks, const ChassisParams *params)
{
  return (ticks * (float)params->odom_mm_per_tick_num) /
         (float)ODOMETRY_MM_PER_TICK_DEN;
}

static int16_t MmToSpeed(float delta_mm, uint32_t dt_ms)
{
  return ClampI16((int32_t)lroundf((delta_mm * 1000.0f) / (float)dt_ms));
}

static void GetWheelDelta(EncoderSample encoder_a,
                          EncoderSample encoder_b,
                          EncoderSample encoder_c,
                          EncoderSample encoder_d,
                          const ChassisParams *params,
                          float *left_mm,
                          float *right_mm)
{
  const float left_ticks =
      ((float)encoder_a.delta * ODOMETRY_MOTOR_A_SIGN +
       (float)encoder_b.delta * ODOMETRY_MOTOR_B_SIGN) *
      0.5f;
  const float right_ticks =
      ((float)encoder_c.delta * ODOMETRY_MOTOR_C_SIGN +
       (float)encoder_d.delta * ODOMETRY_MOTOR_D_SIGN) *
      0.5f;
  *left_mm = TicksToMm(left_ticks, params);
  *right_mm = TicksToMm(right_ticks, params);
}

static void UpdateWheelDebug(float left_mm, float right_mm, uint32_t dt_ms)
{
  g_debug.left_delta_mm = ClampI16((int32_t)lroundf(left_mm));
  g_debug.right_delta_mm = ClampI16((int32_t)lroundf(right_mm));
  g_debug.left_speed_mm_s = MmToSpeed(left_mm, dt_ms);
  g_debug.right_speed_mm_s = MmToSpeed(right_mm, dt_ms);
  g_debug.turn_speed_mm_s =
      ClampI16((int32_t)g_debug.right_speed_mm_s -
               (int32_t)g_debug.left_speed_mm_s);
}

static float GyroRateRad(int16_t raw,
                         int16_t bias,
                         int16_t sign,
                         const ChassisParams *params)
{
  const float corrected = (float)((int32_t)raw * sign - bias);
  const float degrees_per_second =
      (corrected * 10.0f) / (float)params->gyro_lsb_per_dps_x10;
  return degrees_per_second * ODOMETRY_DEG_TO_RAD;
}

static void GetAccelAttitude(const Icm20948Sample *imu,
                             const ChassisParams *params,
                             float *roll_rad,
                             float *pitch_rad)
{
  const float accel_x = (float)imu->accel_x;
  const float accel_y = (float)imu->accel_y;
  const float accel_z = (float)imu->accel_z;
  *roll_rad = atan2f(accel_y, accel_z) -
              ((float)params->imu_roll_offset_mdeg / 1000.0f) *
                  ODOMETRY_DEG_TO_RAD;
  *pitch_rad =
      atan2f(-accel_x, sqrtf(accel_y * accel_y + accel_z * accel_z)) -
      ((float)params->imu_pitch_offset_mdeg / 1000.0f) *
          ODOMETRY_DEG_TO_RAD;
}

static void UpdateSample(float delta_mm,
                         float yaw_rate_rad_s,
                         uint32_t now_ms,
                         uint32_t dt_ms)
{
  g_sample.time_ms = now_ms;
  g_sample.x_mm = (int32_t)lroundf(g_x_mm);
  g_sample.y_mm = (int32_t)lroundf(g_y_mm);
  g_sample.z_mm = (int32_t)lroundf(g_z_mm);
  g_sample.distance_mm = (int32_t)lroundf(g_distance_mm);
  g_sample.speed_mm_s = MmToSpeed(delta_mm, dt_ms);
  g_sample.roll_mdeg = (int32_t)lroundf(g_roll_rad * ODOMETRY_RAD_TO_MDEG);
  g_sample.pitch_mdeg = (int32_t)lroundf(g_pitch_rad * ODOMETRY_RAD_TO_MDEG);
  g_sample.yaw_mdeg = (int32_t)lroundf(g_yaw_rad * ODOMETRY_RAD_TO_MDEG);
  g_sample.yaw_rate_mdeg_s =
      (int32_t)lroundf(yaw_rate_rad_s * ODOMETRY_RAD_TO_MDEG);
}

void Odometry_Init(void)
{
  Odometry_Reset();
}

void Odometry_Reset(void)
{
  g_sample = (OdometrySample){0};
  g_debug = (OdometryDebug){0};
  g_last_update_ms = 0U;
  g_gyro_bias_sum_x = 0;
  g_gyro_bias_sum_y = 0;
  g_gyro_bias_sum_z = 0;
  g_gyro_bias_count = 0U;
  g_gyro_bias_x = 0;
  g_gyro_bias_y = 0;
  g_gyro_bias_z = 0;
  g_x_mm = 0.0f;
  g_y_mm = 0.0f;
  g_z_mm = 0.0f;
  g_distance_mm = 0.0f;
  g_roll_rad = 0.0f;
  g_pitch_rad = 0.0f;
  g_yaw_rad = 0.0f;
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
  if (dt_ms == 0U)
  {
    return;
  }

  const ChassisParams params = ChassisParams_Get();
  float left_mm = 0.0f;
  float right_mm = 0.0f;
  GetWheelDelta(encoder_a,
                encoder_b,
                encoder_c,
                encoder_d,
                &params,
                &left_mm,
                &right_mm);
  UpdateWheelDebug(left_mm, right_mm, dt_ms);
  const float delta_mm = (left_mm + right_mm) * 0.5f;

  float accel_roll_rad = 0.0f;
  float accel_pitch_rad = 0.0f;
  GetAccelAttitude(imu, &params, &accel_roll_rad, &accel_pitch_rad);

  /* 上电或重置后必须保持静止，用前 100 个样本估算三轴陀螺仪零偏。 */
  if (g_gyro_bias_count < ODOMETRY_GYRO_BIAS_SAMPLE_COUNT)
  {
    g_gyro_bias_sum_x += (int32_t)imu->gyro_x * ODOMETRY_GYRO_X_SIGN;
    g_gyro_bias_sum_y += (int32_t)imu->gyro_y * ODOMETRY_GYRO_Y_SIGN;
    g_gyro_bias_sum_z += (int32_t)imu->gyro_z * ODOMETRY_GYRO_Z_SIGN;
    ++g_gyro_bias_count;
    g_roll_rad = accel_roll_rad;
    g_pitch_rad = accel_pitch_rad;
    if (g_gyro_bias_count >= ODOMETRY_GYRO_BIAS_SAMPLE_COUNT)
    {
      g_gyro_bias_x =
          (int16_t)(g_gyro_bias_sum_x / ODOMETRY_GYRO_BIAS_SAMPLE_COUNT);
      g_gyro_bias_y =
          (int16_t)(g_gyro_bias_sum_y / ODOMETRY_GYRO_BIAS_SAMPLE_COUNT);
      g_gyro_bias_z =
          (int16_t)(g_gyro_bias_sum_z / ODOMETRY_GYRO_BIAS_SAMPLE_COUNT);
      g_sample.calibrated = true;
    }
    g_last_update_ms = now_ms;
    g_sample.time_ms = now_ms;
    return;
  }

  const float dt_s = (float)dt_ms / 1000.0f;
  const float gyro_x_rad_s = GyroRateRad(
      imu->gyro_x, g_gyro_bias_x, ODOMETRY_GYRO_X_SIGN, &params);
  const float gyro_y_rad_s = GyroRateRad(
      imu->gyro_y, g_gyro_bias_y, ODOMETRY_GYRO_Y_SIGN, &params);
  float gyro_z_rad_s = GyroRateRad(
      imu->gyro_z, g_gyro_bias_z, ODOMETRY_GYRO_Z_SIGN, &params);

  /* 轮子完全静止且角速度很小时视为静止，抑制陀螺仪噪声造成的 yaw 漂移。 */
  if ((encoder_a.delta == 0) && (encoder_b.delta == 0) &&
      (encoder_c.delta == 0) && (encoder_d.delta == 0) &&
      (fabsf(gyro_z_rad_s) < ODOMETRY_STATIONARY_GYRO_RAD_S))
  {
    gyro_z_rad_s = 0.0f;
  }

  /* 陀螺仪负责短时变化，加速度计重力方向负责抑制 roll/pitch 漂移。 */
  const float attitude_gyro_weight =
      (float)params.attitude_gyro_weight_permille / 1000.0f;
  g_roll_rad = attitude_gyro_weight * (g_roll_rad + gyro_x_rad_s * dt_s) +
               (1.0f - attitude_gyro_weight) * accel_roll_rad;
  const float previous_pitch_rad = g_pitch_rad;
  g_pitch_rad =
      attitude_gyro_weight * (g_pitch_rad + gyro_y_rad_s * dt_s) +
      (1.0f - attitude_gyro_weight) * accel_pitch_rad;

  float delta_yaw_rad = gyro_z_rad_s * dt_s;
  g_sample.wheel_yaw_fused = params.wheel_track_mm > 0;
  if (g_sample.wheel_yaw_fused)
  {
    const float wheel_delta_yaw_rad =
        (right_mm - left_mm) / (float)params.wheel_track_mm;
    const float yaw_gyro_weight =
        (float)params.yaw_gyro_weight_permille / 1000.0f;
    delta_yaw_rad = yaw_gyro_weight * delta_yaw_rad +
                    (1.0f - yaw_gyro_weight) * wheel_delta_yaw_rad;
  }

  const float previous_yaw_rad = g_yaw_rad;
  g_yaw_rad = NormalizeYawRad(g_yaw_rad + delta_yaw_rad);
  const float middle_yaw_rad = previous_yaw_rad + delta_yaw_rad * 0.5f;
  const float middle_pitch_rad =
      (previous_pitch_rad + g_pitch_rad) * 0.5f;

  /* 编码器给出沿坡面的距离，按 pitch/yaw 投影到 ROS 的 X/Y/Z 坐标。 */
  const float horizontal_mm = delta_mm * cosf(middle_pitch_rad);
  g_x_mm += horizontal_mm * cosf(middle_yaw_rad);
  g_y_mm += horizontal_mm * sinf(middle_yaw_rad);
  g_z_mm += delta_mm * sinf(middle_pitch_rad);
  g_distance_mm += delta_mm;

  UpdateSample(delta_mm, delta_yaw_rad / dt_s, now_ms, dt_ms);
  g_last_update_ms = now_ms;
}

OdometrySample Odometry_GetSample(void)
{
  return g_sample;
}

OdometryDebug Odometry_GetDebug(void)
{
  return g_debug;
}
