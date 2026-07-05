#include "ui_hal.h"

#include "backend_lvgl/stm32/ui_lv_port_disp.h"

#include "lvgl.h"

void UI_Hal_Init(void)
{
  UI_LvPort_DispInit();
}

int16_t UI_Hal_GetWidth(void)
{
  return UI_LvPort_DisplayWidth();
}

int16_t UI_Hal_GetHeight(void)
{
  return UI_LvPort_DisplayHeight();
}

void UI_Hal_FlushRGB565(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint16_t *color)
{
  UI_LvPort_DispFlush(x, y, width, height, color);
}

void UI_Hal_Tick(uint32_t elapsed_ms)
{
  lv_tick_inc(elapsed_ms);
}
