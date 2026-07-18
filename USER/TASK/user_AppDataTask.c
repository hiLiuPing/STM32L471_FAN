#include "user_AppDataTask.h"

#include "FreeRTOS.h"
#include "data_app.h"
#include "fan_app.h"
#include "sensors_app.h"
#include "system_notify.h"
#include "task.h"
#include "user_TasksInit.h"
#include "weather_app.h"

#define APP_DATA_WEATHER_DEMO_INTERVAL_MS 15000U
#define APP_DATA_NOTIFY_STABLE_SAMPLES    3U
#define APP_DATA_LOW_BATTERY_PERCENT      30
#define APP_DATA_LOW_BATTERY_REARM        35
#define APP_DATA_ENV_TEMP_HYST_X10        10
#define APP_DATA_ENV_HUM_HYST             5

typedef struct
{
    uint8_t power_initialized;
    uint8_t stable_power_present;
    uint8_t power_candidate;
    uint8_t power_candidate_count;
    uint8_t charging_seen;
    uint8_t charge_done_count;
    uint8_t charge_reported;
    uint8_t low_battery_count;
    uint8_t low_battery_latched;
    uint8_t environment_high_count;
    uint8_t environment_clear_count;
    uint8_t environment_latched;
} AppDataNotifyState_t;

static int16_t AppDataNotify_RoundX10(float value)
{
    float scaled = value * 10.0f;

    return (int16_t)(scaled + ((scaled >= 0.0f) ? 0.5f : -0.5f));
}

static int16_t AppDataNotify_ClampPercent(float value)
{
    if (value <= 0.0f)
    {
        return 0;
    }
    if (value >= 100.0f)
    {
        return 100;
    }

    return (int16_t)value;
}

static void AppDataNotify_UpdatePower(AppDataNotifyState_t *state)
{
    uint8_t present = g_sensors_charger.input_present ? 1U : 0U;

    if (state->power_initialized == 0U)
    {
        state->power_initialized = 1U;
        state->stable_power_present = present;
        state->power_candidate = present;
        state->power_candidate_count = 0U;
        return;
    }

    if (present == state->stable_power_present)
    {
        state->power_candidate = present;
        state->power_candidate_count = 0U;
        return;
    }

    if (present != state->power_candidate)
    {
        state->power_candidate = present;
        state->power_candidate_count = 1U;
        return;
    }

    if (state->power_candidate_count < APP_DATA_NOTIFY_STABLE_SAMPLES)
    {
        state->power_candidate_count++;
    }

    if (state->power_candidate_count >= APP_DATA_NOTIFY_STABLE_SAMPLES)
    {
        state->stable_power_present = present;
        state->power_candidate_count = 0U;
        (void)SystemNotify_Post(present ? SYSTEM_NOTIFY_POWER_IN : SYSTEM_NOTIFY_POWER_OUT, 0, 0);
    }
}

static void AppDataNotify_UpdateCharge(AppDataNotifyState_t *state)
{
    bq24295_state_t charge_state = g_sensors_charger.state;

    if (!g_sensors_charger.input_present)
    {
        state->charging_seen = 0U;
        state->charge_done_count = 0U;
        state->charge_reported = 0U;
        return;
    }

    if ((charge_state == BQ_CHG_PRECHARGE) || (charge_state == BQ_CHG_FAST_CHARGE))
    {
        state->charging_seen = 1U;
        state->charge_done_count = 0U;
        state->charge_reported = 0U;
        return;
    }

    if ((charge_state != BQ_CHG_DONE) || (state->charging_seen == 0U) ||
        (state->charge_reported != 0U))
    {
        state->charge_done_count = 0U;
        return;
    }

    if (state->charge_done_count < APP_DATA_NOTIFY_STABLE_SAMPLES)
    {
        state->charge_done_count++;
    }

    if (state->charge_done_count >= APP_DATA_NOTIFY_STABLE_SAMPLES)
    {
        state->charge_reported = 1U;
        state->charge_done_count = 0U;
        (void)SystemNotify_Post(SYSTEM_NOTIFY_CHARGE_COMPLETE, 100, 0);
    }
}

