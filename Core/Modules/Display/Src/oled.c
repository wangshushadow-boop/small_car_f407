/**
 * @file oled.c
 * @brief 实现 SSD1306 兼容 OLED 的 GPIO 串行写入、帧缓冲和 ASCII 绘制。
 *
 * 当前板级接口使用 DC、RST、SDA、SCL 四根 GPIO，由软件产生串行时序，
 * 不占用 STM32 的硬件 I2C 外设。
 */
#include "oled.h"

/*
 * OLED 显示模块。
 *
 * OLED 只显示最关键的运行状态，避免把调试串口的大量信息搬到屏幕上。
 * 详细传感器数据仍以串口和树莓派工具为主。
 */

#include <stddef.h>
#include <string.h>

#include "main.h"

#define OLED_GPIO_PORT GPIOD
#define OLED_DC_PIN GPIO_PIN_11
#define OLED_RST_PIN GPIO_PIN_12
#define OLED_SDA_PIN GPIO_PIN_13
#define OLED_SCL_PIN GPIO_PIN_14

#define OLED_CMD 0U
#define OLED_DATA 1U

static uint8_t oled_gram[OLED_WIDTH][OLED_HEIGHT / 8U];

/** @brief 统一写 OLED 接口引脚，隐藏 HAL 的端口参数。 */
static void Oled_WritePin(uint16_t pin, GPIO_PinState state)
{
  HAL_GPIO_WritePin(OLED_GPIO_PORT, pin, state);
}

/** @brief 提供位操作之间的短延时，满足 OLED 串行时钟建立和保持时间。 */
static void Oled_DelayCycle(void)
{
  for (volatile uint32_t i = 0; i < 8U; ++i)
  {
  }
}

/**
 * @brief 以高位在前的顺序向 OLED 写入一个命令或数据字节。
 *
 * @param data 要发送的 8 位内容。
 * @param type OLED_CMD 表示命令，OLED_DATA 表示显存数据。
 */
static void Oled_WriteByte(uint8_t data, uint8_t type)
{
  Oled_WritePin(OLED_DC_PIN, type == OLED_DATA ? GPIO_PIN_SET : GPIO_PIN_RESET);

  for (uint8_t i = 0; i < 8U; ++i)
  {
    Oled_WritePin(OLED_SCL_PIN, GPIO_PIN_RESET);
    Oled_WritePin(OLED_SDA_PIN, (data & 0x80U) != 0U ? GPIO_PIN_SET : GPIO_PIN_RESET);
    Oled_DelayCycle();
    Oled_WritePin(OLED_SCL_PIN, GPIO_PIN_SET);
    Oled_DelayCycle();
    data <<= 1U;
  }

  Oled_WritePin(OLED_DC_PIN, GPIO_PIN_SET);
}

/** @brief 发送一字节控制命令。 */
static void Oled_WriteCommand(uint8_t command)
{
  Oled_WriteByte(command, OLED_CMD);
}

/** @brief 发送一字节显示数据。 */
static void Oled_WriteData(uint8_t data)
{
  Oled_WriteByte(data, OLED_DATA);
}

/** @brief 配置四根 OLED GPIO 并把总线置于空闲高电平。 */
static void Oled_GPIOInit(void)
{
  GPIO_InitTypeDef gpio_init = {0};

  __HAL_RCC_GPIOD_CLK_ENABLE();

  gpio_init.Pin = OLED_DC_PIN | OLED_RST_PIN | OLED_SDA_PIN | OLED_SCL_PIN;
  gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
  gpio_init.Pull = GPIO_NOPULL;
  gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(OLED_GPIO_PORT, &gpio_init);

  Oled_WritePin(OLED_DC_PIN, GPIO_PIN_SET);
  Oled_WritePin(OLED_RST_PIN, GPIO_PIN_SET);
  Oled_WritePin(OLED_SDA_PIN, GPIO_PIN_SET);
  Oled_WritePin(OLED_SCL_PIN, GPIO_PIN_SET);
}

/**
 * @brief 返回字符对应的 5x7 点阵。
 *
 * 小写字母统一转换为大写；字库之外的字符返回空白点阵。
 */
