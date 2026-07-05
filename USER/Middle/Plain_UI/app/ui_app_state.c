#include "ui_app_state.h"

#include "../datastore/ui_binding.h"
#include "../datastore/ui_model.h"
#include "../core/ui_dirty_region.h"
#include "../effects/ui_fx_emitter.h"
#include "../navigation/ui_page_manager.h"
#include "../popup/ui_popup.h"

#include <stdio.h>

static const ui_app_page_model_t g_app_page_models[] =
{
  { UI_APP_PAGE_HOME_ID, "Home", "live overview" },
  { UI_APP_PAGE_CONTROLS_ID, "Controls", "quick actions" },
  { UI_APP_PAGE_LIST_ID, "Services", "virtual list" },
  { UI_APP_PAGE_DATA_ID, "Telemetry", "live metrics" },
  { UI_APP_PAGE_SETTINGS_ID, "Settings", "platform status" }
};

static const char *g_app_service_items[] =
{
  "Workspace feed",
  "Climate scenes",
  "Device mirror",
  "Route planner",
  "Alerts center",
  "Asset queue",
  "Telemetry jobs",
  "Night profile",
  "Support log",
  "System audit"
};

static ui_app_state_t g_app_state;

static void UI_AppState_SyncPageStatus(void)
{
  char status[32];
  char service[32];
  const ui_app_page_model_t *model = UI_AppState_GetPageModelByIndex(g_app_state.active_page_index);

  if (model == NULL)
  {
    return;
  }

  (void)snprintf(status, sizeof(status), "%s active", model->title);
  (void)snprintf(service, sizeof(service), "%s", model->subtitle);
  UI_PropertyStore_SetString(UI_APP_PROP_STATUS_ID, status);
  UI_PropertyStore_SetString(UI_APP_PROP_SERVICE_ID, service);
}

static void UI_AppState_SelectPage(uint8_t index)
{
  uint8_t page_count = UI_AppState_GetPageCount();

  if (page_count == 0U)
  {
    return;
  }

  /* 先更新页面索引，再同步状态，再交给导航层做页面切换。 */
  g_app_state.active_page_index = (uint8_t)(index % page_count);
  UI_AppState_SyncPageStatus();
  (void)UI_PageManager_Replace(UI_AppState_PageIdFromIndex(g_app_state.active_page_index));
}

static void UI_AppState_ListGetItemText(uint32_t index, char *out_buf, uint16_t buf_len)
{
  if ((out_buf == NULL) || (buf_len == 0U))
  {
    return;
  }

  (void)snprintf(out_buf,
                 buf_len,
                 "%s",
                 g_app_service_items[index % (sizeof(g_app_service_items) / sizeof(g_app_service_items[0]))]);
}

static void UI_AppState_ListOnItemClick(uint32_t index)
{
  ui_rect_t modal_rect = { 92, 42, 244, 58 };
  char message[40];

  (void)snprintf(message,
                 sizeof(message),
                 "%s ready",
                 g_app_service_items[index % (sizeof(g_app_service_items) / sizeof(g_app_service_items[0]))]);
  (void)UI_Popup_Show(&modal_rect, "Service Queue", message, 90U, NULL, NULL);
}

