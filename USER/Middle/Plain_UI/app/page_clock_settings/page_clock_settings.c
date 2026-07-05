#include "page_clock_settings.h"

#include "../../datastore/ui_model.h"
#include "../../effects/ui_weather_effect.h"

#include <stdio.h>

static ui_obj_t *g_clock_settings_root;
static ui_obj_t *g_clock_city_label;
static ui_obj_t *g_clock_weather_label;
static ui_obj_t *g_clock_format_label;

static const char *UI_PageClockSettings_WeatherText(ui_weather_mode_t mode)
{
  switch (mode)
  {
  case UI_WEATHER_RAIN:
    return "rain";
  case UI_WEATHER_SNOW:
    return "snow";
  case UI_WEATHER_CLOUD:
    return "cloud";
  case UI_WEATHER_CLEAR:
  default:
    return "clear";
  }
}

ui_obj_t *UI_PageClockSettings_Create(ui_obj_t *parent)
{
  g_clock_settings_root = UI_Widget_CreatePage(parent);
  (void)UI_Widget_CreateLabel(g_clock_settings_root, "CLOCK MENU", 12, 6);
  (void)UI_Widget_CreateTile(g_clock_settings_root, "City", "travel timezone", 12, 32, 130, 64, UI_WIDGET_VARIANT_SURFACE);
  (void)UI_Widget_CreateTile(g_clock_settings_root, "Weather", "ENTER cycles mode", 150, 32, 130, 64, UI_WIDGET_VARIANT_ACCENT);
  (void)UI_Widget_CreateTile(g_clock_settings_root, "Format", "24 hour display", 288, 32, 126, 64, UI_WIDGET_VARIANT_MUTED);
  g_clock_city_label = UI_Widget_CreateLabel(g_clock_settings_root, "city", 24, 106);
  g_clock_weather_label = UI_Widget_CreateLabel(g_clock_settings_root, "weather", 150, 106);
  g_clock_format_label = UI_Widget_CreateLabel(g_clock_settings_root, "format", 288, 106);
  UI_Widget_SetHidden(g_clock_settings_root, 1U);
  return g_clock_settings_root;
}

void UI_PageClockSettings_Enter(void)
{
  UI_WeatherEffect_Stop();
  UI_Widget_SetHidden(g_clock_settings_root, 0U);
}

void UI_PageClockSettings_Exit(void)
{
  UI_Widget_SetHidden(g_clock_settings_root, 1U);
}

void UI_PageClockSettings_Update(uint32_t elapsed_ms)
{
  char line[32];
  ui_clock_state_t clock_state;

  (void)elapsed_ms;
  UI_Model_GetClockState(&clock_state);
  (void)snprintf(line, sizeof(line), "%s UTC%+d", clock_state.city, clock_state.timezone_offset);
  UI_Widget_SetText(g_clock_city_label, line);
  UI_Widget_SetText(g_clock_weather_label, UI_PageClockSettings_WeatherText(UI_Model_GetWeatherMode()));
  UI_Widget_SetText(g_clock_format_label, clock_state.use_24h ? "24H" : "12H");
}

void UI_PageClockSettings_HandleEvent(const ui_event_t *event)
{
  (void)event;
}
