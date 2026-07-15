#include "user_AppDataTask.h"

#include "FreeRTOS.h"
#include "data_app.h"
#include "log.h"
#include "sensors_app.h"
#include "task.h"
#include "user_TasksInit.h"

static const char *TiltName(TiltKey_t evt)
{
    switch (evt)
    {
    case MSG_TILT_UP: return "UP";
    case MSG_TILT_DOWN: return "DOWN";
    case MSG_TILT_LEFT: return "LEFT";
    case MSG_TILT_RIGHT: return "RIGHT";
    case MSG_FALL_DOWN: return "FALL";
    case MSG_TILT_SHAKE_VERTICAL: return "SHAKE_V";
    case MSG_TILT_SHAKE_HORIZONTAL: return "SHAKE_H";
    default: return "NONE";
    }
}

void AppDataTask(void *argument)
{
    TickType_t last_wake_time;
    TickType_t last_1s_tick;
    TickType_t last_log_tick;
    TiltKey_t last_tilt = MSG_TILT_NONE;

    (void)argument;

    User_Tasks_WaitForHardwareReady();
    last_wake_time = xTaskGetTickCount();
    last_1s_tick = last_wake_time;
    last_log_tick = last_wake_time;

    for (;;)
    {
        TickType_t now = xTaskGetTickCount();
        TiltKey_t tilt_evt;
        TiltKey_t fall_evt;

        Update_Motion(&g_sensors_motion);
        Motion_SwapBuffer(&g_sensors_motion);

        tilt_evt = TiltKey_Update(&g_sensors_motion);
        fall_evt = FallDetect_Check(&g_sensors_motion);

        if ((tilt_evt != MSG_TILT_NONE) && (tilt_evt != last_tilt))
        {
            last_tilt = tilt_evt;
            log_printf("[Motion] %s", TiltName(tilt_evt));
            if (((tilt_evt == MSG_TILT_SHAKE_HORIZONTAL) ||
                 (tilt_evt == MSG_TILT_SHAKE_VERTICAL)) &&
                (xWeatherSyncTaskWakeSemaphore != NULL))
            {
                (void)xSemaphoreGive(xWeatherSyncTaskWakeSemaphore);
            }
        }

        if (fall_evt == MSG_FALL_DOWN)
        {
            log_printf("[Motion] fall");
        }

        DataApp_QuoteServiceUpdate(now);

        if ((TickType_t)(now - last_1s_tick) >= pdMS_TO_TICKS(100U))
        {
            last_1s_tick += pdMS_TO_TICKS(100U);
            Time_BlinkUpdate();
            RTC_ReadToBuffer();
            Buffer_Swap();
            Update_Env(&g_sensors_environment);
            Update_Battery(&g_sensors_battery);
            Update_Charger(&g_sensors_charger);
            Update_INA226(&g_sensors_ina226);
            DataApp_HomeStatus_Update();
        }



        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(30U));
    }
}
