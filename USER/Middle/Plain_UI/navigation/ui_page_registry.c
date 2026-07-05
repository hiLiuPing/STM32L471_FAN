#include "ui_page_registry.h"

#include "../app/ui_app_state.h"

#include "../popup/ui_popup.h"
#include "ui_page_manager.h"

#include "../app/page_controls/page_controls.h"
#include "../app/page_data/page_data.h"
#include "../app/page_home/page_home.h"
#include "../app/page_list/page_list.h"
#include "../app/page_settings/page_settings.h"

static void UI_PageRegistry_PageCreate(ui_page_context_t *page)
{
  (void)page;
  /* 页面第一次创建时同步 viewport，避免切屏后尺寸不一致。 */
  UI_AppState_SyncViewport();
}

static void UI_PageRegistry_PageEnter(ui_page_context_t *page)
{
  ui_app_state_t *state = UI_AppState_Get();

  /* 进入页时，应用状态里的 active_page_index 要跟页面 ID 对齐。 */
  state->active_page_index = UI_AppState_FindPageIndex(page->page_id);
  UI_AppState_InvalidateFull();
}

static void UI_PageRegistry_PageProcess(ui_page_context_t *page)
{
  (void)page;
  UI_AppState_OnFrame();
}

static void UI_PageRegistry_PageDraw(ui_page_context_t *page)
{
  UI_AppState_DrawChrome(page->page_id);

  switch (page->page_id)
  {
  case UI_APP_PAGE_CONTROLS_ID:
    UI_PageControls_Draw();
    break;

  case UI_APP_PAGE_LIST_ID:
    UI_PageList_Draw();
    break;

  case UI_APP_PAGE_DATA_ID:
    UI_PageData_Draw();
    break;

  case UI_APP_PAGE_SETTINGS_ID:
    UI_PageSettings_Draw();
    break;

  case UI_APP_PAGE_HOME_ID:
  default:
    UI_PageHome_Draw();
    break;
  }

  UI_AppState_DrawFooter();
}

static void UI_PageRegistry_PageExit(ui_page_context_t *page)
{
  (void)page;
}

static void UI_PageRegistry_PageDestroy(ui_page_context_t *page)
{
  (void)page;
}

static void UI_PageRegistry_PageHandleMessage(ui_page_context_t *page, const ui_msg_t *msg)
{
  (void)page;
  UI_AppState_OnMessage(msg);
}

static void UI_PageRegistry_PageHandleEvent(ui_page_context_t *page, const ui_event_t *event)
{
  (void)page;
  UI_AppState_OnEvent(event);
}

static ui_page_context_t g_home_page =
{
  UI_APP_PAGE_HOME_ID,
  0U,
  NULL,
  NULL,
  UI_PageRegistry_PageCreate,
  UI_PageRegistry_PageEnter,
  UI_PageRegistry_PageProcess,
  UI_PageRegistry_PageDraw,
  UI_PageRegistry_PageExit,
  UI_PageRegistry_PageDestroy,
  UI_PageRegistry_PageHandleMessage,
  UI_PageRegistry_PageHandleEvent
};

static ui_page_context_t g_controls_page =
{
  UI_APP_PAGE_CONTROLS_ID,
  0U,
  NULL,
  NULL,
  UI_PageRegistry_PageCreate,
  UI_PageRegistry_PageEnter,
  UI_PageRegistry_PageProcess,
  UI_PageRegistry_PageDraw,
  UI_PageRegistry_PageExit,
  UI_PageRegistry_PageDestroy,
  UI_PageRegistry_PageHandleMessage,
  UI_PageRegistry_PageHandleEvent
};

static ui_page_context_t g_list_page =
{
  UI_APP_PAGE_LIST_ID,
  0U,
  NULL,
  NULL,
  UI_PageRegistry_PageCreate,
  UI_PageRegistry_PageEnter,
  UI_PageRegistry_PageProcess,
  UI_PageRegistry_PageDraw,
  UI_PageRegistry_PageExit,
  UI_PageRegistry_PageDestroy,
  UI_PageRegistry_PageHandleMessage,
  UI_PageRegistry_PageHandleEvent
};

static ui_page_context_t g_data_page =
{
  UI_APP_PAGE_DATA_ID,
  0U,
  NULL,
  NULL,
  UI_PageRegistry_PageCreate,
  UI_PageRegistry_PageEnter,
  UI_PageRegistry_PageProcess,
  UI_PageRegistry_PageDraw,
  UI_PageRegistry_PageExit,
  UI_PageRegistry_PageDestroy,
  UI_PageRegistry_PageHandleMessage,
  UI_PageRegistry_PageHandleEvent
};

static ui_page_context_t g_settings_page =
{
  UI_APP_PAGE_SETTINGS_ID,
  0U,
  NULL,
  NULL,
  UI_PageRegistry_PageCreate,
  UI_PageRegistry_PageEnter,
  UI_PageRegistry_PageProcess,
  UI_PageRegistry_PageDraw,
  UI_PageRegistry_PageExit,
  UI_PageRegistry_PageDestroy,
  UI_PageRegistry_PageHandleMessage,
  UI_PageRegistry_PageHandleEvent
};

void UI_PageRegistry_Init(void)
{
  g_home_page.is_created = 0U;
  g_home_page.root_widget = NULL;
  g_home_page.user_data = NULL;
  g_controls_page.is_created = 0U;
  g_controls_page.root_widget = NULL;
  g_controls_page.user_data = NULL;
  g_list_page.is_created = 0U;
  g_list_page.root_widget = NULL;
  g_list_page.user_data = NULL;
  g_data_page.is_created = 0U;
  g_data_page.root_widget = NULL;
  g_data_page.user_data = NULL;
  g_settings_page.is_created = 0U;
  g_settings_page.root_widget = NULL;
  g_settings_page.user_data = NULL;
}

ui_page_context_t *UI_PageRegistry_GetPage(uint16_t page_id)
{
  switch (page_id)
  {
  case UI_APP_PAGE_HOME_ID:
    return &g_home_page;
  case UI_APP_PAGE_CONTROLS_ID:
    return &g_controls_page;
  case UI_APP_PAGE_LIST_ID:
    return &g_list_page;
  case UI_APP_PAGE_DATA_ID:
    return &g_data_page;
  case UI_APP_PAGE_SETTINGS_ID:
    return &g_settings_page;
  default:
    return NULL;
  }
}
