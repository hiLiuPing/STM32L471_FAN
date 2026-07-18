#include "systemMonitor_app.h"

#include <stddef.h>

#include "FreeRTOS.h"
#include "fan_app.h"
#include "log.h"
#include "settings_app.h"
#include "system_power.h"
#include "task.h"
#include "timers.h"
#include "user_EGUITask.h"
#include "user_TasksInit.h"
#include "weather_app.h"

#define USER_MONITOR_FAN_RETRY_MS 100U

_Static_assert(MON_APP_MAX <= MONITOR_MAX_NUM, "App monitor IDs exceed monitor storage");

static volatile uint8_t s_weather_monitor_armed = 0U;
static volatile uint8_t s_sensor_log_pending = 0U;
static volatile uint8_t s_fan_auto_off_retry_pending = 0U;
static volatile uint8_t s_system_auto_off_pending = 0U;
static uint16_t s_applied_screen_timeout_min = 0U;
static uint16_t s_applied_weather_interval_min = 0U;
static uint16_t s_applied_system_auto_off_min = 0U;

static uint32_t UserMonitor_MinutesToMs(uint16_t minutes)
{
    return (uint32_t)minutes * 60U * 1000U;
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
    s_weather_monitor_armed = 0U;
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

static void FanAutoOffTimeout(TimerHandle_t timer)
{
    fan_cmd_t cmd = { FAN_CMD_AUTO_OFF_EXPIRED, 0U, 0U };

    (void)timer;
    if (!FanApp_SendCommand(&cmd, 0U))
    {
        if (Monitor_ChangeTimeout(MON_FAN_AUTO_OFF, USER_MONITOR_FAN_RETRY_MS) != pdPASS)
        {
            s_fan_auto_off_retry_pending = 1U;
        }
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

static void SensorLogTimeout(TimerHandle_t timer)
{
    (void)timer;
    s_sensor_log_pending = 1U;
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

    s_weather_monitor_armed = 0U;
    s_sensor_log_pending = 0U;
    s_fan_auto_off_retry_pending = 0U;
    s_system_auto_off_pending = 0U;
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
    if (Monitor_CreateEx(MON_FAN_AUTO_OFF, "FAN_AUTO_OFF",
                         UserMonitor_MinutesToMs(FAN_AUTO_OFF_MINUTES_DEFAULT),
                         0U, FanAutoOffTimeout) == MONITOR_INVALID_ID)
    {
        log_printf("[Monitor] fan create fail");
    }
    if (Monitor_CreateEx(MON_SYSTEM_AUTO_OFF, "SYSTEM_AUTO_OFF",
                         system_auto_off_ms, system_auto_off_auto_start,
                         SystemAutoOffTimeout) == MONITOR_INVALID_ID)
    {
        log_printf("[Monitor] system off create fail");
    }
    if (Monitor_CreateEx(MON_SENSOR_LOG, "SENSOR_LOG", 30000U, 1U, SensorLogTimeout) == MONITOR_INVALID_ID)
    {
        log_printf("[Monitor] sensor create fail");
    }
}

void UserMonitor_Service(void)
{
    fan_cmd_t fan_cmd = { FAN_CMD_AUTO_OFF_EXPIRED, 0U, 0U };
    uint8_t retry_fan = 0U;
    uint8_t log_sensor = 0U;
    uint8_t system_auto_off = 0U;

    taskENTER_CRITICAL();
    if (s_sensor_log_pending != 0U)
    {
        s_sensor_log_pending = 0U;
        log_sensor = 1U;
    }
    retry_fan = s_fan_auto_off_retry_pending;
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

    if ((retry_fan != 0U) && FanApp_SendCommand(&fan_cmd, 0U))
    {
        taskENTER_CRITICAL();
        s_fan_auto_off_retry_pending = 0U;
        taskEXIT_CRITICAL();
    }

    if (log_sensor != 0U)
    {
        log_printf("[Monitor] sensor timer");
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
        }
    }

    if (weather_interval_min != s_applied_weather_interval_min)
    {
        if (weather_interval_min == SETTINGS_APP_WEATHER_SYNC_INTERVAL_MIN_DISABLED)
        {
            if (Monitor_Stop(MON_WEATHER_SYNC) == pdPASS)
            {
                s_weather_monitor_armed = 0U;
                s_applied_weather_interval_min = weather_interval_min;
            }
            else
            {
                log_printf("[Monitor] weather stop fail");
            }
        }
        else if ((g_weather_module.first_sync_done == 0U) ||
                 (g_weather_module.syncing != 0U))
        {
            s_applied_weather_interval_min = weather_interval_min;
        }
        else if (Monitor_ChangeTimeout(MON_WEATHER_SYNC,
                                       UserMonitor_MinutesToMs(weather_interval_min)) == pdPASS)
        {
            s_weather_monitor_armed = 1U;
            s_applied_weather_interval_min = weather_interval_min;
        }
        else
        {
            log_printf("[Monitor] weather apply fail");
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
    }
}

void UserMonitor_OnDisplaySleep(void)
{
    EGUIHandlerTask_ClearDisplayState();
    (void)Monitor_Stop(MON_SCREEN_IDLE);
}

void UserMonitor_RequestWeatherSync(void)
{
    if ((g_weather_module.first_sync_done == 0U) || (g_weather_module.syncing != 0U))
    {
        return;
    }

    if (Monitor_Stop(MON_WEATHER_SYNC) == pdPASS)
    {
        s_weather_monitor_armed = 0U;
    }
    else
    {
        log_printf("[Monitor] weather manual stop fail");
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

    s_applied_weather_interval_min = interval_min;
    if (interval_min == SETTINGS_APP_WEATHER_SYNC_INTERVAL_MIN_DISABLED)
    {
        UserMonitor_StopWeatherSync();
        return;
    }
    if (Monitor_ChangeTimeout(MON_WEATHER_SYNC,
                              UserMonitor_MinutesToMs(interval_min)) == pdPASS)
    {
        s_weather_monitor_armed = 1U;
    }
    else
    {
        log_printf("[Monitor] weather restart fail");
    }
}

void UserMonitor_StopWeatherSync(void)
{
    if (Monitor_Stop(MON_WEATHER_SYNC) == pdPASS)
    {
        s_weather_monitor_armed = 0U;
    }
}

void UserMonitor_RestartFanAutoOff(uint16_t minutes)
{
    if (minutes == FAN_AUTO_OFF_MINUTES_DISABLED)
    {
        UserMonitor_StopFanAutoOff();
        return;
    }

    s_fan_auto_off_retry_pending = 0U;
    if (Monitor_ChangeTimeout(MON_FAN_AUTO_OFF, UserMonitor_MinutesToMs(minutes)) != pdPASS)
    {
        log_printf("[Monitor] fan restart fail");
    }
}

void UserMonitor_StopFanAutoOff(void)
{
    s_fan_auto_off_retry_pending = 0U;
    (void)Monitor_Stop(MON_FAN_AUTO_OFF);
}

void UserMonitor_StopAll(void)
{
    (void)Monitor_Stop(MON_SCREEN_IDLE);
    (void)Monitor_Stop(MON_WEATHER_SYNC);
    (void)Monitor_Stop(MON_FAN_AUTO_OFF);
    (void)Monitor_Stop(MON_SYSTEM_AUTO_OFF);
    (void)Monitor_Stop(MON_SENSOR_LOG);
    s_weather_monitor_armed = 0U;
    s_fan_auto_off_retry_pending = 0U;
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
    log_printf("[Mem] %s heap=%lu",
               (tag != NULL) ? tag : "snapshot",
               (unsigned long)xPortGetFreeHeapSize());

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
}
