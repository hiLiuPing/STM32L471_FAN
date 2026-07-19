#include "system_power.h"

#include "fan_app.h"
#include "log.h"
#include "main.h"
#include "settings_app.h"
#include "systemMonitor_app.h"
#include "task.h"
#include "user_TasksInit.h"

static volatile uint8_t s_shutdown_requested = 0U;

void SystemPower_ShutdownNow(void)
{
    if (s_shutdown_requested != 0U)
    {
        return;
    }

    s_shutdown_requested = 1U;
    UserMonitor_StopAll();
    if (FanTaskHandle != NULL) vTaskSuspend(FanTaskHandle);
    if (EGUIHandlerTaskHandle != NULL) vTaskSuspend(EGUIHandlerTaskHandle);
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
}
