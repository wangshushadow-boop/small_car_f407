#include "raspi_link.h"

/*
 * 树莓派通信模块。
 *
 * USART3 接收树莓派发来的二进制短帧，MCU 在任务中解析命令并上传状态。
 * 这里仅负责协议收发和命令分发，耗时控制逻辑放到 Chassis、Motor、Servo 等模块。
 */

#include <stddef.h>
#include <string.h>

#include "debug_uart.h"
#include "chassis_params.h"
#include "main.h"
#include "odometry.h"
#include "servo.h"
#include "usart.h"

#define RASPI_FRAME_SYNC_0 0xAAU
#define RASPI_FRAME_SYNC_1 0x55U
#define RASPI_FRAME_VERSION 0x02U
#define RASPI_PAYLOAD_MAX_SIZE 64U
#define RASPI_RX_RING_SIZE 256U
#define RASPI_RX_BUDGET 96U
#define RASPI_TX_TIMEOUT_MS 20U
#define RASPI_CONTROL_TIMEOUT_MS 300U

#define RASPI_MSG_CONTROL 0x01U
#define RASPI_MSG_SERVO 0x02U
#define RASPI_MSG_HEARTBEAT 0x03U
#define RASPI_MSG_PARAM 0x04U
#define RASPI_MSG_CHASSIS_STATUS 0x81U
#define RASPI_MSG_ENCODER_DELTA 0x82U
#define RASPI_MSG_IMU_RAW 0x83U
#define RASPI_MSG_DEVICE_STATUS 0x84U
#define RASPI_MSG_ACK 0x85U
#define RASPI_MSG_ODOMETRY 0x86U
#define RASPI_MSG_ODOMETRY_DEBUG 0x87U
#define RASPI_MSG_PARAM_VALUE 0x88U

#define RASPI_PARAM_LEGACY_ODOMETRY_RESET 1U
#define RASPI_PARAM_OP_SET 1U
#define RASPI_PARAM_OP_GET 2U
#define RASPI_PARAM_OP_ODOMETRY_RESET 3U

#define RASPI_MODE_STOP 0U
#define RASPI_MODE_VELOCITY 1U

#define RASPI_ACK_OK 0U
#define RASPI_ACK_CRC_ERROR 1U
#define RASPI_ACK_LEN_ERROR 2U
#define RASPI_ACK_UNSUPPORTED 3U

typedef enum {
  PARSER_WAIT_SYNC0 = 0,
  PARSER_WAIT_SYNC1,
  PARSER_READ_VER,
  PARSER_READ_MSG,
  PARSER_READ_SEQ,
  PARSER_READ_LEN,
  PARSER_READ_PAYLOAD,
  PARSER_READ_CRC0,
  PARSER_READ_CRC1,
} ParserState;

static uint8_t g_rx_byte = 0U;
static volatile uint16_t g_rx_head = 0U;
static volatile uint16_t g_rx_tail = 0U;
static uint8_t g_rx_ring[RASPI_RX_RING_SIZE];

static ParserState g_parser_state = PARSER_WAIT_SYNC0;
static uint8_t g_frame_version = 0U;
static uint8_t g_frame_msg = 0U;
static uint8_t g_frame_seq = 0U;
static uint8_t g_frame_len = 0U;
static uint8_t g_frame_payload[RASPI_PAYLOAD_MAX_SIZE];
static uint8_t g_frame_payload_index = 0U;
static uint8_t g_frame_crc_low = 0U;

static uint8_t g_tx_seq = 0U;
static ControlCommand g_host_command = {0};
static bool g_host_command_valid = false;
static uint32_t g_last_control_tick = 0U;

