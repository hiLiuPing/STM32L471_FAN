#include "user_WeatherSyncTask.h"

#include "FreeRTOS.h"
#include "log.h"
#include "rtc.h"
#include "settings_app.h"
#include "systemMonitor_app.h"
#include "system_notify.h"
#include "task.h"
#include "user_TasksInit.h"
#include "user_TransmitTask.h"
#include "weather_app.h"

#define WEATHER_SYNC_START_HOUR 6U
#define WEATHER_SYNC_END_HOUR   21U
#define WEATHER_SYNC_COMMAND_GAP_MS 200U
#define WEATHER_SYNC_ROUND_WAIT_MS  800U
#define WEATHER_SYNC_TIMEOUT_MS     30000U
#define WEATHER_FIRST_AUTO_DELAY_MS 5000U

static volatile uint8_t s_manual_request_pending = 0U;
static volatile uint8_t s_request_busy = 0U;

bool WeatherSyncTask_RequestManual(void)
{
    taskENTER_CRITICAL();
    if ((s_request_busy != 0U) || (s_manual_request_pending != 0U))
    {
        taskEXIT_CRITICAL();
        return false;
    }
    s_manual_request_pending = 1U;
    taskEXIT_CRITICAL();

    if (xWeatherSyncTaskWakeSemaphore == NULL)
    {
        taskENTER_CRITICAL();
        s_manual_request_pending = 0U;
        taskEXIT_CRITICAL();
        return false;
    }
    (void)xSemaphoreGive(xWeatherSyncTaskWakeSemaphore);
    return true;
}

static uint8_t WeatherSyncTask_TakeManual(void)
{
    uint8_t pending;

    taskENTER_CRITICAL();
    pending = s_manual_request_pending;
    s_manual_request_pending = 0U;
    taskEXIT_CRITICAL();
    return pending;
}

static uint8_t WeatherSyncTask_BeginSync(uint8_t manual_request)
{
    taskENTER_CRITICAL();
    s_request_busy = 1U;
    if (s_manual_request_pending != 0U)
    {
        manual_request = 1U;
        s_manual_request_pending = 0U;
    }
    taskEXIT_CRITICAL();
    WeatherApp_SetSyncing(1U);
    return manual_request;
}

static void WeatherSyncTask_EndSync(void)
{
    WeatherApp_SetSyncing(0U);
    taskENTER_CRITICAL();
    s_request_busy = 0U;
    taskEXIT_CRITICAL();
}

static uint8_t Weather_PrepareUartRx(const char *tag)
{
    HAL_StatusTypeDef status = uart_dma_restart_rx(&uart2_admin);

    TransmitTask_ResetProtocolState();
    if (status != HAL_OK)
    {
        log_printf("[Weather] %s rx restart fail=%u",
                   (tag != NULL) ? tag : "sync",
                   (unsigned int)status);
        return 0U;
    }
    return 1U;
}

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

static uint8_t Weather_WaitForSyncWindow(void)
{
    TickType_t delay_ticks;

    while ((delay_ticks = Weather_GetSyncWindowDelayTicks()) != 0U)
    {
        if (xSemaphoreTake(xWeatherSyncTaskWakeSemaphore, delay_ticks) == pdPASS)
        {
            if (WeatherSyncTask_TakeManual() != 0U)
            {
                return 1U;
            }
        }
    }
    return 0U;
}

static void Weather_DelayAbortable(uint32_t delay_ms)
{
    uint32_t elapsed = 0U;

    while ((elapsed < delay_ms) && (WeatherApp_IsAbortRequested() == 0U))
    {
        uint32_t step = ((delay_ms - elapsed) > 100U) ? 100U : (delay_ms - elapsed);

        vTaskDelay(pdMS_TO_TICKS(step));
        elapsed += step;
    }
}

static void Weather_SendMissingCommand(EventBits_t bits,
                                       EventBits_t bit,
                                       Weather_cmd_t command)
{
    if (((bits & bit) == 0U) && (WeatherApp_IsAbortRequested() == 0U))
    {
        Weather_SendCommand(command);
        Weather_DelayAbortable(WEATHER_SYNC_COMMAND_GAP_MS);
    }
}

static void Weather_RunSingleSyncRound(EventBits_t bits)
{
    Weather_SendMissingCommand(bits, WEATHER_SYNC_BIT_TIME, CMD_GET_TIME);
    Weather_SendMissingCommand(bits, WEATHER_SYNC_BIT_NOW, CMD_GET_NOW_DETAIL);
    Weather_SendMissingCommand(bits, WEATHER_SYNC_BIT_AIR, CMD_GET_AIR_DETAIL);
    Weather_SendMissingCommand(bits, WEATHER_SYNC_BIT_FUTURE, CMD_GET_FUTURE_7DAY);
}

