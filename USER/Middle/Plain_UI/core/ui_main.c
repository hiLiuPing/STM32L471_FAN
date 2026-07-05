#include "ui_main.h"

#include "ui_event_bridge.h"
#include "ui_tick.h"
#include "ui_update.h"

#include "../app/ui_product_app.h"
#include "../datastore/ui_binding.h"
#include "../datastore/ui_model.h"
#include "../animation/ui_animation.h"
#include "../config/ui_config.h"
#include "../core/ui_dirty_region.h"
#include "../event/ui_event_bus.h"
#include "../event/ui_event_queue.h"
#include "../effects/ui_fx_emitter.h"
#include "../layer/ui_layer_manager.h"
#include "../hal/backend_lvgl/ui_renderer_adapter.h"

#include "task.h"

static StaticTask_t g_ui_task_tcb;
static StackType_t g_ui_task_stack[UI_TASK_STACK_WORDS];
static TaskHandle_t g_ui_task_handle = NULL;

static void UI_Main_TaskRun(void *argument)
{
  TickType_t last_wake_time = xTaskGetTickCount();

  (void)argument;

  /* 固定帧率主循环：先收事件，再做状态更新，最后推进系统时基。 */
  for (;;)
  {
    UI_EventBridge_Pump();
    UI_Update_RunFrame();
    UI_Tick_Advance(UI_FRAME_TICK_MS);
    vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(UI_FRAME_TICK_MS));
  }
}

BaseType_t UI_Main_Init(void)
{
  if (g_ui_task_handle != NULL)
  {
    return pdPASS;
  }

  /* 初始化顺序固定：
   * 1) 事件与数据层先就绪，确保页面一启动就能收发数据。
   * 2) 再初始化渲染器、动画、特效和图层。
   * 3) 最后装配产品 UI 页面。
   */
  if (UI_EventBus_Init() != pdPASS)
  {
    return pdFAIL;
  }

  if (UI_EventQueue_Init() != pdPASS)
  {
    return pdFAIL;
  }

  UI_Model_Init();
  UI_Binding_Init();
  UI_DirtyRegion_Init();
  UI_RendererAdapter_Init();
  UI_AnimationScheduler_Init();
  UI_FXEmitter_Init();
  UI_LayerManager_Init();
  UI_Update_Init();

  if (UI_ProductApp_Init() != pdPASS)
  {
    return pdFAIL;
  }

  g_ui_task_handle = xTaskCreateStatic(UI_Main_TaskRun,
                                       "uiTask",
                                       UI_TASK_STACK_WORDS,
                                       NULL,
                                       UI_TASK_PRIORITY,
                                       g_ui_task_stack,
                                       &g_ui_task_tcb);

  return (g_ui_task_handle != NULL) ? pdPASS : pdFAIL;
}
