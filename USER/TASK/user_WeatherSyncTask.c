#include "user_WeatherSyncTask.h"

#include "FreeRTOS.h"
#include "log.h"
#include "settings_app.h"
#include "task.h"
#include "user_TasksInit.h"
#include "weather_app.h"

static void Weather_RunSingleSyncRound(void)
{
    if (g_weather_module.abort_requested != 0U)
    {
        return;
    }

    Weather_BeginSyncCycle();
    Weather_SendCommand(CMD_GET_TIME);
    vTaskDelay(pdMS_TO_TICKS(200U));
    Weather_SendCommand(CMD_GET_NOW_DETAIL);
    vTaskDelay(pdMS_TO_TICKS(200U));
    Weather_SendCommand(CMD_GET_AIR_DETAIL);
    vTaskDelay(pdMS_TO_TICKS(200U));
    Weather_SendCommand(CMD_GET_FUTURE_7DAY);
    vTaskDelay(pdMS_TO_TICKS(200U));
}

static void Weather_DelayAbortable(uint32_t delay_ms)
{
    uint32_t elapsed = 0U;

    while ((elapsed < delay_ms) && (g_weather_module.abort_requested == 0U))
    {
        uint32_t step = ((delay_ms - elapsed) > 100U) ? 100U : (delay_ms - elapsed);

        vTaskDelay(pdMS_TO_TICKS(step));
        elapsed += step;
    }
}

static uint8_t Weather_RunSyncWithRetry(const char *tag)
{
    for (uint8_t retry = 0U; retry < 30U; retry++)
    {
        if (g_weather_module.abort_requested != 0U)
        {
            break;
        }

        Weather_RunSingleSyncRound();
        if (Weather_HasCompletedSync())
        {
            log_printf("[Weather] %s ok %u", tag, retry + 1U);
            return 1U;
        }
    }

    log_printf("[Weather] %s timeout", tag);
    return 0U;
}

void WeatherSyncTask(void *argument)
{
    (void)argument;

    User_Tasks_WaitForHardwareReady();
    vTaskDelay(pdMS_TO_TICKS(10000U));

    for (;;)
    {
        if (g_weather_module.first_sync_done == 0U)
        {
            Weather_PowerOn();
            Weather_DelayAbortable(6000U);
            (void)Weather_RunSyncWithRetry("first");
            Weather_PowerOff();
            g_weather_module.first_sync_done = 1U;
            continue;
        }

        {
            uint32_t interval_ms = (uint32_t)SettingsApp_GetWeatherTimeSyncIntervalMin() * 60U * 1000U;
            (void)xSemaphoreTake(xWeatherSyncTaskWakeSemaphore, pdMS_TO_TICKS(interval_ms));
        }

        if (g_weather_module.syncing != 0U)
        {
            continue;
        }

        g_weather_module.syncing = 1U;
        Weather_PowerOn();
        Weather_DelayAbortable(6000U);
        if (g_weather_module.abort_requested == 0U)
        {
            (void)Weather_RunSyncWithRetry("sync");
        }
        Weather_PowerOff();
        g_weather_module.syncing = 0U;
    }
}
