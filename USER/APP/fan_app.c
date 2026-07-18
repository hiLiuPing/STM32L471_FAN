#include "fan_app.h"

#include <string.h>

#include "eeprom_app.h"
#include "log.h"
#include "main.h"
#include "pwm_fan_control.h"
#include "sensors_app.h"
#include "systemMonitor_app.h"
#include "tim.h"

#define FAN_APP_DEFAULT_BASE_SPEED          45U
#define FAN_APP_DEFAULT_INTENSITY           80U
#define FAN_APP_DEFAULT_SMART_TEMP_X10      280U
#define FAN_APP_DEFAULT_SMART_HUM_PERCENT   65U
#define FAN_APP_DEFAULT_NATURAL_AMPLITUDE   18U

#define FAN_APP_START_BOOST_PERCENT         35U
#define FAN_APP_START_BOOST_MS              300U
#define FAN_APP_PWM_ALGO_PERIOD_MS          100U
#define FAN_APP_PWM_AUTO_RELOAD             3199U
#define FAN_APP_SMART_TEMP_SPAN_X10         40
#define FAN_APP_SMART_HUM_BOOST_MAX         20.0f
#define FAN_APP_SMART_HUM_BOOST_PER_PERCENT 0.5f
#define FAN_APP_NATURAL_AMPLITUDE_DEFAULT   FAN_APP_DEFAULT_NATURAL_AMPLITUDE
#define FAN_APP_TACH_TIMER_HZ               1000000UL
#define FAN_APP_TACH_PULSES_PER_REV         2UL
#define FAN_APP_TACH_TIMEOUT_MS              1000U
#define FAN_APP_SAVE_DELAY_MS                1000U
#define FAN_APP_STORAGE_VERSION              1U

#pragma pack(push, 1)
typedef struct
{
    uint32_t magic;
    uint8_t version;
    uint8_t mode;
    uint8_t base_speed_percent;
    uint8_t intensity_percent;
    uint16_t smart_temp_threshold_x10;
    uint8_t smart_hum_threshold_percent;
    uint8_t natural_amplitude_percent;
    uint16_t auto_off_min;
    uint8_t reserved[240];
    uint16_t crc;
} FanApp_Storage_t;
#pragma pack(pop)

typedef struct
{
    fan_mode_t mode;
    uint8_t base_speed_percent;
    uint8_t intensity_percent;
    uint16_t smart_temp_threshold_x10;
    uint8_t smart_hum_threshold_percent;
    uint8_t natural_amplitude_percent;
    uint16_t auto_off_min;
} FanApp_PersistedSettings_t;

_Static_assert(sizeof(FanApp_Storage_t) == EE_BLOCK_SIZE,
               "FanApp storage must occupy one EEPROM block");


static const char *const s_mode_names[FAN_MODE_COUNT] = {
    "Off",
    "Manual",
    "Natural",
    "Smart",
};


static QueueHandle_t s_command_queue = NULL;
static fan_state_t s_state;
static TickType_t s_boost_until_tick = 0U;
static TickType_t s_auto_off_deadline_tick = 0U;
static uint8_t s_auto_off_timer_active = 0U;
static PwmFanController_t s_pwm_fan;
static TickType_t s_pwm_last_tick = 0U;
static uint8_t s_pwm_cached_percent = 0U;
static volatile uint32_t s_tach_last_capture = 0U;
static volatile uint32_t s_tach_period_ticks = 0U;
static volatile TickType_t s_tach_last_edge_tick = 0U;
static volatile uint32_t s_tach_sequence = 0U;
static volatile uint8_t s_tach_capture_seen = 0U;
static uint32_t s_tach_processed_sequence = 0U;
static uint32_t s_tach_filtered_period_ticks = 0U;
static TickType_t s_save_deadline_tick = 0U;
static uint8_t s_save_dirty = 0U;
static uint8_t s_pwm_running = 0U;

static void FanApp_MarkSettingsDirty(TickType_t now);

static uint8_t FanApp_ClampPercent(uint16_t value)
{
    return (value > 100U) ? 100U : (uint8_t)value;
}

static uint8_t FanApp_ClampPercentFloat(float value)
{
    if (value <= 0.0f)
    {
        return 0U;
    }

    if (value >= 100.0f)
    {
        return 100U;
    }

    return (uint8_t)(value + 0.5f);
}

static uint16_t FanApp_ClampRangeU16(uint16_t value, uint16_t min, uint16_t max)
{
    if (value < min)
    {
        return min;
    }

    if (value > max)
    {
        return max;
    }

    return value;
}

static uint16_t FanApp_NormalizeAutoOffMinutes(uint16_t minutes)
{
    if (minutes == FAN_AUTO_OFF_MINUTES_DISABLED)
    {
        return FAN_AUTO_OFF_MINUTES_DISABLED;
    }

    return FanApp_ClampRangeU16(minutes, FAN_AUTO_OFF_MINUTES_MIN, FAN_AUTO_OFF_MINUTES_MAX);
}

