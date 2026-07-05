#include "ui_weather_effect.h"

#include <string.h>

#include "lvgl.h"

#define UI_WEATHER_PARTICLE_COUNT 18U

typedef struct
{
  ui_obj_t *obj;
  int16_t x;
  int16_t y;
  int16_t speed;
  int16_t drift;
  uint8_t size;
} ui_weather_particle_t;

typedef struct
{
  ui_obj_t *layer;
  ui_obj_t *parent;
  ui_weather_mode_t mode;
  uint8_t is_started;
  uint32_t seed;
  uint32_t accum_ms;
  ui_weather_particle_t particles[UI_WEATHER_PARTICLE_COUNT];
} ui_weather_effect_t;

static ui_weather_effect_t g_weather;

static uint32_t UI_WeatherEffect_NextRand(void)
{
  g_weather.seed = (g_weather.seed * 1103515245UL) + 12345UL;
  return (g_weather.seed >> 16) & 0x7FFFUL;
}

static int16_t UI_WeatherEffect_Width(void)
{
  return (g_weather.layer != NULL) ? (int16_t)lv_obj_get_width(g_weather.layer) : 0;
}

static int16_t UI_WeatherEffect_Height(void)
{
  return (g_weather.layer != NULL) ? (int16_t)lv_obj_get_height(g_weather.layer) : 0;
}

static lv_color_t UI_WeatherEffect_Color(void)
{
  switch (g_weather.mode)
  {
  case UI_WEATHER_RAIN:
    return lv_color_make(86U, 178U, 240U);
  case UI_WEATHER_SNOW:
    return lv_color_make(235U, 246U, 255U);
  case UI_WEATHER_CLOUD:
    return lv_color_make(130U, 150U, 165U);
  case UI_WEATHER_CLEAR:
  default:
    return lv_color_make(246U, 184U, 82U);
  }
}

static void UI_WeatherEffect_ResetParticle(uint32_t index)
{
  ui_weather_particle_t *particle;
  int16_t width = UI_WeatherEffect_Width();
  int16_t height = UI_WeatherEffect_Height();

  if ((index >= UI_WEATHER_PARTICLE_COUNT) || (width <= 0) || (height <= 0))
  {
    return;
  }

  particle = &g_weather.particles[index];
  particle->x = (int16_t)(UI_WeatherEffect_NextRand() % (uint32_t)width);
  particle->y = (int16_t)(UI_WeatherEffect_NextRand() % (uint32_t)height);
  particle->speed = (int16_t)(1 + (UI_WeatherEffect_NextRand() % 4U));
  particle->drift = (int16_t)((int32_t)(UI_WeatherEffect_NextRand() % 3U) - 1);
  particle->size = (uint8_t)(2U + (UI_WeatherEffect_NextRand() % 4U));

  if (particle->obj != NULL)
  {
    uint8_t w = particle->size;
    uint8_t h = particle->size;

    if (g_weather.mode == UI_WEATHER_RAIN)
    {
      w = 2U;
      h = (uint8_t)(8U + particle->size);
    }
    else if (g_weather.mode == UI_WEATHER_CLOUD)
    {
      w = (uint8_t)(18U + (particle->size * 2U));
      h = (uint8_t)(8U + particle->size);
    }

    lv_obj_set_size(particle->obj, w, h);
    lv_obj_set_style_bg_color(particle->obj, UI_WeatherEffect_Color(), 0);
    lv_obj_set_style_radius(particle->obj, (g_weather.mode == UI_WEATHER_RAIN) ? 1 : 6, 0);
    lv_obj_set_pos(particle->obj, particle->x, particle->y);
  }
}

