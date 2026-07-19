#include "systemMonitor_app.h"

#include <stddef.h>

#include "FreeRTOS.h"
#include "fan_app.h"
#include "log.h"
#include "settings_app.h"
#include "system_power.h"
#include "system_notify.h"
#include "task.h"
#include "time_utils.h"
#include "timers.h"
#include "user_EGUITask.h"
#include "user_TasksInit.h"
#include "weather_app.h"

#define USER_MONITOR_RETRY_MS 100U
#define USER_MONITOR_RETRY_SCREEN_APPLY   (1U << 0)
#define USER_MONITOR_RETRY_SCREEN_RESTART (1U << 1)
#define USER_MONITOR_RETRY_WEATHER_APPLY  (1U << 2)
#define USER_MONITOR_RETRY_WEATHER_STOP   (1U << 3)
#define USER_MONITOR_RETRY_SYSTEM_APPLY   (1U << 4)
#define USER_MONITOR_RETRY_SYSTEM_RESTART (1U << 5)
#define USER_MONITOR_RETRY_SCREEN_STOP    (1U << 6)
#define USER_MONITOR_RETRY_WEATHER_MANUAL (1U << 7)

_Static_assert(MON_APP_MAX <= MONITOR_MAX_NUM, "App monitor IDs exceed monitor storage");

static volatile uint8_t s_system_auto_off_pending = 0U;
static volatile uint8_t s_retry_mask = 0U;
static TickType_t s_retry_after_tick = 0U;
static uint16_t s_applied_screen_timeout_min = 0U;
static uint16_t s_applied_weather_interval_min = 0U;
static uint16_t s_applied_system_auto_off_min = 0U;
static UBaseType_t s_queue_peak_key_power = 0U;
static UBaseType_t s_queue_peak_egui_key = 0U;
static UBaseType_t s_queue_peak_display = 0U;
static UBaseType_t s_queue_peak_fan = 0U;

static UBaseType_t UserMonitor_TrackQueuePeak(QueueHandle_t queue, UBaseType_t *peak)
{
    UBaseType_t current = (queue != NULL) ? uxQueueMessagesWaiting(queue) : 0U;

    if ((peak != NULL) && (current > *peak)) *peak = current;
    return current;
}

static uint32_t UserMonitor_MinutesToMs(uint16_t minutes)
{
    return (uint32_t)minutes * 60U * 1000U;
}

static void UserMonitor_ScheduleRetry(uint8_t retry_bit)
{
    taskENTER_CRITICAL();
    s_retry_mask |= retry_bit;
    s_retry_after_tick = xTaskGetTickCount() + pdMS_TO_TICKS(USER_MONITOR_RETRY_MS);
    taskEXIT_CRITICAL();
}

static void ScreenIdleTimeout(TimerHandle_t timer)
{
    (void)timer;
    if (SettingsApp_GetScreenIdleTimeoutMin() != SETTINGS_APP_SCREEN_IDLE_TIMEOUT_MIN_DISABLED)
    {
        (void)EGUIHandlerTask_PostDisplayState(EGUI_DISPLAY_STATE_SLEEP);
    }
}

static void WeatherSyncTimeout(TimerHandle_t timer)
{
    (void)timer;
    if (SettingsApp_GetWeatherTimeSyncIntervalMin() ==
        SETTINGS_APP_WEATHER_SYNC_INTERVAL_MIN_DISABLED)
    {
        return;
    }
    if (xWeatherSyncTaskWakeSemaphore != NULL)
    {
        (void)xSemaphoreGive(xWeatherSyncTaskWakeSemaphore);
    }
}

static void SystemAutoOffTimeout(TimerHandle_t timer)
{
    (void)timer;
    if (SettingsApp_GetSystemAutoOffMin() == SETTINGS_APP_SYSTEM_AUTO_OFF_MIN_DISABLED)
    {
        return;
    }
    taskENTER_CRITICAL();
    s_system_auto_off_pending = 1U;
    taskEXIT_CRITICAL();
}

static void MemDiag_LogTaskStack(const char *task_name, TaskHandle_t task_handle)
{
    UBaseType_t high_water_words;

    if (task_handle == NULL)
    {
        log_printf("[Mem] %s null", (task_name != NULL) ? task_name : "task");
        return;
    }

    high_water_words = uxTaskGetStackHighWaterMark(task_handle);
    log_printf("[Mem] %s %luw",
               (task_name != NULL) ? task_name : "task",
               (unsigned long)high_water_words);
}