void UI_AppState_Init(void)
{
  ui_list_provider_t provider;
  ui_viewport_t fx_viewport;
  ui_rect_t progress_rect = { 12, 82, 384, 30 };
  ui_rect_t heartbeat_rect = { 151, 40, 126, 78 };
  ui_rect_t temperature_rect = { 218, 82, 178, 30 };

  g_app_state.viewport.x = 0;
  g_app_state.viewport.y = 0;
  g_app_state.viewport.width = UI_RendererAdapter_GetWidth();
  g_app_state.viewport.height = UI_RendererAdapter_GetHeight();
  g_app_state.viewport.scroll_x = 0;
  g_app_state.viewport.scroll_y = 0;
  g_app_state.frame_count = 0U;
  g_app_state.heartbeat = 0;
  g_app_state.progress = 35;
  g_app_state.temperature = 25;
  g_app_state.toggle_on = 1U;
  g_app_state.checkbox_on = 1U;
  g_app_state.selected_row = 0U;
  g_app_state.active_page_index = 0U;
  fx_viewport = g_app_state.viewport;

  provider.total_items = (uint32_t)(sizeof(g_app_service_items) / sizeof(g_app_service_items[0]));
  provider.get_item_text = UI_AppState_ListGetItemText;
  provider.on_item_click = UI_AppState_ListOnItemClick;
  UI_VirtualList_Init(&g_app_state.virtual_list, &provider, 4U, 22);
  UI_Marquee_Init(&g_app_state.marquee, "PLAIN UI  LVGL backend  live status  focusable controls  modular pages", 520);
  (void)UI_FXEmitter_Start(&fx_viewport, NULL);

  UI_PropertyStore_SetString(UI_APP_PROP_TITLE_ID, "PLAIN UI");
  UI_PropertyStore_SetString(UI_APP_PROP_STATUS_ID, "All systems online");
  UI_PropertyStore_SetString(UI_APP_PROP_SERVICE_ID, "North cluster synced");
  UI_PropertyStore_SetInt(UI_APP_PROP_HEARTBEAT_ID, g_app_state.heartbeat);
  UI_PropertyStore_SetInt(UI_APP_PROP_PROGRESS_ID, g_app_state.progress);
  UI_PropertyStore_SetInt(UI_APP_PROP_TEMPERATURE_ID, g_app_state.temperature);
  UI_PropertyStore_SetInt(UI_APP_PROP_MODE_ID, 2);
  UI_AppState_SyncPageStatus();
  (void)UI_Binding_BindRect(UI_APP_PROP_HEARTBEAT_ID, &heartbeat_rect);
  (void)UI_Binding_BindRect(UI_APP_PROP_PROGRESS_ID, &progress_rect);
  (void)UI_Binding_BindRect(UI_APP_PROP_TEMPERATURE_ID, &temperature_rect);
}

ui_app_state_t *UI_AppState_Get(void)
{
  return &g_app_state;
}

const ui_app_state_t *UI_AppState_GetConst(void)
{
  return &g_app_state;
}

void UI_AppState_SyncViewport(void)
{
  g_app_state.viewport.width = UI_RendererAdapter_GetWidth();
  g_app_state.viewport.height = UI_RendererAdapter_GetHeight();
}

void UI_AppState_InvalidateFull(void)
{
  ui_rect_t full_rect = { 0, 0, g_app_state.viewport.width, g_app_state.viewport.height };

  UI_DirtyRegion_Invalidate(&full_rect);
}

uint8_t UI_AppState_GetPageCount(void)
{
  return (uint8_t)(sizeof(g_app_page_models) / sizeof(g_app_page_models[0]));
}

uint8_t UI_AppState_FindPageIndex(uint16_t page_id)
{
  uint8_t i;

  for (i = 0U; i < UI_AppState_GetPageCount(); ++i)
  {
    if (g_app_page_models[i].page_id == page_id)
    {
      return i;
    }
  }

  return 0U;
}

uint16_t UI_AppState_PageIdFromIndex(uint8_t index)
{
  return g_app_page_models[index % UI_AppState_GetPageCount()].page_id;
}

const ui_app_page_model_t *UI_AppState_GetPageModelByIndex(uint8_t index)
{
  return &g_app_page_models[index % UI_AppState_GetPageCount()];
}

ui_color_t UI_AppState_Color(uint8_t red, uint8_t green, uint8_t blue)
{
  return UI_RendererAdapter_Color(red, green, blue);
}

ui_render_style_t UI_AppState_Style(ui_color_t accent)
{
  ui_render_style_t style;

  style.text = UI_AppState_Color(232U, 240U, 248U);
  style.muted = UI_AppState_Color(128U, 150U, 170U);
  style.background = UI_AppState_Color(16U, 28U, 40U);
  style.border = UI_AppState_Color(42U, 62U, 78U);
  style.accent = accent;
  style.radius = 4U;
  style.border_width = 1U;
  return style;
}

