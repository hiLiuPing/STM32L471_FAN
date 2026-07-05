#include "page_travel_clock.h"

#include "../../datastore/ui_model.h"
#include "../../effects/ui_weather_effect.h"

#include <stdio.h>

static ui_obj_t *g_clock_root;
static ui_obj_t *g_time_label;
static ui_obj_t *g_city_label;
static ui_obj_t *g_phase_label;

ui_obj_t *UI_PageTravelClock_Create(ui_obj_t *parent)
{
  g_clock_root = UI_Widget_CreatePage(parent);
  (void)UI_Widget_CreateLabel(g_clock_root, "TRAVEL CLOCK", 12, 6);
  (void)UI_Widget_CreateTile(g_clock_root, "Local time", "animated weather layer", 12, 32, 196, 86, UI_WIDGET_VARIANT_ACCENT);
  (void)UI_Widget_CreateTile(g_clock_root, "Weather", "ENTER in clock menu changes mode", 218, 32, 198, 86, UI_WIDGET_VARIANT_SURFACE);
  g_time_label = UI_Widget_CreateLabel(g_clock_root, "--:--:--", 30, 64);
  g_city_label = UI_Widget_CreateLabel(g_clock_root, "city", 236, 64);
  g_phase_label = UI_Widget_CreateLabel(g_clock_root, "phase", 236, 82);
  UI_Widget_SetHidden(g_clock_root, 1U);
  return g_clock_root;
}

void UI_PageTravelClock_Enter(void)
{
  UI_Widget_SetHidden(g_clock_root, 0U);
  UI_WeatherEffect_Init(g_clock_root);
  UI_WeatherEffect_SetMode(UI_Model_GetWeatherMode());
  UI_WeatherEffect_Start();
}

void UI_PageTravelClock_Exit(void)
{
  UI_WeatherEffect_Stop();
  UI_Widget_SetHidden(g_clock_root, 1U);
}

void UI_PageTravelClock_Update(uint32_t elapsed_ms)
{
  char line[32];
  ui_clock_state_t clock_state;

  (void)elapsed_ms;
  UI_Model_GetClockState(&clock_state);
  (void)snprintf(line, sizeof(line), "%02u:%02u:%02u", clock_state.hour, clock_state.minute, clock_state.second);
  UI_Widget_SetText(g_time_label, line);
  (void)snprintf(line, sizeof(line), "%s UTC%+d", clock_state.city, clock_state.timezone_offset);
  UI_Widget_SetText(g_city_label, line);
  (void)snprintf(line, sizeof(line), "anim %u", (unsigned int)clock_state.anim_phase);
  UI_Widget_SetText(g_phase_label, line);
}

void UI_PageTravelClock_HandleEvent(const ui_event_t *event)
{
  (void)event;
}
