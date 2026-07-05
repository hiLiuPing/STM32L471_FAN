/*
 * 平台 HAL 抽象。
 * 负责把显示尺寸、flush 和 tick 能力统一暴露给上层。
 * 业务层只依赖这个接口，不直接访问 BSP。
 */
#ifndef UI_HAL_H
#define UI_HAL_H

#include <stdint.h>

#include "backend_lvgl/stm32/ui_lv_port_disp.h"

void UI_Hal_Init(void);
int16_t UI_Hal_GetWidth(void);
int16_t UI_Hal_GetHeight(void);
void UI_Hal_FlushRGB565(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint16_t *color);
void UI_Hal_Tick(uint32_t elapsed_ms);

#endif /* UI_HAL_H */