void UserMonitor_Init(void)
{
    uint32_t screen_timeout_ms;
    uint32_t weather_timeout_ms;
    uint32_t system_auto_off_ms;
    uint8_t system_auto_off_auto_start;

    s_system_auto_off_pending = 0U;
    s_retry_mask = 0U;
    s_applied_screen_timeout_min = SettingsApp_GetScreenIdleTimeoutMin();
    s_applied_weather_interval_min = SettingsApp_GetWeatherTimeSyncIntervalMin();
    s_applied_system_auto_off_min = SettingsApp_GetSystemAutoOffMin();
    screen_timeout_ms = UserMonitor_MinutesToMs(
        (s_applied_screen_timeout_min == SETTINGS_APP_SCREEN_IDLE_TIMEOUT_MIN_DISABLED) ?
            SETTINGS_APP_SCREEN_IDLE_TIMEOUT_MIN_MIN : s_applied_screen_timeout_min);
    weather_timeout_ms = UserMonitor_MinutesToMs(
        (s_applied_weather_interval_min == SETTINGS_APP_WEATHER_SYNC_INTERVAL_MIN_DISABLED) ?
            SETTINGS_APP_WEATHER_SYNC_INTERVAL_MIN_MIN : s_applied_weather_interval_min);
    system_auto_off_ms = UserMonitor_MinutesToMs(
        (s_applied_system_auto_off_min == SETTINGS_APP_SYSTEM_AUTO_OFF_MIN_DISABLED) ?
            SETTINGS_APP_SYSTEM_AUTO_OFF_MIN_MIN : s_applied_system_auto_off_min);
    system_auto_off_auto_start =
        (s_applied_system_auto_off_min != SETTINGS_APP_SYSTEM_AUTO_OFF_MIN_DISABLED) ? 1U : 0U;

    if (Monitor_CreateEx(MON_SCREEN_IDLE, "SCREEN_IDLE", screen_timeout_ms, 0U, ScreenIdleTimeout) == MONITOR_INVALID_ID)
    {
        log_printf("[Monitor] screen create fail");
    }
    if (Monitor_CreateEx(MON_WEATHER_SYNC, "WEATHER_SYNC", weather_timeout_ms, 0U, WeatherSyncTimeout) == MONITOR_INVALID_ID)
    {
        log_printf("[Monitor] weather create fail");
    }
    if (Monitor_CreateEx(MON_SYSTEM_AUTO_OFF, "SYSTEM_AUTO_OFF",
                         system_auto_off_ms, system_auto_off_auto_start,
                         SystemAutoOffTimeout) == MONITOR_INVALID_ID)
    {
        log_printf("[Monitor] system off create fail");
    }
}

void UserMonitor_Service(void)
{
    uint8_t retry_mask = 0U;
    uint8_t system_auto_off = 0U;

    (void)UserMonitor_TrackQueuePeak(Key_Power_queue, &s_queue_peak_key_power);
    (void)UserMonitor_TrackQueuePeak(EGUI_Key_queue, &s_queue_peak_egui_key);
    (void)UserMonitor_TrackQueuePeak(EGUI_DisplayState_queue, &s_queue_peak_display);
    (void)UserMonitor_TrackQueuePeak(Fan_Command_queue, &s_queue_peak_fan);

    taskENTER_CRITICAL();
    if ((s_retry_mask != 0U) && Time32_Reached(xTaskGetTickCount(), s_retry_after_tick))
    {
        retry_mask = s_retry_mask;
        s_retry_mask = 0U;
    }
    if (s_system_auto_off_pending != 0U)
    {
        s_system_auto_off_pending = 0U;
        system_auto_off = 1U;
    }
    taskEXIT_CRITICAL();

    if ((system_auto_off != 0U) &&
        (SettingsApp_GetSystemAutoOffMin() != SETTINGS_APP_SYSTEM_AUTO_OFF_MIN_DISABLED))
    {
        SystemPower_ShutdownNow();
        return;
    }

    if ((retry_mask & (USER_MONITOR_RETRY_SCREEN_APPLY |
                       USER_MONITOR_RETRY_SYSTEM_APPLY)) != 0U)
    {
        UserMonitor_ApplySettings();
    }
    if ((retry_mask & USER_MONITOR_RETRY_SCREEN_RESTART) != 0U)
    {
        UserMonitor_OnDisplayWake();
    }
    if ((retry_mask & USER_MONITOR_RETRY_WEATHER_STOP) != 0U)
    {
        UserMonitor_StopWeatherSync();
    }
    if ((retry_mask & USER_MONITOR_RETRY_WEATHER_APPLY) != 0U)
    {
        UserMonitor_RestartWeatherSync();
    }
    if ((retry_mask & USER_MONITOR_RETRY_SCREEN_STOP) != 0U)
    {
        UserMonitor_OnDisplaySleep();
    }
    if ((retry_mask & USER_MONITOR_RETRY_WEATHER_MANUAL) != 0U)
    {
        UserMonitor_RequestWeatherSync();
    }
    if ((retry_mask & USER_MONITOR_RETRY_SYSTEM_RESTART) != 0U)
    {
        UserMonitor_OnKeyActivity();
    }
}

