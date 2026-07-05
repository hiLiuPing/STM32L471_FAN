#include "ui_marquee.h"

#include <stddef.h>

void UI_Marquee_Init(ui_marquee_t *marquee, const char *text, int16_t text_width)
{
  if (marquee == NULL)
  {
    return;
  }

  marquee->text = text;
  marquee->scroll_x = 0;
  marquee->text_width = text_width;
  marquee->hold_frames = 30U;
  marquee->hold_counter = 30U;
  marquee->is_forward = 1U;
}

void UI_Marquee_Update(ui_marquee_t *marquee, int16_t viewport_width)
{
  int16_t max_scroll;

  if ((marquee == NULL) || (marquee->text_width <= viewport_width))
  {
    return;
  }

  if (marquee->hold_counter > 0U)
  {
    marquee->hold_counter--;
    return;
  }

  max_scroll = (int16_t)(marquee->text_width - viewport_width);
  if (marquee->is_forward != 0U)
  {
    marquee->scroll_x++;
    if (marquee->scroll_x >= max_scroll)
    {
      marquee->scroll_x = max_scroll;
      marquee->is_forward = 0U;
      marquee->hold_counter = marquee->hold_frames;
    }
  }
  else
  {
    marquee->scroll_x = 0;
    marquee->is_forward = 1U;
    marquee->hold_counter = marquee->hold_frames;
  }
}

void UI_Marquee_Draw(ui_marquee_t *marquee,
                     const ui_viewport_t *viewport,
                     const ui_rect_t *rect,
                     const ui_text_style_t *style)
{
  ui_viewport_t clipped_viewport;
  ui_rect_t local_rect;

  if ((marquee == NULL) || (viewport == NULL) || (rect == NULL) || (style == NULL) || (marquee->text == NULL))
  {
    return;
  }

  clipped_viewport.x = viewport->x + rect->x;
  clipped_viewport.y = viewport->y + rect->y;
  clipped_viewport.width = rect->width;
  clipped_viewport.height = rect->height;
  clipped_viewport.scroll_x = marquee->scroll_x;
  clipped_viewport.scroll_y = 0;

  local_rect.x = 0;
  local_rect.y = 0;
  local_rect.width = rect->width;
  local_rect.height = rect->height;
  UI_RendererAdapter_DrawTextInRect(&clipped_viewport, &local_rect, marquee->text, style);
}
