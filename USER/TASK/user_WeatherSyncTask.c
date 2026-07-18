#include "user_WeatherSyncTask.h"

#include "FreeRTOS.h"
#include "log.h"
#include "rtc.h"
#include "settings_app.h"
#include "systemMonitor_app.h"
#include "system_notify.h"
#include "task.h"
#include "user_TasksInit.h"
#include "weather_app.h"

#define WEATHER_SYNC_START_HOUR 6U
#define WEATHER_SYNC_END_HOUR   21U

static TickType_t Weather_GetSyncWindowDelayTicks(void)
{
    RTC_TimeTypeDef time = {0};
    RTC_DateTypeDef date = {0};
    uint32_t delay_seconds;

    if ((HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN) != HAL_OK) ||
        (HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN) != HAL_OK))
    {
        /* Weather sync is also responsible for establishing valid RTC time. */
        return 0U;
    }

    if (date.Year < 20U)
    {
        return 0U;
    }

    if ((time.Hours >= WEATHER_SYNC_START_HOUR) &&
        (time.Hours < WEATHER_SYNC_END_HOUR))
    {
        return 0U;
    }

    if (time.Hours < WEATHER_SYNC_START_HOUR)
    {
        delay_seconds = ((uint32_t)(WEATHER_SYNC_START_HOUR - time.Hours) * 3600U) -
                        ((uint32_t)time.Minutes * 60U) -
                        (uint32_t)time.Seconds;
    }
    else
    {
        delay_seconds = ((24U - (uint32_t)time.Hours + WEATHER_SYNC_START_HOUR) * 3600U) -
                        ((uint32_t)time.Minutes * 60U) -
                        (uint32_t)time.Seconds;
    }

    return (TickType_t)delay_seconds * pdMS_TO_TICKS(1000U);
}

static void Weather_WaitForSyncWindow(void)
{
    TickType_t delay_ticks;

    while ((delay_ticks = Weather_GetSyncWindowDelayTicks()) != 0U)
    {
        (void)xSemaphoreTake(xWeatherSyncTaskWakeSemaphore, delay_ticks);
    }
}

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
            uint8_t sync_ok;

            Weather_WaitForSyncWindow();
            Weather_PowerOn();
            (void)SystemNotify_Post(SYSTEM_NOTIFY_WEATHER_SYNC_START, 0, 0);
            Weather_DelayAbortable(6000U);
            sync_ok = Weather_RunSyncWithRetry("first");
            
            if (sync_ok != 0U)
            {
                SettingsApp_ApplyActiveBrightness();
                (void)SystemNotify_Post(SYSTEM_NOTIFY_WEATHER_SYNC_COMPLETE, 0, 0);
                MemDiag_LogSnapshot("weather-first");
            }
            Weather_PowerOff();
            g_weather_module.first_sync_done = 1U;
            continue;
        }

        {
            TickType_t interval_ticks = (TickType_t)SettingsApp_GetWeatherTimeSyncIntervalMin() *
                                        60U * pdMS_TO_TICKS(1000U);
            (void)xSemaphoreTake(xWeatherSyncTaskWakeSemaphore, interval_ticks);
        }

        Weather_WaitForSyncWindow();

        if (g_weather_module.syncing != 0U)
        {
            continue;
        }

        g_weather_module.syncing = 1U;
        Weather_PowerOn();
        (void)SystemNotify_Post(SYSTEM_NOTIFY_WEATHER_SYNC_START, 0, 0);
        Weather_DelayAbortable(6000U);
        if (g_weather_module.abort_requested == 0U)
        {
            if (Weather_RunSyncWithRetry("sync") != 0U)
            {
                SettingsApp_ApplyActiveBrightness();
                (void)SystemNotify_Post(SYSTEM_NOTIFY_WEATHER_SYNC_COMPLETE, 0, 0);
                MemDiag_LogSnapshot("weather-sync");
            }
        }
        Weather_PowerOff();
        g_weather_module.syncing = 0U;
    }
}
