/* 首页：展示品牌入口、运行状态和快捷入口卡片。 */
#include "page_home.h"

#include "../ui_app_state.h"
#include "../../datastore/ui_property_store.h"
#include "../../datastore/ui_model.h"
#include "../../effects/ui_weather_effect.h"

#include <stdio.h>

static ui_obj_t *g_home_root;
static ui_obj_t *g_home_clock_label;
static ui_obj_t *g_home_fan_label;
static ui_obj_t *g_home_weather_label;

static void UI_PageHome_DrawTile(const ui_viewport_t *viewport,
                                 int16_t x,
                                 int16_t y,
                                 int16_t width,
                                 int16_t height,
                                 const char *tag,
                                 const char *title,
                                 const char *note,
                                 const ui_render_style_t *style)
{
  ui_rect_t panel = { x, y, width, height };
  ui_rect_t tag_rect = { (int16_t)(x + 8), (int16_t)(y + 6), 42, 14 };

  UI_RendererAdapter_DrawPanel(viewport, &panel, style);
  UI_RendererAdapter_DrawBadge(viewport, &tag_rect, tag, style);
  UI_RendererAdapter_DrawText(viewport, (int16_t)(x + 56), (int16_t)(y + 5), title, style->text);
  UI_RendererAdapter_DrawText(viewport, (int16_t)(x + 56), (int16_t)(y + 20), note, style->muted);
}

void UI_PageHome_Draw(void)
{
  char heartbeat_line[24];
  char progress_line[24];
  char temp_line[24];
  const ui_app_state_t *state = UI_AppState_GetConst();
  ui_render_style_t blue = UI_AppState_Style(UI_AppState_Color(61U, 174U, 235U));
  ui_render_style_t green = UI_AppState_Style(UI_AppState_Color(87U, 220U, 142U));
  ui_render_style_t amber = UI_AppState_Style(UI_AppState_Color(246U, 184U, 82U));
  ui_render_style_t panel = UI_AppState_Style(UI_AppState_Color(22U, 38U, 52U));
  ui_render_style_t soft = UI_AppState_Style(UI_AppState_Color(30U, 48U, 64U));
  ui_rect_t hero = { 12, 32, 214, 44 };
  ui_rect_t status = { 234, 32, 186, 44 };
  ui_rect_t tile0 = { 12, 84, 130, 32 };
  ui_rect_t tile1 = { 151, 84, 130, 32 };
  ui_rect_t tile2 = { 290, 84, 130, 32 };
  ui_rect_t live_rect = { 20, 60, 40, 14 };
  ui_rect_t progress_rect = { 66, 60, 58, 14 };
  ui_rect_t heartbeat_rect = { 130, 60, 64, 14 };
  ui_rect_t temp_rect = { 352, 60, 52, 14 };

  (void)snprintf(heartbeat_line, sizeof(heartbeat_line), "hb %ld", (long)UI_PropertyStore_GetFrontInt(UI_APP_PROP_HEARTBEAT_ID, 0));
  (void)snprintf(progress_line, sizeof(progress_line), "%ld%%", (long)UI_PropertyStore_GetFrontInt(UI_APP_PROP_PROGRESS_ID, 0));
  (void)snprintf(temp_line, sizeof(temp_line), "%ld C", (long)UI_PropertyStore_GetFrontInt(UI_APP_PROP_TEMPERATURE_ID, 0));

  UI_RendererAdapter_DrawPanel(&state->viewport, &hero, &panel);
  UI_RendererAdapter_DrawText(&state->viewport, 20, 38, "PLAIN UI", blue.text);
  UI_RendererAdapter_DrawText(&state->viewport, 20, 51, "Real home / live overview", blue.muted);
  UI_RendererAdapter_DrawBadge(&state->viewport, &live_rect, "LIVE", &green);
  UI_RendererAdapter_DrawBadge(&state->viewport, &progress_rect, progress_line, &amber);
  UI_RendererAdapter_DrawBadge(&state->viewport, &heartbeat_rect, heartbeat_line, &blue);

  UI_RendererAdapter_DrawPanel(&state->viewport, &status, &soft);
  UI_RendererAdapter_DrawText(&state->viewport, 242, 38, "Status feed", blue.text);
  UI_RendererAdapter_DrawText(&state->viewport, 242, 51, UI_PropertyStore_GetFrontString(UI_APP_PROP_STATUS_ID, "Ready"), green.muted);
  UI_RendererAdapter_DrawText(&state->viewport, 242, 64, UI_PropertyStore_GetFrontString(UI_APP_PROP_SERVICE_ID, "Idle"), amber.muted);
  UI_RendererAdapter_DrawBadge(&state->viewport, &temp_rect, temp_line, &green);

  UI_PageHome_DrawTile(&state->viewport, tile0.x, tile0.y, tile0.width, tile0.height, "CTRL", "Controls", "actions", &blue);
  UI_PageHome_DrawTile(&state->viewport, tile1.x, tile1.y, tile1.width, tile1.height, "LIST", "Services", "provider", &green);
  UI_PageHome_DrawTile(&state->viewport, tile2.x, tile2.y, tile2.width, tile2.height, "DATA", "Telemetry", "charts", &amber);
}