static bool PopRxByte(uint8_t *data);
static void ProcessByte(uint8_t data);
static void ResetParser(void);
static void HandleFrame(uint8_t msg, uint8_t seq, const uint8_t *payload, uint8_t length);
static void HandleControlCommand(uint8_t seq, const uint8_t *payload, uint8_t length);
static void HandleServoCommand(uint8_t seq, const uint8_t *payload, uint8_t length);
static void HandleParamCommand(uint8_t seq, const uint8_t *payload, uint8_t length);
static void SendFrame(uint8_t msg, const uint8_t *payload, uint8_t length);
static void SendAck(uint8_t ack_msg, uint8_t ack_seq, uint8_t result);
static uint16_t Crc16CcittFalse(const uint8_t *data, uint16_t length);
static uint16_t ReadU16Le(const uint8_t *data);
static int16_t ReadI16Le(const uint8_t *data);
static void WriteU16Le(uint8_t *data, uint16_t value);
static void WriteI16Le(uint8_t *data, int16_t value);
static uint32_t ReadU32Le(const uint8_t *data);
static int32_t ReadI32Le(const uint8_t *data);
static void WriteU32Le(uint8_t *data, uint32_t value);
static void WriteI32Le(uint8_t *data, int32_t value);
static void SendParamValue(uint8_t param_id, int32_t value);

void RaspiLink_Init(void)
{
  DebugUart_WriteString("[RPI] USART3 binary link init\r\n");
  (void)HAL_UART_Receive_IT(&huart3, &g_rx_byte, 1U);
}

void RaspiLink_TaskStep(void)
{
  for (uint32_t i = 0U; i < RASPI_RX_BUDGET; ++i)
  {
    uint8_t data = 0U;
    if (!PopRxByte(&data))
    {
      break;
    }

    ProcessByte(data);
  }
}

bool RaspiLink_GetControlCommand(ControlCommand *command)
{
  if (command == NULL)
  {
    return false;
  }

  if (!g_host_command_valid)
  {
    return false;
  }

  if ((HAL_GetTick() - g_last_control_tick) > RASPI_CONTROL_TIMEOUT_MS)
  {
    g_host_command_valid = false;
    return false;
  }

  *command = g_host_command;
  return true;
}

void RaspiLink_SendChassisStatus(const ControlCommand *command, int16_t ultra_mm)
{
  if (command == NULL)
  {
    return;
  }

  uint8_t payload[13] = {0};
  WriteU32Le(&payload[0], HAL_GetTick());
  payload[4] = (uint8_t)command->source;
  payload[5] = command->enabled ? 1U : 0U;
  payload[6] = (uint8_t)command->value_type;
  WriteI16Le(&payload[7], command->forward);
  WriteI16Le(&payload[9], command->turn);
  WriteI16Le(&payload[11], ultra_mm);
  SendFrame(RASPI_MSG_CHASSIS_STATUS, payload, sizeof(payload));
}

void RaspiLink_SendEncoderDelta(int16_t delta_a,
                                int16_t delta_b,
                                int16_t delta_c,
                                int16_t delta_d)
{
  uint8_t payload[12] = {0};
  WriteU32Le(&payload[0], HAL_GetTick());
  WriteI16Le(&payload[4], delta_a);
  WriteI16Le(&payload[6], delta_b);
  WriteI16Le(&payload[8], delta_c);
  WriteI16Le(&payload[10], delta_d);
  SendFrame(RASPI_MSG_ENCODER_DELTA, payload, sizeof(payload));
}

void RaspiLink_SendImuRaw(int16_t ax,
                          int16_t ay,
                          int16_t az,
                          int16_t gx,
                          int16_t gy,
                          int16_t gz)
{
  uint8_t payload[16] = {0};
  WriteU32Le(&payload[0], HAL_GetTick());
  WriteI16Le(&payload[4], ax);
  WriteI16Le(&payload[6], ay);
  WriteI16Le(&payload[8], az);
  WriteI16Le(&payload[10], gx);
  WriteI16Le(&payload[12], gy);
  WriteI16Le(&payload[14], gz);
  SendFrame(RASPI_MSG_IMU_RAW, payload, sizeof(payload));
}