static void AppDataNotify_UpdateBattery(AppDataNotifyState_t *state)
{
    int16_t percent;

    if (g_sensors_battery.voltage <= 0.0f)
    {
        return;
    }

    percent = AppDataNotify_ClampPercent(g_sensors_battery.soc);
    if (state->low_battery_latched != 0U)
    {
        if (percent >= APP_DATA_LOW_BATTERY_REARM)
        {
            state->low_battery_latched = 0U;
        }
        return;
    }

    if (percent >= APP_DATA_LOW_BATTERY_PERCENT)
    {
        state->low_battery_count = 0U;
        return;
    }

    if (state->low_battery_count < APP_DATA_NOTIFY_STABLE_SAMPLES)
    {
        state->low_battery_count++;
    }

    if (state->low_battery_count >= APP_DATA_NOTIFY_STABLE_SAMPLES)
    {
        state->low_battery_latched = 1U;
        state->low_battery_count = 0U;
        (void)SystemNotify_Post(SYSTEM_NOTIFY_LOW_BATTERY, percent, 0);
    }
}

static void AppDataNotify_UpdateEnvironment(AppDataNotifyState_t *state)
{
    fan_state_t fan_state;
    int16_t temp_x10 = AppDataNotify_RoundX10(g_sensors_environment.temp);
    int16_t hum_percent = AppDataNotify_ClampPercent(g_sensors_environment.hum);
    int16_t hum_threshold;
    uint8_t is_high;
    uint8_t is_clear;

    FanApp_GetState(&fan_state);
    hum_threshold = (int16_t)fan_state.smart_hum_threshold_percent;
    is_high = (uint8_t)((temp_x10 >= (int16_t)fan_state.smart_temp_threshold_x10) ||
                        (hum_percent >= hum_threshold));
    is_clear = (uint8_t)((temp_x10 <= ((int16_t)fan_state.smart_temp_threshold_x10 -
                                       APP_DATA_ENV_TEMP_HYST_X10)) &&
                         (hum_percent <= (hum_threshold - APP_DATA_ENV_HUM_HYST)));

    if (state->environment_latched != 0U)
    {
        state->environment_high_count = 0U;
        if (is_clear == 0U)
        {
            state->environment_clear_count = 0U;
            return;
        }

        if (state->environment_clear_count < APP_DATA_NOTIFY_STABLE_SAMPLES)
        {
            state->environment_clear_count++;
        }
        if (state->environment_clear_count >= APP_DATA_NOTIFY_STABLE_SAMPLES)
        {
            state->environment_latched = 0U;
            state->environment_clear_count = 0U;
        }
        return;
    }

    state->environment_clear_count = 0U;
    if (is_high == 0U)
    {
        state->environment_high_count = 0U;
        return;
    }

    if (state->environment_high_count < APP_DATA_NOTIFY_STABLE_SAMPLES)
    {
        state->environment_high_count++;
    }
    if (state->environment_high_count >= APP_DATA_NOTIFY_STABLE_SAMPLES)
    {
        state->environment_latched = 1U;
        state->environment_high_count = 0U;
        (void)SystemNotify_Post(SYSTEM_NOTIFY_ENVIRONMENT_ALERT, temp_x10, hum_percent);
    }
}

static void AppDataNotify_Update(AppDataNotifyState_t *state)
{
    AppDataNotify_UpdatePower(state);
    AppDataNotify_UpdateCharge(state);
    AppDataNotify_UpdateBattery(state);
    AppDataNotify_UpdateEnvironment(state);
}

void AppDataTask(void *argument)
{
    TickType_t last_wake_time;
    TickType_t last_1s_tick;
    TickType_t last_weather_demo_tick;
    AppDataNotifyState_t notify_state = {0};

    (void)argument;

    User_Tasks_WaitForHardwareReady();
    last_wake_time = xTaskGetTickCount();
    last_1s_tick = last_wake_time;
    last_weather_demo_tick = last_wake_time;

    for (;;)
    {
        TickType_t now = xTaskGetTickCount();
        DataApp_QuoteServiceUpdate(now);

        // if ((TickType_t)(now - last_weather_demo_tick) >= pdMS_TO_TICKS(APP_DATA_WEATHER_DEMO_INTERVAL_MS))
        // {
        //     last_weather_demo_tick += pdMS_TO_TICKS(APP_DATA_WEATHER_DEMO_INTERVAL_MS);
        //     Weather_FillDemoData();
        // }

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
            FanApp_UpdateRpm(xTaskGetTickCount());
            DataApp_HomeStatus_Update();
            AppDataNotify_Update(&notify_state);
        }

        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(30U));
    }
}
