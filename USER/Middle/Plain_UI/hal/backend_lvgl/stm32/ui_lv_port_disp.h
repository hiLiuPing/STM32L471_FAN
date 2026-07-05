#ifndef UI_LV_PORT_DISP_H
#define UI_LV_PORT_DISP_H

#include <stdint.h>

#include "lcd.h"

#define UI_LV_PORT_DISP_WIDTH  LCD_W
#define UI_LV_PORT_DISP_HEIGHT LCD_H

void UI_LvPort_DispInit(void);
void UI_LvPort_DispFlush(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint16_t *color);
int16_t UI_LvPort_DisplayWidth(void);
int16_t UI_LvPort_DisplayHeight(void);

#endif /* UI_LV_PORT_DISP_H */