static TickType_t FanApp_AutoOffMinutesToTicks(uint16_t minutes)
{
    return (TickType_t)minutes * pdMS_TO_TICKS(60000U);
}

static fan_mode_t FanApp_NormalizePersistedMode(uint8_t mode)
{
    if ((mode <= (uint8_t)FAN_MODE_OFF) || (mode >= (uint8_t)FAN_MODE_COUNT))
    {
        return FAN_MODE_MANUAL;
    }

    return (fan_mode_t)mode;
}

static void FanApp_GetPersistedSettings(FanApp_PersistedSettings_t *settings)
{
    memset(settings, 0, sizeof(*settings));
    settings->mode = FanApp_NormalizePersistedMode((uint8_t)s_state.last_mode);
    settings->base_speed_percent = s_state.base_speed_percent;
    settings->intensity_percent = s_state.intensity_percent;
    settings->smart_temp_threshold_x10 = s_state.smart_temp_threshold_x10;
    settings->smart_hum_threshold_percent = s_state.smart_hum_threshold_percent;
    settings->natural_amplitude_percent = s_state.natural_amplitude_percent;
    settings->auto_off_min = s_state.auto_off_min;
}

static void FanApp_ToStorage(const FanApp_PersistedSettings_t *settings,
                             FanApp_Storage_t *storage)
{
    memset(storage, 0, sizeof(*storage));
    storage->version = FAN_APP_STORAGE_VERSION;
    storage->mode = (uint8_t)settings->mode;
    storage->base_speed_percent = settings->base_speed_percent;
    storage->intensity_percent = settings->intensity_percent;
    storage->smart_temp_threshold_x10 = settings->smart_temp_threshold_x10;
    storage->smart_hum_threshold_percent = settings->smart_hum_threshold_percent;
    storage->natural_amplitude_percent = settings->natural_amplitude_percent;
    storage->auto_off_min = settings->auto_off_min;
}

static void FanApp_FromStorage(const FanApp_Storage_t *storage)
{
    s_state.power_on = 0U;
    s_state.mode = FAN_MODE_OFF;
    s_state.last_mode = FanApp_NormalizePersistedMode(storage->mode);
    s_state.base_speed_percent = FanApp_ClampPercent(storage->base_speed_percent);
    s_state.intensity_percent = FanApp_ClampPercent(storage->intensity_percent);
    s_state.smart_temp_threshold_x10 =
        FanApp_ClampRangeU16(storage->smart_temp_threshold_x10, 180U, 360U);
    s_state.smart_hum_threshold_percent =
        FanApp_ClampPercent(storage->smart_hum_threshold_percent);
    s_state.natural_amplitude_percent =
        (uint8_t)FanApp_ClampRangeU16(storage->natural_amplitude_percent, 0U, 60U);
    s_state.auto_off_min = FanApp_NormalizeAutoOffMinutes(storage->auto_off_min);
}

static bool FanApp_SaveSettings(const FanApp_PersistedSettings_t *settings)
{
    FanApp_Storage_t storage;

    FanApp_ToStorage(settings, &storage);
    return AppConfig_Save(OFF_FAN_SETTINGS, &storage, (uint16_t)sizeof(storage));
}

static bool FanApp_LoadSettings(void)
{
    FanApp_Storage_t storage;
    FanApp_PersistedSettings_t loaded;
    FanApp_PersistedSettings_t normalized;

    if (!AppConfig_Load(OFF_FAN_SETTINGS, &storage, (uint16_t)sizeof(storage)) ||
        (storage.version != FAN_APP_STORAGE_VERSION))
    {
        return false;
    }

    memset(&loaded, 0, sizeof(loaded));
    loaded.mode = (fan_mode_t)storage.mode;
    loaded.base_speed_percent = storage.base_speed_percent;
    loaded.intensity_percent = storage.intensity_percent;
    loaded.smart_temp_threshold_x10 = storage.smart_temp_threshold_x10;
    loaded.smart_hum_threshold_percent = storage.smart_hum_threshold_percent;
    loaded.natural_amplitude_percent = storage.natural_amplitude_percent;
    loaded.auto_off_min = storage.auto_off_min;

    FanApp_FromStorage(&storage);
    FanApp_GetPersistedSettings(&normalized);
    if (memcmp(&loaded, &normalized, sizeof(loaded)) != 0)
    {
        FanApp_MarkSettingsDirty(xTaskGetTickCount());
    }
    return true;
}

static void FanApp_MarkSettingsDirty(TickType_t now)
{
    s_save_dirty = 1U;
    s_save_deadline_tick = now + pdMS_TO_TICKS(FAN_APP_SAVE_DELAY_MS);
}

static void FanApp_ResetPwmAlgorithm(void)
{
    PwmFan_Init(&s_pwm_fan);
    s_pwm_last_tick = 0U;
    s_pwm_cached_percent = 0U;
}

