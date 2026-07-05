#include "ui_model.h"

#include <string.h>

static ui_fan_state_t g_fan_state;
static ui_clock_state_t g_clock_state;
static ui_setting_state_t g_setting_state;
static ui_weather_mode_t g_weather_mode;
static uint32_t g_clock_accum_ms;

void UI_Model_Init(void)
{
  UI_PropertyStore_Init();

  memset(&g_fan_state, 0, sizeof(g_fan_state));
  memset(&g_clock_state, 0, sizeof(g_clock_state));
  memset(&g_setting_state, 0, sizeof(g_setting_state));

  g_fan_state.enabled = 1U;
  g_fan_state.speed_percent = 42U;
  g_fan_state.rpm = 1180U;
  g_fan_state.temperature_c = 28;
  g_fan_state.mode = UI_FAN_MODE_AUTO;

  g_clock_state.hour = 9U;
  g_clock_state.minute = 30U;
  g_clock_state.second = 0U;
  g_clock_state.timezone_offset = 8;
  g_clock_state.use_24h = 1U;
  g_clock_state.anim_phase = 0U;
  (void)strncpy(g_clock_state.city, "Shanghai", sizeof(g_clock_state.city) - 1U);

  g_setting_state.brightness_percent = 80U;
  g_setting_state.sound_enabled = 1U;
  g_weather_mode = UI_WEATHER_CLEAR;
  g_clock_accum_ms = 0U;
}

void UI_Model_Sync(void)
{
  UI_PropertyStore_SyncFront();
}

void UI_Model_Tick(uint32_t elapsed_ms)
{
  g_clock_accum_ms += elapsed_ms;
  g_clock_state.anim_phase = (uint16_t)((g_clock_state.anim_phase + elapsed_ms) % 60000U);

  while (g_clock_accum_ms >= 1000U)
  {
    g_clock_accum_ms -= 1000U;
    g_clock_state.second++;
    if (g_clock_state.second >= 60U)
    {
      g_clock_state.second = 0U;
      g_clock_state.minute++;
      if (g_clock_state.minute >= 60U)
      {
        g_clock_state.minute = 0U;
        g_clock_state.hour = (uint8_t)((g_clock_state.hour + 1U) % 24U);
      }
    }

    if (g_fan_state.enabled != 0U)
    {
      g_fan_state.rpm = (uint16_t)(780U + ((uint16_t)g_fan_state.speed_percent * 18U));
    }
    else
    {
      g_fan_state.rpm = 0U;
    }
  }
}

void UI_Model_SetFanState(const ui_fan_state_t *state)
{
  if (state != NULL)
  {
    g_fan_state = *state;
  }
}

void UI_Model_GetFanState(ui_fan_state_t *state)
{
  if (state != NULL)
  {
    *state = g_fan_state;
  }
}

void UI_Model_SetClockState(const ui_clock_state_t *state)
{
  if (state != NULL)
  {
    g_clock_state = *state;
    g_clock_state.city[sizeof(g_clock_state.city) - 1U] = '\0';
  }
}

void UI_Model_GetClockState(ui_clock_state_t *state)
{
  if (state != NULL)
  {
    *state = g_clock_state;
  }
}

void UI_Model_SetSettings(const ui_setting_state_t *state)
{
  if (state != NULL)
  {
    g_setting_state = *state;
  }
}

void UI_Model_GetSettings(ui_setting_state_t *state)
{
  if (state != NULL)
  {
    *state = g_setting_state;
  }
}

void UI_Model_SetWeatherMode(ui_weather_mode_t mode)
{
  g_weather_mode = mode;
}

ui_weather_mode_t UI_Model_GetWeatherMode(void)
{
  return g_weather_mode;
}
