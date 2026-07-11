#include "user_KeyManllegeTask.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "key.h"
#include "log.h"
#include "egui_port_stm32l471_fan.h"

#include "user_TasksInit.h"



static void KeyManager_Dispatch(const key_event_t *key_event)
{
    if (key_event == NULL)
    {
        return;
    }

    switch (key_event->type)
    {
    case KEY_EVT_CLICK:
    case KEY_EVT_LONG:
    case KEY_EVT_REPEAT:
        egui_port_handle_key_event(key_event);
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