static void FanApp_ResetTachState(void)
{
    s_tach_last_capture = 0U;
    s_tach_period_ticks = 0U;
    s_tach_last_edge_tick = 0U;
    s_tach_sequence = 0U;
    s_tach_capture_seen = 0U;
    s_tach_processed_sequence = 0U;
    s_tach_filtered_period_ticks = 0U;
}

static void FanApp_ResetTachCapture(void)
{
    s_tach_last_capture = 0U;
    s_tach_period_ticks = 0U;
    s_tach_last_edge_tick = 0U;
    s_tach_capture_seen = 0U;
    s_tach_sequence++;
}

static uint8_t FanApp_IsPwmAlgorithmMode(fan_mode_t mode)
{
    return ((mode == FAN_MODE_MANUAL) ||
            (mode == FAN_MODE_SMART) ||
            (mode == FAN_MODE_NATURAL)) ? 1U : 0U;
}

static PwmFanMode_t FanApp_ToPwmFanMode(fan_mode_t mode)
{
    switch (mode)
    {
    case FAN_MODE_SMART:
        return PWM_FAN_MODE_SMART;
    case FAN_MODE_NATURAL:
        return PWM_FAN_MODE_NATURAL;
    case FAN_MODE_MANUAL:
    default:
        return PWM_FAN_MODE_NORMAL;
    }
}

static uint16_t FanApp_PercentToCompare(uint8_t percent)
{
    uint32_t period_counts = (uint32_t)FAN_APP_PWM_AUTO_RELOAD + 1UL;

    return (uint16_t)(((uint32_t)(100U - percent) * period_counts) / 100U);
}

static bool FanApp_EnsurePwmRunning(void)
{
    if (s_pwm_running != 0U)
    {
        return true;
    }

    if (HAL_TIM_PWM_Start(&htim17, TIM_CHANNEL_1) != HAL_OK)
    {
        return false;
    }

    s_pwm_running = 1U;
    return true;
}

static void FanApp_ApplyHardware(uint8_t output_percent)
{
    uint16_t compare = FanApp_PercentToCompare(output_percent);

    if (output_percent == 0U)
    {
        HAL_GPIO_WritePin(FAN_EN_GPIO_Port, FAN_EN_Pin, GPIO_PIN_RESET);

        if (s_pwm_running != 0U)
        {
            if (HAL_TIM_PWM_Stop(&htim17, TIM_CHANNEL_1) == HAL_OK)
            {
                s_pwm_running = 0U;
            }
        }

        __HAL_TIM_SET_COMPARE(&htim17, TIM_CHANNEL_1, compare);
        taskENTER_CRITICAL();
        s_state.pwm_compare = compare;
        taskEXIT_CRITICAL();
        return;
    }

    __HAL_TIM_SET_COMPARE(&htim17, TIM_CHANNEL_1, compare);
    taskENTER_CRITICAL();
    s_state.pwm_compare = compare;
    taskEXIT_CRITICAL();

    if (!FanApp_EnsurePwmRunning())
    {
        HAL_GPIO_WritePin(FAN_EN_GPIO_Port, FAN_EN_Pin, GPIO_PIN_RESET);
        return;
    }

    if (output_percent != 0U)
    {
        HAL_GPIO_WritePin(FAN_EN_GPIO_Port, FAN_EN_Pin, GPIO_PIN_SET);
    }
}

static void FanApp_UpdatePwmAlgorithmConfig(const fan_state_t *state, uint8_t scaled_base_percent)
{
    PwmFanConfig_t config = s_pwm_fan.config;
    int16_t temp_center_x10 = (int16_t)state->smart_temp_threshold_x10;
    float natural_scale;

    config.temp_min = (float)(temp_center_x10 - FAN_APP_SMART_TEMP_SPAN_X10) / 10.0f;
    config.temp_max = (float)(temp_center_x10 + FAN_APP_SMART_TEMP_SPAN_X10) / 10.0f;
    config.min_base_duty = (float)scaled_base_percent * 0.70f;
    config.max_base_duty = (float)scaled_base_percent * 1.45f;

    if (config.min_base_duty < config.absolute_min_duty)
    {
        config.min_base_duty = config.absolute_min_duty;
    }
    if (config.max_base_duty < config.min_base_duty)
    {
        config.max_base_duty = config.min_base_duty;
    }
    if (config.max_base_duty > config.absolute_max_duty)
    {
        config.max_base_duty = config.absolute_max_duty;
    }

    natural_scale = (float)state->natural_amplitude_percent /
                    (float)FAN_APP_NATURAL_AMPLITUDE_DEFAULT;
    config.weather_amp_1 = 10.0f * natural_scale;
    config.weather_amp_2 = 4.0f * natural_scale;
    config.turb_amplitude = 5.0f * natural_scale;
    config.gust_amp_min = 15.0f * natural_scale;
    config.gust_amp_max = 30.0f * natural_scale;

    PwmFan_Configure(&s_pwm_fan, &config);
}

