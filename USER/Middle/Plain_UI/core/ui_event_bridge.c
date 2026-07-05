/* 把输入队列和消息总线统一泵到页面管理器。 */
#include "ui_event_bridge.h"

#include "../event/ui_event_bus.h"
#include "../event/ui_event_queue.h"

#include "../navigation/ui_page_manager.h"

void UI_EventBridge_Pump(void)
{
  ui_msg_t msg;
  ui_event_t event;

  /* 先处理输入事件，再处理消息事件，保证用户操作优先响应。 */
  while (UI_EventQueue_Receive(&event, 0U) == pdPASS)
  {
    UI_PageManager_DispatchEvent(&event);
  }

  while (UI_EventBus_Receive(&msg, 0U) == pdPASS)
  {
    UI_PageManager_HandleMessage(&msg);
  }
}
