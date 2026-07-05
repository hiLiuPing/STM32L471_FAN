#include "ui_page_manager.h"

#include "ui_focus_engine.h"
#include "ui_navigation.h"
#include "../popup/ui_modal_manager.h"

static ui_page_factory_fn g_page_factory = NULL;

static void UI_PageManager_SyncFocusRoot(void)
{
  ui_page_context_t *active = UI_Navigation_GetActive();
  /* 焦点根节点跟随当前页同步，避免页面切换后焦点还停在旧树上。 */
  UI_FocusEngine_SetRoot((active != NULL) ? active->root_widget : NULL);
}

void UI_PageManager_Init(ui_page_factory_fn factory)
{
  g_page_factory = factory;
  UI_Navigation_Init();
  UI_FocusEngine_Init();
  UI_ModalManager_Init();
}

BaseType_t UI_PageManager_Open(uint16_t page_id)
{
  return UI_PageManager_Replace(page_id);
}

BaseType_t UI_PageManager_Back(void)
{
  return UI_PageManager_Pop();
}

void UI_PageManager_Update(void)
{
  UI_PageManager_ProcessActivePage();
}

void UI_PageManager_HandleEvent(const ui_event_t *event)
{
  UI_PageManager_DispatchEvent(event);
}

BaseType_t UI_PageManager_OpenRoot(uint16_t page_id)
{
  ui_page_context_t *page;

  if (g_page_factory == NULL)
  {
    return pdFAIL;
  }

  page = g_page_factory(page_id);
  if (page == NULL)
  {
    return pdFAIL;
  }

  if (UI_Navigation_Push(page) != pdPASS)
  {
    return pdFAIL;
  }

  UI_PageManager_SyncFocusRoot();
  return pdPASS;
}

BaseType_t UI_PageManager_Push(uint16_t page_id)
{
  ui_page_context_t *page;

  if (g_page_factory == NULL)
  {
    return pdFAIL;
  }

  page = g_page_factory(page_id);
  if (page == NULL)
  {
    return pdFAIL;
  }

  if (UI_Navigation_Push(page) != pdPASS)
  {
    return pdFAIL;
  }

  UI_PageManager_SyncFocusRoot();
  return pdPASS;
}

BaseType_t UI_PageManager_Replace(uint16_t page_id)
{
  ui_page_context_t *page;

  if (g_page_factory == NULL)
  {
    return pdFAIL;
  }

  page = g_page_factory(page_id);
  if (page == NULL)
  {
    return pdFAIL;
  }

  if (UI_Navigation_Replace(page) != pdPASS)
  {
    return pdFAIL;
  }

  UI_PageManager_SyncFocusRoot();
  return pdPASS;
}

BaseType_t UI_PageManager_Pop(void)
{
  BaseType_t result = UI_Navigation_Pop();

  UI_PageManager_SyncFocusRoot();
  return result;
}

void UI_PageManager_HandleMessage(const ui_msg_t *msg)
{
  ui_page_context_t *active = UI_Navigation_GetActive();

  if ((active != NULL) && (active->handle_message != NULL))
  {
    active->handle_message(active, msg);
  }
}

void UI_PageManager_DispatchEvent(const ui_event_t *event)
{
  ui_page_context_t *active = UI_Navigation_GetActive();

  /* 模态弹窗优先拦截事件，其次是焦点层，最后才到页面本体。 */
  if (UI_ModalManager_HandleEvent(event) == pdPASS)
  {
    return;
  }

  if (UI_FocusEngine_HandleEvent(event) == pdPASS)
  {
    return;
  }

  if ((active != NULL) && (active->handle_event != NULL))
  {
    active->handle_event(active, event);
  }
}

void UI_PageManager_ProcessActivePage(void)
{
  ui_page_context_t *active = UI_Navigation_GetActive();

  if ((active != NULL) && (active->process != NULL))
  {
    active->process(active);
  }
}

void UI_PageManager_DrawActivePage(void)
{
  ui_page_context_t *active = UI_Navigation_GetActive();

  if ((active != NULL) && (active->draw != NULL))
  {
    active->draw(active);
  }
}

ui_page_context_t *UI_PageManager_GetActivePage(void)
{
  return UI_Navigation_GetActive();
}
