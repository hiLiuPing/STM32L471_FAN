#include "systemMonitor_app.h"

#include <stddef.h>

#include "FreeRTOS.h"
#include "log.h"
#include "task.h"
#include "timers.h"
#include "user_TasksInit.h"

static void SensorLogTimeout(TimerHandle_t timer)
{
    (void)timer;
    log_printf("[Monitor] sensor timer");
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
    (void)Monitor_CreateEx(MON_SENSOR_LOG, "SENSOR_LOG", 30000U, 1U, SensorLogTimeout);
}

void Key_Event(void)
{
    Monitor_Restart(MON_SENSOR_LOG);
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
    MemDiag_LogTaskStack("LED", LEDTaskHandle);
    MemDiag_LogTaskStack("LVGL", LvHandlerTaskHandle);
    MemDiag_LogTaskStack("TX", TransmitTaskHandle);
    MemDiag_LogTaskStack("AppData", AppDataTaskHandle);
    MemDiag_LogTaskStack("Weather", WeatherSyncTaskHandle);
}
