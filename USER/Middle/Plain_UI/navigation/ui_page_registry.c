#include "ui_page_registry.h"

#include "../app/ui_app_state.h"
#include "../app/page_clock_settings/page_clock_settings.h"
#include "../app/page_fan_settings/page_fan_settings.h"
#include "../app/page_fan_status/page_fan_status.h"
#include "../app/page_home/page_home.h"
#include "../app/page_travel_clock/page_travel_clock.h"
#include "../config/ui_config.h"

static void UI_PageRegistry_PageCreate(ui_page_context_t *page)
{
  ui_obj_t *root = NULL;
  ui_obj_t *screen = UI_Widget_GetRootScreen();

  if (page == NULL)
  {
    return;
  }

  UI_AppState_SyncViewport();

  switch (page->page_id)
  {
  case UI_APP_PAGE_TRAVEL_CLOCK_ID:
    root = UI_PageTravelClock_Create(screen);
    break;
  case UI_APP_PAGE_FAN_STATUS_ID:
    root = UI_PageFanStatus_Create(screen);
    break;
  case UI_APP_PAGE_CLOCK_SETTINGS_ID:
    root = UI_PageClockSettings_Create(screen);
    break;
  case UI_APP_PAGE_FAN_SETTINGS_ID:
    root = UI_PageFanSettings_Create(screen);
    break;
  case UI_APP_PAGE_HOME_ID:
  default:
    root = UI_PageHome_Create(screen);
    break;
  }

  page->user_data = root;
}

static void UI_PageRegistry_PageEnter(ui_page_context_t *page)
{
  ui_app_state_t *state;

  if (page == NULL)
  {
    return;
  }

  state = UI_AppState_Get();
  state->active_page_index = UI_AppState_FindPageIndex(page->page_id);

  switch (page->page_id)
  {
  case UI_APP_PAGE_TRAVEL_CLOCK_ID:
    UI_PageTravelClock_Enter();
    break;
  case UI_APP_PAGE_FAN_STATUS_ID:
    UI_PageFanStatus_Enter();
    break;
  case UI_APP_PAGE_CLOCK_SETTINGS_ID:
    UI_PageClockSettings_Enter();
    break;
  case UI_APP_PAGE_FAN_SETTINGS_ID:
    UI_PageFanSettings_Enter();
    break;
  case UI_APP_PAGE_HOME_ID:
  default:
    UI_PageHome_Enter();
    break;
  }
}

static void UI_PageRegistry_PageProcess(ui_page_context_t *page)
{
  if (page == NULL)
  {
    return;
  }

  UI_AppState_OnFrame();

  switch (page->page_id)
  {
  case UI_APP_PAGE_TRAVEL_CLOCK_ID:
    UI_PageTravelClock_Update(UI_FRAME_TICK_MS);
    break;
  case UI_APP_PAGE_FAN_STATUS_ID:
    UI_PageFanStatus_Update(UI_FRAME_TICK_MS);
    break;
  case UI_APP_PAGE_CLOCK_SETTINGS_ID:
    UI_PageClockSettings_Update(UI_FRAME_TICK_MS);
    break;
  case UI_APP_PAGE_FAN_SETTINGS_ID:
    UI_PageFanSettings_Update(UI_FRAME_TICK_MS);
    break;
  case UI_APP_PAGE_HOME_ID:
  default:
    UI_PageHome_Update(UI_FRAME_TICK_MS);
    break;
  }
}

static void UI_PageRegistry_PageExit(ui_page_context_t *page)
{
  if (page == NULL)
  {
    return;
  }

  switch (page->page_id)
  {
  case UI_APP_PAGE_TRAVEL_CLOCK_ID:
    UI_PageTravelClock_Exit();
    break;
  case UI_APP_PAGE_FAN_STATUS_ID:
    UI_PageFanStatus_Exit();
    break;
  case UI_APP_PAGE_CLOCK_SETTINGS_ID:
    UI_PageClockSettings_Exit();
    break;
  case UI_APP_PAGE_FAN_SETTINGS_ID:
    UI_PageFanSettings_Exit();
    break;
  case UI_APP_PAGE_HOME_ID:
  default:
    UI_PageHome_Exit();
    break;
  }
}

