#ifndef RASPI_LINK_H_
#define RASPI_LINK_H_

#include "usart.h"

void RaspiLink_Init(void);
void RaspiLink_TaskStep(void);
void RaspiLink_WriteString(const char *text);
void RaspiLink_OnUartRxCpltCallback(UART_HandleTypeDef *huart);

#endif  // RASPI_LINK_H_
