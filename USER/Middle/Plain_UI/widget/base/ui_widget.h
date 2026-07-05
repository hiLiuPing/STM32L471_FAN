#ifndef UI_WIDGET_H
#define UI_WIDGET_H

#include <stdint.h>

#include "../../event/ui_event.h"
#include "../../core/ui_types.h"

typedef struct ui_widget ui_widget_t;

typedef void (*ui_widget_measure_fn)(ui_widget_t *widget, int16_t parent_max_w, int16_t parent_max_h);
typedef void (*ui_widget_arrange_fn)(ui_widget_t *widget, int16_t abs_x, int16_t abs_y);
typedef void (*ui_widget_draw_fn)(ui_widget_t *widget, const ui_viewport_t *viewport);
typedef void (*ui_widget_event_fn)(ui_widget_t *widget, const ui_event_t *event);

struct ui_widget
{
  uint16_t id;
  int16_t x;
  int16_t y;
  int16_t width;
  int16_t height;
  uint8_t is_visible;
  uint8_t is_focused;
  uint8_t is_dirty;
  uint8_t accepts_focus;
  ui_widget_t *parent;
  ui_widget_t *first_child;
  ui_widget_t *next_sibling;
  ui_widget_measure_fn measure;
  ui_widget_arrange_fn arrange;
  ui_widget_draw_fn draw;
  ui_widget_event_fn on_event;
};

#endif /* UI_WIDGET_H */
