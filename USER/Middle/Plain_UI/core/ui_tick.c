/* 只把系统 tick 透传给 HAL，保持上层时基统一。 */
#include "ui_tick.h"

#include "../hal/ui_hal.h"

void UI_Tick_Advance(uint32_t elapsed_ms)
{
  UI_Hal_Tick(elapsed_ms);
}
