#include "ui_style.h"

static lv_color_t UI_Style_Color(uint8_t red, uint8_t green, uint8_t blue)
{
  return lv_color_make(red, green, blue);
}

void UI_Style_Init(void)
{
}

void UI_Style_ApplyMetro(ui_obj_t *obj, ui_widget_variant_t variant)
{
  lv_color_t bg;
  lv_color_t border;

  if (obj == NULL)
  {
    return;
  }

  switch (variant)
  {
  case UI_WIDGET_VARIANT_ACCENT:
    bg = UI_Style_Color(24U, 84U, 116U);
    border = UI_Style_Color(61U, 174U, 235U);
    break;

  case UI_WIDGET_VARIANT_MUTED:
    bg = UI_Style_Color(22U, 38U, 52U);
    border = UI_Style_Color(42U, 62U, 78U);
    break;

  case UI_WIDGET_VARIANT_SURFACE:
  default:
    bg = UI_Style_Color(15U, 28U, 40U);
    border = UI_Style_Color(45U, 68U, 86U);
    break;
  }

  lv_obj_set_style_radius(obj, 4, 0);
  lv_obj_set_style_bg_color(obj, bg, 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(obj, border, 0);
  lv_obj_set_style_border_width(obj, 1, 0);
  lv_obj_set_style_pad_all(obj, 0, 0);
}
