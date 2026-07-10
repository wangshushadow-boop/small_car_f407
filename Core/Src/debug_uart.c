#include "debug_uart.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "main.h"

#define DEBUG_UART_INSTANCE USART1
#define DEBUG_UART_TX_PIN GPIO_PIN_9
#define DEBUG_UART_RX_PIN GPIO_PIN_10
#define DEBUG_UART_GPIO_AF 7U
#define DEBUG_UART_CR1_UE USART_CR1_UE
#define DEBUG_UART_CR1_TE USART_CR1_TE
#define DEBUG_UART_CR1_RE USART_CR1_RE
#define DEBUG_UART_SR_TXE USART_SR_TXE
#define DEBUG_UART_SR_RXNE USART_SR_RXNE

static void DebugUart_GPIOInit(void)
{
  __HAL_RCC_GPIOA_CLK_ENABLE();

  GPIOA->MODER &= ~((3UL << (9U * 2U)) | (3UL << (10U * 2U)));
  GPIOA->MODER |= (2UL << (9U * 2U)) | (2UL << (10U * 2U));

  GPIOA->OTYPER &= ~(DEBUG_UART_TX_PIN | DEBUG_UART_RX_PIN);

  GPIOA->OSPEEDR &= ~((3UL << (9U * 2U)) | (3UL << (10U * 2U)));
  GPIOA->OSPEEDR |= (3UL << (9U * 2U)) | (3UL << (10U * 2U));

  GPIOA->PUPDR &= ~((3UL << (9U * 2U)) | (3UL << (10U * 2U)));
  GPIOA->PUPDR |= (1UL << (9U * 2U)) | (1UL << (10U * 2U));

  GPIOA->AFR[1] &= ~((0xFUL << ((9U - 8U) * 4U)) | (0xFUL << ((10U - 8U) * 4U)));
  GPIOA->AFR[1] |= (DEBUG_UART_GPIO_AF << ((9U - 8U) * 4U)) | (DEBUG_UART_GPIO_AF << ((10U - 8U) * 4U));
}

void DebugUart_Init(void)
{
  DebugUart_GPIOInit();
  __HAL_RCC_USART1_CLK_ENABLE();

  DEBUG_UART_INSTANCE->CR1 = 0U;
  DEBUG_UART_INSTANCE->CR2 = 0U;
  DEBUG_UART_INSTANCE->CR3 = 0U;
  DEBUG_UART_INSTANCE->BRR = (HAL_RCC_GetPCLK2Freq() + (DEBUG_UART_BAUDRATE / 2U)) / DEBUG_UART_BAUDRATE;
  DEBUG_UART_INSTANCE->CR1 = DEBUG_UART_CR1_TE | DEBUG_UART_CR1_RE | DEBUG_UART_CR1_UE;
}

void DebugUart_Write(const uint8_t *data, size_t length)
{
  if (data == NULL || length == 0U)
  {
    return;
  }

  for (size_t i = 0; i < length; ++i)
  {
    while ((DEBUG_UART_INSTANCE->SR & DEBUG_UART_SR_TXE) == 0U)
    {
    }

    DEBUG_UART_INSTANCE->DR = data[i];
  }

  while ((DEBUG_UART_INSTANCE->SR & USART_SR_TC) == 0U)
  {
  }
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
  while ((DEBUG_UART_INSTANCE->SR & DEBUG_UART_SR_RXNE) == 0U)
  {
  }

  return (int)(DEBUG_UART_INSTANCE->DR & 0xFFU);
}
