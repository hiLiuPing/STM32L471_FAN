#include "system_power.h"

#include "fan_app.h"
#include "main.h"
#include "systemMonitor_app.h"

static volatile uint8_t s_shutdown_requested = 0U;

void SystemPower_ShutdownNow(void)
{
    if (s_shutdown_requested != 0U)
    {
        return;
    }

    s_shutdown_requested = 1U;
    UserMonitor_StopAll();
    FanApp_ForceStop();
    __DSB();
    HAL_GPIO_WritePin(ARM_RST_GPIO_Port, ARM_RST_Pin, GPIO_PIN_SET);
    __DSB();
    __ISB();
}
