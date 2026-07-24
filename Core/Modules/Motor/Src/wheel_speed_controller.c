#include "wheel_speed_controller.h"

/*
 * 左右轮速度 PI 控制器。
 *
 * 编码器给出本周期 tick 增量，模块先换算为 mm/s，再用“前馈 PWM + PI 修正”
 * 控制两侧电机。前馈保证基础驱动力，PI 补偿电池电压、负载和机械差异。
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "chassis_params.h"
#include "encoder.h"
#include "motor.h"
#include "odometry.h"

typedef struct {
  int16_t target_mm_s;
  int16_t ramped_target_mm_s;
  /* 误差积分保存为 (mm/s)*ms，避免 20 ms 周期下的小误差被整数除法丢失。 */
  int32_t integral_mm_s_ms;
} WheelControllerState;

static WheelControllerState g_left;
static WheelControllerState g_right;
static bool g_enabled;

static int16_t ClampI16(int32_t value, int16_t minimum, int16_t maximum)
{
  if (value < minimum)
  {
    return minimum;
  }
  if (value > maximum)
  {
    return maximum;
  }
  return (int16_t)value;
}

static int32_t ClampI32(int64_t value, int32_t minimum, int32_t maximum)
{
  if (value < minimum)
  {
    return minimum;
  }
  if (value > maximum)
  {
    return maximum;
  }
  return (int32_t)value;
}

static int16_t MoveTowards(int16_t current, int16_t target, int16_t step)
{
  if (current < target)
  {
    return ClampI16((int32_t)current + step, current, target);
  }
  if (current > target)
  {
    return ClampI16((int32_t)current - step, target, current);
  }
  return current;
}

static int16_t EncoderDeltaToSpeed(int16_t first_delta,
                                   int16_t second_delta,
                                   int16_t first_sign,
                                   int16_t second_sign,
                                   uint32_t dt_ms,
                                   const ChassisParams *params)
{
  if ((dt_ms == 0U) || (params == NULL))
  {
    return 0;
  }

  /* 先求同侧两轮的平均 tick，再按里程计标定比例换算为 mm/s。 */
  const int32_t tick_sum =
      (int32_t)first_delta * first_sign + (int32_t)second_delta * second_sign;
  const int64_t numerator =
      (int64_t)tick_sum * params->odom_mm_per_tick_num * 1000;
  const int64_t denominator =
      (int64_t)2 * ODOMETRY_MM_PER_TICK_DEN * dt_ms;
  return ClampI16((int32_t)(numerator / denominator), -3000, 3000);
}

static int16_t ApplyPi(WheelControllerState *state,
                       int16_t measured_mm_s,
                       int16_t output_scale_permille,
                       uint32_t dt_ms,
                       const ChassisParams *params)
{
  const int16_t target = state->ramped_target_mm_s;
  if (target == 0)
  {
    state->integral_mm_s_ms = 0;
    return 0;
  }

  const int32_t error = (int32_t)target - measured_mm_s;
  const int32_t integral_limit =
      (int32_t)params->wheel_speed_integral_limit * 1000;
  state->integral_mm_s_ms =
      ClampI32((int64_t)state->integral_mm_s_ms + error * (int32_t)dt_ms,
               -integral_limit,
               integral_limit);

  /*
   * 前馈沿用原来的速度上限映射：最大物理速度对应 MOTOR_MAX_SPEED。
   * Kp/Ki 使用 x100 定点数，避免控制循环引入浮点运算。
   */
  int32_t output =
      ((int32_t)target * MOTOR_MAX_SPEED) / params->max_linear_speed_mm_s;
  output += ((int32_t)params->wheel_speed_kp_x100 * error) / 100;
  output += (int32_t)(((int64_t)params->wheel_speed_ki_x100 *
                      state->integral_mm_s_ms) /
                     100000);
  output = (output * output_scale_permille) / 1000;

  /* PI 只减小当前方向输出，不主动反向制动，降低低速时来回抖动的风险。 */
  if (target > 0)
  {
    output = ClampI16(output, 0, MOTOR_MAX_SPEED);
  }
  else
  {
    output = ClampI16(output, -MOTOR_MAX_SPEED, 0);
  }

  if ((output > 0) && (output < params->wheel_pwm_min))
  {
    output = params->wheel_pwm_min;
  }
  else if ((output < 0) && (output > -params->wheel_pwm_min))
  {
    output = -params->wheel_pwm_min;
  }
  return ClampI16(output, -MOTOR_MAX_SPEED, MOTOR_MAX_SPEED);
}

