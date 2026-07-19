#include "user_AppDataTask.h"

#include "FreeRTOS.h"
#include "data_app.h"
#include "fan_app.h"
#include "sensors_app.h"
#include "settings_app.h"
#include "systemMonitor_app.h"
#include "system_notify.h"
#include "task.h"
#include "user_TasksInit.h"
#include "weather_app.h"

#define APP_DATA_NOTIFY_STABLE_SAMPLES    3U
#define APP_DATA_LOW_BATTERY_PERCENT      30
#define APP_DATA_LOW_BATTERY_REARM        35
#define APP_DATA_ENV_TEMP_HYST_X10        10
#define APP_DATA_ENV_HUM_HYST             5
#define APP_DATA_SENSOR_PERIOD_MS         1000U
#define APP_DATA_RPM_PERIOD_MS            100U
#if defined(WEATHER_DEMO_ENABLE) && WEATHER_DEMO_ENABLE
#define APP_DATA_WEATHER_DEMO_INTERVAL_MS 10000U
#endif

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

static void AppDataNotify_UpdatePower(AppDataNotifyState_t *state,
                                      const sensors_snapshot_t *sensors)
{
    uint8_t present;

    if ((sensors->charger.health.valid == 0U) ||
        (sensors->charger.health.stale != 0U))
    {
        state->power_candidate_count = 0U;
        return;
    }
    present = sensors->charger.value.input_present;

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

static void AppDataNotify_UpdateCharge(AppDataNotifyState_t *state,
                                       const sensors_snapshot_t *sensors)
{
    bq24295_state_t charge_state;

    if ((sensors->charger.health.valid == 0U) ||
        (sensors->charger.health.stale != 0U))
    {
        state->charge_done_count = 0U;
        return;
    }
    charge_state = sensors->charger.value.state;
    if (sensors->charger.value.input_present == 0U)
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

static void AppDataNotify_UpdateBattery(AppDataNotifyState_t *state,
                                        const sensors_snapshot_t *sensors)
{
    int16_t percent;

    if ((sensors->battery.health.valid == 0U) ||
        (sensors->battery.health.stale != 0U) ||
        (sensors->battery.value.voltage <= 0.0f))
    {
        state->low_battery_count = 0U;
        return;
    }

    percent = AppDataNotify_ClampPercent(sensors->battery.value.soc);
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

static void AppDataNotify_UpdateEnvironment(AppDataNotifyState_t *state,
                                            const sensors_snapshot_t *sensors)
{
    fan_state_t fan_state;
    int16_t temp_x10;
    int16_t hum_percent;
    int16_t hum_threshold;
    uint8_t is_high;
    uint8_t is_clear;

    if ((sensors->environment.health.valid == 0U) ||
        (sensors->environment.health.stale != 0U))
    {
        state->environment_high_count = 0U;
        state->environment_clear_count = 0U;
        return;
    }

    temp_x10 = AppDataNotify_RoundX10(sensors->environment.value.temp);
    hum_percent = AppDataNotify_ClampPercent(sensors->environment.value.hum);
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

static void AppDataNotify_Update(AppDataNotifyState_t *state,
                                 const sensors_snapshot_t *sensors)
{
    AppDataNotify_UpdatePower(state, sensors);
    AppDataNotify_UpdateCharge(state, sensors);
    AppDataNotify_UpdateBattery(state, sensors);
    AppDataNotify_UpdateEnvironment(state, sensors);
}

void AppDataTask(void *argument)
{
    TickType_t last_wake_time;
    TickType_t last_sensor_tick;
    TickType_t last_rpm_tick;
#if defined(WEATHER_DEMO_ENABLE) && WEATHER_DEMO_ENABLE
    TickType_t last_weather_demo_tick;
#endif
    AppDataNotifyState_t notify_state = {0};

    (void)argument;

    User_Tasks_WaitForHardwareReady();
    last_wake_time = xTaskGetTickCount();
    last_sensor_tick = last_wake_time;
    last_rpm_tick = last_wake_time;
#if defined(WEATHER_DEMO_ENABLE) && WEATHER_DEMO_ENABLE
    last_weather_demo_tick = last_wake_time;
    Weather_FillDemoData();
#endif

    for (;;)
    {
        TickType_t now = xTaskGetTickCount();
        UserMonitor_Service();
        SettingsApp_PersistPending(now);

#if defined(WEATHER_DEMO_ENABLE) && WEATHER_DEMO_ENABLE
        if ((TickType_t)(now - last_weather_demo_tick) >=
            pdMS_TO_TICKS(APP_DATA_WEATHER_DEMO_INTERVAL_MS))
        {
            last_weather_demo_tick += pdMS_TO_TICKS(APP_DATA_WEATHER_DEMO_INTERVAL_MS);
            Weather_FillDemoData();
        }
#endif

        if ((TickType_t)(now - last_rpm_tick) >= pdMS_TO_TICKS(APP_DATA_RPM_PERIOD_MS))
        {
            last_rpm_tick += pdMS_TO_TICKS(APP_DATA_RPM_PERIOD_MS);
            FanApp_UpdateRpm(now);
        }

        if ((TickType_t)(now - last_sensor_tick) >= pdMS_TO_TICKS(APP_DATA_SENSOR_PERIOD_MS))
        {
            last_sensor_tick += pdMS_TO_TICKS(APP_DATA_SENSOR_PERIOD_MS);
            Time_BlinkUpdate();
            RTC_ReadToBuffer();
            Buffer_Swap();
            sensors_snapshot_t sensors;

            (void)SensorsApp_UpdateEnvironment();
            (void)SensorsApp_UpdateBattery();
            (void)SensorsApp_UpdateCharger();
            (void)SensorsApp_UpdateINA226();
            SensorsApp_GetSnapshot(&sensors);
            DataApp_HomeStatus_Update();
            AppDataNotify_Update(&notify_state, &sensors);
        }

        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(30U));
    }
}
