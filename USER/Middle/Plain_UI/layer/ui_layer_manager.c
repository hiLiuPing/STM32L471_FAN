#include "ui_layer_manager.h"

#include <string.h>

typedef struct
{
  ui_layer_draw_fn draw_cb;
  void *user_data;
} ui_layer_slot_t;

static ui_layer_slot_t g_layers[UI_LAYER_COUNT];

void UI_LayerManager_Init(void)
{
  memset(g_layers, 0, sizeof(g_layers));
}

void UI_LayerManager_SetDrawCallback(ui_layer_id_t layer, ui_layer_draw_fn draw_cb, void *user_data)
{
  if (layer >= UI_LAYER_COUNT)
  {
    return;
  }

  g_layers[layer].draw_cb = draw_cb;
  g_layers[layer].user_data = user_data;
}

void UI_LayerManager_Draw(const ui_viewport_t *viewport)
{
  uint32_t i;

  for (i = 0U; i < (uint32_t)UI_LAYER_COUNT; ++i)
  {
    if (g_layers[i].draw_cb != NULL)
    {
      g_layers[i].draw_cb(viewport, g_layers[i].user_data);
    }
  }
}