void UI_AppState_DrawChrome(uint16_t page_id)
{
  uint8_t i;
  char page_line[24];
  const char *status_text = UI_PropertyStore_GetFrontString(UI_APP_PROP_STATUS_ID, "Running");
  uint8_t page_count = UI_AppState_GetPageCount();
  const ui_app_page_model_t *model = &g_app_page_models[UI_AppState_FindPageIndex(page_id)];
  ui_color_t text = UI_AppState_Color(232U, 240U, 248U);
  ui_color_t muted = UI_AppState_Color(136U, 154U, 174U);
  ui_color_t blue = UI_AppState_Color(61U, 174U, 235U);
  ui_render_style_t status_style = UI_AppState_Style(UI_AppState_Color(87U, 220U, 142U));
  ui_rect_t status_rect = { 236, 5, 116, 16 };

  /* 顶栏同时提供品牌、页名、页副标题和运行状态。 */
  (void)snprintf(page_line,
                 sizeof(page_line),
                 "%u/%u",
                 (unsigned int)(UI_AppState_FindPageIndex(page_id) + 1U),
                 (unsigned int)page_count);

  UI_RendererAdapter_DrawFillRect(&g_app_state.viewport, 0, 0, g_app_state.viewport.width, g_app_state.viewport.height, UI_AppState_Color(7U, 16U, 24U));
  UI_RendererAdapter_DrawFillRect(&g_app_state.viewport, 0, 0, g_app_state.viewport.width, 26, UI_AppState_Color(10U, 27U, 40U));
  UI_RendererAdapter_DrawFillRect(&g_app_state.viewport, 0, 25, g_app_state.viewport.width, 2, blue);
  UI_RendererAdapter_DrawText(&g_app_state.viewport, 12, 6, "PLAIN UI", blue);
  UI_RendererAdapter_DrawText(&g_app_state.viewport, 92, 6, model->title, text);
  UI_RendererAdapter_DrawText(&g_app_state.viewport, 154, 6, model->subtitle, muted);
  UI_RendererAdapter_DrawBadge(&g_app_state.viewport, &status_rect, status_text, &status_style);
  UI_RendererAdapter_DrawText(&g_app_state.viewport, 378, 6, page_line, blue);

  for (i = 0U; i < page_count; ++i)
  {
    int16_t x = (int16_t)(334 + (i * 8U));
    UI_RendererAdapter_DrawFillRect(&g_app_state.viewport, x, 18, 5, 5, (i == UI_AppState_FindPageIndex(page_id)) ? blue : UI_AppState_Color(40U, 61U, 82U));
  }
}

void UI_AppState_DrawFooter(void)
{
  UI_RendererAdapter_DrawLine(&g_app_state.viewport, 12, 130, 416, 130, UI_AppState_Color(28U, 50U, 70U));
  UI_RendererAdapter_DrawText(&g_app_state.viewport, 14, 133, "LEFT/RIGHT switch  ENTER confirm  BACK home", UI_AppState_Color(136U, 154U, 174U));
}

void UI_AppState_OnFrame(void)
{
  g_app_state.frame_count++;
  if ((g_app_state.frame_count % 20U) == 0U)
  {
    g_app_state.heartbeat++;
    g_app_state.progress = (g_app_state.progress + 5) % 101;
    g_app_state.temperature = 25 + (int32_t)((g_app_state.frame_count / 20U) % 16U);
    UI_PropertyStore_SetInt(UI_APP_PROP_MODE_ID, 1 + (int32_t)((g_app_state.frame_count / 20U) % 4U));
    UI_PropertyStore_SetInt(UI_APP_PROP_HEARTBEAT_ID, g_app_state.heartbeat);
    UI_PropertyStore_SetInt(UI_APP_PROP_PROGRESS_ID, g_app_state.progress);
    UI_PropertyStore_SetInt(UI_APP_PROP_TEMPERATURE_ID, g_app_state.temperature);
    UI_AppState_InvalidateFull();
  }

  UI_Marquee_Update(&g_app_state.marquee, 190);
  if ((g_app_state.frame_count % 2U) == 0U)
  {
    UI_AppState_InvalidateFull();
  }
}

