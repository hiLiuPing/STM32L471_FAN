/* 控件页：承载常用交互控件和场景动作。 */
#include "page_controls.h"

#include "../ui_app_state.h"
#include "../../datastore/ui_property_store.h"

#include <stdio.h>

void UI_PageControls_Draw(void)
{
  char progress_text[24];
  char mode_text[24];
  const ui_app_state_t *state = UI_AppState_GetConst();
  ui_render_style_t blue = UI_AppState_Style(UI_AppState_Color(61U, 174U, 235U));
  ui_render_style_t green = UI_AppState_Style(UI_AppState_Color(87U, 220U, 142U));
  ui_render_style_t amber = UI_AppState_Style(UI_AppState_Color(246U, 184U, 82U));
  ui_render_style_t panel = UI_AppState_Style(UI_AppState_Color(22U, 38U, 52U));
  ui_render_style_t soft = UI_AppState_Style(UI_AppState_Color(30U, 48U, 64U));
  ui_rect_t btn0 = { 12, 32, 88, 22 };
  ui_rect_t btn1 = { 106, 32, 88, 22 };
  ui_rect_t btn2 = { 200, 32, 88, 22 };
  ui_rect_t toggle = { 298, 32, 126, 22 };
  ui_rect_t left_panel = { 12, 60, 248, 62 };
  ui_rect_t right_panel = { 268, 60, 152, 62 };
  ui_rect_t progress = { 24, 92, 200, 14 };
  ui_rect_t slider = { 24, 108, 200, 10 };
  ui_rect_t checkbox = { 280, 92, 118, 16 };
  ui_rect_t mode_rect = { 280, 108, 44, 14 };
  ui_rect_t state_rect = { 330, 108, 76, 14 };

  (void)snprintf(progress_text, sizeof(progress_text), "progress %ld%%", (long)state->progress);
  (void)snprintf(mode_text, sizeof(mode_text), "M%ld", (long)UI_PropertyStore_GetFrontInt(UI_APP_PROP_MODE_ID, 1));

  UI_RendererAdapter_DrawButton(&state->viewport, &btn0, "Primary", &blue, 1U);
  UI_RendererAdapter_DrawButton(&state->viewport, &btn1, "Ghost", &green, 0U);
  UI_RendererAdapter_DrawButton(&state->viewport, &btn2, "Danger", &amber, 0U);
  UI_RendererAdapter_DrawToggle(&state->viewport, &toggle, state->toggle_on, &green);

  UI_RendererAdapter_DrawPanel(&state->viewport, &left_panel, &panel);
  UI_RendererAdapter_DrawText(&state->viewport, 24, 67, "Interaction deck", blue.text);
  UI_RendererAdapter_DrawText(&state->viewport, 24, 80, progress_text, blue.muted);
  UI_RendererAdapter_DrawProgressBar(&state->viewport, &progress, (uint8_t)state->progress, &blue);
  UI_RendererAdapter_DrawSlider(&state->viewport, &slider, state->progress, 0, 100, &amber);

  UI_RendererAdapter_DrawPanel(&state->viewport, &right_panel, &soft);
  UI_RendererAdapter_DrawText(&state->viewport, 280, 67, "Scene state", blue.text);
  UI_RendererAdapter_DrawText(&state->viewport, 280, 80, state->toggle_on ? "toggle armed" : "toggle idle", green.muted);
  UI_RendererAdapter_DrawCheckbox(&state->viewport, &checkbox, state->checkbox_on, "checked option", &green);
  UI_RendererAdapter_DrawBadge(&state->viewport, &mode_rect, mode_text, &amber);
  UI_RendererAdapter_DrawBadge(&state->viewport, &state_rect, state->checkbox_on ? "checked" : "paused", &green);
}