static uint8_t FanApp_PwmAlgorithmTarget(const fan_state_t *state, TickType_t now)
{
    uint8_t scaled_base = (uint8_t)(((uint16_t)state->base_speed_percent *
                                     (uint16_t)state->intensity_percent) /
                                    100U);
    float ambient_temp;
    float duty;

    if (scaled_base == 0U)
    {
        s_pwm_last_tick = 0U;
        s_pwm_cached_percent = 0U;
        return 0U;
    }

    if ((s_pwm_last_tick != 0U) &&
        ((TickType_t)(now - s_pwm_last_tick) < pdMS_TO_TICKS(FAN_APP_PWM_ALGO_PERIOD_MS)))
    {
        return s_pwm_cached_percent;
    }

    FanApp_UpdatePwmAlgorithmConfig(state, scaled_base);
    PwmFan_SetTarget(&s_pwm_fan, FanApp_ToPwmFanMode(state->mode), (float)scaled_base);

    ambient_temp = (float)state->current_temp_x10 / 10.0f;
    duty = PwmFan_ProcessTick100ms(&s_pwm_fan, ambient_temp);
    if ((state->mode == FAN_MODE_SMART) &&
        (state->current_hum_percent > state->smart_hum_threshold_percent))
    {
        float hum_boost = (float)(state->current_hum_percent -
                                  state->smart_hum_threshold_percent) *
                          FAN_APP_SMART_HUM_BOOST_PER_PERCENT;

        if (hum_boost > FAN_APP_SMART_HUM_BOOST_MAX)
        {
            hum_boost = FAN_APP_SMART_HUM_BOOST_MAX;
        }
        duty += hum_boost;
    }
    s_pwm_cached_percent = FanApp_ClampPercentFloat(duty);
    s_pwm_last_tick = now;

    return s_pwm_cached_percent;
}

static void FanApp_SetDefaults(void)
{
    memset(&s_state, 0, sizeof(s_state));
    s_state.mode = FAN_MODE_OFF;
    s_state.last_mode = FAN_MODE_MANUAL;
    s_state.base_speed_percent = FAN_APP_DEFAULT_BASE_SPEED;
    s_state.intensity_percent = FAN_APP_DEFAULT_INTENSITY;
    s_state.smart_temp_threshold_x10 = FAN_APP_DEFAULT_SMART_TEMP_X10;
    s_state.smart_hum_threshold_percent = FAN_APP_DEFAULT_SMART_HUM_PERCENT;
    s_state.natural_amplitude_percent = FAN_APP_DEFAULT_NATURAL_AMPLITUDE;
    s_state.auto_off_min = FAN_AUTO_OFF_MINUTES_DEFAULT;
    s_boost_until_tick = 0U;
    s_auto_off_deadline_tick = 0U;
    s_auto_off_timer_active = 0U;
    FanApp_ResetPwmAlgorithm();
}

static void FanApp_StartBoost(TickType_t now)
{
    s_boost_until_tick = now + pdMS_TO_TICKS(FAN_APP_START_BOOST_MS);
}

static void FanApp_ResetAutoOffTimer(TickType_t now)
{
    if ((s_state.power_on != 0U) && (s_state.auto_off_min > 0U))
    {
        s_auto_off_deadline_tick = now + FanApp_AutoOffMinutesToTicks(s_state.auto_off_min);
        s_auto_off_timer_active = 1U;
        s_state.auto_off_remaining_min = s_state.auto_off_min;
        s_state.auto_off_remaining_s = (uint32_t)s_state.auto_off_min * 60U;
    }
    else
    {
        s_auto_off_deadline_tick = 0U;
        s_auto_off_timer_active = 0U;
        s_state.auto_off_remaining_min = 0U;
        s_state.auto_off_remaining_s = 0U;
    }
}

static void FanApp_SetPowerInternal(bool enable, TickType_t now)
{
    uint8_t was_power_on = s_state.power_on;

    if (!enable)
    {
        if (s_state.mode != FAN_MODE_OFF)
        {
            s_state.last_mode = s_state.mode;
        }

        s_state.power_on = 0U;
        s_state.mode = FAN_MODE_OFF;
        s_state.target_speed_percent = 0U;
        s_state.current_speed_percent = 0U;
        s_state.current_rpm = 0U;
        FanApp_ResetTachCapture();
        s_boost_until_tick = 0U;
        s_auto_off_deadline_tick = 0U;
        s_auto_off_timer_active = 0U;
        s_state.auto_off_remaining_min = 0U;
        s_state.auto_off_remaining_s = 0U;
        FanApp_ResetPwmAlgorithm();
        return;
    }

    s_state.power_on = 1U;
    if (s_state.mode == FAN_MODE_OFF)
    {
        s_state.mode = (s_state.last_mode == FAN_MODE_OFF) ? FAN_MODE_MANUAL : s_state.last_mode;
    }
    s_state.last_mode = s_state.mode;
    s_pwm_last_tick = 0U;
    FanApp_StartBoost(now);
    if (was_power_on == 0U)
    {
        FanApp_ResetAutoOffTimer(now);
    }
}

