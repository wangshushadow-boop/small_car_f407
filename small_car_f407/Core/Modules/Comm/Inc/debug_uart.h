/**
 * @file debug_uart.h
 * @brief 声明 USART1 调试输出和按类别启停日志的接口。
 */
#ifndef DEBUG_UART_H_
#define DEBUG_UART_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DEBUG_UART_BAUDRATE 115200U

/** 调试日志分类位，可通过 USART1 文本命令独立启停。 */
typedef enum {
  DEBUG_LOG_BOOT = 1U << 0,
  DEBUG_LOG_RTOS = 1U << 1,
  DEBUG_LOG_USB = 1U << 2,
  DEBUG_LOG_GAMEPAD = 1U << 3,
  DEBUG_LOG_GAMEPAD_DATA = 1U << 4,
  DEBUG_LOG_IMU = 1U << 5,
  DEBUG_LOG_ENCODER = 1U << 6,
  DEBUG_LOG_CONTROL = 1U << 7,
  DEBUG_LOG_MOTOR = 1U << 8,
  DEBUG_LOG_SERVO = 1U << 9,
  DEBUG_LOG_ULTRASONIC = 1U << 10,
  DEBUG_LOG_ODOMETRY = 1U << 11,
} DebugLogCategory;

/** 初始化日志掩码；UART 外设本身由 CubeMX 初始化。 */
void DebugUart_Init(void);
/** 同步写出任意字节数组。 */
void DebugUart_Write(const uint8_t *data, size_t length);
/** 同步写出以 '\0' 结尾的字符串。 */
void DebugUart_WriteString(const char *text);
/** 使用固定大小临时缓冲格式化并发送日志。 */
void DebugUart_Printf(const char *format, ...);
/** 仅在分类开启时发送字符串或格式化日志。 */
void DebugUart_WriteStringIf(uint32_t category, const char *text);
void DebugUart_PrintfIf(uint32_t category, const char *format, ...);
/** 修改单个分类或一次替换完整日志掩码。 */
void DebugUart_SetLogEnabled(uint32_t category, bool enabled);
void DebugUart_SetLogMask(uint32_t mask);
/** 读取日志掩码或判断指定分类是否开启。 */
uint32_t DebugUart_GetLogMask(void);
bool DebugUart_IsLogEnabled(uint32_t category);

#endif  // DEBUG_UART_H_
