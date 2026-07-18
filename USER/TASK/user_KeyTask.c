#include "user_KeyTask.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

#include "data_app.h"
#include "fan_app.h"
#include "key.h"
#include "log.h"
#include "sensors_app.h"
#include "user_TasksInit.h"

#define KEY_TASK_SCAN_PERIOD_MS          10U
#define KEY_TASK_MOTION_PERIOD_MS        30U
#define KEY_TASK_PWR_DOUBLE_CLICK_MS      300U
#define KEY_TASK_TILT_WINDOW_MS          3000U
#define KEY_TASK_TILT_SPEED_STEP_PERCENT 5U

typedef struct
{
    TickType_t motion_last_tick;
    TickType_t pwr_click_tick;
    TickType_t tilt_deadline_tick;
    uint8_t pwr_click_pending;
    uint8_t tilt_active;
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
    case MSG_TILT_SHAKE_VERTICAL: return "SHAKE_V";
    case MSG_TILT_SHAKE_HORIZONTAL: return "SHAKE_H";
    default: return "NONE";
    }
}

static void KeyTask_ArmTiltControl(KeyTask_State_t *state, TickType_t now)
{
    fan_state_t fan_state;

    FanApp_GetState(&fan_state);
    if (fan_state.power_on == 0U)
    {
        state->tilt_active = 0U;
        return;
    }

    state->tilt_active = 1U;
    state->tilt_deadline_tick = now + pdMS_TO_TICKS(KEY_TASK_TILT_WINDOW_MS);
}

static void KeyTask_RecordPowerClick(KeyTask_State_t *state, TickType_t now)
{
    if ((state->pwr_click_pending != 0U) &&
        ((TickType_t)(now - state->pwr_click_tick) <
         pdMS_TO_TICKS(KEY_TASK_PWR_DOUBLE_CLICK_MS)))
    {
        state->pwr_click_pending = 0U;
        return;
    }

    state->pwr_click_pending = 1U;
    state->pwr_click_tick = now;
}

static void KeyTask_ProcessPendingPowerClick(KeyTask_State_t *state, TickType_t now)
{
    if ((state->pwr_click_pending != 0U) &&
        ((TickType_t)(now - state->pwr_click_tick) >=
         pdMS_TO_TICKS(KEY_TASK_PWR_DOUBLE_CLICK_MS)))
    {
        state->pwr_click_pending = 0U;
        KeyTask_ArmTiltControl(state, now);
    }
}

static void KeyTask_AdjustFanFromTilt(KeyTask_State_t *state, TickType_t now)
{
    fan_state_t fan_state;
    uint8_t next_speed;

    if (state->tilt_active == 0U)
    {
        return;
    }

    if ((int32_t)(now - state->tilt_deadline_tick) >= 0)
    {
        state->tilt_active = 0U;
        return;
    }

    FanApp_GetState(&fan_state);
    if (fan_state.power_on == 0U)
    {
        state->tilt_active = 0U;
        return;
    }

    next_speed = (fan_state.base_speed_percent > KEY_TASK_TILT_SPEED_STEP_PERCENT) ?
                     (uint8_t)(fan_state.base_speed_percent - KEY_TASK_TILT_SPEED_STEP_PERCENT) :
                     0U;
    if (FanApp_SetSpeed(next_speed, 0U))
    {
        state->tilt_deadline_tick = now + pdMS_TO_TICKS(KEY_TASK_TILT_WINDOW_MS);
        log_printf("[Motion] fan speed=%u", (unsigned)next_speed);
    }
}

static void KeyTask_ProcessMotion(KeyTask_State_t *state, TickType_t now)
{
    TiltKey_t tilt_event;
    TiltKey_t fall_event;

    if ((TickType_t)(now - state->motion_last_tick) <
        pdMS_TO_TICKS(KEY_TASK_MOTION_PERIOD_MS))
    {
        return;
    }
    state->motion_last_tick = now;

    Update_Motion(&g_sensors_motion);
    Motion_SwapBuffer(&g_sensors_motion);
    tilt_event = TiltKey_Update(&g_sensors_motion);
    fall_event = FallDetect_Check(&g_sensors_motion);

    if (tilt_event != MSG_TILT_NONE)
    {
        log_printf("[Motion] %s", KeyTask_TiltName(tilt_event));
        if ((tilt_event == MSG_TILT_SHAKE_HORIZONTAL) ||
            (tilt_event == MSG_TILT_SHAKE_VERTICAL))
        {
            if (xWeatherSyncTaskWakeSemaphore != NULL)
            {
                (void)xSemaphoreGive(xWeatherSyncTaskWakeSemaphore);
            }
        }
        else if ((tilt_event == MSG_TILT_LEFT) ||
                 (tilt_event == MSG_TILT_RIGHT) ||
                 (tilt_event == MSG_TILT_UP) ||
                 (tilt_event == MSG_TILT_DOWN))
        {
            KeyTask_AdjustFanFromTilt(state, now);
        }
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

    for (;;)
    {
        TickType_t now = xTaskGetTickCount();

        KeyTask_ProcessPendingPowerClick(&state, now);
        if (Key_Scan(&key_event))
        {
            if ((key_event.id == KEY_ID_PWR) && (key_event.type == KEY_EVT_CLICK))
            {
                KeyTask_RecordPowerClick(&state, now);
            }
            KeyTask_SendEvent(&key_event);
        }

        KeyTask_ProcessMotion(&state, now);
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(KEY_TASK_SCAN_PERIOD_MS));
    }
}
