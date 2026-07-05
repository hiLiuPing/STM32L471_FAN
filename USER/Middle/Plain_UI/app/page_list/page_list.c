/* 列表页：演示虚拟列表 provider 和选择流。 */
#include "page_list.h"

#include "../ui_app_state.h"

#include <stdio.h>

static void UI_PageList_GetSelectedItem(const ui_app_state_t *state, char *out_buf, uint16_t out_len)
{
  if ((state == NULL) || (out_buf == NULL) || (out_len == 0U) ||
      (state->virtual_list.provider.get_item_text == NULL))
  {
    return;
  }

  state->virtual_list.provider.get_item_text(state->virtual_list.focus_index, out_buf, out_len);
}

void UI_PageList_Draw(void)
{
  ui_app_state_t *state = UI_AppState_Get();
  char selected_item[40] = { 0 };
  char count_text[24];
  char focus_text[24];
  ui_render_style_t blue = UI_AppState_Style(UI_AppState_Color(61U, 174U, 235U));
  ui_render_style_t green = UI_AppState_Style(UI_AppState_Color(87U, 220U, 142U));
  ui_render_style_t amber = UI_AppState_Style(UI_AppState_Color(246U, 184U, 82U));
  ui_render_style_t panel = UI_AppState_Style(UI_AppState_Color(22U, 38U, 52U));
  ui_render_style_t soft = UI_AppState_Style(UI_AppState_Color(30U, 48U, 64U));
  ui_rect_t list_panel = { 12, 32, 232, 96 };
  ui_rect_t list_rect = { 16, 40, 220, 84 };
  ui_rect_t detail_top = { 252, 32, 168, 42 };
  ui_rect_t detail_bottom = { 252, 80, 168, 48 };
  ui_rect_t count_rect = { 260, 88, 50, 14 };
  ui_rect_t focus_rect = { 316, 88, 48, 14 };
  ui_rect_t scroll_rect = { 370, 88, 42, 14 };

  UI_PageList_GetSelectedItem(state, selected_item, sizeof(selected_item));
  (void)snprintf(count_text, sizeof(count_text), "%lu", (unsigned long)state->virtual_list.provider.total_items);
  (void)snprintf(focus_text, sizeof(focus_text), "%lu", (unsigned long)state->virtual_list.focus_index);

  UI_RendererAdapter_DrawPanel(&state->viewport, &list_panel, &panel);
  UI_RendererAdapter_DrawText(&state->viewport, 20, 36, "Service queue", blue.text);
  UI_RendererAdapter_DrawText(&state->viewport, 20, 49, "virtual provider / reusable rows", blue.muted);
  UI_VirtualList_Draw(&state->virtual_list, &state->viewport, &list_rect, &blue);

  UI_RendererAdapter_DrawPanel(&state->viewport, &detail_top, &soft);
  UI_RendererAdapter_DrawText(&state->viewport, 260, 38, "Selected item", blue.text);
  UI_RendererAdapter_DrawText(&state->viewport, 260, 52, selected_item[0] != '\0' ? selected_item : "No selection", green.muted);

  UI_RendererAdapter_DrawPanel(&state->viewport, &detail_bottom, &panel);
  UI_RendererAdapter_DrawBadge(&state->viewport, &count_rect, count_text, &blue);
  UI_RendererAdapter_DrawBadge(&state->viewport, &focus_rect, focus_text, &amber);
  UI_RendererAdapter_DrawBadge(&state->viewport, &scroll_rect, "reuse", &green);
  UI_RendererAdapter_DrawText(&state->viewport, 260, 107, "UP/DOWN browse", blue.muted);
}
