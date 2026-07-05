#include "page_fan_status.h"

#include "../../datastore/ui_model.h"
#include "../../effects/ui_weather_effect.h"

#include <stdio.h>

static ui_obj_t *g_fan_root;
static ui_obj_t *g_fan_rpm_label;
static ui_obj_t *g_fan_temp_label;
static ui_obj_t *g_fan_mode_label;
static ui_obj_t *g_fan_bar;
static ui_obj_t *g_fan_switch;

static const char *UI_PageFanStatus_ModeText(ui_fan_mode_t mode)
{
  switch (mode)
  {
  case UI_FAN_MODE_SLEEP:
    return "sleep";
  case UI_FAN_MODE_MANUAL:
    return "manual";
  case UI_FAN_MODE_BOOST:
    return "boost";
  case UI_FAN_MODE_AUTO:
  default:
    return "auto";
  }
}

ui_obj_t *UI_PageFanStatus_Create(ui_obj_t *parent)
{
  g_fan_root = UI_Widget_CreatePage(parent);
  (void)UI_Widget_CreateLabel(g_fan_root, "FAN STATUS", 12, 6);
  (void)UI_Widget_CreateTile(g_fan_root, "Airflow", "live speed", 12, 32, 196, 86, UI_WIDGET_VARIANT_ACCENT);
  (void)UI_Widget_CreateTile(g_fan_root, "Thermal", "motor state", 218, 32, 198, 86, UI_WIDGET_VARIANT_SURFACE);
  g_fan_rpm_label = UI_Widget_CreateLabel(g_fan_root, "rpm", 30, 58);
  g_fan_bar = UI_Widget_CreateProgress(g_fan_root, 30, 82, 150, 10);
  g_fan_temp_label = UI_Widget_CreateLabel(g_fan_root, "temp", 236, 58);
  g_fan_mode_label = UI_Widget_CreateLabel(g_fan_root, "mode", 236, 78);
  g_fan_switch = UI_Widget_CreateSwitch(g_fan_root, 350, 78);
  UI_Widget_SetHidden(g_fan_root, 1U);
  return g_fan_root;
}

void UI_PageFanStatus_Enter(void)
{
  UI_WeatherEffect_Stop();
  UI_Widget_SetHidden(g_fan_root, 0U);
}

void UI_PageFanStatus_Exit(void)
{
  UI_Widget_SetHidden(g_fan_root, 1U);
}

void UI_PageFanStatus_Update(uint32_t elapsed_ms)
{
  char line[32];
  ui_fan_state_t fan_state;

  (void)elapsed_ms;
  UI_Model_GetFanState(&fan_state);
  (void)snprintf(line, sizeof(line), "%u rpm / %u%%", (unsigned int)fan_state.rpm, fan_state.speed_percent);
  UI_Widget_SetText(g_fan_rpm_label, line);
  UI_Widget_SetValue(g_fan_bar, fan_state.speed_percent);
  (void)snprintf(line, sizeof(line), "%d C", (int)fan_state.temperature_c);
  UI_Widget_SetText(g_fan_temp_label, line);
  (void)snprintf(line, sizeof(line), "%s %s", UI_PageFanStatus_ModeText(fan_state.mode), fan_state.enabled ? "on" : "off");
  UI_Widget_SetText(g_fan_mode_label, line);
  UI_Widget_SetChecked(g_fan_switch, fan_state.enabled);
}

void UI_PageFanStatus_HandleEvent(const ui_event_t *event)
{
  (void)event;
}
