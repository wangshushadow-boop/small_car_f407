#include "raspi_link.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "debug_uart.h"
#include "main.h"
#include "usart.h"

#define RASPI_LINK_LINE_SIZE 64U
#define RASPI_LINK_RX_RING_SIZE 128U
#define RASPI_LINK_RX_BUDGET 64U
#define RASPI_LINK_IDLE_TIMEOUT_MS 300U
#define RASPI_LINK_TX_TIMEOUT_MS 20U

static char g_line_buffer[RASPI_LINK_LINE_SIZE];
static size_t g_line_length = 0U;
static uint32_t g_last_rx_tick = 0U;
static uint8_t g_rx_byte = 0U;
static volatile uint16_t g_rx_head = 0U;
static volatile uint16_t g_rx_tail = 0U;
static uint8_t g_rx_ring[RASPI_LINK_RX_RING_SIZE];

static void ProcessByte(uint8_t data);
static void ProcessCommand(const char *line);
static bool PopRxByte(uint8_t *data);

void RaspiLink_Init(void)
{
  DebugUart_WriteString("[RPI] USART3 init\r\n");
  RaspiLink_WriteString("[RPI] USART3 ready, 115200 8N1\r\n");
  RaspiLink_WriteString("[RPI] commands: ping, help, status, echo <text>\r\n");
  (void)HAL_UART_Receive_IT(&huart3, &g_rx_byte, 1U);
}

void RaspiLink_TaskStep(void)
{
  bool received = false;

  for (uint32_t i = 0U; i < RASPI_LINK_RX_BUDGET; ++i)
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
      ((HAL_GetTick() - g_last_rx_tick) >= RASPI_LINK_IDLE_TIMEOUT_MS))
  {
    g_line_buffer[g_line_length] = '\0';
    ProcessCommand(g_line_buffer);
    g_line_length = 0U;
  }
}

void RaspiLink_OnUartRxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance != USART3)
  {
    return;
  }

  uint16_t next_head = (uint16_t)((g_rx_head + 1U) % RASPI_LINK_RX_RING_SIZE);
  if (next_head != g_rx_tail)
  {
    g_rx_ring[g_rx_head] = g_rx_byte;
    g_rx_head = next_head;
  }

  (void)HAL_UART_Receive_IT(&huart3, &g_rx_byte, 1U);
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

  if (g_line_length < (RASPI_LINK_LINE_SIZE - 1U))
  {
    g_line_buffer[g_line_length] = (char)data;
    ++g_line_length;
  }
  else
  {
    g_line_length = 0U;
    g_last_rx_tick = 0U;
    RaspiLink_WriteString("[RPI] line too long\r\n");
  }
}

static void ProcessCommand(const char *line)
{
  char command[16] = {0};
  char value[40] = {0};
  int fields = sscanf(line, "%15s %39[^\r\n]", command, value);

  if (fields <= 0)
  {
    return;
  }

  if (strcmp(command, "ping") == 0)
  {
    RaspiLink_WriteString("[RPI] pong\r\n");
    return;
  }

  if ((strcmp(command, "help") == 0) || (strcmp(command, "?") == 0))
  {
    RaspiLink_WriteString("[RPI] commands: ping, help, status, echo <text>\r\n");
    return;
  }

  if (strcmp(command, "status") == 0)
  {
    RaspiLink_WriteString("[RPI] ok\r\n");
    return;
  }

  if (strcmp(command, "echo") == 0)
  {
    RaspiLink_WriteString("[RPI] echo ");
    RaspiLink_WriteString(value);
    RaspiLink_WriteString("\r\n");
    return;
  }

  RaspiLink_WriteString("[RPI] unknown command\r\n");
}

static bool PopRxByte(uint8_t *data)
{
  if ((data == NULL) || (g_rx_tail == g_rx_head))
  {
    return false;
  }

  *data = g_rx_ring[g_rx_tail];
  g_rx_tail = (uint16_t)((g_rx_tail + 1U) % RASPI_LINK_RX_RING_SIZE);
  return true;
}

void RaspiLink_WriteString(const char *text)
{
  if (text == NULL)
  {
    return;
  }

  size_t length = strlen(text);
  if (length == 0U)
  {
    return;
  }

  (void)HAL_UART_Transmit(&huart3, (uint8_t *)text, (uint16_t)length,
                          RASPI_LINK_TX_TIMEOUT_MS);
}
