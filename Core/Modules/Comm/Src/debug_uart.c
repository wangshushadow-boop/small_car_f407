#include "debug_uart.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "usart.h"

#define DEBUG_UART_TX_TIMEOUT_MS 100U

/* 默认关闭所有分类日志，用户通过串口命令手动打开需要观察的模块。 */
static uint32_t g_log_mask = 0U;

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

  /*
   * 当前调试口输出使用阻塞发送，代码简单、可靠。
   * 注意不要在高频中断里调用打印函数，避免阻塞系统。
   */
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
  /* 小缓冲区足够调试日志使用，超长日志会被截断，避免栈占用过大。 */
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

void DebugUart_WriteStringIf(uint32_t category, const char *text)
{
  /* 分类日志统一从这里判断开关，便于运行时通过串口命令控制输出量。 */
  if (!DebugUart_IsLogEnabled(category))
  {
    return;
  }
  DebugUart_WriteString(text);
}

void DebugUart_PrintfIf(uint32_t category, const char *format, ...)
{
  char buffer[128];
  va_list args;

  if (!DebugUart_IsLogEnabled(category) || (format == NULL))
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

void DebugUart_SetLogEnabled(uint32_t category, bool enabled)
{
  /* 每个日志分类对应 g_log_mask 中的一位。 */
  if (enabled)
  {
    g_log_mask |= category;
  }
  else
  {
    g_log_mask &= ~category;
  }
}

void DebugUart_SetLogMask(uint32_t mask)
{
  g_log_mask = mask;
}

uint32_t DebugUart_GetLogMask(void)
{
  return g_log_mask;
}

bool DebugUart_IsLogEnabled(uint32_t category)
{
  return (g_log_mask & category) != 0U;
}

int __io_putchar(int ch)
{
  /* printf 重定向入口：标准库 printf 最终会走到这里。 */
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