static void FanApp_SetModeInternal(fan_mode_t mode, TickType_t now)
{
    uint8_t was_power_on;

    if ((uint32_t)mode >= (uint32_t)FAN_MODE_COUNT)
    {
        return;
    }

    if (mode == FAN_MODE_OFF)
    {
        FanApp_SetPowerInternal(false, now);
        return;
    }

    was_power_on = s_state.power_on;
    s_state.power_on = 1U;
    s_state.mode = mode;
    s_state.last_mode = mode;
    s_pwm_last_tick = 0U;
    FanApp_StartBoost(now);
    if (was_power_on == 0U)
    {
        FanApp_ResetAutoOffTimer(now);
    }
}

static void FanApp_ReadSensorSnapshot(int16_t *temp_x10, uint8_t *hum_percent)
{
    float temp;
    float hum;

    taskENTER_CRITICAL();
    temp = g_sensors_environment.temp;
    hum = g_sensors_environment.hum;
    taskEXIT_CRITICAL();

    if (temp != temp)
    {
        temp = 0.0f;
    }
    else if (temp < -20.0f)
    {
        temp = -20.0f;
    }
    else if (temp > 80.0f)
    {
        temp = 80.0f;
    }

    if (hum != hum)
    {
        hum = 0.0f;
    }
    else if (hum < 0.0f)
    {
        hum = 0.0f;
    }
    else if (hum > 100.0f)
    {
        hum = 100.0f;
    }

    *temp_x10 = (int16_t)(temp * 10.0f);
    *hum_percent = (uint8_t)(hum + 0.5f);
}

static uint8_t FanApp_ModeTarget(const fan_state_t *state, TickType_t now)
{
    switch (state->mode)
    {
    case FAN_MODE_OFF:
        return 0U;
    case FAN_MODE_MANUAL:
    case FAN_MODE_NATURAL:
    case FAN_MODE_SMART:
        return FanApp_PwmAlgorithmTarget(state, now);
    default:
        return 0U;
    }
}

static void FanApp_UpdateAutoOff(TickType_t now)
{
    if ((s_state.power_on == 0U) || (s_auto_off_timer_active == 0U))
    {
        s_state.auto_off_remaining_min = 0U;
        s_state.auto_off_remaining_s = 0U;
        return;
    }

    if ((int32_t)(now - s_auto_off_deadline_tick) >= 0)
    {
        s_state.auto_off_remaining_min = 0U;
        s_state.auto_off_remaining_s = 0U;
        return;
    }

    {
        TickType_t ticks_left = s_auto_off_deadline_tick - now;
        TickType_t ticks_per_second = pdMS_TO_TICKS(1000U);
        uint32_t remaining_s = ((uint32_t)ticks_left + (uint32_t)ticks_per_second - 1U) /
                               (uint32_t)ticks_per_second;

        s_state.auto_off_remaining_s = remaining_s;
        s_state.auto_off_remaining_min = (uint16_t)((remaining_s + 59U) / 60U);
    }
}

void FanApp_Init(void)
{
    FanApp_PersistedSettings_t settings;

    FanApp_SetDefaults();
    FanApp_ResetTachState();
    s_pwm_running = 0U;
    s_save_dirty = 0U;
    s_save_deadline_tick = 0U;

    if (FanApp_LoadSettings())
    {
        log_printf("[Fan] settings load ok");
    }
    else
    {
        FanApp_GetPersistedSettings(&settings);
        if (FanApp_SaveSettings(&settings))
        {
            log_printf("[Fan] settings defaults saved");
        }
        else
        {
            FanApp_MarkSettingsDirty(xTaskGetTickCount());
            log_printf("[Fan] settings use defaults");
        }
    }

    __HAL_TIM_SET_COUNTER(&htim2, 0U);
    (void)HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_1);
    FanApp_ApplyHardware(0U);
}

void FanApp_AttachCommandQueue(QueueHandle_t queue)
{
    s_command_queue = queue;
}

bool FanApp_SendCommand(const fan_cmd_t *cmd, TickType_t timeout)
{
    if ((s_command_queue == NULL) || (cmd == NULL))
    {
        return false;
    }

    return (xQueueSend(s_command_queue, cmd, timeout) == pdPASS);
}

bool FanApp_SetPower(bool enable, TickType_t timeout)
{
    return FanApp_SetPowerWithFlags(enable, 0U, timeout);
}

bool FanApp_SetPowerWithFlags(bool enable, uint8_t flags, TickType_t timeout)
{
    fan_cmd_t cmd = { FAN_CMD_SET_POWER, enable ? 1U : 0U, flags };
    return FanApp_SendCommand(&cmd, timeout);
}

bool FanApp_SetMode(fan_mode_t mode, TickType_t timeout)
{
    fan_cmd_t cmd = { FAN_CMD_SET_MODE, (uint16_t)mode };
    return FanApp_SendCommand(&cmd, timeout);
}