static const uint8_t *Oled_GetGlyph(char ch)
{
  static const uint8_t blank[5] = {0x00, 0x00, 0x00, 0x00, 0x00};
  static const uint8_t digits[10][5] = {
      {0x3E, 0x51, 0x49, 0x45, 0x3E}, {0x00, 0x42, 0x7F, 0x40, 0x00},
      {0x42, 0x61, 0x51, 0x49, 0x46}, {0x21, 0x41, 0x45, 0x4B, 0x31},
      {0x18, 0x14, 0x12, 0x7F, 0x10}, {0x27, 0x45, 0x45, 0x45, 0x39},
      {0x3C, 0x4A, 0x49, 0x49, 0x30}, {0x01, 0x71, 0x09, 0x05, 0x03},
      {0x36, 0x49, 0x49, 0x49, 0x36}, {0x06, 0x49, 0x49, 0x29, 0x1E},
  };
  static const uint8_t uppercase[26][5] = {
      {0x7E, 0x11, 0x11, 0x11, 0x7E}, {0x7F, 0x49, 0x49, 0x49, 0x36},
      {0x3E, 0x41, 0x41, 0x41, 0x22}, {0x7F, 0x41, 0x41, 0x22, 0x1C},
      {0x7F, 0x49, 0x49, 0x49, 0x41}, {0x7F, 0x09, 0x09, 0x09, 0x01},
      {0x3E, 0x41, 0x49, 0x49, 0x7A}, {0x7F, 0x08, 0x08, 0x08, 0x7F},
      {0x00, 0x41, 0x7F, 0x41, 0x00}, {0x20, 0x40, 0x41, 0x3F, 0x01},
      {0x7F, 0x08, 0x14, 0x22, 0x41}, {0x7F, 0x40, 0x40, 0x40, 0x40},
      {0x7F, 0x02, 0x0C, 0x02, 0x7F}, {0x7F, 0x04, 0x08, 0x10, 0x7F},
      {0x3E, 0x41, 0x41, 0x41, 0x3E}, {0x7F, 0x09, 0x09, 0x09, 0x06},
      {0x3E, 0x41, 0x51, 0x21, 0x5E}, {0x7F, 0x09, 0x19, 0x29, 0x46},
      {0x46, 0x49, 0x49, 0x49, 0x31}, {0x01, 0x01, 0x7F, 0x01, 0x01},
      {0x3F, 0x40, 0x40, 0x40, 0x3F}, {0x1F, 0x20, 0x40, 0x20, 0x1F},
      {0x3F, 0x40, 0x38, 0x40, 0x3F}, {0x63, 0x14, 0x08, 0x14, 0x63},
      {0x07, 0x08, 0x70, 0x08, 0x07}, {0x61, 0x51, 0x49, 0x45, 0x43},
  };
  static const uint8_t colon[5] = {0x00, 0x36, 0x36, 0x00, 0x00};
  static const uint8_t dot[5] = {0x00, 0x40, 0x60, 0x00, 0x00};
  static const uint8_t dash[5] = {0x08, 0x08, 0x08, 0x08, 0x08};

  if (ch >= 'a' && ch <= 'z')
  {
    ch = (char)(ch - ('a' - 'A'));
  }

  if (ch >= '0' && ch <= '9')
  {
    return digits[ch - '0'];
  }

  if (ch >= 'A' && ch <= 'Z')
  {
    return uppercase[ch - 'A'];
  }

  if (ch == ':')
  {
    return colon;
  }

  if (ch == '.')
  {
    return dot;
  }

  if (ch == '-')
  {
    return dash;
  }

  return blank;
}

void Oled_Init(void)
{
  /* 硬件复位后依次配置时钟、扫描方向、页寻址、对比度和电荷泵。 */
  Oled_GPIOInit();

  Oled_WritePin(OLED_RST_PIN, GPIO_PIN_RESET);
  HAL_Delay(100);
  Oled_WritePin(OLED_RST_PIN, GPIO_PIN_SET);

  Oled_WriteCommand(0xAE);
  Oled_WriteCommand(0xD5);
  Oled_WriteCommand(0x80);
  Oled_WriteCommand(0xA8);
  Oled_WriteCommand(0x3F);
  Oled_WriteCommand(0xD3);
  Oled_WriteCommand(0x00);
  Oled_WriteCommand(0x40);
  Oled_WriteCommand(0x8D);
  Oled_WriteCommand(0x14);
  Oled_WriteCommand(0x20);
  Oled_WriteCommand(0x02);
  Oled_WriteCommand(0xA1);
  Oled_WriteCommand(0xC0);
  Oled_WriteCommand(0xDA);
  Oled_WriteCommand(0x12);
  Oled_WriteCommand(0x81);
  Oled_WriteCommand(0xEF);
  Oled_WriteCommand(0xD9);
  Oled_WriteCommand(0xF1);
  Oled_WriteCommand(0xDB);
  Oled_WriteCommand(0x30);
  Oled_WriteCommand(0xA4);
  Oled_WriteCommand(0xA6);
  Oled_WriteCommand(0xAF);

  Oled_Clear();
}

