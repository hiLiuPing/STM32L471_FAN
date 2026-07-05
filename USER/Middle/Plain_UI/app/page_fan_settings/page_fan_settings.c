#include "page_fan_settings.h"

#include "../../datastore/ui_model.h"
#include "../../effects/ui_weather_effect.h"

#include <stdio.h>

static ui_obj_t *g_fan_settings_root;
static ui_obj_t *g_fan_speed_label;
static ui_obj_t *g_fan_speed_slider;
static ui_obj_t *g_fan_enabled_switch;

ui_obj_t *UI_PageFanSettings_Create(ui_obj_t *parent)
{
  g_fan_settings_root = UI_Widget_CreatePage(parent);
  (void)UI_Widget_CreateLabel(g_fan_settings_root, "FAN MENU", 12, 6);
  (void)UI_Widget_CreateTile(g_fan_settings_root, "Speed", "UP/DOWN adjusts", 12, 32, 196, 86, UI_WIDGET_VARIANT_ACCENT);
  (void)UI_Widget_CreateTile(g_fan_settings_root, "Power", "ENTER toggles", 218, 32, 198, 86, UI_WIDGET_VARIANT_SURFACE);
  g_fan_speed_label = UI_Widget_CreateLabel(g_fan_settings_root, "speed", 30, 60);
  g_fan_speed_slider = UI_Widget_CreateSlider(g_fan_settings_root, 30, 86, 150, 12);
  g_fan_enabled_switch = UI_Widget_CreateSwitch(g_fan_settings_root, 350, 74);
  UI_Widget_SetHidden(g_fan_settings_root, 1U);
  return g_fan_settings_root;
}

void UI_PageFanSettings_Enter(void)
{
  UI_WeatherEffect_Stop();
  UI_Widget_SetHidden(g_fan_settings_root, 0U);
}

void UI_PageFanSettings_Exit(void)
{
  UI_Widget_SetHidden(g_fan_settings_root, 1U);
}

void UI_PageFanSettings_Update(uint32_t elapsed_ms)
{
  char line[24];
  ui_fan_state_t fan_state;

  (void)elapsed_ms;
  UI_Model_GetFanState(&fan_state);
  (void)snprintf(line, sizeof(line), "%u%% target", fan_state.speed_percent);
  UI_Widget_SetText(g_fan_speed_label, line);
  UI_Widget_SetValue(g_fan_speed_slider, fan_state.speed_percent);
  UI_Widget_SetChecked(g_fan_enabled_switch, fan_state.enabled);
}

void UI_PageFanSettings_HandleEvent(const ui_event_t *event)
{
  (void)event;
}
