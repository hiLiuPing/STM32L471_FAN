/* 数据页：承载趋势图、关键指标和状态聚合。 */
#include "page_data.h"

#include "../ui_app_state.h"
#include "../../datastore/ui_property_store.h"

#include <stdio.h>

void UI_PageData_Draw(void)
{
  static const int16_t values[] = { 28, 32, 35, 31, 42, 48, 45, 55, 52, 61, 58, 66 };
  char frame_line[24];
  char heartbeat_line[24];
  char temp_line[24];
  char mode_line[24];
  const ui_app_state_t *state = UI_AppState_GetConst();
  ui_render_style_t blue = UI_AppState_Style(UI_AppState_Color(61U, 174U, 235U));
  ui_render_style_t green = UI_AppState_Style(UI_AppState_Color(87U, 220U, 142U));
  ui_render_style_t amber = UI_AppState_Style(UI_AppState_Color(246U, 184U, 82U));
  ui_render_style_t panel = UI_AppState_Style(UI_AppState_Color(22U, 38U, 52U));
  ui_render_style_t soft = UI_AppState_Style(UI_AppState_Color(30U, 48U, 64U));
  ui_rect_t chart_panel = { 12, 32, 244, 70 };
  ui_rect_t chart = { 18, 45, 232, 46 };
  ui_rect_t metric_top = { 266, 32, 154, 30 };
  ui_rect_t metric_bottom = { 266, 68, 154, 34 };
  ui_rect_t progress = { 12, 108, 408, 14 };
  ui_rect_t frame_rect = { 324, 96, 52, 14 };
  ui_rect_t mode_rect = { 382, 96, 38, 14 };

  (void)snprintf(frame_line, sizeof(frame_line), "F%lu", (unsigned long)state->frame_count);
  (void)snprintf(heartbeat_line, sizeof(heartbeat_line), "hb %ld", (long)UI_PropertyStore_GetFrontInt(UI_APP_PROP_HEARTBEAT_ID, 0));
  (void)snprintf(temp_line, sizeof(temp_line), "temp %ld C", (long)UI_PropertyStore_GetFrontInt(UI_APP_PROP_TEMPERATURE_ID, 25));
  (void)snprintf(mode_line, sizeof(mode_line), "M%ld", (long)UI_PropertyStore_GetFrontInt(UI_APP_PROP_MODE_ID, 1));

  UI_RendererAdapter_DrawPanel(&state->viewport, &chart_panel, &panel);
  UI_RendererAdapter_DrawText(&state->viewport, 18, 36, "Telemetry window", blue.text);
  UI_RendererAdapter_DrawText(&state->viewport, 18, 49, "trend / sample / snapshot", blue.muted);
  UI_RendererAdapter_DrawSparkline(&state->viewport, &chart, values, (uint8_t)(sizeof(values) / sizeof(values[0])), 20, 70, &blue);

  UI_RendererAdapter_DrawPanel(&state->viewport, &metric_top, &soft);
  UI_RendererAdapter_DrawText(&state->viewport, 274, 37, "Temperature", blue.text);
  UI_RendererAdapter_DrawText(&state->viewport, 274, 50, temp_line, amber.muted);

  UI_RendererAdapter_DrawPanel(&state->viewport, &metric_bottom, &panel);
  UI_RendererAdapter_DrawText(&state->viewport, 274, 73, "Mode / heartbeat", blue.text);
  UI_RendererAdapter_DrawText(&state->viewport, 274, 86, mode_line, green.muted);
  UI_RendererAdapter_DrawText(&state->viewport, 320, 86, heartbeat_line, blue.muted);

  UI_RendererAdapter_DrawText(&state->viewport, 12, 96, "Progress", blue.muted);
  UI_RendererAdapter_DrawProgressBar(&state->viewport, &progress, (uint8_t)state->progress, &green);
  UI_RendererAdapter_DrawBadge(&state->viewport, &frame_rect, frame_line, &blue);
  UI_RendererAdapter_DrawBadge(&state->viewport, &mode_rect, mode_line, &amber);
}
