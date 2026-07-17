#include "user_FanTask.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "fan_app.h"
#include "system_notify.h"
#include "user_TasksInit.h"

#define FAN_TASK_PERIOD_MS 50U

static void FanTask_NotifyPowerChange(uint8_t *previous_power_on, uint8_t suppress_notify)
{
    fan_state_t state;

    FanApp_GetState(&state);
    if (state.power_on == *previous_power_on)
    {
        return;
    }

    *previous_power_on = state.power_on;
    if (suppress_notify == 0U)
    {
        (void)SystemNotify_Post(state.power_on ? SYSTEM_NOTIFY_FAN_ON : SYSTEM_NOTIFY_FAN_OFF, 0, 0);
    }
}

void FanTask(void *argument)
{
    fan_cmd_t cmd;
    fan_state_t initial_state;
    uint8_t previous_power_on;

    (void)argument;

    User_Tasks_WaitForHardwareReady();
    FanApp_GetState(&initial_state);
    previous_power_on = initial_state.power_on;

    for (;;)
    {
        if (xQueueReceive(Fan_Command_queue, &cmd, pdMS_TO_TICKS(FAN_TASK_PERIOD_MS)) == pdPASS)
        {
            FanApp_HandleCommand(&cmd);
            FanTask_NotifyPowerChange(&previous_power_on,
                                      (uint8_t)((cmd.flags & FAN_CMD_FLAG_SUPPRESS_POWER_NOTIFY) != 0U));

            while (xQueueReceive(Fan_Command_queue, &cmd, 0U) == pdPASS)
            {
                FanApp_HandleCommand(&cmd);
                FanTask_NotifyPowerChange(&previous_power_on,
                                          (uint8_t)((cmd.flags & FAN_CMD_FLAG_SUPPRESS_POWER_NOTIFY) != 0U));
            }
        }

        {
            TickType_t now = xTaskGetTickCount();

            FanApp_Service(now);
            FanTask_NotifyPowerChange(&previous_power_on, 0U);
            FanApp_PersistPending(now);
        }
    }
}
