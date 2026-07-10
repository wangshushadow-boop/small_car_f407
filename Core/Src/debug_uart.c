#include "debug_uart.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "usart.h"

#define DEBUG_UART_TX_TIMEOUT_MS 100U

void DebugUart_Init(void)
{
  /* USART1 is initialized by MX_USART1_UART_Init(). */
}

void DebugUart_Write(const uint8_t *data, size_t length)
{
  if (data == NULL || length == 0U)
  {
    return;
  }

  (void)HAL_UART_Transmit(&huart1, (uint8_t *)data, (uint16_t)length, DEBUG_UART_TX_TIMEOUT_MS);
}

void DebugUart_WriteString(const char *text)
{
  if (text == NULL)
  {
    return;
  }

  DebugUart_Write((const uint8_t *)text, strlen(text));
}

void DebugUart_Printf(const char *format, ...)
{
  char buffer[128];
  va_list args;

  if (format == NULL)
  {
    return;
  }

  va_start(args, format);
  int length = vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  if (length <= 0)
  {
    return;
  }

  size_t write_length = (size_t)length;
  if (write_length >= sizeof(buffer))
  {
    write_length = sizeof(buffer) - 1U;
  }

  DebugUart_Write((const uint8_t *)buffer, write_length);
}

int __io_putchar(int ch)
{
  uint8_t data = (uint8_t)ch;
  DebugUart_Write(&data, 1U);
  return ch;
}

int __io_getchar(void)
{
  uint8_t data = 0U;
  if (HAL_UART_Receive(&huart1, &data, 1U, HAL_MAX_DELAY) != HAL_OK)
  {
    return -1;
  }

  return (int)data;
}
