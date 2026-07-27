/**
 * @file oled.h
 * @brief 声明 128x64 OLED 帧缓冲绘图和屏幕刷新接口。
 */
#ifndef OLED_H_
#define OLED_H_

#include <stdint.h>

/** SSD1306 面板像素尺寸。 */
#define OLED_WIDTH 128U
#define OLED_HEIGHT 64U

/** 初始化 OLED 控制器并清空软件帧缓冲。 */
void Oled_Init(void);
/** 仅清空 RAM 帧缓冲；调用 Oled_Refresh 后才更新屏幕。 */
void Oled_Clear(void);
/** 将完整帧缓冲发送到 OLED。 */
void Oled_Refresh(void);
/** 打开或关闭面板显示，不丢弃帧缓冲内容。 */
void Oled_SetDisplayEnabled(uint8_t enabled);
/** 修改一个像素，越界坐标会被忽略。 */
void Oled_DrawPixel(uint8_t x, uint8_t y, uint8_t enabled);
/** 在指定像素坐标绘制一个 ASCII 字符。 */
void Oled_ShowChar(uint8_t x, uint8_t y, char ch);
/** 连续绘制以 '\0' 结尾的 ASCII 字符串。 */
void Oled_ShowString(uint8_t x, uint8_t y, const char *text);
/** 显示项目启动画面，供上电自检使用。 */
void Oled_ShowBootScreen(void);

#endif  // OLED_H_
