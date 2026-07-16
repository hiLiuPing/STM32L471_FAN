#include "user_FanTask.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "fan_app.h"
#include "user_TasksInit.h"

#define FAN_TASK_PERIOD_MS 50U

void FanTask(void *argument)
{
    fan_cmd_t cmd;

    (void)argument;

    User_Tasks_WaitForHardwareReady();

    for (;;)
    {
        if (xQueueReceive(Fan_Command_queue, &cmd, pdMS_TO_TICKS(FAN_TASK_PERIOD_MS)) == pdPASS)
        {
            FanApp_HandleCommand(&cmd);

            while (xQueueReceive(Fan_Command_queue, &cmd, 0U) == pdPASS)
            {
                FanApp_HandleCommand(&cmd);
            }
        }

        {
            TickType_t now = xTaskGetTickCount();

            FanApp_Service(now);
            FanApp_PersistPending(now);
        }
    }
}
