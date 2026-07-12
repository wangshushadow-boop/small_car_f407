#include "host_link.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "debug_uart.h"
#include "main.h"
#include "usart.h"

#define HOST_LINK_LINE_SIZE 64U
#define HOST_LINK_RX_RING_SIZE 128U
#define HOST_LINK_RX_BUDGET 64U
#define HOST_LINK_IDLE_TIMEOUT_MS 300U

static char g_line_buffer[HOST_LINK_LINE_SIZE];
static size_t g_line_length = 0U;
static uint32_t g_last_rx_tick = 0U;
static uint8_t g_rx_byte = 0U;
static volatile uint16_t g_rx_head = 0U;
static volatile uint16_t g_rx_tail = 0U;
static uint8_t g_rx_ring[HOST_LINK_RX_RING_SIZE];

static void ProcessByte(uint8_t data);
static void ProcessCommand(const char *line);
static bool PopRxByte(uint8_t *data);
static const char *BoolToText(bool enabled);
static bool SetLogSwitch(const char *name, const char *action, uint32_t category);
static void PrintLogHelp(void);
static void PrintLogStatus(void);

void HostLink_Init(void)
{
  DebugUart_WriteString("[CMD] send 'help' for commands\r\n");
  (void)HAL_UART_Receive_IT(&huart1, &g_rx_byte, 1U);
}

void HostLink_TaskStep(void)
{
  bool received = false;
  for (uint32_t i = 0U; i < HOST_LINK_RX_BUDGET; ++i)
  {
    uint8_t data = 0U;
    if (!PopRxByte(&data))
    {
      break;
    }
    received = true;
    g_last_rx_tick = HAL_GetTick();
    ProcessByte(data);
  }

  if (!received && (g_line_length > 0U) &&
      ((HAL_GetTick() - g_last_rx_tick) >= HOST_LINK_IDLE_TIMEOUT_MS))
  {
    g_line_buffer[g_line_length] = '\0';
    ProcessCommand(g_line_buffer);
    g_line_length = 0U;
  }
}

bool HostLink_GetControlCommand(ControlCommand *command)
{
  if (command == NULL)
  {
    return false;
  }

  command->source = CONTROL_SOURCE_HOST;
  command->enabled = false;
  command->forward = 0;
  command->turn = 0;
  return false;
}

void HostLink_OnUartRxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance != USART1)
  {
    return;
  }

  uint16_t next_head = (uint16_t)((g_rx_head + 1U) % HOST_LINK_RX_RING_SIZE);
  if (next_head != g_rx_tail)
  {
    g_rx_ring[g_rx_head] = g_rx_byte;
    g_rx_head = next_head;
  }

  (void)HAL_UART_Receive_IT(&huart1, &g_rx_byte, 1U);
}

static void ProcessByte(uint8_t data)
{
  if ((data == '\r') || (data == '\n'))
  {
    if (g_line_length > 0U)
    {
      g_line_buffer[g_line_length] = '\0';
      ProcessCommand(g_line_buffer);
      g_line_length = 0U;
      g_last_rx_tick = 0U;
    }
    return;
  }

  if ((data == '\b') || (data == 0x7FU))
  {
    if (g_line_length > 0U)
    {
      --g_line_length;
    }
    return;
  }

  if (g_line_length < (HOST_LINK_LINE_SIZE - 1U))
  {
    g_line_buffer[g_line_length] = (char)data;
    ++g_line_length;
  }
  else
  {
    g_line_length = 0U;
    g_last_rx_tick = 0U;
    DebugUart_WriteString("[CMD] line too long\r\n");
  }
}

static void ProcessCommand(const char *line)
{
  char command[16] = {0};
  char action[8] = {0};
  int fields = sscanf(line, "%15s %7s", command, action);

  if (fields <= 0)
  {
    return;
  }

  if ((strcmp(command, "help") == 0) || (strcmp(command, "?") == 0))
  {
    PrintLogHelp();
    return;
  }

  if (strcmp(command, "status") == 0)
  {
    PrintLogStatus();
    return;
  }

  if (strcmp(command, "imu") == 0)
  {
    SetLogSwitch("imu", action, DEBUG_LOG_IMU);
    return;
  }

  if (strcmp(command, "pad") == 0)
  {
    SetLogSwitch("pad", action, DEBUG_LOG_GAMEPAD_DATA);
    return;
  }

  if (strcmp(command, "servo") == 0)
  {
    SetLogSwitch("servo", action, DEBUG_LOG_SERVO);
    return;
  }

  if (strcmp(command, "motor") == 0)
  {
    SetLogSwitch("motor", action, DEBUG_LOG_MOTOR);
    return;
  }

  if (strcmp(command, "ultra") == 0)
  {
    SetLogSwitch("ultra", action, DEBUG_LOG_ULTRASONIC);
    return;
  }

  DebugUart_WriteString("[CMD] unknown command\r\n");
  PrintLogHelp();
}

static bool PopRxByte(uint8_t *data)
{
  if ((data == NULL) || (g_rx_tail == g_rx_head))
  {
    return false;
  }

  *data = g_rx_ring[g_rx_tail];
  g_rx_tail = (uint16_t)((g_rx_tail + 1U) % HOST_LINK_RX_RING_SIZE);
  return true;
}

static const char *BoolToText(bool enabled)
{
  return enabled ? "on" : "off";
}

static bool SetLogSwitch(const char *name, const char *action, uint32_t category)
{
  bool enabled = false;
  if (strcmp(action, "on") == 0)
  {
    enabled = true;
  }
  else if (strcmp(action, "off") == 0)
  {
    enabled = false;
  }
  else
  {
    DebugUart_Printf("[CMD] usage: %s on | %s off\r\n", name, name);
    return false;
  }

  DebugUart_SetLogEnabled(category, enabled);
  DebugUart_Printf("[CMD] %s %s\r\n", name, BoolToText(enabled));
  return true;
}

static void PrintLogHelp(void)
{
  DebugUart_WriteString(
      "[CMD] commands: imu on/off, pad on/off, servo on/off, motor on/off, ultra on/off, status, help\r\n");
}

static void PrintLogStatus(void)
{
  DebugUart_Printf("[CMD] imu=%s pad=%s servo=%s motor=%s ultra=%s\r\n",
                   BoolToText(DebugUart_IsLogEnabled(DEBUG_LOG_IMU)),
                   BoolToText(DebugUart_IsLogEnabled(DEBUG_LOG_GAMEPAD_DATA)),
                   BoolToText(DebugUart_IsLogEnabled(DEBUG_LOG_SERVO)),
                   BoolToText(DebugUart_IsLogEnabled(DEBUG_LOG_MOTOR)),
                   BoolToText(DebugUart_IsLogEnabled(DEBUG_LOG_ULTRASONIC)));
}
