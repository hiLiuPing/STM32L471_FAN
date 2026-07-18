#include "user_EGUITask.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "egui_port_stm32l471_fan.h"
#include "key.h"
#include "page_manager.h"
#include "system_notify.h"
#include "ui_shutdown_popup.h"
#include "ui_system_popup.h"
#include "user_TasksInit.h"

static void EGUIHandlerTask_DispatchQueuedKeys(void)
{
    key_event_t key_event;

    if (EGUI_Key_queue == NULL)
    {
        return;
    }

    while (xQueueReceive(EGUI_Key_queue, &key_event, 0U) == pdPASS)
    {
        egui_port_handle_key_event(&key_event);
        if (!egui_port_is_display_on())
        {
            return;
        }
    }
}

static void EGUIHandlerTask_WaitForWakeKey(void)
{
    key_event_t key_event;

    if ((EGUI_Key_queue != NULL) &&
        (xQueueReceive(EGUI_Key_queue, &key_event, portMAX_DELAY) == pdPASS))
    {
        egui_port_handle_key_event(&key_event);
    }
}

static void EGUIHandlerTask_DispatchSystemNotification(void)
{
    SystemNotifyMessage_t message;

    if (ui_page_manager_is_startup_active() ||
        ui_shutdown_popup_is_active() ||
        ui_system_popup_is_visible())
    {
        return;
    }

    if (SystemNotify_TryReceive(&message))
    {
        ui_system_popup_show(&message);
    }
}

void EGUIHandlerTask(void *argument)
{
    (void)argument;

    User_Tasks_WaitForHardwareReady();

    for (;;)
    {
        if (!egui_port_is_display_on())
        {
            EGUIHandlerTask_WaitForWakeKey();
            continue;
        }

        EGUIHandlerTask_DispatchQueuedKeys();
        if (!egui_port_is_display_on())
        {
            continue;
        }

        EGUIHandlerTask_DispatchSystemNotification();
        egui_port_poll();
        if (!egui_port_is_display_on())
        {
            continue;
        }

        vTaskDelay(pdMS_TO_TICKS(5U));
    }
}