bool FanApp_SetSpeed(uint8_t percent, TickType_t timeout)
{
    fan_cmd_t cmd = { FAN_CMD_SET_SPEED, FanApp_ClampPercent(percent) };
    return FanApp_SendCommand(&cmd, timeout);
}

bool FanApp_SetIntensity(uint8_t percent, TickType_t timeout)
{
    fan_cmd_t cmd = { FAN_CMD_SET_INTENSITY, FanApp_ClampPercent(percent) };
    return FanApp_SendCommand(&cmd, timeout);
}

bool FanApp_SetSmartTempThreshold(uint16_t temp_x10, TickType_t timeout)
{
    fan_cmd_t cmd = { FAN_CMD_SET_SMART_TEMP, temp_x10 };
    return FanApp_SendCommand(&cmd, timeout);
}

bool FanApp_SetSmartHumidityThreshold(uint8_t percent, TickType_t timeout)
{
    fan_cmd_t cmd = { FAN_CMD_SET_SMART_HUM, FanApp_ClampPercent(percent) };
    return FanApp_SendCommand(&cmd, timeout);
}

bool FanApp_SetNaturalAmplitude(uint8_t percent, TickType_t timeout)
{
    fan_cmd_t cmd = { FAN_CMD_SET_NATURAL_AMPLITUDE, FanApp_ClampPercent(percent) };
    return FanApp_SendCommand(&cmd, timeout);
}

bool FanApp_SetAutoOffMinutes(uint16_t minutes, TickType_t timeout)
{
    fan_cmd_t cmd = { FAN_CMD_SET_AUTO_OFF_MIN, minutes };
    return FanApp_SendCommand(&cmd, timeout);
}

void FanApp_ForceStop(void)
{
    taskENTER_CRITICAL();
    if (s_state.mode != FAN_MODE_OFF)
    {
        s_state.last_mode = s_state.mode;
    }
    s_state.power_on = 0U;
    s_state.mode = FAN_MODE_OFF;
    s_state.target_speed_percent = 0U;
    s_state.current_speed_percent = 0U;
    s_state.current_rpm = 0U;
    s_boost_until_tick = 0U;
    s_auto_off_deadline_tick = 0U;
    s_auto_off_timer_active = 0U;
    s_state.auto_off_remaining_min = 0U;
    s_state.auto_off_remaining_s = 0U;
    FanApp_ResetTachCapture();
    FanApp_ResetPwmAlgorithm();
    taskEXIT_CRITICAL();
    UserMonitor_StopFanAutoOff();
    FanApp_ApplyHardware(0U);
}

void FanApp_GetState(fan_state_t *out_state)
{
    if (out_state == NULL)
    {
        return;
    }

    taskENTER_CRITICAL();
    *out_state = s_state;
    taskEXIT_CRITICAL();
}

void FanApp_UpdateRpm(TickType_t now)
{
    uint32_t period_ticks;
    uint32_t sequence;
    TickType_t last_edge_tick;
    int32_t edge_age_ticks;
    uint8_t capture_seen;
    uint8_t power_on;
    uint16_t rpm = 0U;

    taskENTER_CRITICAL();
    period_ticks = s_tach_period_ticks;
    sequence = s_tach_sequence;
    last_edge_tick = s_tach_last_edge_tick;
    capture_seen = s_tach_capture_seen;
    power_on = s_state.power_on;
    taskEXIT_CRITICAL();

    edge_age_ticks = (int32_t)(now - last_edge_tick);

    if ((power_on == 0U) ||
        ((capture_seen != 0U) &&
         (edge_age_ticks >= (int32_t)pdMS_TO_TICKS(FAN_APP_TACH_TIMEOUT_MS))))
    {
        taskENTER_CRITICAL();
        s_tach_capture_seen = 0U;
        s_tach_period_ticks = 0U;
        s_state.current_rpm = 0U;
        taskEXIT_CRITICAL();
        s_tach_filtered_period_ticks = 0U;
        s_tach_processed_sequence = sequence;
        return;
    }

    if ((capture_seen == 0U) || (period_ticks == 0U))
    {
        s_tach_filtered_period_ticks = 0U;
        s_tach_processed_sequence = sequence;
        taskENTER_CRITICAL();
        s_state.current_rpm = 0U;
        taskEXIT_CRITICAL();
        return;
    }

    if (sequence != s_tach_processed_sequence)
    {
        if (s_tach_filtered_period_ticks == 0U)
        {
            s_tach_filtered_period_ticks = period_ticks;
        }
        else
        {
            s_tach_filtered_period_ticks =
                (uint32_t)((((uint64_t)s_tach_filtered_period_ticks * 3ULL) +
                            (uint64_t)period_ticks + 2ULL) /
                           4ULL);
        }
        s_tach_processed_sequence = sequence;
    }

    if (s_tach_filtered_period_ticks > 0U)
    {
        uint64_t denominator = (uint64_t)s_tach_filtered_period_ticks * FAN_APP_TACH_PULSES_PER_REV;
        uint64_t rpm_value = ((uint64_t)FAN_APP_TACH_TIMER_HZ * 60ULL + (denominator / 2ULL)) /
                             denominator;

        rpm = (rpm_value > UINT16_MAX) ? UINT16_MAX : (uint16_t)rpm_value;
    }

    taskENTER_CRITICAL();
    s_state.current_rpm = rpm;
    taskEXIT_CRITICAL();
}

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
    uint32_t capture;

    if ((htim == NULL) || (htim->Instance != TIM2) ||
        (htim->Channel != HAL_TIM_ACTIVE_CHANNEL_1))
    {
        return;
    }

    capture = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
    if (s_tach_capture_seen != 0U)
    {
        uint32_t period_ticks = capture - s_tach_last_capture;

        if (period_ticks > 0U)
        {
            s_tach_period_ticks = period_ticks;
            s_tach_last_edge_tick = xTaskGetTickCountFromISR();
            s_tach_sequence++;
        }
    }
    else
    {
        s_tach_capture_seen = 1U;
        s_tach_last_edge_tick = xTaskGetTickCountFromISR();
    }

    s_tach_last_capture = capture;
}

