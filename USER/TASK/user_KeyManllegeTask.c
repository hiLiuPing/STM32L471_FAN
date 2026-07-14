#include "user_KeyManllegeTask.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "key.h"
#include "log.h"

#include "user_TasksInit.h"



static void KeyManager_Dispatch(const key_event_t *key_event)
{
    static uint32_t s_egui_key_drop_count = 0U;

    if (key_event == NULL)
    {
        return;
    }

    switch (key_event->type)
    {
    case KEY_EVT_CLICK:
    case KEY_EVT_LONG:
    case KEY_EVT_REPEAT:
        if ((EGUI_Key_queue != NULL) && (xQueueSend(EGUI_Key_queue, key_event, 0U) != pdPASS))
        {
            s_egui_key_drop_count++;
            if ((s_egui_key_drop_count & 0x07U) == 0U)
            {
                log_printf("[EGUIKeyQ] dropped=%lu\r\n", (unsigned long)s_egui_key_drop_count);
            }
        }
        break;

    default:
        break;
    }
}

void KeyManllegeTask(void *argument)
{
    key_event_t key_event;

    (void)argument;

    User_Tasks_WaitForHardwareReady();

    for (;;)
    {
        if (xQueueReceive(Key_Power_queue, &key_event, portMAX_DELAY) == pdPASS)
        {
            KeyManager_Dispatch(&key_event);
        }
    }
}
