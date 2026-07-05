#include "ui_animation.h"

#include <string.h>

static ui_animation_t g_animations[UI_ANIMATION_CAPACITY];
static uint32_t g_animation_tick_ms;

static float UI_AnimationScheduler_Linear(float progress)
{
  return progress;
}

void UI_AnimationScheduler_Init(void)
{
  memset(g_animations, 0, sizeof(g_animations));
  g_animation_tick_ms = 0U;
}

int32_t UI_AnimationScheduler_Start(const ui_animation_t *animation)
{
  uint32_t i;

  if ((animation == NULL) || (animation->current_val_ptr == NULL))
  {
    return -1;
  }

  for (i = 0U; i < UI_ANIMATION_CAPACITY; ++i)
  {
    if (g_animations[i].is_active == 0U)
    {
      g_animations[i] = *animation;
      g_animations[i].start_tick_ms = g_animation_tick_ms;
      g_animations[i].is_active = 1U;
      *g_animations[i].current_val_ptr = g_animations[i].start_val;
      return (int32_t)i;
    }
  }

  return -1;
}

void UI_AnimationScheduler_Update(uint32_t elapsed_ms)
{
  uint32_t i;

  g_animation_tick_ms += elapsed_ms;

  for (i = 0U; i < UI_ANIMATION_CAPACITY; ++i)
  {
    ui_animation_t *anim = &g_animations[i];
    float progress;
    float eased;
    int32_t next_value;

    if ((anim->is_active == 0U) || (anim->current_val_ptr == NULL))
    {
      continue;
    }

    if (anim->duration_ms == 0U)
    {
      progress = 1.0f;
    }
    else
    {
      progress = (float)(g_animation_tick_ms - anim->start_tick_ms) / (float)anim->duration_ms;
      if (progress > 1.0f)
      {
        progress = 1.0f;
      }
    }

    eased = (anim->easing_cb != NULL) ? anim->easing_cb(progress) : UI_AnimationScheduler_Linear(progress);
    next_value = anim->start_val + (int32_t)((float)(anim->target_val - anim->start_val) * eased);
    *anim->current_val_ptr = next_value;

    if (anim->dirty_cb != NULL)
    {
      anim->dirty_cb(anim->user_data);
    }

    if (progress >= 1.0f)
    {
      *anim->current_val_ptr = anim->target_val;
      anim->is_active = 0U;
      if (anim->complete_cb != NULL)
      {
        anim->complete_cb(anim->user_data);
      }
    }
  }
}
