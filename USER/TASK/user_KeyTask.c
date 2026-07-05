#include "user_KeyTask.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "key.h"
#include "log.h"
#include "user_TasksInit.h"

static void KeyTask_SendEvent(const key_event_t *event)
{
    static uint32_t s_key_drop_count = 0U;

    if ((Key_Power_queue == NULL) || (event == NULL))
    {
        return;
    }

    if (xQueueSend(Key_Power_queue, event, 0U) != pdPASS)
    {
        s_key_drop_count++;
        if ((s_key_drop_count & 0x07U) == 0U)
        {
            log_printf("[KeyQ] dropped=%lu\r\n", (unsigned long)s_key_drop_count);
        }
    }
}

void KeyTask(void *argument)
{
    key_event_t key_event;

    (void)argument;

    User_Tasks_WaitForHardwareReady();

    for (;;)
    {
        if (Key_Scan(&key_event))
        {
            KeyTask_SendEvent(&key_event);
        }

        vTaskDelay(pdMS_TO_TICKS(10U));
    }
}