ui_obj_t *UI_PageHome_Create(ui_obj_t *parent)
{
  g_home_root = UI_Widget_CreatePage(parent);
  (void)UI_Widget_CreateLabel(g_home_root, "PLAIN UI", 12, 6);
  (void)UI_Widget_CreateTile(g_home_root, "Travel Clock", "time / weather", 12, 32, 130, 54, UI_WIDGET_VARIANT_ACCENT);
  (void)UI_Widget_CreateTile(g_home_root, "Fan", "status monitor", 150, 32, 130, 54, UI_WIDGET_VARIANT_SURFACE);
  (void)UI_Widget_CreateTile(g_home_root, "Menus", "settings", 288, 32, 126, 54, UI_WIDGET_VARIANT_MUTED);
  g_home_clock_label = UI_Widget_CreateLabel(g_home_root, "--:--", 24, 96);
  g_home_fan_label = UI_Widget_CreateLabel(g_home_root, "fan --%", 150, 96);
  g_home_weather_label = UI_Widget_CreateLabel(g_home_root, "clear", 288, 96);
  UI_WeatherEffect_Init(g_home_root);
  UI_Widget_SetHidden(g_home_root, 1U);
  return g_home_root;
}

void UI_PageHome_Enter(void)
{
  UI_Widget_SetHidden(g_home_root, 0U);
  UI_WeatherEffect_Init(g_home_root);
  UI_WeatherEffect_SetMode(UI_Model_GetWeatherMode());
  UI_WeatherEffect_Start();
}

void UI_PageHome_Exit(void)
{
  UI_WeatherEffect_Stop();
  UI_Widget_SetHidden(g_home_root, 1U);
}

void UI_PageHome_Update(uint32_t elapsed_ms)
{
  char line[24];
  ui_clock_state_t clock_state;
  ui_fan_state_t fan_state;
  const char *weather_text = "clear";

  (void)elapsed_ms;
  UI_Model_GetClockState(&clock_state);
  UI_Model_GetFanState(&fan_state);

  (void)snprintf(line, sizeof(line), "%02u:%02u:%02u", clock_state.hour, clock_state.minute, clock_state.second);
  UI_Widget_SetText(g_home_clock_label, line);
  (void)snprintf(line, sizeof(line), "fan %u%% %urpm", fan_state.speed_percent, (unsigned int)fan_state.rpm);
  UI_Widget_SetText(g_home_fan_label, line);

  if (UI_Model_GetWeatherMode() == UI_WEATHER_RAIN)
  {
    weather_text = "rain";
  }
  else if (UI_Model_GetWeatherMode() == UI_WEATHER_SNOW)
  {
    weather_text = "snow";
  }
  else if (UI_Model_GetWeatherMode() == UI_WEATHER_CLOUD)
  {
    weather_text = "cloud";
  }
  UI_Widget_SetText(g_home_weather_label, weather_text);
}

void UI_PageHome_HandleEvent(const ui_event_t *event)
{
  (void)event;
}