void RaspiLink_SendDeviceStatus(bool pad_ok, bool imu_ok, bool ultra_ok, uint8_t error)
{
  uint8_t payload[8] = {0};
  WriteU32Le(&payload[0], HAL_GetTick());
  payload[4] = pad_ok ? 1U : 0U;
  payload[5] = imu_ok ? 1U : 0U;
  payload[6] = ultra_ok ? 1U : 0U;
  payload[7] = error;
  SendFrame(RASPI_MSG_DEVICE_STATUS, payload, sizeof(payload));
}

void RaspiLink_SendOdometry(const OdometrySample *odometry)
{
  if (odometry == NULL)
  {
    return;
  }

  uint8_t payload[40] = {0};
  WriteU32Le(&payload[0], odometry->time_ms);
  WriteI32Le(&payload[4], odometry->x_mm);
  WriteI32Le(&payload[8], odometry->y_mm);
  WriteI32Le(&payload[12], odometry->z_mm);
  WriteI32Le(&payload[16], odometry->distance_mm);
  WriteI16Le(&payload[20], odometry->speed_mm_s);
  WriteI32Le(&payload[22], odometry->roll_mdeg);
  WriteI32Le(&payload[26], odometry->pitch_mdeg);
  WriteI32Le(&payload[30], odometry->yaw_mdeg);
  WriteI32Le(&payload[34], odometry->yaw_rate_mdeg_s);
  payload[38] = odometry->calibrated ? 1U : 0U;
  payload[39] = odometry->wheel_yaw_fused ? 1U : 0U;
  SendFrame(RASPI_MSG_ODOMETRY, payload, sizeof(payload));
}

void RaspiLink_SendOdometryDebug(uint32_t odom_time_ms,
                                 int16_t left_speed_mm_s,
                                 int16_t right_speed_mm_s,
                                 int16_t turn_speed_mm_s,
                                 int16_t left_delta_mm,
                                 int16_t right_delta_mm)
{
  uint8_t payload[14] = {0};
  WriteU32Le(&payload[0], odom_time_ms);
  WriteI16Le(&payload[4], left_speed_mm_s);
  WriteI16Le(&payload[6], right_speed_mm_s);
  WriteI16Le(&payload[8], turn_speed_mm_s);
  WriteI16Le(&payload[10], left_delta_mm);
  WriteI16Le(&payload[12], right_delta_mm);
  SendFrame(RASPI_MSG_ODOMETRY_DEBUG, payload, sizeof(payload));
}

void RaspiLink_OnUartRxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance != USART3)
  {
    return;
  }

  uint16_t next_head = (uint16_t)((g_rx_head + 1U) % RASPI_RX_RING_SIZE);
  if (next_head != g_rx_tail)
  {
    g_rx_ring[g_rx_head] = g_rx_byte;
    g_rx_head = next_head;
  }

  (void)HAL_UART_Receive_IT(&huart3, &g_rx_byte, 1U);
}

void RaspiLink_OnUartErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance != USART3)
  {
    return;
  }

  /*
   * USART 发生溢出、噪声或帧错误后，HAL 会终止当前中断接收。
   * 如果不重新启动，MCU 仍能向树莓派发送传感器数据，但再也收不到控制命令。
   * 丢弃错误发生前未完成的半帧，并从下一帧帧头重新同步。
   */
  g_rx_head = 0U;
  g_rx_tail = 0U;
  ResetParser();
  __HAL_UART_CLEAR_PEFLAG(huart);
  (void)HAL_UART_Receive_IT(&huart3, &g_rx_byte, 1U);
}

static bool PopRxByte(uint8_t *data)
{
  if ((data == NULL) || (g_rx_tail == g_rx_head))
  {
    return false;
  }

  *data = g_rx_ring[g_rx_tail];
  g_rx_tail = (uint16_t)((g_rx_tail + 1U) % RASPI_RX_RING_SIZE);
  return true;
}