void UI_AppState_OnMessage(const ui_msg_t *msg)
{
  if ((msg != NULL) && (msg->payload_len > 0U))
  {
    UI_PropertyStore_SetString(UI_APP_PROP_SERVICE_ID, "Inbound sync acknowledged");
    UI_PropertyStore_SetString(UI_APP_PROP_STATUS_ID, "Feed updated");
    UI_AppState_InvalidateFull();
  }
}

void UI_AppState_OnEvent(const ui_event_t *event)
{
  uint8_t page_count = UI_AppState_GetPageCount();

  if (event == NULL)
  {
    return;
  }

  /* 列表页先把方向键和确认键交给虚拟列表，其它页再走全局导航。 */
  if ((g_app_state.active_page_index == UI_AppState_FindPageIndex(UI_APP_PAGE_LIST_ID)) &&
      ((event->type == UI_KEY_UP) || (event->type == UI_KEY_DOWN) ||
       (event->type == UI_KEY_ENTER) || (event->type == UI_KEY_CLICK)))
  {
    (void)UI_VirtualList_HandleEvent(&g_app_state.virtual_list, event);
    UI_AppState_InvalidateFull();
  }
  else if (event->type == UI_KEY_RIGHT)
  {
    UI_AppState_SelectPage((uint8_t)(g_app_state.active_page_index + 1U));
  }
  else if (event->type == UI_KEY_LEFT)
  {
    UI_AppState_SelectPage((g_app_state.active_page_index == 0U) ? (uint8_t)(page_count - 1U) : (uint8_t)(g_app_state.active_page_index - 1U));
  }
  else if (event->type == UI_KEY_DOWN)
  {
    UI_AppState_SelectPage((uint8_t)(g_app_state.active_page_index + 1U));
  }
  else if (event->type == UI_KEY_UP)
  {
    UI_AppState_SelectPage((g_app_state.active_page_index == 0U) ? (uint8_t)(page_count - 1U) : (uint8_t)(g_app_state.active_page_index - 1U));
  }
  else if (event->type == UI_KEY_BACK)
  {
    UI_AppState_SelectPage(0U);
  }
  else if ((event->type == UI_KEY_ENTER) || (event->type == UI_KEY_CLICK))
  {
    ui_rect_t modal_rect = { 92, 42, 244, 58 };
    const char *popup_title = "Quick Action";
    const char *popup_message = "Home shortcut applied";

    g_app_state.toggle_on = (g_app_state.toggle_on == 0U) ? 1U : 0U;
    g_app_state.checkbox_on = (g_app_state.checkbox_on == 0U) ? 1U : 0U;
    g_app_state.selected_row = (uint8_t)((g_app_state.selected_row + 1U) % 4U);
    g_app_state.progress = (g_app_state.progress + 10) % 101;
    UI_PropertyStore_SetInt(UI_APP_PROP_PROGRESS_ID, g_app_state.progress);
    UI_PropertyStore_SetString(UI_APP_PROP_STATUS_ID, g_app_state.toggle_on ? "Action armed" : "Action standby");
    UI_PropertyStore_SetString(UI_APP_PROP_SERVICE_ID, g_app_state.checkbox_on ? "Sync channel enabled" : "Sync channel paused");

    if (g_app_state.active_page_index == UI_AppState_FindPageIndex(UI_APP_PAGE_CONTROLS_ID))
    {
      popup_title = "Control Deck";
      popup_message = g_app_state.toggle_on ? "Scene activated" : "Scene released";
    }
    else if (g_app_state.active_page_index == UI_AppState_FindPageIndex(UI_APP_PAGE_DATA_ID))
    {
      popup_title = "Telemetry";
      popup_message = "Snapshot pinned";
    }
    else if (g_app_state.active_page_index == UI_AppState_FindPageIndex(UI_APP_PAGE_SETTINGS_ID))
    {
      popup_title = "Settings";
      popup_message = "Platform preferences saved";
    }

    (void)UI_Popup_Show(&modal_rect, popup_title, popup_message, 60U, NULL, NULL);
    UI_AppState_InvalidateFull();
  }
}
