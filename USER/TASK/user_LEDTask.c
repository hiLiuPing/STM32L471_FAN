#include "user_LEDTask.h"

#include "FreeRTOS.h"
#include "task.h"

#include "led_app.h"
#include "multi_led.h"
#include "user_TasksInit.h"

void LEDTask(void *pvParameters)
{
    const uint32_t loop_delay_ms = 10U;
    const uint32_t max_elapsed_ms = 50U;
    TickType_t last_wake_tick;
    TickType_t previous_update_tick;

    (void)pvParameters;
    User_Tasks_WaitForHardwareReady();
    last_wake_tick = xTaskGetTickCount();
    previous_update_tick = last_wake_tick - pdMS_TO_TICKS(loop_delay_ms);

    for (;;)
    {
        TickType_t now = xTaskGetTickCount();
        TickType_t elapsed_ticks = now - previous_update_tick;
        uint32_t elapsed_ms = ((uint32_t)elapsed_ticks * 1000U) / configTICK_RATE_HZ;

        previous_update_tick = now;
        if (elapsed_ms == 0U) elapsed_ms = 1U;
        if (elapsed_ms > max_elapsed_ms) elapsed_ms = max_elapsed_ms;
        LED_Driver_Update();
        LED_App_Update(elapsed_ms);
        vTaskDelayUntil(&last_wake_tick, pdMS_TO_TICKS(loop_delay_ms));
    }
}
