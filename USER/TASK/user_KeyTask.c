#include "user_KeyTask.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "data_app.h"
#include "key.h"
#include "log.h"
#include "sensors_app.h"
#include "shake_detector.h"
#include "systemMonitor_app.h"
#include "user_TasksInit.h"
#include "user_WeatherSyncTask.h"

#define KEY_TASK_SCAN_PERIOD_MS   10U
#define KEY_TASK_MOTION_PERIOD_MS 30U

typedef struct
{
    TickType_t motion_last_tick;
    uint32_t last_pressed_mask;
    ShakeDetector_t shake_detector;
} KeyTask_State_t;

static void KeyTask_SendEvent(const key_event_t *event)
{
    static uint32_t s_key_drop_count = 0U;

    if ((Key_Power_queue == NULL) || (event == NULL))
    {
        return;
    }

    if (xQueueSend(Key_Power_queue, event, 0U) != pdPASS)
    {
        s_key_drop_count++;
        if ((s_key_drop_count & 0x07U) == 0U)
        {
            log_printf("[KeyQ] dropped=%lu\r\n", (unsigned long)s_key_drop_count);
        }
    }
}

static const char *KeyTask_TiltName(TiltKey_t event)
{
    switch (event)
    {
    case MSG_TILT_UP: return "UP";
    case MSG_TILT_DOWN: return "DOWN";
    case MSG_TILT_LEFT: return "LEFT";
    case MSG_TILT_RIGHT: return "RIGHT";
    case MSG_FALL_DOWN: return "FALL";
    default: return "NONE";
    }
}

static void KeyTask_ProcessMotion(KeyTask_State_t *state, TickType_t now)
{
    TiltKey_t tilt_event;
    TiltKey_t fall_event;
    ShakeDetectorSample_t shake_sample;
    sensors_snapshot_t sensors;

    if ((TickType_t)(now - state->motion_last_tick) <
        pdMS_TO_TICKS(KEY_TASK_MOTION_PERIOD_MS))
    {
        return;
    }
    state->motion_last_tick = now;

    (void)SensorsApp_UpdateMotion();
    SensorsApp_GetSnapshot(&sensors);
    if ((sensors.motion.health.valid == 0U) ||
        (sensors.motion.health.stale != 0U))
    {
        return;
    }
    shake_sample.x = sensors.motion.value.x;
    shake_sample.y = sensors.motion.value.y;
    shake_sample.z = sensors.motion.value.z;
    if (ShakeDetector_Update(&state->shake_detector,
                             &shake_sample,
                             (uint32_t)now))
    {
        log_printf("[Motion] SHAKE");
        (void)WeatherSyncTask_RequestManual();
    }
    tilt_event = TiltKey_Update(&sensors.motion.value);
    fall_event = FallDetect_Check(&sensors.motion.value);

    if (tilt_event != MSG_TILT_NONE)
    {
        log_printf("[Motion] %s", KeyTask_TiltName(tilt_event));
    }

    if (fall_event == MSG_FALL_DOWN)
    {
        log_printf("[Motion] fall");
    }
}

void KeyTask(void *argument)
{
    key_event_t key_event;
    KeyTask_State_t state = {0};
    TickType_t last_wake_time;

    (void)argument;

    User_Tasks_WaitForHardwareReady();
    last_wake_time = xTaskGetTickCount();
    state.motion_last_tick = last_wake_time;
    state.last_pressed_mask = Key_GetPressedMask();
    ShakeDetector_Init(&state.shake_detector);

    for (;;)
    {
        TickType_t now = xTaskGetTickCount();
        uint32_t pressed_mask;

        if (Key_Scan(&key_event))
        {
            KeyTask_SendEvent(&key_event);
        }
        pressed_mask = Key_GetPressedMask();
        if ((pressed_mask & ~state.last_pressed_mask) != 0U)
        {
            UserMonitor_OnKeyActivity();
        }
        state.last_pressed_mask = pressed_mask;

        KeyTask_ProcessMotion(&state, now);
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(KEY_TASK_SCAN_PERIOD_MS));
    }
}
