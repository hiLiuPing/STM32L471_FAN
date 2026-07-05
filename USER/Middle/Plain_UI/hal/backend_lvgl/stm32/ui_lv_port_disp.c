#include "ui_lv_port_disp.h"

#include "lcd_init.h"

void UI_LvPort_DispInit(void)
{
  LCD_Init();
}

void UI_LvPort_DispFlush(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint16_t *color)
{
  uint32_t i;
  uint32_t pixel_count;

  if ((width == 0U) || (height == 0U) || (color == NULL))
  {
    return;
  }

  LCD_Address_Set(x, y, (uint16_t)(x + width - 1U), (uint16_t)(y + height - 1U));
  pixel_count = (uint32_t)width * (uint32_t)height;
  for (i = 0U; i < pixel_count; ++i)
  {
    LCD_WR_DATA(color[i]);
  }
}

int16_t UI_LvPort_DisplayWidth(void)
{
  return LCD_W;
}

int16_t UI_LvPort_DisplayHeight(void)
{
  return LCD_H;
}
