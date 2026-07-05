/*
 * 图层管理器。
 * 统一维护 Base / Climate / FX Particle / Top Window 四层顺序。
 * 所有可视内容都应注册到层，不应在页面里手写前后关系。
 */
#ifndef UI_LAYER_MANAGER_H
#define UI_LAYER_MANAGER_H

#include <stdint.h>

#include "../core/ui_types.h"

typedef enum
{
  UI_LAYER_BASE = 0,
  UI_LAYER_CLIMATE,
  UI_LAYER_FX_PARTICLE,
  UI_LAYER_TOP_WINDOW,
  UI_LAYER_COUNT
} ui_layer_id_t;

typedef void (*ui_layer_draw_fn)(const ui_viewport_t *viewport, void *user_data);

void UI_LayerManager_Init(void);
void UI_LayerManager_SetDrawCallback(ui_layer_id_t layer, ui_layer_draw_fn draw_cb, void *user_data);
void UI_LayerManager_Draw(const ui_viewport_t *viewport);

#endif /* UI_LAYER_MANAGER_H */