static void ProcessByte(uint8_t data)
{
  switch (g_parser_state)
  {
    case PARSER_WAIT_SYNC0:
      if (data == RASPI_FRAME_SYNC_0)
      {
        g_parser_state = PARSER_WAIT_SYNC1;
      }
      break;

    case PARSER_WAIT_SYNC1:
      if (data == RASPI_FRAME_SYNC_1)
      {
        g_parser_state = PARSER_READ_VER;
      }
      else
      {
        g_parser_state = (data == RASPI_FRAME_SYNC_0) ? PARSER_WAIT_SYNC1 : PARSER_WAIT_SYNC0;
      }
      break;

    case PARSER_READ_VER:
      g_frame_version = data;
      g_parser_state = PARSER_READ_MSG;
      break;

    case PARSER_READ_MSG:
      g_frame_msg = data;
      g_parser_state = PARSER_READ_SEQ;
      break;

    case PARSER_READ_SEQ:
      g_frame_seq = data;
      g_parser_state = PARSER_READ_LEN;
      break;

    case PARSER_READ_LEN:
      g_frame_len = data;
      g_frame_payload_index = 0U;
      if (g_frame_len > RASPI_PAYLOAD_MAX_SIZE)
      {
        SendAck(g_frame_msg, g_frame_seq, RASPI_ACK_LEN_ERROR);
        ResetParser();
      }
      else if (g_frame_len == 0U)
      {
        g_parser_state = PARSER_READ_CRC0;
      }
      else
      {
        g_parser_state = PARSER_READ_PAYLOAD;
      }
      break;

    case PARSER_READ_PAYLOAD:
      g_frame_payload[g_frame_payload_index] = data;
      ++g_frame_payload_index;
      if (g_frame_payload_index >= g_frame_len)
      {
        g_parser_state = PARSER_READ_CRC0;
      }
      break;

    case PARSER_READ_CRC0:
      g_frame_crc_low = data;
      g_parser_state = PARSER_READ_CRC1;
      break;

    case PARSER_READ_CRC1: {
      uint8_t crc_data[4U + RASPI_PAYLOAD_MAX_SIZE] = {0};
      crc_data[0] = g_frame_version;
      crc_data[1] = g_frame_msg;
      crc_data[2] = g_frame_seq;
      crc_data[3] = g_frame_len;
      (void)memcpy(&crc_data[4], g_frame_payload, g_frame_len);

      uint16_t expected_crc = (uint16_t)g_frame_crc_low | ((uint16_t)data << 8U);
      uint16_t actual_crc = Crc16CcittFalse(crc_data, (uint16_t)(4U + g_frame_len));
      if (actual_crc == expected_crc)
      {
        HandleFrame(g_frame_msg, g_frame_seq, g_frame_payload, g_frame_len);
      }
      else
      {
        SendAck(g_frame_msg, g_frame_seq, RASPI_ACK_CRC_ERROR);
      }
      ResetParser();
      break;
    }

    default:
      ResetParser();
      break;
  }
}

static void ResetParser(void)
{
  g_parser_state = PARSER_WAIT_SYNC0;
  g_frame_payload_index = 0U;
}

static void HandleFrame(uint8_t msg, uint8_t seq, const uint8_t *payload, uint8_t length)
{
  if (g_frame_version != RASPI_FRAME_VERSION)
  {
    SendAck(msg, seq, RASPI_ACK_UNSUPPORTED);
    return;
  }

  switch (msg)
  {
    case RASPI_MSG_CONTROL:
      HandleControlCommand(seq, payload, length);
      break;
    case RASPI_MSG_SERVO:
      HandleServoCommand(seq, payload, length);
      break;
    case RASPI_MSG_HEARTBEAT:
      SendAck(msg, seq, (length == 4U) ? RASPI_ACK_OK : RASPI_ACK_LEN_ERROR);
      break;
    case RASPI_MSG_PARAM:
      HandleParamCommand(seq, payload, length);
      break;
    default:
      SendAck(msg, seq, RASPI_ACK_UNSUPPORTED);
      break;
  }
}