void WheelSpeedController_Init(void)
{
  g_left = (WheelControllerState){0};
  g_right = (WheelControllerState){0};
  g_enabled = false;
}

void WheelSpeedController_SetTarget(int16_t left_mm_s, int16_t right_mm_s)
{
  const ChassisParams params = ChassisParams_Get();
  const int16_t left_target =
      ClampI16(left_mm_s, -params.max_linear_speed_mm_s, params.max_linear_speed_mm_s);
  const int16_t right_target =
      ClampI16(right_mm_s, -params.max_linear_speed_mm_s, params.max_linear_speed_mm_s);
  if ((left_target == 0) ||
      ((left_target > 0) != (g_left.target_mm_s > 0)))
  {
    g_left.integral_mm_s_ms = 0;
  }
  if ((right_target == 0) ||
      ((right_target > 0) != (g_right.target_mm_s > 0)))
  {
    g_right.integral_mm_s_ms = 0;
  }
  g_left.target_mm_s = left_target;
  g_right.target_mm_s = right_target;
  g_enabled = true;
}

void WheelSpeedController_TaskStep(uint32_t dt_ms)
{
  if (!g_enabled || (dt_ms == 0U))
  {
    return;
  }

  const ChassisParams params = ChassisParams_Get();
  if (!params.wheel_speed_closed_loop_enabled)
  {
    const int16_t left_output =
        (int16_t)(((int32_t)g_left.target_mm_s * MOTOR_MAX_SPEED) /
                  params.max_linear_speed_mm_s);
    const int16_t right_output =
        (int16_t)(((int32_t)g_right.target_mm_s * MOTOR_MAX_SPEED) /
                  params.max_linear_speed_mm_s);
    Motor_SetSpeed(MOTOR_A, left_output);
    Motor_SetSpeed(MOTOR_B, left_output);
    Motor_SetSpeed(MOTOR_C, right_output);
    Motor_SetSpeed(MOTOR_D, right_output);
    return;
  }

  int32_t ramp_step =
      ((int32_t)params.wheel_accel_limit_mm_s2 * (int32_t)dt_ms) / 1000;
  if (ramp_step < 1)
  {
    ramp_step = 1;
  }
  g_left.ramped_target_mm_s =
      MoveTowards(g_left.ramped_target_mm_s, g_left.target_mm_s, (int16_t)ramp_step);
  g_right.ramped_target_mm_s =
      MoveTowards(g_right.ramped_target_mm_s, g_right.target_mm_s, (int16_t)ramp_step);

  const EncoderSample encoder_a = Encoder_GetSample(MOTOR_A);
  const EncoderSample encoder_b = Encoder_GetSample(MOTOR_B);
  const EncoderSample encoder_c = Encoder_GetSample(MOTOR_C);
  const EncoderSample encoder_d = Encoder_GetSample(MOTOR_D);
  const int16_t left_measured =
      EncoderDeltaToSpeed(encoder_a.delta,
                          encoder_b.delta,
                          ODOMETRY_MOTOR_A_SIGN,
                          ODOMETRY_MOTOR_B_SIGN,
                          dt_ms,
                          &params);
  const int16_t right_measured =
      EncoderDeltaToSpeed(encoder_c.delta,
                          encoder_d.delta,
                          ODOMETRY_MOTOR_C_SIGN,
                          ODOMETRY_MOTOR_D_SIGN,
                          dt_ms,
                          &params);

  const int16_t left_output =
      ApplyPi(&g_left, left_measured, params.wheel_left_output_permille, dt_ms, &params);
  const int16_t right_output =
      ApplyPi(&g_right, right_measured, params.wheel_right_output_permille, dt_ms, &params);
  Motor_SetSpeed(MOTOR_A, left_output);
  Motor_SetSpeed(MOTOR_B, left_output);
  Motor_SetSpeed(MOTOR_C, right_output);
  Motor_SetSpeed(MOTOR_D, right_output);
}

void WheelSpeedController_Stop(void)
{
  g_left = (WheelControllerState){0};
  g_right = (WheelControllerState){0};
  g_enabled = false;
  Motor_StopAll();
}
