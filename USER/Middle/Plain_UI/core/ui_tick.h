/*
 * UI 时基入口。
 * 用来把系统节拍转给 HAL/LVGL。
 * 上层不直接操作底层 tick。
 */
#ifndef UI_TICK_H
#define UI_TICK_H

#include <stdint.h>

void UI_Tick_Advance(uint32_t elapsed_ms);

#endif /* UI_TICK_H */
