/* 设置页：承接系统配置、诊断和平台状态。 */
#include "page_settings.h"

#include "../ui_app_state.h"
#include "../../config/ui_config.h"

#include <stdio.h>

void UI_PageSettings_Draw(void)
{
  char tick_line[24];
  char config_line[40];
  char backend_line[40];
  const ui_app_state_t *state = UI_AppState_GetConst();
  ui_render_style_t blue = UI_AppState_Style(UI_AppState_Color(61U, 174U, 235U));
  ui_render_style_t green = UI_AppState_Style(UI_AppState_Color(87U, 220U, 142U));
  ui_render_style_t amber = UI_AppState_Style(UI_AppState_Color(246U, 184U, 82U));
  ui_render_style_t panel = UI_AppState_Style(UI_AppState_Color(22U, 38U, 52U));
  ui_render_style_t soft = UI_AppState_Style(UI_AppState_Color(30U, 48U, 64U));
  ui_rect_t left_top = { 12, 32, 196, 50 };
  ui_rect_t right_top = { 216, 32, 204, 50 };
  ui_rect_t left_bottom = { 12, 88, 196, 38 };
  ui_rect_t right_bottom = { 216, 88, 204, 38 };
  ui_rect_t cb0 = { 24, 99, 118, 16 };
  ui_rect_t cb1 = { 24, 112, 118, 16 };
  ui_rect_t toggle = { 326, 98, 70, 18 };
  ui_rect_t sync_rect = { 224, 112, 46, 14 };
  ui_rect_t reset_rect = { 276, 112, 52, 14 };
  ui_rect_t export_rect = { 334, 112, 58, 14 };

  (void)snprintf(tick_line, sizeof(tick_line), "tick %u ms", (unsigned int)UI_FRAME_TICK_MS);
  (void)snprintf(config_line, sizeof(config_line), "props %u  fx %u  list %u",
                 (unsigned int)UI_MAX_PROPERTY_COUNT,
                 (unsigned int)UI_FX_PARTICLE_CAPACITY,
                 (unsigned int)UI_VIRTUAL_LIST_VISIBLE_ROWS);
  (void)snprintf(backend_line, sizeof(backend_line), "queue %u  bind %u  input %u",
                 (unsigned int)UI_MESSAGE_QUEUE_LENGTH,
                 (unsigned int)UI_PROPERTY_BINDING_CAPACITY,
                 (unsigned int)UI_INPUT_EVENT_QUEUE_LENGTH);

  UI_RendererAdapter_DrawPanel(&state->viewport, &left_top, &panel);
  UI_RendererAdapter_DrawText(&state->viewport, 20, 38, "Platform status", blue.text);
  UI_RendererAdapter_DrawText(&state->viewport, 20, 52, "LVGL backend only", blue.muted);
  UI_RendererAdapter_DrawText(&state->viewport, 20, 64, tick_line, green.muted);

  UI_RendererAdapter_DrawPanel(&state->viewport, &right_top, &soft);
  UI_RendererAdapter_DrawText(&state->viewport, 224, 38, "Architecture contract", blue.text);
  UI_RendererAdapter_DrawText(&state->viewport, 224, 52, config_line, amber.muted);
  UI_RendererAdapter_DrawText(&state->viewport, 224, 64, backend_line, blue.muted);

  UI_RendererAdapter_DrawPanel(&state->viewport, &left_bottom, &panel);
  UI_RendererAdapter_DrawCheckbox(&state->viewport, &cb0, 1U, "backend locked", &green);
  UI_RendererAdapter_DrawCheckbox(&state->viewport, &cb1, 1U, "dirty local", &green);

  UI_RendererAdapter_DrawPanel(&state->viewport, &right_bottom, &soft);
  UI_RendererAdapter_DrawText(&state->viewport, 224, 94, "System actions", blue.text);
  UI_RendererAdapter_DrawToggle(&state->viewport, &toggle, state->toggle_on, &amber);
  UI_RendererAdapter_DrawBadge(&state->viewport, &sync_rect, "SYNC", &blue);
  UI_RendererAdapter_DrawBadge(&state->viewport, &reset_rect, "RESET", &green);
  UI_RendererAdapter_DrawBadge(&state->viewport, &export_rect, "EXPORT", &amber);
}
