#include "ui_fx_emitter.h"

#include <string.h>

#include "../config/ui_config.h"
#include "../core/ui_dirty_region.h"
#include "../hal/backend_lvgl/ui_renderer_adapter.h"

typedef struct
{
  uint8_t is_active;
  ui_viewport_t viewport;
  ui_fx_particle_t particles[UI_FX_PARTICLE_CAPACITY];
  ui_fx_behavior_fn behavior;
} ui_fx_emitter_t;

static ui_fx_emitter_t g_fx_emitter;

static void UI_FXEmitter_ResetParticle(uint32_t index)
{
  ui_fx_particle_t *p = &g_fx_emitter.particles[index];

  /* 粒子池复位时按索引撒开，避免所有粒子从同一点同时出现。 */
  p->x = (int16_t)((index * 27U) % (uint32_t)((g_fx_emitter.viewport.width > 0) ? g_fx_emitter.viewport.width : 1));
  p->y = (int16_t)((index * 11U) % (uint32_t)((g_fx_emitter.viewport.height > 0) ? g_fx_emitter.viewport.height : 1));
  p->vx = (int16_t)(1 + (int16_t)(index % 3U));
  p->vy = (int16_t)(1 + (int16_t)(index % 2U));
  p->life_frames = (uint8_t)(20U + (index % 20U));
}

void UI_FXEmitter_Init(void)
{
  memset(&g_fx_emitter, 0, sizeof(g_fx_emitter));
}

BaseType_t UI_FXEmitter_Start(const ui_viewport_t *viewport, ui_fx_behavior_fn behavior)
{
  uint32_t i;

  if (viewport == NULL)
  {
    return pdFAIL;
  }

  memset(&g_fx_emitter, 0, sizeof(g_fx_emitter));
  g_fx_emitter.is_active = 1U;
  g_fx_emitter.viewport = *viewport;
  g_fx_emitter.behavior = (behavior != NULL) ? behavior : UI_FXEmitter_DefaultBehavior;

  for (i = 0U; i < UI_FX_PARTICLE_CAPACITY; ++i)
  {
    UI_FXEmitter_ResetParticle(i);
  }

  {
    ui_rect_t rect = { g_fx_emitter.viewport.x, g_fx_emitter.viewport.y, g_fx_emitter.viewport.width, g_fx_emitter.viewport.height };
    UI_DirtyRegion_Invalidate(&rect);
  }
  return pdPASS;
}

void UI_FXEmitter_Stop(void)
{
  if (g_fx_emitter.is_active != 0U)
  {
    ui_rect_t rect = { g_fx_emitter.viewport.x, g_fx_emitter.viewport.y, g_fx_emitter.viewport.width, g_fx_emitter.viewport.height };
    UI_DirtyRegion_Invalidate(&rect);
  }

  memset(&g_fx_emitter, 0, sizeof(g_fx_emitter));
}

void UI_FXEmitter_DefaultBehavior(ui_fx_particle_t *particle, int16_t view_w, int16_t view_h)
{
  if (particle == NULL)
  {
    return;
  }

  particle->x = (int16_t)(particle->x + particle->vx);
  particle->y = (int16_t)(particle->y + particle->vy);
  if (particle->life_frames > 0U)
  {
    particle->life_frames--;
  }

  if ((particle->life_frames == 0U) || (particle->x >= view_w) || (particle->y >= view_h))
  {
    particle->x = 0;
    particle->y = (int16_t)((particle->y + 17) % ((view_h > 0) ? view_h : 1));
    particle->life_frames = 30U;
  }
}

void UI_FXEmitter_Update(void)
{
  uint32_t i;

  if (g_fx_emitter.is_active == 0U)
  {
    return;
  }

  /* 每帧只更新粒子状态，不在这里直接绘制。 */
  for (i = 0U; i < UI_FX_PARTICLE_CAPACITY; ++i)
  {
    g_fx_emitter.behavior(&g_fx_emitter.particles[i], g_fx_emitter.viewport.width, g_fx_emitter.viewport.height);
  }

  {
    ui_rect_t rect = { g_fx_emitter.viewport.x, g_fx_emitter.viewport.y, g_fx_emitter.viewport.width, g_fx_emitter.viewport.height };
    UI_DirtyRegion_Invalidate(&rect);
  }
}

void UI_FXEmitter_Draw(const ui_viewport_t *viewport)
{
  uint32_t i;
  ui_color_t color;

  if ((viewport == NULL) || (g_fx_emitter.is_active == 0U))
  {
    return;
  }

  color = UI_RendererAdapter_Color(82U, 160U, 220U);
  for (i = 0U; i < UI_FX_PARTICLE_CAPACITY; ++i)
  {
    const ui_fx_particle_t *p = &g_fx_emitter.particles[i];
    UI_RendererAdapter_DrawLine(viewport, p->x, p->y, (int16_t)(p->x + 4), (int16_t)(p->y + 2), color);
  }
}
