#ifndef DEBUG_UART_H_
#define DEBUG_UART_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DEBUG_UART_BAUDRATE 115200U

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

void DebugUart_Init(void);
void DebugUart_Write(const uint8_t *data, size_t length);
void DebugUart_WriteString(const char *text);
void DebugUart_Printf(const char *format, ...);
void DebugUart_WriteStringIf(uint32_t category, const char *text);
void DebugUart_PrintfIf(uint32_t category, const char *format, ...);
void DebugUart_SetLogEnabled(uint32_t category, bool enabled);
void DebugUart_SetLogMask(uint32_t mask);
uint32_t DebugUart_GetLogMask(void);
bool DebugUart_IsLogEnabled(uint32_t category);

#endif  // DEBUG_UART_H_
