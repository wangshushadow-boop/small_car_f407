#ifndef DEBUG_UART_H_
#define DEBUG_UART_H_

#include <stddef.h>
#include <stdint.h>

#define DEBUG_UART_BAUDRATE 115200U

void DebugUart_Init(void);
void DebugUart_Write(const uint8_t *data, size_t length);
void DebugUart_WriteString(const char *text);
void DebugUart_Printf(const char *format, ...);

#endif  // DEBUG_UART_H_
