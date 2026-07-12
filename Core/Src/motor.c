#include "motor.h"

#include "debug_uart.h"
#include "main.h"
#include "tim.h"

#define MOTOR_TEST_RUN_TICKS 20U
#define MOTOR_TEST_STOP_TICKS 3U
#define MOTOR_SPEED_DEADBAND 10
#define MOTOR_REPORT_SPEED_UNKNOWN ((int16_t)-32768)
#define MOTOR_REPORT_DIRECTION_UNKNOWN 0xFFU

typedef struct {
  /* AT8236 每路电机有 IN1/IN2 两个 PWM 输入，用两个通道表示方向和占空比。 */
  TIM_HandleTypeDef *in1_timer;
  uint32_t in1_channel;
  TIM_HandleTypeDef *in2_timer;
  uint32_t in2_channel;
  const char *name;
} MotorPwm;

static const MotorPwm kMotorPwms[] = {
    /* 表格顺序必须和 MotorId 枚举保持一致，便于按电机编号索引。 */
    [MOTOR_A] = {&htim11, TIM_CHANNEL_1, &htim10, TIM_CHANNEL_1, "A"},
    [MOTOR_B] = {&htim9, TIM_CHANNEL_1, &htim9, TIM_CHANNEL_2, "B"},
    [MOTOR_C] = {&htim1, TIM_CHANNEL_2, &htim1, TIM_CHANNEL_1, "C"},
    [MOTOR_D] = {&htim1, TIM_CHANNEL_4, &htim1, TIM_CHANNEL_3, "D"},
};

static int16_t g_reported_speed[] = {
    [MOTOR_A] = MOTOR_REPORT_SPEED_UNKNOWN,
    [MOTOR_B] = MOTOR_REPORT_SPEED_UNKNOWN,
    [MOTOR_C] = MOTOR_REPORT_SPEED_UNKNOWN,
    [MOTOR_D] = MOTOR_REPORT_SPEED_UNKNOWN,
};
static uint8_t g_reported_direction[] = {
    [MOTOR_A] = MOTOR_REPORT_DIRECTION_UNKNOWN,
    [MOTOR_B] = MOTOR_REPORT_DIRECTION_UNKNOWN,
    [MOTOR_C] = MOTOR_REPORT_DIRECTION_UNKNOWN,
    [MOTOR_D] = MOTOR_REPORT_DIRECTION_UNKNOWN,
};

static const char *Motor_DirectionName(MotorDirection direction)
{
  switch (direction)
  {
    case MOTOR_DIRECTION_FORWARD:
      return "forward";

    case MOTOR_DIRECTION_REVERSE:
      return "reverse";

    case MOTOR_DIRECTION_BRAKE:
      return "brake";

    case MOTOR_DIRECTION_STOP:
    default:
      return "stop";
  }
}

static void Motor_ReportDirection(MotorId motor, MotorDirection direction)
{
  /* 避免同一方向重复打印，串口日志只保留状态变化。 */
  if (g_reported_direction[motor] == (uint8_t)direction)
  {
    return;
  }

  g_reported_direction[motor] = (uint8_t)direction;
  DebugUart_PrintfIf(DEBUG_LOG_MOTOR, "[MOTOR] %s %s\r\n", kMotorPwms[motor].name,
                     Motor_DirectionName(direction));
}

static void Motor_ReportSpeed(MotorId motor, int16_t speed)
{
  /* 避免速度未变化时刷屏，打开 motor 日志后更容易看关键变化。 */
  if (g_reported_speed[motor] == speed)
  {
    return;
  }

  g_reported_speed[motor] = speed;
  DebugUart_PrintfIf(DEBUG_LOG_MOTOR, "[MOTOR] %s speed=%d\r\n", kMotorPwms[motor].name, speed);
}

static uint32_t Motor_SpeedToCompare(const TIM_HandleTypeDef *timer, int16_t speed)
{
  /*
   * speed 范围为 0 到 MOTOR_MAX_SPEED。
   * 将速度比例映射为 CCR 比较值，也就是 PWM 占空比。
   */
  const uint32_t period = __HAL_TIM_GET_AUTORELOAD((TIM_HandleTypeDef *)timer);
  uint32_t compare = ((period + 1U) * (uint32_t)speed) / MOTOR_MAX_SPEED;
  if (compare > period)
  {
    compare = period;
  }

  return compare;
}

static void Motor_SetPwm(const MotorPwm *pwm, int16_t in1_speed, int16_t in2_speed)
{
  /* 对 AT8236 来说，一个方向通道给 PWM，另一个方向通道给 0，即可实现正/反转。 */
  __HAL_TIM_SET_COMPARE(pwm->in1_timer, pwm->in1_channel, Motor_SpeedToCompare(pwm->in1_timer, in1_speed));
  __HAL_TIM_SET_COMPARE(pwm->in2_timer, pwm->in2_channel, Motor_SpeedToCompare(pwm->in2_timer, in2_speed));
}

static void Motor_StartPwmChannel(TIM_HandleTypeDef *timer, uint32_t channel)
{
  /* PWM 启动失败通常表示 CubeMX 定时器或引脚配置有问题，直接进入 Error_Handler。 */
  if (HAL_TIM_PWM_Start(timer, channel) != HAL_OK)
  {
    Error_Handler();
  }
}

void Motor_Init(void)
{
  /* 每个电机需要启动 IN1 和 IN2 两路 PWM。 */
  for (MotorId motor = MOTOR_A; motor <= MOTOR_D; ++motor)
  {
    const MotorPwm *pwm = &kMotorPwms[motor];
    Motor_StartPwmChannel(pwm->in1_timer, pwm->in1_channel);
    Motor_StartPwmChannel(pwm->in2_timer, pwm->in2_channel);
  }

  Motor_StopAll();
}

