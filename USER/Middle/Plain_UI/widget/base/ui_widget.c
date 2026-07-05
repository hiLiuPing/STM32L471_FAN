#include "ui_widget.h"

#include "../../style/ui_style.h"

static lv_color_t UI_Widget_Color(uint8_t red, uint8_t green, uint8_t blue)
{
  return lv_color_make(red, green, blue);
}

ui_obj_t *UI_Widget_CreatePage(ui_obj_t *parent)
{
  ui_obj_t *page = lv_obj_create(parent);

  lv_obj_remove_style_all(page);
  lv_obj_set_size(page, lv_pct(100), lv_pct(100));
  lv_obj_set_style_bg_color(page, UI_Widget_Color(7U, 16U, 24U), 0);
  lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);
  lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
  return page;
}

ui_obj_t *UI_Widget_GetRootScreen(void)
{
  return lv_scr_act();
}

ui_obj_t *UI_Widget_CreateLabel(ui_obj_t *parent, const char *text, int16_t x, int16_t y)
{
  ui_obj_t *label = lv_label_create(parent);

  lv_obj_set_pos(label, x, y);
  lv_label_set_text(label, (text != NULL) ? text : "");
  lv_obj_set_style_text_color(label, UI_Widget_Color(232U, 240U, 248U), 0);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);
  return label;
}

ui_obj_t *UI_Widget_CreateButton(ui_obj_t *parent, const char *text, int16_t x, int16_t y, int16_t width, int16_t height)
{
  ui_obj_t *button = lv_btn_create(parent);
  ui_obj_t *label;

  lv_obj_set_pos(button, x, y);
  lv_obj_set_size(button, width, height);
  UI_Style_ApplyMetro(button, UI_WIDGET_VARIANT_ACCENT);
  label = lv_label_create(button);
  lv_label_set_text(label, (text != NULL) ? text : "");
  lv_obj_center(label);
  return button;
}

ui_obj_t *UI_Widget_CreateTile(ui_obj_t *parent,
                               const char *title,
                               const char *subtitle,
                               int16_t x,
                               int16_t y,
                               int16_t width,
                               int16_t height,
                               ui_widget_variant_t variant)
{
  ui_obj_t *tile = lv_obj_create(parent);
  ui_obj_t *title_label;
  ui_obj_t *subtitle_label;

  lv_obj_set_pos(tile, x, y);
  lv_obj_set_size(tile, width, height);
  lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
  UI_Style_ApplyMetro(tile, variant);

  title_label = UI_Widget_CreateLabel(tile, title, 8, 7);
  lv_obj_set_style_text_color(title_label, UI_Widget_Color(238U, 245U, 250U), 0);
  subtitle_label = UI_Widget_CreateLabel(tile, subtitle, 8, 23);
  lv_obj_set_style_text_color(subtitle_label, UI_Widget_Color(140U, 160U, 180U), 0);
  return tile;
}

ui_obj_t *UI_Widget_CreateSlider(ui_obj_t *parent, int16_t x, int16_t y, int16_t width, int16_t height)
{
  ui_obj_t *slider = lv_slider_create(parent);

  lv_obj_set_pos(slider, x, y);
  lv_obj_set_size(slider, width, height);
  lv_slider_set_range(slider, 0, 100);
  return slider;
}

ui_obj_t *UI_Widget_CreateSwitch(ui_obj_t *parent, int16_t x, int16_t y)
{
  ui_obj_t *sw = lv_switch_create(parent);

  lv_obj_set_pos(sw, x, y);
  lv_obj_set_size(sw, 48, 24);
  return sw;
}

ui_obj_t *UI_Widget_CreateProgress(ui_obj_t *parent, int16_t x, int16_t y, int16_t width, int16_t height)
{
  ui_obj_t *bar = lv_bar_create(parent);

  lv_obj_set_pos(bar, x, y);
  lv_obj_set_size(bar, width, height);
  lv_bar_set_range(bar, 0, 100);
  lv_obj_set_style_bg_color(bar, UI_Widget_Color(25U, 41U, 55U), 0);
  lv_obj_set_style_bg_color(bar, UI_Widget_Color(61U, 174U, 235U), LV_PART_INDICATOR);
  return bar;
}

void UI_Widget_SetText(ui_obj_t *obj, const char *text)
{
  if (obj != NULL)
  {
    lv_label_set_text(obj, (text != NULL) ? text : "");
  }
}

void UI_Widget_SetValue(ui_obj_t *obj, int32_t value)
{
  if (obj != NULL)
  {
    if (value < 0)
    {
      value = 0;
    }
    if (value > 100)
    {
      value = 100;
    }
    lv_bar_set_value(obj, value, LV_ANIM_ON);
  }
}

void UI_Widget_SetChecked(ui_obj_t *obj, uint8_t checked)
{
  if (obj == NULL)
  {
    return;
  }

  if (checked != 0U)
  {
    lv_obj_add_state(obj, LV_STATE_CHECKED);
  }
  else
  {
    lv_obj_clear_state(obj, LV_STATE_CHECKED);
  }
}

void UI_Widget_SetHidden(ui_obj_t *obj, uint8_t hidden)
{
  if (obj == NULL)
  {
    return;
  }

  if (hidden != 0U)
  {
    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
  }
  else
  {
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
  }
}

void UI_Widget_SetPos(ui_obj_t *obj, int16_t x, int16_t y)
{
  if (obj != NULL)
  {
    lv_obj_set_pos(obj, x, y);
  }
}