void UserMonitor_ApplySettings(void)
{
    uint16_t screen_timeout_min = SettingsApp_GetScreenIdleTimeoutMin();
    uint16_t weather_interval_min = SettingsApp_GetWeatherTimeSyncIntervalMin();
    uint16_t system_auto_off_min = SettingsApp_GetSystemAutoOffMin();

    if (screen_timeout_min != s_applied_screen_timeout_min)
    {
        BaseType_t status;

        if (screen_timeout_min == SETTINGS_APP_SCREEN_IDLE_TIMEOUT_MIN_DISABLED)
        {
            status = Monitor_Stop(MON_SCREEN_IDLE);
        }
        else
        {
            status = Monitor_ChangeTimeout(MON_SCREEN_IDLE,
                                           UserMonitor_MinutesToMs(screen_timeout_min));
        }

        if (status == pdPASS)
        {
            s_applied_screen_timeout_min = screen_timeout_min;
        }
        else
        {
            log_printf("[Monitor] screen apply fail");
            UserMonitor_ScheduleRetry(USER_MONITOR_RETRY_SCREEN_APPLY);
        }
    }

    if (weather_interval_min != s_applied_weather_interval_min)
    {
        if (weather_interval_min == SETTINGS_APP_WEATHER_SYNC_INTERVAL_MIN_DISABLED)
        {
            if (Monitor_Stop(MON_WEATHER_SYNC) == pdPASS)
            {
                s_applied_weather_interval_min = weather_interval_min;
            }
            else
            {
                log_printf("[Monitor] weather stop fail");
                UserMonitor_ScheduleRetry(USER_MONITOR_RETRY_WEATHER_APPLY);
            }
        }
        else if ((WeatherApp_IsFirstSyncDone() == 0U) ||
                 (WeatherApp_IsSyncing() != 0U))
        {
            s_applied_weather_interval_min = weather_interval_min;
        }
        else if (Monitor_ChangeTimeout(MON_WEATHER_SYNC,
                                       UserMonitor_MinutesToMs(weather_interval_min)) == pdPASS)
        {
            s_applied_weather_interval_min = weather_interval_min;
        }
        else
        {
            log_printf("[Monitor] weather apply fail");
            UserMonitor_ScheduleRetry(USER_MONITOR_RETRY_WEATHER_APPLY);
        }
    }

    if (system_auto_off_min != s_applied_system_auto_off_min)
    {
        BaseType_t status;

        taskENTER_CRITICAL();
        s_system_auto_off_pending = 0U;
        taskEXIT_CRITICAL();

        if (system_auto_off_min == SETTINGS_APP_SYSTEM_AUTO_OFF_MIN_DISABLED)
        {
            status = Monitor_Stop(MON_SYSTEM_AUTO_OFF);
        }
        else
        {
            status = Monitor_ChangeTimeout(MON_SYSTEM_AUTO_OFF,
                                           UserMonitor_MinutesToMs(system_auto_off_min));
        }

        if (status == pdPASS)
        {
            s_applied_system_auto_off_min = system_auto_off_min;
        }
        else
        {
            log_printf("[Monitor] system off apply fail");
            UserMonitor_ScheduleRetry(USER_MONITOR_RETRY_SYSTEM_APPLY);
        }
    }
}

void UserMonitor_OnDisplayWake(void)
{
    if (SettingsApp_GetScreenIdleTimeoutMin() == SETTINGS_APP_SCREEN_IDLE_TIMEOUT_MIN_DISABLED)
    {
        return;
    }
    if (Monitor_Restart(MON_SCREEN_IDLE) != pdPASS)
    {
        log_printf("[Monitor] screen restart fail");
        UserMonitor_ScheduleRetry(USER_MONITOR_RETRY_SCREEN_RESTART);
    }
}

void UserMonitor_OnDisplaySleep(void)
{
    EGUIHandlerTask_ClearDisplayState();
    if (Monitor_Stop(MON_SCREEN_IDLE) != pdPASS)
    {
        UserMonitor_ScheduleRetry(USER_MONITOR_RETRY_SCREEN_STOP);
    }
}

void UserMonitor_RequestWeatherSync(void)
{
    if ((WeatherApp_IsFirstSyncDone() == 0U) || (WeatherApp_IsSyncing() != 0U))
    {
        return;
    }

    if (Monitor_Stop(MON_WEATHER_SYNC) == pdPASS)
    {
    }
    else
    {
        log_printf("[Monitor] weather manual stop fail");
        UserMonitor_ScheduleRetry(USER_MONITOR_RETRY_WEATHER_MANUAL);
        return;
    }
    if (xWeatherSyncTaskWakeSemaphore != NULL)
    {
        (void)xSemaphoreGive(xWeatherSyncTaskWakeSemaphore);
    }
}