void UI_WeatherEffect_Init(ui_obj_t *parent)
{
  uint32_t i;

  if (parent == NULL)
  {
    return;
  }

  if (g_weather.layer == NULL)
  {
    memset(&g_weather, 0, sizeof(g_weather));
    g_weather.seed = 0x13579BDFUL;
    g_weather.mode = UI_WEATHER_CLEAR;
    g_weather.layer = lv_obj_create(parent);
    lv_obj_remove_style_all(g_weather.layer);
    lv_obj_set_size(g_weather.layer, lv_pct(100), lv_pct(100));
    lv_obj_clear_flag(g_weather.layer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(g_weather.layer, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(g_weather.layer, LV_OPA_TRANSP, 0);

    for (i = 0U; i < UI_WEATHER_PARTICLE_COUNT; ++i)
    {
      g_weather.particles[i].obj = lv_obj_create(g_weather.layer);
      lv_obj_remove_style_all(g_weather.particles[i].obj);
      lv_obj_set_style_bg_opa(g_weather.particles[i].obj, LV_OPA_60, 0);
    }
  }
  else if (g_weather.parent != parent)
  {
    lv_obj_set_parent(g_weather.layer, parent);
  }

  g_weather.parent = parent;
  lv_obj_move_foreground(g_weather.layer);
  UI_WeatherEffect_SetMode(g_weather.mode);
}

void UI_WeatherEffect_SetMode(ui_weather_mode_t mode)
{
  uint32_t i;

  g_weather.mode = mode;
  UI_Model_SetWeatherMode(mode);

  for (i = 0U; i < UI_WEATHER_PARTICLE_COUNT; ++i)
  {
    UI_WeatherEffect_ResetParticle(i);
  }

  if (g_weather.layer != NULL)
  {
    UI_Widget_SetHidden(g_weather.layer, (mode == UI_WEATHER_CLEAR) || (g_weather.is_started == 0U));
  }
}

void UI_WeatherEffect_Start(void)
{
  g_weather.is_started = 1U;
  if (g_weather.layer != NULL)
  {
    UI_Widget_SetHidden(g_weather.layer, g_weather.mode == UI_WEATHER_CLEAR);
  }
}

void UI_WeatherEffect_Stop(void)
{
  g_weather.is_started = 0U;
  if (g_weather.layer != NULL)
  {
    UI_Widget_SetHidden(g_weather.layer, 1U);
  }
}

void UI_WeatherEffect_Update(uint32_t elapsed_ms)
{
  uint32_t i;
  int16_t width = UI_WeatherEffect_Width();
  int16_t height = UI_WeatherEffect_Height();

  if ((g_weather.is_started == 0U) || (g_weather.mode == UI_WEATHER_CLEAR) ||
      (g_weather.layer == NULL) || (width <= 0) || (height <= 0))
  {
    return;
  }

  g_weather.accum_ms += elapsed_ms;
  if (g_weather.accum_ms < 30U)
  {
    return;
  }
  g_weather.accum_ms = 0U;

  for (i = 0U; i < UI_WEATHER_PARTICLE_COUNT; ++i)
  {
    ui_weather_particle_t *particle = &g_weather.particles[i];

    if (g_weather.mode == UI_WEATHER_RAIN)
    {
      particle->y = (int16_t)(particle->y + particle->speed + 2);
      particle->x = (int16_t)(particle->x + 1);
    }
    else if (g_weather.mode == UI_WEATHER_SNOW)
    {
      particle->y = (int16_t)(particle->y + 1);
      particle->x = (int16_t)(particle->x + particle->drift);
    }
    else
    {
      particle->x = (int16_t)(particle->x + 1 + particle->drift);
      if ((i % 3U) == 0U)
      {
        particle->y = (int16_t)(particle->y + particle->drift);
      }
    }

    if ((particle->x > width) || (particle->y > height) || (particle->x < -24) || (particle->y < -16))
    {
      UI_WeatherEffect_ResetParticle(i);
      particle->y = 0;
    }
    else if (particle->obj != NULL)
    {
      lv_obj_set_pos(particle->obj, particle->x, particle->y);
    }
  }
}
