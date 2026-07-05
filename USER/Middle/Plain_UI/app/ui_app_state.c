#include "ui_app_state.h"

#include "../config/ui_config.h"
#include "../core/ui_dirty_region.h"
#include "../navigation/ui_page_manager.h"

#include <stdio.h>
#include <string.h>

static const ui_app_page_model_t g_app_page_models[] =
{
  { UI_APP_PAGE_HOME_ID, "Home", "overview" },
  { UI_APP_PAGE_TRAVEL_CLOCK_ID, "Travel Clock", "animated weather" },
  { UI_APP_PAGE_FAN_STATUS_ID, "Fan Status", "speed and thermal" },
  { UI_APP_PAGE_CLOCK_SETTINGS_ID, "Clock Menu", "city and weather" },
  { UI_APP_PAGE_FAN_SETTINGS_ID, "Fan Menu", "mode and speed" }
};

static ui_app_state_t g_app_state;

static void UI_AppState_SelectPage(uint8_t index)
{
  uint8_t page_count = UI_AppState_GetPageCount();

  if (page_count == 0U)
  {
    return;
  }

  g_app_state.active_page_index = (uint8_t)(index % page_count);
  (void)UI_PageManager_Replace(UI_AppState_PageIdFromIndex(g_app_state.active_page_index));
}

static void UI_AppState_AdjustWeather(void)
{
  ui_weather_mode_t mode = UI_Model_GetWeatherMode();

  mode = (ui_weather_mode_t)(((uint8_t)mode + 1U) % 4U);
  UI_Model_SetWeatherMode(mode);
}

static void UI_AppState_AdjustFanSpeed(int8_t delta)
{
  ui_fan_state_t fan;
  int16_t next_speed;

  UI_Model_GetFanState(&fan);
  next_speed = (int16_t)fan.speed_percent + delta;
  if (next_speed < 0)
  {
    next_speed = 0;
  }
  if (next_speed > 100)
  {
    next_speed = 100;
  }
  fan.speed_percent = (uint8_t)next_speed;
  UI_Model_SetFanState(&fan);
}

void UI_AppState_Init(void)
{
  memset(&g_app_state, 0, sizeof(g_app_state));
  g_app_state.viewport.x = 0;
  g_app_state.viewport.y = 0;
  g_app_state.viewport.width = UI_RendererAdapter_GetWidth();
  g_app_state.viewport.height = UI_RendererAdapter_GetHeight();
  g_app_state.progress = 42;
  g_app_state.temperature = 28;
  g_app_state.toggle_on = 1U;
  g_app_state.checkbox_on = 1U;
  g_app_state.active_page_index = 0U;
}

ui_app_state_t *UI_AppState_Get(void)
{
  return &g_app_state;
}

const ui_app_state_t *UI_AppState_GetConst(void)
{
  return &g_app_state;
}

void UI_AppState_SyncViewport(void)
{
  g_app_state.viewport.width = UI_RendererAdapter_GetWidth();
  g_app_state.viewport.height = UI_RendererAdapter_GetHeight();
}

void UI_AppState_InvalidateFull(void)
{
  ui_rect_t full_rect = { 0, 0, g_app_state.viewport.width, g_app_state.viewport.height };

  UI_DirtyRegion_Invalidate(&full_rect);
}

uint8_t UI_AppState_GetPageCount(void)
{
  return (uint8_t)(sizeof(g_app_page_models) / sizeof(g_app_page_models[0]));
}

uint8_t UI_AppState_FindPageIndex(uint16_t page_id)
{
  uint8_t i;

  for (i = 0U; i < UI_AppState_GetPageCount(); ++i)
  {
    if (g_app_page_models[i].page_id == page_id)
    {
      return i;
    }
  }

  return 0U;
}

uint16_t UI_AppState_PageIdFromIndex(uint8_t index)
{
  return g_app_page_models[index % UI_AppState_GetPageCount()].page_id;
}

const ui_app_page_model_t *UI_AppState_GetPageModelByIndex(uint8_t index)
{
  return &g_app_page_models[index % UI_AppState_GetPageCount()];
}

ui_color_t UI_AppState_Color(uint8_t red, uint8_t green, uint8_t blue)
{
  return UI_RendererAdapter_Color(red, green, blue);
}

ui_render_style_t UI_AppState_Style(ui_color_t accent)
{
  ui_render_style_t style;

  style.text = UI_AppState_Color(232U, 240U, 248U);
  style.muted = UI_AppState_Color(128U, 150U, 170U);
  style.background = UI_AppState_Color(16U, 28U, 40U);
  style.border = UI_AppState_Color(42U, 62U, 78U);
  style.accent = accent;
  style.radius = 4U;
  style.border_width = 1U;
  return style;
}

void UI_AppState_DrawChrome(uint16_t page_id)
{
  (void)page_id;
}

void UI_AppState_DrawFooter(void)
{
}

void UI_AppState_OnFrame(void)
{
  g_app_state.frame_count++;
  UI_Model_Tick(UI_FRAME_TICK_MS);
}

void UI_AppState_OnMessage(const ui_msg_t *msg)
{
  (void)msg;
}

void UI_AppState_OnEvent(const ui_event_t *event)
{
  uint8_t page_count = UI_AppState_GetPageCount();
  uint16_t active_page = UI_AppState_GetActivePageId();

  if ((event == NULL) || (page_count == 0U))
  {
    return;
  }

  if (event->type == UI_KEY_RIGHT)
  {
    UI_AppState_SelectPage((uint8_t)(g_app_state.active_page_index + 1U));
  }
  else if (event->type == UI_KEY_LEFT)
  {
    UI_AppState_SelectPage((g_app_state.active_page_index == 0U) ? (uint8_t)(page_count - 1U) : (uint8_t)(g_app_state.active_page_index - 1U));
  }
  else if (event->type == UI_KEY_BACK)
  {
    UI_AppState_SelectPage(0U);
  }
  else if ((event->type == UI_KEY_UP) && (active_page == UI_APP_PAGE_FAN_SETTINGS_ID))
  {
    UI_AppState_AdjustFanSpeed(5);
  }
  else if ((event->type == UI_KEY_DOWN) && (active_page == UI_APP_PAGE_FAN_SETTINGS_ID))
  {
    UI_AppState_AdjustFanSpeed(-5);
  }
  else if ((event->type == UI_KEY_ENTER) || (event->type == UI_KEY_CLICK))
  {
    if (active_page == UI_APP_PAGE_CLOCK_SETTINGS_ID)
    {
      UI_AppState_AdjustWeather();
    }
    else if ((active_page == UI_APP_PAGE_FAN_STATUS_ID) || (active_page == UI_APP_PAGE_FAN_SETTINGS_ID))
    {
      ui_fan_state_t fan;

      UI_Model_GetFanState(&fan);
      fan.enabled = (fan.enabled == 0U) ? 1U : 0U;
      UI_Model_SetFanState(&fan);
    }
  }
}

uint16_t UI_AppState_GetActivePageId(void)
{
  return UI_AppState_PageIdFromIndex(g_app_state.active_page_index);
}