static void UI_PageRegistry_PageHandleMessage(ui_page_context_t *page, const ui_msg_t *msg)
{
  (void)page;
  UI_AppState_OnMessage(msg);
}

static void UI_PageRegistry_PageHandleEvent(ui_page_context_t *page, const ui_event_t *event)
{
  if (page == NULL)
  {
    return;
  }

  switch (page->page_id)
  {
  case UI_APP_PAGE_TRAVEL_CLOCK_ID:
    UI_PageTravelClock_HandleEvent(event);
    break;
  case UI_APP_PAGE_FAN_STATUS_ID:
    UI_PageFanStatus_HandleEvent(event);
    break;
  case UI_APP_PAGE_CLOCK_SETTINGS_ID:
    UI_PageClockSettings_HandleEvent(event);
    break;
  case UI_APP_PAGE_FAN_SETTINGS_ID:
    UI_PageFanSettings_HandleEvent(event);
    break;
  case UI_APP_PAGE_HOME_ID:
  default:
    UI_PageHome_HandleEvent(event);
    break;
  }

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
  NULL,
  UI_PageRegistry_PageExit,
  NULL,
  UI_PageRegistry_PageHandleMessage,
  UI_PageRegistry_PageHandleEvent
};

static ui_page_context_t g_travel_clock_page =
{
  UI_APP_PAGE_TRAVEL_CLOCK_ID,
  0U,
  NULL,
  NULL,
  UI_PageRegistry_PageCreate,
  UI_PageRegistry_PageEnter,
  UI_PageRegistry_PageProcess,
  NULL,
  UI_PageRegistry_PageExit,
  NULL,
  UI_PageRegistry_PageHandleMessage,
  UI_PageRegistry_PageHandleEvent
};

static ui_page_context_t g_fan_status_page =
{
  UI_APP_PAGE_FAN_STATUS_ID,
  0U,
  NULL,
  NULL,
  UI_PageRegistry_PageCreate,
  UI_PageRegistry_PageEnter,
  UI_PageRegistry_PageProcess,
  NULL,
  UI_PageRegistry_PageExit,
  NULL,
  UI_PageRegistry_PageHandleMessage,
  UI_PageRegistry_PageHandleEvent
};

static ui_page_context_t g_clock_settings_page =
{
  UI_APP_PAGE_CLOCK_SETTINGS_ID,
  0U,
  NULL,
  NULL,
  UI_PageRegistry_PageCreate,
  UI_PageRegistry_PageEnter,
  UI_PageRegistry_PageProcess,
  NULL,
  UI_PageRegistry_PageExit,
  NULL,
  UI_PageRegistry_PageHandleMessage,
  UI_PageRegistry_PageHandleEvent
};

static ui_page_context_t g_fan_settings_page =
{
  UI_APP_PAGE_FAN_SETTINGS_ID,
  0U,
  NULL,
  NULL,
  UI_PageRegistry_PageCreate,
  UI_PageRegistry_PageEnter,
  UI_PageRegistry_PageProcess,
  NULL,
  UI_PageRegistry_PageExit,
  NULL,
  UI_PageRegistry_PageHandleMessage,
  UI_PageRegistry_PageHandleEvent
};

void UI_PageRegistry_Init(void)
{
  g_home_page.is_created = 0U;
  g_home_page.user_data = NULL;
  g_travel_clock_page.is_created = 0U;
  g_travel_clock_page.user_data = NULL;
  g_fan_status_page.is_created = 0U;
  g_fan_status_page.user_data = NULL;
  g_clock_settings_page.is_created = 0U;
  g_clock_settings_page.user_data = NULL;
  g_fan_settings_page.is_created = 0U;
  g_fan_settings_page.user_data = NULL;
}

ui_page_context_t *UI_PageRegistry_GetPage(uint16_t page_id)
{
  switch (page_id)
  {
  case UI_APP_PAGE_HOME_ID:
    return &g_home_page;
  case UI_APP_PAGE_TRAVEL_CLOCK_ID:
    return &g_travel_clock_page;
  case UI_APP_PAGE_FAN_STATUS_ID:
    return &g_fan_status_page;
  case UI_APP_PAGE_CLOCK_SETTINGS_ID:
    return &g_clock_settings_page;
  case UI_APP_PAGE_FAN_SETTINGS_ID:
    return &g_fan_settings_page;
  default:
    return NULL;
  }
}
