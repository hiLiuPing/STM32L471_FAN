#include "ui_lv_port_disp.h"

#include "lcd_init.h"

void UI_LvPort_DispInit(void)
{
  LCD_Init();
}

void UI_LvPort_DispFlush(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint16_t *color)
{
  LCD_DrawRGB565Buffer(x, y, width, height, color);
}

int16_t UI_LvPort_DisplayWidth(void)
{
  return LCD_W;
}

int16_t UI_LvPort_DisplayHeight(void)
{
  return LCD_H;
}
