#include "user_KeyManllegeTask.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "key.h"
#include "log.h"

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

        break;

    case KEY_EVT_LONG:

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
