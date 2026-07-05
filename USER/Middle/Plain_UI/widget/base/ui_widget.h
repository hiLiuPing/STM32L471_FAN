#ifndef UI_WIDGET_H
#define UI_WIDGET_H

#include <stdint.h>

#include "../../event/ui_event.h"
#include "../../core/ui_types.h"
#include "lvgl.h"

typedef lv_obj_t ui_obj_t;

typedef enum
{
  UI_WIDGET_VARIANT_SURFACE = 0,
  UI_WIDGET_VARIANT_ACCENT,
  UI_WIDGET_VARIANT_MUTED
} ui_widget_variant_t;

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

ui_obj_t *UI_Widget_CreatePage(ui_obj_t *parent);
ui_obj_t *UI_Widget_GetRootScreen(void);
ui_obj_t *UI_Widget_CreateLabel(ui_obj_t *parent, const char *text, int16_t x, int16_t y);
ui_obj_t *UI_Widget_CreateButton(ui_obj_t *parent, const char *text, int16_t x, int16_t y, int16_t width, int16_t height);
ui_obj_t *UI_Widget_CreateTile(ui_obj_t *parent,
                               const char *title,
                               const char *subtitle,
                               int16_t x,
                               int16_t y,
                               int16_t width,
                               int16_t height,
                               ui_widget_variant_t variant);
ui_obj_t *UI_Widget_CreateSlider(ui_obj_t *parent, int16_t x, int16_t y, int16_t width, int16_t height);
ui_obj_t *UI_Widget_CreateSwitch(ui_obj_t *parent, int16_t x, int16_t y);
ui_obj_t *UI_Widget_CreateProgress(ui_obj_t *parent, int16_t x, int16_t y, int16_t width, int16_t height);
void UI_Widget_SetText(ui_obj_t *obj, const char *text);
void UI_Widget_SetValue(ui_obj_t *obj, int32_t value);
void UI_Widget_SetChecked(ui_obj_t *obj, uint8_t checked);
void UI_Widget_SetHidden(ui_obj_t *obj, uint8_t hidden);
void UI_Widget_SetPos(ui_obj_t *obj, int16_t x, int16_t y);

#endif /* UI_WIDGET_H */