static void HandleControlCommand(uint8_t seq, const uint8_t *payload, uint8_t length)
{
  if ((payload == NULL) || (length != 10U))
  {
    SendAck(RASPI_MSG_CONTROL, seq, RASPI_ACK_LEN_ERROR);
    return;
  }

  uint8_t mode = payload[4];
  uint8_t enable = payload[5];

  g_host_command.source = CONTROL_SOURCE_HOST;
  g_host_command.value_type = CONTROL_VALUE_PHYSICAL_VELOCITY;
  g_host_command.enabled = false;
  g_host_command.forward = 0;
  g_host_command.turn = 0;

  if ((mode == RASPI_MODE_STOP) || (enable == 0U))
  {
    /* STOP 或 enable=0 都表示上位机主动要求停车。 */
  }
  else if (mode == RASPI_MODE_VELOCITY)
  {
    g_host_command.enabled = true;
    /* 协议 v2 直接传输 ROS 物理速度：mm/s 和 mrad/s。 */
    g_host_command.forward = ReadI16Le(&payload[6]);
    g_host_command.turn = ReadI16Le(&payload[8]);
  }
  else
  {
    SendAck(RASPI_MSG_CONTROL, seq, RASPI_ACK_UNSUPPORTED);
    return;
  }

  g_host_command_valid = true;
  g_last_control_tick = HAL_GetTick();
  SendAck(RASPI_MSG_CONTROL, seq, RASPI_ACK_OK);
}

static void HandleServoCommand(uint8_t seq, const uint8_t *payload, uint8_t length)
{
  if ((payload == NULL) || (length != 8U))
  {
    SendAck(RASPI_MSG_SERVO, seq, RASPI_ACK_LEN_ERROR);
    return;
  }

  Servo_SetBothPulse(ReadU16Le(&payload[4]), ReadU16Le(&payload[6]));
  SendAck(RASPI_MSG_SERVO, seq, RASPI_ACK_OK);
}

static void HandleParamCommand(uint8_t seq, const uint8_t *payload, uint8_t length)
{
  if (payload == NULL)
  {
    SendAck(RASPI_MSG_PARAM, seq, RASPI_ACK_LEN_ERROR);
    return;
  }

  /*
   * payload[0..3] 是树莓派发送时间，当前只保留给后续时间同步使用。
   * payload[4] 是参数子命令，1 表示清零里程计并重新校准陀螺仪零偏。
   */
  if ((length == 5U) &&
      ((payload[4] == RASPI_PARAM_LEGACY_ODOMETRY_RESET) ||
       (payload[4] == RASPI_PARAM_OP_ODOMETRY_RESET)))
  {
    Odometry_Reset();
    SendAck(RASPI_MSG_PARAM, seq, RASPI_ACK_OK);
    return;
  }

  if ((length == 10U) && (payload[4] == RASPI_PARAM_OP_SET))
  {
    const uint8_t param_id = payload[5];
    const int32_t value = ReadI32Le(&payload[6]);
    if (ChassisParams_Set((ChassisParamId)param_id, value))
    {
      SendAck(RASPI_MSG_PARAM, seq, RASPI_ACK_OK);
    }
    else
    {
      SendAck(RASPI_MSG_PARAM, seq, RASPI_ACK_UNSUPPORTED);
    }
    return;
  }

  if ((length == 6U) && (payload[4] == RASPI_PARAM_OP_GET))
  {
    const uint8_t param_id = payload[5];
    int32_t value = 0;
    if (ChassisParams_GetValue((ChassisParamId)param_id, &value))
    {
      SendParamValue(param_id, value);
      SendAck(RASPI_MSG_PARAM, seq, RASPI_ACK_OK);
    }
    else
    {
      SendAck(RASPI_MSG_PARAM, seq, RASPI_ACK_UNSUPPORTED);
    }
    return;
  }

  SendAck(RASPI_MSG_PARAM, seq, RASPI_ACK_LEN_ERROR);
}