void Motor_SetDirection(MotorId motor, MotorDirection direction)
{
  if (motor > MOTOR_D)
  {
    return;
  }

  const MotorPwm *pwm = &kMotorPwms[motor];
  /*
   * 方向控制表：
   * - forward: IN1=PWM, IN2=0
   * - reverse: IN1=0, IN2=PWM
   * - brake:   IN1=PWM, IN2=PWM
   * - stop:    IN1=0, IN2=0
   */
  switch (direction)
  {
    case MOTOR_DIRECTION_FORWARD:
      Motor_SetPwm(pwm, MOTOR_MAX_SPEED, 0);
      break;

    case MOTOR_DIRECTION_REVERSE:
      Motor_SetPwm(pwm, 0, MOTOR_MAX_SPEED);
      break;

    case MOTOR_DIRECTION_BRAKE:
      Motor_SetPwm(pwm, MOTOR_MAX_SPEED, MOTOR_MAX_SPEED);
      break;

    case MOTOR_DIRECTION_STOP:
    default:
      Motor_SetPwm(pwm, 0, 0);
      break;
  }

  Motor_ReportDirection(motor, direction);
}

static int16_t Motor_ClampSpeed(int16_t speed)
{
  /* 对外接口允许传入任意值，这里统一限幅到驱动层约定范围。 */
  if (speed > MOTOR_MAX_SPEED)
  {
    return MOTOR_MAX_SPEED;
  }

  if (speed < -MOTOR_MAX_SPEED)
  {
    return -MOTOR_MAX_SPEED;
  }

  return speed;
}

void Motor_SetSpeed(MotorId motor, int16_t speed)
{
  if (motor > MOTOR_D)
  {
    return;
  }

  const int16_t clamped_speed = Motor_ClampSpeed(speed);

  /* 正速度走正转通道，负速度走反转通道；绝对值决定 PWM 占空比。 */
  if (clamped_speed > MOTOR_SPEED_DEADBAND)
  {
    Motor_SetPwm(&kMotorPwms[motor], clamped_speed, 0);
    Motor_ReportSpeed(motor, clamped_speed);
    return;
  }

  if (clamped_speed < -MOTOR_SPEED_DEADBAND)
  {
    Motor_SetPwm(&kMotorPwms[motor], 0, (int16_t)-clamped_speed);
    Motor_ReportSpeed(motor, clamped_speed);
    return;
  }

  /* 速度很小时直接停车，避免低占空比下电机只抖不转。 */
  Motor_SetDirection(motor, MOTOR_DIRECTION_STOP);
  Motor_ReportSpeed(motor, 0);
}

void Motor_SetAllSpeed(int16_t speed)
{
  for (MotorId motor = MOTOR_A; motor <= MOTOR_D; ++motor)
  {
    Motor_SetSpeed(motor, speed);
  }
}

void Motor_StopAll(void)
{
  for (MotorId motor = MOTOR_A; motor <= MOTOR_D; ++motor)
  {
    Motor_SetDirection(motor, MOTOR_DIRECTION_STOP);
  }
}

static void Motor_SetAllDirection(MotorDirection direction)
{
  for (MotorId motor = MOTOR_A; motor <= MOTOR_D; ++motor)
  {
    Motor_SetDirection(motor, direction);
  }
}

void Motor_TestTaskStep(void)
{
  /*
   * 非阻塞测试状态机：
   * 每次被任务周期调用一次，通过 ticks_in_state 计数切换“正转-停止-反转-停止”。
   */
  typedef enum {
    TEST_ALL_FORWARD = 0,
    TEST_STOP_AFTER_FORWARD,
    TEST_ALL_REVERSE,
    TEST_STOP_AFTER_REVERSE,
  } TestState;

  static TestState state = TEST_ALL_FORWARD;
  static uint8_t ticks_in_state = 0U;
  static uint8_t initialized = 0U;

  if (initialized == 0U)
  {
    initialized = 1U;
    DebugUart_WriteStringIf(DEBUG_LOG_MOTOR, "[MOTOR] all-motor forward/reverse test start\r\n");
  }

  if (ticks_in_state == 0U)
  {
    /* 进入新状态的第一拍才下发动作和打印日志。 */
    switch (state)
    {
      case TEST_ALL_FORWARD:
        Motor_SetAllDirection(MOTOR_DIRECTION_FORWARD);
        DebugUart_WriteStringIf(DEBUG_LOG_MOTOR, "[MOTOR] all forward\r\n");
        break;

      case TEST_STOP_AFTER_FORWARD:
        Motor_StopAll();
        DebugUart_WriteStringIf(DEBUG_LOG_MOTOR, "[MOTOR] all stop\r\n");
        break;

      case TEST_ALL_REVERSE:
        Motor_SetAllDirection(MOTOR_DIRECTION_REVERSE);
        DebugUart_WriteStringIf(DEBUG_LOG_MOTOR, "[MOTOR] all reverse\r\n");
        break;

      case TEST_STOP_AFTER_REVERSE:
      default:
        Motor_StopAll();
        DebugUart_WriteStringIf(DEBUG_LOG_MOTOR, "[MOTOR] all stop\r\n");
        break;
    }
  }

  ++ticks_in_state;
  const uint8_t state_ticks =
      (state == TEST_ALL_FORWARD || state == TEST_ALL_REVERSE) ? MOTOR_TEST_RUN_TICKS : MOTOR_TEST_STOP_TICKS;
  if (ticks_in_state < state_ticks)
  {
    return;
  }

  ticks_in_state = 0U;
  if (state == TEST_STOP_AFTER_REVERSE)
  {
    state = TEST_ALL_FORWARD;
  }
  else
  {
    state = (TestState)(state + 1);
  }
}
