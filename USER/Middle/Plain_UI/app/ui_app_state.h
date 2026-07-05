/*
 * 业务页面共享状态。
 * 这里集中放 5 页 UI 的运行时状态、页面元数据和页面公共绘制能力。
 * 页面文件读取这里的状态，但不直接越层访问 HAL 或导航栈内部实现。
 */
#ifndef UI_APP_STATE_H
#define UI_APP_STATE_H

#include <stdint.h>

#include "../event/ui_event_type.h"
#include "../event/ui_event_bus.h"
#include "../datastore/ui_model.h"
#include "../effects/effect_marquee.h"
#include "../widget/advanced/ui_virtual_list.h"

#include "../hal/backend_lvgl/ui_renderer_adapter.h"
#include "../core/ui_types.h"

#define UI_APP_PAGE_HOME_ID           1U
#define UI_APP_PAGE_TRAVEL_CLOCK_ID   2U
#define UI_APP_PAGE_FAN_STATUS_ID     3U
#define UI_APP_PAGE_CLOCK_SETTINGS_ID 4U
#define UI_APP_PAGE_FAN_SETTINGS_ID   5U

#define UI_APP_PAGE_CONTROLS_ID UI_APP_PAGE_TRAVEL_CLOCK_ID
#define UI_APP_PAGE_LIST_ID     UI_APP_PAGE_FAN_STATUS_ID
#define UI_APP_PAGE_DATA_ID     UI_APP_PAGE_CLOCK_SETTINGS_ID
#define UI_APP_PAGE_SETTINGS_ID UI_APP_PAGE_FAN_SETTINGS_ID

#define UI_APP_PROP_TITLE_ID      1U
#define UI_APP_PROP_HEARTBEAT_ID  2U
#define UI_APP_PROP_STATUS_ID     3U
#define UI_APP_PROP_SERVICE_ID    4U
#define UI_APP_PROP_PROGRESS_ID   5U
#define UI_APP_PROP_TEMPERATURE_ID 6U
#define UI_APP_PROP_MODE_ID       7U

typedef struct
{
  uint16_t page_id;
  const char *title;
  const char *subtitle;
} ui_app_page_model_t;

typedef struct
{
  ui_viewport_t viewport;
  uint32_t frame_count;
  int32_t heartbeat;
  int32_t progress;
  int32_t temperature;
  uint8_t toggle_on;
  uint8_t checkbox_on;
  uint8_t selected_row;
  uint8_t active_page_index;
  ui_virtual_list_t virtual_list;
  ui_marquee_t marquee;
} ui_app_state_t;

void UI_AppState_Init(void);
ui_app_state_t *UI_AppState_Get(void);
const ui_app_state_t *UI_AppState_GetConst(void);
void UI_AppState_SyncViewport(void);
void UI_AppState_InvalidateFull(void);
uint8_t UI_AppState_GetPageCount(void);
uint8_t UI_AppState_FindPageIndex(uint16_t page_id);
uint16_t UI_AppState_PageIdFromIndex(uint8_t index);
const ui_app_page_model_t *UI_AppState_GetPageModelByIndex(uint8_t index);
ui_color_t UI_AppState_Color(uint8_t red, uint8_t green, uint8_t blue);
ui_render_style_t UI_AppState_Style(ui_color_t accent);
void UI_AppState_DrawChrome(uint16_t page_id);
void UI_AppState_DrawFooter(void);
void UI_AppState_OnFrame(void);
void UI_AppState_OnMessage(const ui_msg_t *msg);
void UI_AppState_OnEvent(const ui_event_t *event);
uint16_t UI_AppState_GetActivePageId(void);

#endif /* UI_APP_STATE_H */