uint8_t FanApp_GetModeCount(void)
{
    return (uint8_t)FAN_MODE_COUNT;
}

const char *FanApp_GetModeName(fan_mode_t mode)
{
    if ((uint32_t)mode >= (uint32_t)FAN_MODE_COUNT)
    {
        return "Unknown";
    }

    return s_mode_names[mode];
}

fan_mode_t FanApp_ModeFromIndex(uint8_t index)
{
    if (index >= (uint8_t)FAN_MODE_COUNT)
    {
        return FAN_MODE_MANUAL;
    }

    return (fan_mode_t)index;
}

uint8_t FanApp_ModeToIndex(fan_mode_t mode)
{
    if ((uint32_t)mode >= (uint32_t)FAN_MODE_COUNT)
    {
        return (uint8_t)FAN_MODE_MANUAL;
    }

    return (uint8_t)mode;
}

void FanApp_HandleCommand(const fan_cmd_t *cmd)
{
    TickType_t now = xTaskGetTickCount();
    FanApp_PersistedSettings_t before;
    FanApp_PersistedSettings_t after;
    uint16_t auto_off_before;
    uint16_t monitor_auto_off_min;
    uint8_t power_before;
    uint8_t monitor_power_on;
    uint8_t sync_auto_off_monitor;
    uint8_t force_save = 0U;

    if (cmd == NULL)
    {
        return;
    }

    taskENTER_CRITICAL();
    FanApp_GetPersistedSettings(&before);
    power_before = s_state.power_on;
    auto_off_before = s_state.auto_off_min;

    switch (cmd->type)
    {
    case FAN_CMD_SET_POWER:
        FanApp_SetPowerInternal(cmd->value != 0U, now);
        break;

    case FAN_CMD_SET_MODE:
        FanApp_SetModeInternal((fan_mode_t)cmd->value, now);
        break;

    case FAN_CMD_SET_SPEED:
        {
            uint8_t value = FanApp_ClampPercent(cmd->value);
            if (s_state.base_speed_percent != value)
            {
                s_state.base_speed_percent = value;
                s_pwm_last_tick = 0U;
            }
        }
        break;

    case FAN_CMD_SET_INTENSITY:
        {
            uint8_t value = FanApp_ClampPercent(cmd->value);
            if (s_state.intensity_percent != value)
            {
                s_state.intensity_percent = value;
                s_pwm_last_tick = 0U;
            }
        }
        break;

    case FAN_CMD_SET_SMART_TEMP:
        {
            uint16_t value = FanApp_ClampRangeU16(cmd->value, 180U, 360U);
            if (s_state.smart_temp_threshold_x10 != value)
            {
                s_state.smart_temp_threshold_x10 = value;
                s_pwm_last_tick = 0U;
            }
        }
        break;

    case FAN_CMD_SET_SMART_HUM:
        s_state.smart_hum_threshold_percent = FanApp_ClampPercent(cmd->value);
        break;

    case FAN_CMD_SET_NATURAL_AMPLITUDE:
        {
            uint8_t value = (uint8_t)FanApp_ClampRangeU16(cmd->value, 0U, 60U);
            if (s_state.natural_amplitude_percent != value)
            {
                s_state.natural_amplitude_percent = value;
                s_pwm_last_tick = 0U;
            }
        }
        break;

    case FAN_CMD_SET_AUTO_OFF_MIN:
        {
            uint16_t value = FanApp_NormalizeAutoOffMinutes(cmd->value);
            if (s_state.auto_off_min != value)
            {
                s_state.auto_off_min = value;
                FanApp_ResetAutoOffTimer(now);
            }
        }
        break;

    case FAN_CMD_RESET_DEFAULTS:
        FanApp_SetDefaults();
        FanApp_ResetTachCapture();
        force_save = 1U;
        break;

    case FAN_CMD_AUTO_OFF_EXPIRED:
        if ((s_state.power_on != 0U) &&
            (s_auto_off_timer_active != 0U) &&
            (s_state.auto_off_min != FAN_AUTO_OFF_MINUTES_DISABLED) &&
            ((int32_t)(now - s_auto_off_deadline_tick) >= 0))
        {
            FanApp_SetPowerInternal(false, now);
        }
        break;

    default:
        break;
    }

    FanApp_GetPersistedSettings(&after);
    if ((force_save != 0U) || (memcmp(&before, &after, sizeof(before)) != 0))
    {
        FanApp_MarkSettingsDirty(now);
    }

    sync_auto_off_monitor = (uint8_t)((power_before != s_state.power_on) ||
                                      (auto_off_before != s_state.auto_off_min));
    monitor_power_on = s_state.power_on;
    monitor_auto_off_min = s_state.auto_off_min;

    taskEXIT_CRITICAL();

    if (sync_auto_off_monitor != 0U)
    {
        if ((monitor_power_on != 0U) &&
            (monitor_auto_off_min != FAN_AUTO_OFF_MINUTES_DISABLED))
        {
            UserMonitor_RestartFanAutoOff(monitor_auto_off_min);
        }
        else
        {
            UserMonitor_StopFanAutoOff();
        }
    }
}

