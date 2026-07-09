#ifndef OLED_H_
#define OLED_H_

#include <stdint.h>

#define OLED_WIDTH 128U
#define OLED_HEIGHT 64U

void Oled_Init(void);
void Oled_Clear(void);
void Oled_Refresh(void);
void Oled_SetDisplayEnabled(uint8_t enabled);
void Oled_DrawPixel(uint8_t x, uint8_t y, uint8_t enabled);
void Oled_ShowChar(uint8_t x, uint8_t y, char ch);
void Oled_ShowString(uint8_t x, uint8_t y, const char *text);
void Oled_ShowBootScreen(void);

#endif  // OLED_H_
