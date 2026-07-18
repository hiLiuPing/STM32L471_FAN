#include "user_KeyManllegeTask.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "key.h"
#include "led_app.h"
#include "log.h"

#include "user_TasksInit.h"

#define KEY_MANAGER_PWR_DOUBLE_CLICK_MS 300U

static void KeyManager_SendToUI(const key_event_t *key_event)
{
    static uint32_t s_egui_key_drop_count = 0U;

    if (key_event == NULL)
    {
        return;
    }

    switch (key_event->type)
    {
    case KEY_EVT_CLICK:
    case KEY_EVT_DOUBLE_CLICK:
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
    key_event_t pending_pwr_click;
    TickType_t pwr_click_tick = 0U;
    const TickType_t double_click_ticks = pdMS_TO_TICKS(KEY_MANAGER_PWR_DOUBLE_CLICK_MS);
    BaseType_t pwr_click_pending = pdFALSE;

    (void)argument;

    User_Tasks_WaitForHardwareReady();

    for (;;)
    {
        TickType_t wait_ticks = portMAX_DELAY;

        if (pwr_click_pending != pdFALSE)
        {
            TickType_t elapsed = xTaskGetTickCount() - pwr_click_tick;

            if (elapsed >= double_click_ticks)
            {
                pwr_click_pending = pdFALSE;
                KeyManager_SendToUI(&pending_pwr_click);
                continue;
            }
            wait_ticks = double_click_ticks - elapsed;
        }

        if (xQueueReceive(Key_Power_queue, &key_event, wait_ticks) != pdPASS)
        {
            if (pwr_click_pending != pdFALSE)
            {
                pwr_click_pending = pdFALSE;
                KeyManager_SendToUI(&pending_pwr_click);
            }
            continue;
        }

        if (key_event.type == KEY_EVT_CLICK)
        {
            LED_CMD_OnEvent(LED_TARGET_SW, LED_EVT_FLASH_BLUE);
        }

        if ((key_event.id == KEY_ID_PWR) && (key_event.type == KEY_EVT_CLICK))
        {
            TickType_t now = xTaskGetTickCount();

            if ((pwr_click_pending != pdFALSE) &&
                ((TickType_t)(now - pwr_click_tick) < double_click_ticks))
            {
                key_event_t double_click_event = {
                    .id = KEY_ID_PWR,
                    .type = KEY_EVT_DOUBLE_CLICK,
                };

                pwr_click_pending = pdFALSE;
                KeyManager_SendToUI(&double_click_event);
                continue;
            }

            if (pwr_click_pending != pdFALSE)
            {
                pwr_click_pending = pdFALSE;
                KeyManager_SendToUI(&pending_pwr_click);
            }

            pending_pwr_click = key_event;
            pwr_click_tick = now;
            pwr_click_pending = pdTRUE;
            continue;
        }

        if (pwr_click_pending != pdFALSE)
        {
            pwr_click_pending = pdFALSE;
            KeyManager_SendToUI(&pending_pwr_click);
        }

        KeyManager_SendToUI(&key_event);
    }
}