void FanApp_PersistPending(TickType_t now)
{
    FanApp_PersistedSettings_t settings;
    TickType_t save_deadline = 0U;
    uint8_t save_due = 0U;

    taskENTER_CRITICAL();
    if ((s_save_dirty != 0U) &&
        ((int32_t)(now - s_save_deadline_tick) >= 0))
    {
        FanApp_GetPersistedSettings(&settings);
        save_deadline = s_save_deadline_tick;
        save_due = 1U;
    }
    taskEXIT_CRITICAL();

    if (save_due == 0U)
    {
        return;
    }

    if (FanApp_SaveSettings(&settings))
    {
        taskENTER_CRITICAL();
        if (s_save_deadline_tick == save_deadline)
        {
            s_save_dirty = 0U;
        }
        taskEXIT_CRITICAL();
        log_printf("[Fan] settings saved");
    }
    else
    {
        taskENTER_CRITICAL();
        s_save_deadline_tick = now + pdMS_TO_TICKS(FAN_APP_SAVE_DELAY_MS);
        taskEXIT_CRITICAL();
    }
}

void FanApp_Service(TickType_t now)
{
    fan_state_t snapshot;
    TickType_t boost_until_tick;
    int16_t current_temp_x10;
    uint8_t target;
    uint8_t scaled_target;
    uint8_t current;
    uint8_t current_hum_percent;
    uint8_t output_percent;

    FanApp_ReadSensorSnapshot(&current_temp_x10, &current_hum_percent);

    taskENTER_CRITICAL();
    s_state.current_temp_x10 = current_temp_x10;
    s_state.current_hum_percent = current_hum_percent;
    FanApp_UpdateAutoOff(now);
    snapshot = s_state;
    boost_until_tick = s_boost_until_tick;
    taskEXIT_CRITICAL();

    if (snapshot.power_on == 0U)
    {
        taskENTER_CRITICAL();
        s_state.target_speed_percent = 0U;
        s_state.current_speed_percent = 0U;
        taskEXIT_CRITICAL();
        FanApp_ApplyHardware(0U);
        return;
    }

    target = FanApp_ModeTarget(&snapshot, now);
    if (FanApp_IsPwmAlgorithmMode(snapshot.mode) != 0U)
    {
        scaled_target = target;
    }
    else
    {
        scaled_target = (uint8_t)(((uint16_t)target * snapshot.intensity_percent) / 100U);
    }

    current = snapshot.current_speed_percent;

    if ((boost_until_tick != 0U) && (now < boost_until_tick) &&
        (scaled_target > 0U))
    {
        if (scaled_target < FAN_APP_START_BOOST_PERCENT)
        {
            scaled_target = FAN_APP_START_BOOST_PERCENT;
        }
        if (current < FAN_APP_START_BOOST_PERCENT)
        {
            current = FAN_APP_START_BOOST_PERCENT;
        }
    }

    if (current < scaled_target)
    {
        uint16_t next = (uint16_t)current + 3U;
        current = (next > scaled_target) ? scaled_target : (uint8_t)next;
    }
    else if (current > scaled_target)
    {
        current = (current > (scaled_target + 4U)) ? (uint8_t)(current - 4U) : scaled_target;
    }

    taskENTER_CRITICAL();
    if (s_state.power_on != 0U)
    {
        s_state.target_speed_percent = scaled_target;
        s_state.current_speed_percent = current;
        output_percent = current;
    }
    else
    {
        s_state.target_speed_percent = 0U;
        s_state.current_speed_percent = 0U;
        output_percent = 0U;
    }
    taskEXIT_CRITICAL();
    FanApp_ApplyHardware(output_percent);
}
