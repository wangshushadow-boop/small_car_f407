#include "motor.h"

#include "debug_uart.h"
#include "main.h"

#define MOTOR_TEST_RUN_TICKS 20U
#define MOTOR_TEST_STOP_TICKS 3U

typedef struct {
  GPIO_TypeDef *in1_port;
  uint16_t in1_pin;
  GPIO_TypeDef *in2_port;
  uint16_t in2_pin;
  const char *name;
} MotorPins;

static const MotorPins kMotorPins[] = {
    [MOTOR_A] = {MOTOR_A_IN1_GPIO_Port, MOTOR_A_IN1_Pin, MOTOR_A_IN2_GPIO_Port, MOTOR_A_IN2_Pin, "A"},
    [MOTOR_B] = {MOTOR_B_IN1_GPIO_Port, MOTOR_B_IN1_Pin, MOTOR_B_IN2_GPIO_Port, MOTOR_B_IN2_Pin, "B"},
    [MOTOR_C] = {MOTOR_C_IN1_GPIO_Port, MOTOR_C_IN1_Pin, MOTOR_C_IN2_GPIO_Port, MOTOR_C_IN2_Pin, "C"},
    [MOTOR_D] = {MOTOR_D_IN1_GPIO_Port, MOTOR_D_IN1_Pin, MOTOR_D_IN2_GPIO_Port, MOTOR_D_IN2_Pin, "D"},
};

static void Motor_WritePins(const MotorPins *pins, GPIO_PinState in1_state, GPIO_PinState in2_state)
{
  HAL_GPIO_WritePin(pins->in1_port, pins->in1_pin, in1_state);
  HAL_GPIO_WritePin(pins->in2_port, pins->in2_pin, in2_state);
}

void Motor_Init(void)
{
  Motor_StopAll();
}

void Motor_SetDirection(MotorId motor, MotorDirection direction)
{
  if (motor > MOTOR_D)
  {
    return;
  }

  const MotorPins *pins = &kMotorPins[motor];
  switch (direction)
  {
    case MOTOR_DIRECTION_FORWARD:
      Motor_WritePins(pins, GPIO_PIN_SET, GPIO_PIN_RESET);
      break;

    case MOTOR_DIRECTION_REVERSE:
      Motor_WritePins(pins, GPIO_PIN_RESET, GPIO_PIN_SET);
      break;

    case MOTOR_DIRECTION_BRAKE:
      Motor_WritePins(pins, GPIO_PIN_SET, GPIO_PIN_SET);
      break;

    case MOTOR_DIRECTION_STOP:
    default:
      Motor_WritePins(pins, GPIO_PIN_RESET, GPIO_PIN_RESET);
      break;
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
    DebugUart_WriteString("[MOTOR] all-motor forward/reverse test start\r\n");
  }

  if (ticks_in_state == 0U)
  {
    switch (state)
    {
      case TEST_ALL_FORWARD:
        Motor_SetAllDirection(MOTOR_DIRECTION_FORWARD);
        DebugUart_WriteString("[MOTOR] all forward\r\n");
        break;

      case TEST_STOP_AFTER_FORWARD:
        Motor_StopAll();
        DebugUart_WriteString("[MOTOR] all stop\r\n");
        break;

      case TEST_ALL_REVERSE:
        Motor_SetAllDirection(MOTOR_DIRECTION_REVERSE);
        DebugUart_WriteString("[MOTOR] all reverse\r\n");
        break;

      case TEST_STOP_AFTER_REVERSE:
      default:
        Motor_StopAll();
        DebugUart_WriteString("[MOTOR] all stop\r\n");
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