static uint8_t Weather_RunSyncWithRetry(const char *tag)
{
    const TickType_t timeout_ticks = pdMS_TO_TICKS(WEATHER_SYNC_TIMEOUT_MS);
    TickType_t started = xTaskGetTickCount();
    uint16_t round = 0U;

    Weather_BeginSyncCycle();
    while ((TickType_t)(xTaskGetTickCount() - started) < timeout_ticks)
    {
        EventBits_t bits;

        if (WeatherApp_IsAbortRequested() != 0U)
        {
            break;
        }

        bits = WeatherApp_GetSyncBits();
        if ((bits & WEATHER_SYNC_BITS_REQUIRED) == WEATHER_SYNC_BITS_REQUIRED)
        {
            WeatherApp_CommitSync();
            log_printf("[Weather] %s ok %u", tag, (unsigned int)round);
            return 1U;
        }

        round++;
        Weather_RunSingleSyncRound(bits);
        bits = WeatherApp_WaitSyncBits(WEATHER_SYNC_BITS_REQUIRED,
                                       pdMS_TO_TICKS(WEATHER_SYNC_ROUND_WAIT_MS));
        if ((bits & WEATHER_SYNC_BITS_REQUIRED) == WEATHER_SYNC_BITS_REQUIRED)
        {
            WeatherApp_CommitSync();
            log_printf("[Weather] %s ok %u", tag, (unsigned int)round);
            return 1U;
        }
    }

    log_printf("[Weather] %s timeout bits=%02X",
               tag,
               (unsigned int)WeatherApp_GetSyncBits());
    return 0U;
}

static void Weather_PerformSync(const char *tag,
                                const char *mem_tag,
                                uint8_t manual_request)
{
    uint8_t sync_ok;

    manual_request = WeatherSyncTask_BeginSync(manual_request);
    Weather_PowerOn();
    (void)SystemNotify_Post(SYSTEM_NOTIFY_WEATHER_SYNC_START, 0, 0);
    sync_ok = (uint8_t)((WeatherApp_IsAbortRequested() == 0U) &&
                        (Weather_PrepareUartRx(tag) != 0U) &&
                        (Weather_RunSyncWithRetry(tag) != 0U));

    if (sync_ok != 0U)
    {
        SettingsApp_ApplyActiveBrightness();
        (void)SystemNotify_Post(SYSTEM_NOTIFY_WEATHER_SYNC_COMPLETE, 0, 0);
        MemDiag_LogSnapshot(mem_tag);
    }
    else if (WeatherApp_IsAbortRequested() == 0U)
    {
        WeatherApp_MarkSyncFailed();
        if (manual_request != 0U)
        {
            (void)SystemNotify_Post(SYSTEM_NOTIFY_WEATHER_SYNC_FAILED, 0, 0);
        }
    }

    Weather_PowerOffUnlessProvisioning();
}

static void Weather_RestartPeriodicMonitor(void)
{
    if (xWeatherSyncTaskWakeSemaphore != NULL)
    {
        while (xSemaphoreTake(xWeatherSyncTaskWakeSemaphore, 0U) == pdPASS)
        {
        }
    }
    UserMonitor_RestartWeatherSync();
    if (xWeatherSyncTaskWakeSemaphore != NULL)
    {
        while (xSemaphoreTake(xWeatherSyncTaskWakeSemaphore, 0U) == pdPASS)
        {
        }
    }
}

void WeatherSyncTask(void *argument)
{
    (void)argument;

    User_Tasks_WaitForHardwareReady();
#if defined(WEATHER_DEMO_ENABLE) && WEATHER_DEMO_ENABLE
    UserMonitor_StopWeatherSync();
    /* WeatherApp_Init() clears the cached power state after the early board
     * power-on, so drive the pin through a known on/off sequence here. */
    Weather_PowerOn();
    Weather_PowerOff();
    log_printf("[WeatherDemo] normal sync suspended");
    vTaskSuspend(NULL);
    return;
#endif
    (void)xSemaphoreTake(xWeatherSyncTaskWakeSemaphore,
                         pdMS_TO_TICKS(WEATHER_FIRST_AUTO_DELAY_MS));

    for (;;)
    {
        if (WeatherApp_IsFirstSyncDone() == 0U)
        {
            uint8_t manual_request = WeatherSyncTask_TakeManual();

            while (xSemaphoreTake(xWeatherSyncTaskWakeSemaphore, 0U) == pdPASS)
            {
            }
            Weather_PerformSync("first", "weather-first", manual_request);
            WeatherApp_SetFirstSyncDone(1U);
            Weather_RestartPeriodicMonitor();
            WeatherSyncTask_EndSync();
            continue;
        }

        (void)xSemaphoreTake(xWeatherSyncTaskWakeSemaphore, portMAX_DELAY);
        {
            uint8_t manual_request = WeatherSyncTask_TakeManual();

            UserMonitor_StopWeatherSync();
            while (xSemaphoreTake(xWeatherSyncTaskWakeSemaphore, 0U) == pdPASS)
            {
            }

            if (manual_request == 0U)
            {
                manual_request = Weather_WaitForSyncWindow();
            }

            Weather_PerformSync(manual_request != 0U ? "manual" : "sync",
                                manual_request != 0U ? "weather-manual" : "weather-sync",
                                manual_request);
            Weather_RestartPeriodicMonitor();
            WeatherSyncTask_EndSync();
        }
    }
}
