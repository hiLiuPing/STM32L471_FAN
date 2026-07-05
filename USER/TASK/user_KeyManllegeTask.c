#include "user_KeyManllegeTask.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "key.h"
#include "log.h"
#include "../Middle/Plain_UI/event/ui_event_queue.h"
#include "user_TasksInit.h"

static ui_event_type_t KeyManager_MapClickEvent(key_id_t key_id)
{
    switch (key_id)
    {
    case KEY_ID_N:
        return UI_KEY_UP;

    case KEY_ID_R:
        return UI_KEY_DOWN;

    case KEY_ID_L:
        return UI_KEY_LEFT;

    case KEY_ID_B:
        return UI_KEY_ENTER;

    default:
        return UI_EVENT_NONE;
    }
}

static void KeyManager_Publish(ui_event_type_t type, const key_event_t *key_event)
{
    ui_event_t ui_event;

    if ((type == UI_EVENT_NONE) || (key_event == NULL))
    {
        return;
    }

    ui_event.type = type;
    ui_event.param0 = (int32_t)key_event->id;
    ui_event.param1 = (int32_t)key_event->type;

    if (UI_EventQueue_Publish(&ui_event) != pdPASS)
    {
        log_printf("[UIKeyQ] drop type=%d key=%d\r\n", (int)type, (int)key_event->id);
    }
}

static void KeyManager_Dispatch(const key_event_t *key_event)
{
    if (key_event == NULL)
    {
        return;
    }

    switch (key_event->type)
    {
    case KEY_EVT_CLICK:
        KeyManager_Publish(KeyManager_MapClickEvent(key_event->id), key_event);
        break;

    case KEY_EVT_LONG:
        if (key_event->id == KEY_ID_B)
        {
            KeyManager_Publish(UI_KEY_BACK, key_event);
        }
        else
        {
            KeyManager_Publish(UI_KEY_LONG_PRESS, key_event);
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