void Oled_SetDisplayEnabled(uint8_t enabled)
{
  /* 关闭时同时关闭电荷泵，以降低屏幕不使用时的功耗。 */
  if (enabled != 0U)
  {
    Oled_WriteCommand(0x8D);
    Oled_WriteCommand(0x14);
    Oled_WriteCommand(0xAF);
  }
  else
  {
    Oled_WriteCommand(0x8D);
    Oled_WriteCommand(0x10);
    Oled_WriteCommand(0xAE);
  }
}

void Oled_Clear(void)
{
  /* 清空 RAM 后立即刷新，保证调用返回时物理屏幕已经变黑。 */
  (void)memset(oled_gram, 0, sizeof(oled_gram));
  Oled_Refresh();
}

void Oled_Refresh(void)
{
  /* SSD1306 每页对应垂直 8 个像素，逐页写入 128 列数据。 */
  for (uint8_t page = 0; page < (OLED_HEIGHT / 8U); ++page)
  {
    Oled_WriteCommand((uint8_t)(0xB0U + page));
    Oled_WriteCommand(0x00);
    Oled_WriteCommand(0x10);
    for (uint8_t column = 0; column < OLED_WIDTH; ++column)
    {
      Oled_WriteData(oled_gram[column][page]);
    }
  }
}

void Oled_DrawPixel(uint8_t x, uint8_t y, uint8_t enabled)
{
  if (x >= OLED_WIDTH || y >= OLED_HEIGHT)
  {
    return;
  }

  /*
   * 当前模块安装方向与控制器默认坐标相反，因此页号和页内位序都倒置。
   * 这里完成坐标转换，上层始终使用左上角为原点的屏幕坐标。
   */
  uint8_t page = (uint8_t)(7U - (y / 8U));
  uint8_t mask = (uint8_t)(1U << (7U - (y % 8U)));

  if (enabled != 0U)
  {
    oled_gram[x][page] |= mask;
  }
  else
  {
    oled_gram[x][page] &= (uint8_t)~mask;
  }
}

void Oled_ShowChar(uint8_t x, uint8_t y, char ch)
{
  const uint8_t *glyph = Oled_GetGlyph(ch);

  /* 字形宽 5 列，末尾再清一列作为字符间距。 */
  for (uint8_t column = 0; column < 5U; ++column)
  {
    uint8_t line = glyph[column];
    for (uint8_t row = 0; row < 7U; ++row)
    {
      Oled_DrawPixel((uint8_t)(x + column), (uint8_t)(y + row), (line >> row) & 0x01U);
    }
  }

  for (uint8_t row = 0; row < 7U; ++row)
  {
    Oled_DrawPixel((uint8_t)(x + 5U), (uint8_t)(y + row), 0U);
  }
}

void Oled_ShowString(uint8_t x, uint8_t y, const char *text)
{
  if (text == NULL)
  {
    return;
  }

  /* 每个字符占 6x8 像素；到达右边界自动换行，超出底部后停止。 */
  while (*text != '\0')
  {
    if (x > (OLED_WIDTH - 6U))
    {
      x = 0;
      y = (uint8_t)(y + 8U);
    }

    if (y > (OLED_HEIGHT - 8U))
    {
      return;
    }

    Oled_ShowChar(x, y, *text);
    x = (uint8_t)(x + 6U);
    ++text;
  }
}

void Oled_ShowBootScreen(void)
{
  /* 启动画面只用于确认屏幕和接口正常，不承载实时系统状态。 */
  Oled_Clear();
  Oled_ShowString(0, 0, "OLED OK");
  Oled_ShowString(0, 10, "C30D V2.2");
  Oled_Refresh();
}
