/*
 * 粒子发射器抽象。
 * 负责维护粒子池、生命周期和行为函数入口。
 * 行为函数可以替换，但页面不需要知道粒子池内部结构。
 */
#ifndef UI_FX_EMITTER_H
#define UI_FX_EMITTER_H

#include "FreeRTOS.h"

#include <stdint.h>

#include "../core/ui_types.h"

typedef struct
{
  int16_t x;
  int16_t y;
  int16_t vx;
  int16_t vy;
  uint8_t life_frames;
} ui_fx_particle_t;

typedef void (*ui_fx_behavior_fn)(ui_fx_particle_t *particle, int16_t view_w, int16_t view_h);

void UI_FXEmitter_Init(void);
BaseType_t UI_FXEmitter_Start(const ui_viewport_t *viewport, ui_fx_behavior_fn behavior);
void UI_FXEmitter_Stop(void);
void UI_FXEmitter_Update(void);
void UI_FXEmitter_Draw(const ui_viewport_t *viewport);
void UI_FXEmitter_DefaultBehavior(ui_fx_particle_t *particle, int16_t view_w, int16_t view_h);

#endif /* UI_FX_EMITTER_H */