static void SendFrame(uint8_t msg, const uint8_t *payload, uint8_t length)
{
  if ((length > RASPI_PAYLOAD_MAX_SIZE) ||
      ((payload == NULL) && (length > 0U)))
  {
    return;
  }

  uint8_t frame[2U + 4U + RASPI_PAYLOAD_MAX_SIZE + 2U] = {0};
  uint16_t index = 0U;
  frame[index++] = RASPI_FRAME_SYNC_0;
  frame[index++] = RASPI_FRAME_SYNC_1;
  frame[index++] = RASPI_FRAME_VERSION;
  frame[index++] = msg;
  frame[index++] = g_tx_seq++;
  frame[index++] = length;
  if (length > 0U)
  {
    (void)memcpy(&frame[index], payload, length);
    index = (uint16_t)(index + length);
  }

  /* CRC 覆盖 VER 到 PAYLOAD，帧同步 AA 55 不参与校验。 */
  uint16_t crc = Crc16CcittFalse(&frame[2], (uint16_t)(4U + length));
  frame[index++] = (uint8_t)(crc & 0xFFU);
  frame[index++] = (uint8_t)(crc >> 8U);

  (void)HAL_UART_Transmit(&huart3, frame, index, RASPI_TX_TIMEOUT_MS);
}

static void SendAck(uint8_t ack_msg, uint8_t ack_seq, uint8_t result)
{
  uint8_t payload[3] = {ack_msg, ack_seq, result};
  SendFrame(RASPI_MSG_ACK, payload, sizeof(payload));
}

static void SendParamValue(uint8_t param_id, int32_t value)
{
  uint8_t payload[9] = {0};
  WriteU32Le(&payload[0], HAL_GetTick());
  payload[4] = param_id;
  WriteI32Le(&payload[5], value);
  SendFrame(RASPI_MSG_PARAM_VALUE, payload, sizeof(payload));
}

static uint16_t Crc16CcittFalse(const uint8_t *data, uint16_t length)
{
  uint16_t crc = 0xFFFFU;
  for (uint16_t i = 0U; i < length; ++i)
  {
    crc ^= (uint16_t)data[i] << 8U;
    for (uint8_t bit = 0U; bit < 8U; ++bit)
    {
      if ((crc & 0x8000U) != 0U)
      {
        crc = (uint16_t)((crc << 1U) ^ 0x1021U);
      }
      else
      {
        crc = (uint16_t)(crc << 1U);
      }
    }
  }
  return crc;
}

static uint16_t ReadU16Le(const uint8_t *data)
{
  return (uint16_t)data[0] | ((uint16_t)data[1] << 8U);
}

static uint32_t ReadU32Le(const uint8_t *data)
{
  return (uint32_t)data[0] | ((uint32_t)data[1] << 8U) |
         ((uint32_t)data[2] << 16U) | ((uint32_t)data[3] << 24U);
}

static int16_t ReadI16Le(const uint8_t *data)
{
  return (int16_t)ReadU16Le(data);
}

static int32_t ReadI32Le(const uint8_t *data)
{
  return (int32_t)ReadU32Le(data);
}

static void WriteU16Le(uint8_t *data, uint16_t value)
{
  data[0] = (uint8_t)(value & 0xFFU);
  data[1] = (uint8_t)(value >> 8U);
}

static void WriteI16Le(uint8_t *data, int16_t value)
{
  WriteU16Le(data, (uint16_t)value);
}

static void WriteU32Le(uint8_t *data, uint32_t value)
{
  data[0] = (uint8_t)(value & 0xFFU);
  data[1] = (uint8_t)((value >> 8U) & 0xFFU);
  data[2] = (uint8_t)((value >> 16U) & 0xFFU);
  data[3] = (uint8_t)((value >> 24U) & 0xFFU);
}

static void WriteI32Le(uint8_t *data, int32_t value)
{
  WriteU32Le(data, (uint32_t)value);
}
