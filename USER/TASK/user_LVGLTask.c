#include "user_LVGLTask.h"

#include "FreeRTOS.h"
#include "task.h"

#include "lvgl.h"
#include "user_TasksInit.h"

void LvHandlerTask(void *argument)
{
    (void)argument;

    User_Tasks_WaitForHardwareReady();

    for (;;)
    {
        uint32_t delay_ms = lv_timer_handler();

        if (delay_ms == 0U)
        {
            delay_ms = 1U;
        }
        else if (delay_ms > 30U)
        {
            delay_ms = 30U;
        }

        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}
