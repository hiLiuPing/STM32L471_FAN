/*
 * 跑马灯状态机。
 * 负责维护滚动位置、停顿帧和往返方向。
 * 页面只提供文字和可视区域，不关心滚动细节。
 */
#ifndef UI_MARQUEE_H
#define UI_MARQUEE_H

#include <stdint.h>

#include "../hal/backend_lvgl/ui_renderer_adapter.h"
#include "../core/ui_types.h"

typedef struct
{
  const char *text;
  int16_t scroll_x;
  int16_t text_width;
  uint16_t hold_frames;
  uint16_t hold_counter;
  uint8_t is_forward;
} ui_marquee_t;

void UI_Marquee_Init(ui_marquee_t *marquee, const char *text, int16_t text_width);
void UI_Marquee_Update(ui_marquee_t *marquee, int16_t viewport_width);
void UI_Marquee_Draw(ui_marquee_t *marquee,
                     const ui_viewport_t *viewport,
                     const ui_rect_t *rect,
                     const ui_text_style_t *style);

#endif /* UI_MARQUEE_H */
