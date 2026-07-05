#ifndef UI_ANIMATION_H
#define UI_ANIMATION_H

#include <stdint.h>

#include "../config/ui_config.h"

typedef float (*ui_animation_easing_fn)(float progress);
typedef void (*ui_animation_complete_fn)(void *user_data);
typedef void (*ui_animation_dirty_fn)(void *user_data);

typedef struct
{
  uint32_t start_tick_ms;
  uint32_t duration_ms;
  int32_t start_val;
  int32_t target_val;
  int32_t *current_val_ptr;
  ui_animation_easing_fn easing_cb;
  ui_animation_complete_fn complete_cb;
  ui_animation_dirty_fn dirty_cb;
  void *user_data;
  uint8_t is_active;
} ui_animation_t;

void UI_AnimationScheduler_Init(void);
int32_t UI_AnimationScheduler_Start(const ui_animation_t *animation);
void UI_AnimationScheduler_Update(uint32_t elapsed_ms);

#endif /* UI_ANIMATION_H */
