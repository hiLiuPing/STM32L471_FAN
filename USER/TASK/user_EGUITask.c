#include "user_EGUITask.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "egui_port_stm32l471_fan.h"
#include "key.h"
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
    }
}

void EGUIHandlerTask(void *argument)
{
    (void)argument;

    User_Tasks_WaitForHardwareReady();

    for (;;)
    {
        EGUIHandlerTask_DispatchQueuedKeys();
        egui_port_poll();
        vTaskDelay(pdMS_TO_TICKS(5U));
    }
}
