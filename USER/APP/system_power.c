#include "system_power.h"

#include "fan_app.h"
#include "log.h"
#include "main.h"
#include "settings_app.h"
#include "systemMonitor_app.h"
#include "task.h"
#include "user_TasksInit.h"
#include "weather_app.h"

typedef enum
{
    SYSTEM_POWER_IDLE = 0,
    SYSTEM_POWER_REQUESTED,
    SYSTEM_POWER_EXECUTING
} SystemPower_State_t;

static volatile SystemPower_State_t s_shutdown_state = SYSTEM_POWER_IDLE;

void SystemPower_RequestShutdown(void)
{
    taskENTER_CRITICAL();
    if (s_shutdown_state == SYSTEM_POWER_IDLE)
    {
        s_shutdown_state = SYSTEM_POWER_REQUESTED;
    }
    taskEXIT_CRITICAL();
}

void SystemPower_Service(void)
{
    TaskHandle_t current_task;

    taskENTER_CRITICAL();
    if (s_shutdown_state != SYSTEM_POWER_REQUESTED)
    {
        taskEXIT_CRITICAL();
        return;
    }
    s_shutdown_state = SYSTEM_POWER_EXECUTING;
    taskEXIT_CRITICAL();

    current_task = xTaskGetCurrentTaskHandle();
    UserMonitor_StopAll();
    Weather_RequestAbortForSleep();
    if ((FanTaskHandle != NULL) && (FanTaskHandle != current_task))
    {
        vTaskSuspend(FanTaskHandle);
    }
    if ((EGUIHandlerTaskHandle != NULL) && (EGUIHandlerTaskHandle != current_task))
    {
        vTaskSuspend(EGUIHandlerTaskHandle);
    }
    FanApp_ForceStop();
    if (!FanApp_PersistNow())
    {
        log_printf("[Power] fan persist fail");
    }
    if (!SettingsApp_PersistNow())
    {
        log_printf("[Power] settings persist fail");
    }
    __DSB();
    HAL_GPIO_WritePin(ARM_RST_GPIO_Port, ARM_RST_Pin, GPIO_PIN_SET);
    __DSB();
    __ISB();

    for (;;)
    {
        vTaskSuspend(NULL);
    }
}