void UserMonitor_RestartWeatherSync(void)
{
    uint16_t interval_min = SettingsApp_GetWeatherTimeSyncIntervalMin();

    if (interval_min == SETTINGS_APP_WEATHER_SYNC_INTERVAL_MIN_DISABLED)
    {
        UserMonitor_StopWeatherSync();
        return;
    }
    if (Monitor_ChangeTimeout(MON_WEATHER_SYNC,
                              UserMonitor_MinutesToMs(interval_min)) == pdPASS)
    {
        s_applied_weather_interval_min = interval_min;
    }
    else
    {
        log_printf("[Monitor] weather restart fail");
        UserMonitor_ScheduleRetry(USER_MONITOR_RETRY_WEATHER_APPLY);
    }
}

void UserMonitor_StopWeatherSync(void)
{
    if (Monitor_Stop(MON_WEATHER_SYNC) != pdPASS)
    {
        UserMonitor_ScheduleRetry(USER_MONITOR_RETRY_WEATHER_STOP);
    }
}

void UserMonitor_StopAll(void)
{
    (void)Monitor_Stop(MON_SCREEN_IDLE);
    (void)Monitor_Stop(MON_WEATHER_SYNC);
    (void)Monitor_Stop(MON_SYSTEM_AUTO_OFF);
    s_retry_mask = 0U;
    s_system_auto_off_pending = 0U;
}

void UserMonitor_OnKeyActivity(void)
{
    taskENTER_CRITICAL();
    s_system_auto_off_pending = 0U;
    taskEXIT_CRITICAL();
    if (SettingsApp_GetSystemAutoOffMin() == SETTINGS_APP_SYSTEM_AUTO_OFF_MIN_DISABLED)
    {
        return;
    }
    if (Monitor_Restart(MON_SYSTEM_AUTO_OFF) != pdPASS)
    {
        log_printf("[Monitor] system off restart fail");
        UserMonitor_ScheduleRetry(USER_MONITOR_RETRY_SYSTEM_RESTART);
    }
}

void Key_Event(void)
{
    (void)EGUIHandlerTask_PostDisplayState(EGUI_DISPLAY_STATE_WAKE);
    UserMonitor_OnDisplayWake();
    UserMonitor_OnKeyActivity();
}

void MemDiag_LogSnapshot(const char *tag)
{
    UBaseType_t key_current;
    UBaseType_t egui_current;
    UBaseType_t display_current;
    UBaseType_t fan_current;
    UBaseType_t notify_current;
    UBaseType_t notify_peak;
    log_printf("[Mem] %s heap=%lu min=%lu",
               (tag != NULL) ? tag : "snapshot",
               (unsigned long)xPortGetFreeHeapSize(),
               (unsigned long)xPortGetMinimumEverFreeHeapSize());

    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED)
    {
        return;
    }

    MemDiag_LogTaskStack("HwInit", HardwareInitTaskHandle);
    MemDiag_LogTaskStack("Key", KeyTaskHandle);
    MemDiag_LogTaskStack("KeyMgr", KeyManllegeTaskHandle);
    MemDiag_LogTaskStack("LED", LEDTaskHandle);
    MemDiag_LogTaskStack("Fan", FanTaskHandle);
    MemDiag_LogTaskStack("EGUI", EGUIHandlerTaskHandle);
    MemDiag_LogTaskStack("TX", TransmitTaskHandle);
    MemDiag_LogTaskStack("AppData", AppDataTaskHandle);
    MemDiag_LogTaskStack("Weather", WeatherSyncTaskHandle);
    MemDiag_LogTaskStack("Timer", xTimerGetTimerDaemonTaskHandle());

    key_current = UserMonitor_TrackQueuePeak(Key_Power_queue, &s_queue_peak_key_power);
    egui_current = UserMonitor_TrackQueuePeak(EGUI_Key_queue, &s_queue_peak_egui_key);
    display_current = UserMonitor_TrackQueuePeak(EGUI_DisplayState_queue, &s_queue_peak_display);
    fan_current = UserMonitor_TrackQueuePeak(Fan_Command_queue, &s_queue_peak_fan);
    SystemNotify_GetQueueUsage(&notify_current, &notify_peak);
    log_printf("[Queue] Key=%lu/%lu EGUI=%lu/%lu Display=%lu/%lu Fan=%lu/%lu Notify=%lu/%lu",
               (unsigned long)key_current, (unsigned long)s_queue_peak_key_power,
               (unsigned long)egui_current, (unsigned long)s_queue_peak_egui_key,
               (unsigned long)display_current, (unsigned long)s_queue_peak_display,
               (unsigned long)fan_current, (unsigned long)s_queue_peak_fan,
               (unsigned long)notify_current, (unsigned long)notify_peak);
}
