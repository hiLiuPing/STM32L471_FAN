#include "fan_app.h"

#include <string.h>

#include "main.h"
#include "sensors_app.h"
#include "tim.h"

#define FAN_APP_DEFAULT_BASE_SPEED          45U
#define FAN_APP_DEFAULT_INTENSITY           80U
#define FAN_APP_DEFAULT_SMART_TEMP_X10      280U
#define FAN_APP_DEFAULT_SMART_HUM_PERCENT   65U
#define FAN_APP_DEFAULT_NATURAL_AMPLITUDE   18U
#define FAN_APP_DEFAULT_RANDOM_AMPLITUDE    12U

#define FAN_APP_START_BOOST_PERCENT         35U
#define FAN_APP_START_BOOST_MS              300U
#define FAN_APP_RANDOM_UPDATE_MS            350U
#define FAN_APP_AUTO_OFF_MAX_MIN            120U
#define FAN_APP_PWM_PERIOD                  3199U

static const char *const s_mode_names[FAN_MODE_COUNT] = {
    "Off",
    "Manual",
    "Natural",
    "Smart",
    "Random",
    "Sleep",
    "Turbo",
    "Eco",
};

static QueueHandle_t s_command_queue = NULL;
static fan_state_t s_state;
static TickType_t s_boost_until_tick = 0U;
static TickType_t s_auto_off_deadline_tick = 0U;
static TickType_t s_last_random_tick = 0U;
static uint32_t s_wave_phase = 0U;
static uint32_t s_rng_state = 0x21524111UL;
static int16_t s_random_offset = 0;

static uint8_t FanApp_ClampPercent(uint16_t value)
{
    return (value > 100U) ? 100U : (uint8_t)value;
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

static uint32_t FanApp_NextRandom(void)
{
    s_rng_state = (s_rng_state * 1664525UL) + 1013904223UL;
    return s_rng_state;
}

static uint16_t FanApp_PercentToCompare(uint8_t percent)
{
    if (percent == 0U)
    {
        return 0U;
    }

    return (uint16_t)(((uint32_t)percent * FAN_APP_PWM_PERIOD) / 100U);
}

static void FanApp_ApplyHardware(uint8_t output_percent)
{
    uint16_t compare = FanApp_PercentToCompare(output_percent);

    __HAL_TIM_SET_COMPARE(&htim17, TIM_CHANNEL_1, compare);
    s_state.pwm_compare = compare;

    if (output_percent == 0U)
    {
        HAL_GPIO_WritePin(FAN_EN_GPIO_Port, FAN_EN_Pin, GPIO_PIN_RESET);
    }
    else
    {
        HAL_GPIO_WritePin(FAN_EN_GPIO_Port, FAN_EN_Pin, GPIO_PIN_SET);
    }
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
    s_state.random_amplitude_percent = FAN_APP_DEFAULT_RANDOM_AMPLITUDE;
    s_boost_until_tick = 0U;
    s_auto_off_deadline_tick = 0U;
    s_last_random_tick = 0U;
    s_wave_phase = 0U;
    s_random_offset = 0;
}

static void FanApp_StartBoost(TickType_t now)
{
    s_boost_until_tick = now + pdMS_TO_TICKS(FAN_APP_START_BOOST_MS);
}

static void FanApp_ResetAutoOffTimer(TickType_t now)
{
    if ((s_state.power_on != 0U) && (s_state.auto_off_min > 0U))
    {
        s_auto_off_deadline_tick = now + pdMS_TO_TICKS((uint32_t)s_state.auto_off_min * 60000UL);
        s_state.auto_off_remaining_min = s_state.auto_off_min;
    }
    else
    {
        s_auto_off_deadline_tick = 0U;
        s_state.auto_off_remaining_min = 0U;
    }
}

static void FanApp_SetPowerInternal(bool enable, TickType_t now)
{
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
        s_boost_until_tick = 0U;
        s_auto_off_deadline_tick = 0U;
        s_state.auto_off_remaining_min = 0U;
        FanApp_ApplyHardware(0U);
        return;
    }

    s_state.power_on = 1U;
    if (s_state.mode == FAN_MODE_OFF)
    {
        s_state.mode = (s_state.last_mode == FAN_MODE_OFF) ? FAN_MODE_MANUAL : s_state.last_mode;
    }
    s_state.last_mode = s_state.mode;
    FanApp_StartBoost(now);
    FanApp_ResetAutoOffTimer(now);
}

static void FanApp_SetModeInternal(fan_mode_t mode, TickType_t now)
{
    if (mode >= FAN_MODE_COUNT)
    {
        return;
    }

    if (mode == FAN_MODE_OFF)
    {
        FanApp_SetPowerInternal(false, now);
        return;
    }

    s_state.power_on = 1U;
    s_state.mode = mode;
    s_state.last_mode = mode;
    FanApp_StartBoost(now);
    FanApp_ResetAutoOffTimer(now);
}

static void FanApp_UpdateSensorSnapshot(void)
{
    float temp = g_sensors_environment.temp;
    float hum = g_sensors_environment.hum;

    if (temp < -20.0f)
    {
        temp = -20.0f;
    }
    else if (temp > 80.0f)
    {
        temp = 80.0f;
    }

    if (hum < 0.0f)
    {
        hum = 0.0f;
    }
    else if (hum > 100.0f)
    {
        hum = 100.0f;
    }

    s_state.current_temp_x10 = (int16_t)(temp * 10.0f);
    s_state.current_hum_percent = (uint8_t)(hum + 0.5f);
}

static uint8_t FanApp_NaturalTarget(void)
{
    uint8_t base = s_state.base_speed_percent;
    uint8_t amplitude = s_state.natural_amplitude_percent;
    uint32_t phase;
    int16_t triangle;
    int16_t offset;

    s_wave_phase = (s_wave_phase + 1U) % 40U;
    phase = s_wave_phase;
    triangle = (phase < 20U) ? (int16_t)phase : (int16_t)(40U - phase);
    offset = (int16_t)(((triangle - 10) * (int16_t)amplitude) / 10);

    if ((int16_t)base + offset < 0)
    {
        return 0U;
    }

    return FanApp_ClampPercent((uint16_t)((int16_t)base + offset));
}

static uint8_t FanApp_RandomTarget(TickType_t now)
{
    uint8_t base = s_state.base_speed_percent;
    uint8_t amplitude = s_state.random_amplitude_percent;

    if ((s_last_random_tick == 0U) ||
        ((TickType_t)(now - s_last_random_tick) >= pdMS_TO_TICKS(FAN_APP_RANDOM_UPDATE_MS)))
    {
        uint32_t span = ((uint32_t)amplitude * 2UL) + 1UL;
        s_last_random_tick = now;
        s_random_offset = (int16_t)(FanApp_NextRandom() % span) - (int16_t)amplitude;
    }

    if ((int16_t)base + s_random_offset < 0)
    {
        return 0U;
    }

    return FanApp_ClampPercent((uint16_t)((int16_t)base + s_random_offset));
}

static uint8_t FanApp_SmartTarget(void)
{
    int16_t temp_delta = s_state.current_temp_x10 - (int16_t)s_state.smart_temp_threshold_x10;
    int16_t hum_delta = (int16_t)s_state.current_hum_percent -
                        (int16_t)s_state.smart_hum_threshold_percent;
    int16_t target = (int16_t)s_state.base_speed_percent;

    if (temp_delta > 0)
    {
        target += temp_delta / 2;
    }
    else
    {
        target += temp_delta / 8;
    }

    if (hum_delta > 0)
    {
        target += hum_delta / 2;
    }

    if (target < 18)
    {
        target = 18;
    }
    else if (target > 100)
    {
        target = 100;
    }

    return (uint8_t)target;
}

static uint8_t FanApp_ModeTarget(TickType_t now)
{
    switch (s_state.mode)
    {
    case FAN_MODE_OFF:
        return 0U;
    case FAN_MODE_MANUAL:
        return s_state.base_speed_percent;
    case FAN_MODE_NATURAL:
        return FanApp_NaturalTarget();
    case FAN_MODE_SMART:
        return FanApp_SmartTarget();
    case FAN_MODE_RANDOM:
        return FanApp_RandomTarget(now);
    case FAN_MODE_SLEEP:
        return FanApp_ClampPercent((uint16_t)(15U + (s_state.base_speed_percent / 5U)));
    case FAN_MODE_TURBO:
        return 100U;
    case FAN_MODE_ECO:
        return FanApp_ClampPercent((uint16_t)((uint16_t)s_state.base_speed_percent * 65U / 100U));
    default:
        return 0U;
    }
}

static void FanApp_UpdateAutoOff(TickType_t now)
{
    if ((s_state.power_on == 0U) || (s_auto_off_deadline_tick == 0U))
    {
        s_state.auto_off_remaining_min = 0U;
        return;
    }

    if (now >= s_auto_off_deadline_tick)
    {
        FanApp_SetPowerInternal(false, now);
        return;
    }

    {
        TickType_t ticks_left = s_auto_off_deadline_tick - now;
        uint32_t remaining_ms = (uint32_t)ticks_left * (uint32_t)portTICK_PERIOD_MS;
        s_state.auto_off_remaining_min = (uint16_t)((remaining_ms + 59999UL) / 60000UL);
    }
}

void FanApp_Init(void)
{
    FanApp_SetDefaults();
    s_rng_state ^= (uint32_t)HAL_GetTick();

    (void)HAL_TIM_PWM_Start(&htim17, TIM_CHANNEL_1);
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
    fan_cmd_t cmd = { FAN_CMD_SET_POWER, enable ? 1U : 0U };
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

bool FanApp_SetRandomAmplitude(uint8_t percent, TickType_t timeout)
{
    fan_cmd_t cmd = { FAN_CMD_SET_RANDOM_AMPLITUDE, FanApp_ClampPercent(percent) };
    return FanApp_SendCommand(&cmd, timeout);
}

bool FanApp_SetAutoOffMinutes(uint16_t minutes, TickType_t timeout)
{
    fan_cmd_t cmd = { FAN_CMD_SET_AUTO_OFF_MIN, minutes };
    return FanApp_SendCommand(&cmd, timeout);
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

uint8_t FanApp_GetModeCount(void)
{
    return (uint8_t)FAN_MODE_COUNT;
}

const char *FanApp_GetModeName(fan_mode_t mode)
{
    if (mode >= FAN_MODE_COUNT)
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
    if (mode >= FAN_MODE_COUNT)
    {
        return (uint8_t)FAN_MODE_MANUAL;
    }

    return (uint8_t)mode;
}

void FanApp_HandleCommand(const fan_cmd_t *cmd)
{
    TickType_t now = xTaskGetTickCount();

    if (cmd == NULL)
    {
        return;
    }

    taskENTER_CRITICAL();

    switch (cmd->type)
    {
    case FAN_CMD_SET_POWER:
        FanApp_SetPowerInternal(cmd->value != 0U, now);
        break;

    case FAN_CMD_SET_MODE:
        FanApp_SetModeInternal((fan_mode_t)cmd->value, now);
        break;

    case FAN_CMD_SET_SPEED:
        s_state.base_speed_percent = FanApp_ClampPercent(cmd->value);
        break;

    case FAN_CMD_SET_INTENSITY:
        s_state.intensity_percent = FanApp_ClampPercent(cmd->value);
        break;

    case FAN_CMD_SET_SMART_TEMP:
        s_state.smart_temp_threshold_x10 = FanApp_ClampRangeU16(cmd->value, 180U, 360U);
        break;

    case FAN_CMD_SET_SMART_HUM:
        s_state.smart_hum_threshold_percent = FanApp_ClampPercent(cmd->value);
        break;

    case FAN_CMD_SET_NATURAL_AMPLITUDE:
        s_state.natural_amplitude_percent = FanApp_ClampRangeU16(cmd->value, 0U, 60U);
        break;

    case FAN_CMD_SET_RANDOM_AMPLITUDE:
        s_state.random_amplitude_percent = FanApp_ClampRangeU16(cmd->value, 0U, 60U);
        break;

    case FAN_CMD_SET_AUTO_OFF_MIN:
        s_state.auto_off_min = FanApp_ClampRangeU16(cmd->value, 0U, FAN_APP_AUTO_OFF_MAX_MIN);
        FanApp_ResetAutoOffTimer(now);
        break;

    case FAN_CMD_RESET_DEFAULTS:
        FanApp_SetDefaults();
        FanApp_ApplyHardware(0U);
        break;

    default:
        break;
    }

    taskEXIT_CRITICAL();
}

void FanApp_Service(TickType_t now)
{
    uint8_t target;
    uint8_t scaled_target;
    uint8_t current;

    taskENTER_CRITICAL();
    FanApp_UpdateSensorSnapshot();
    FanApp_UpdateAutoOff(now);

    if (s_state.power_on == 0U)
    {
        s_state.target_speed_percent = 0U;
        s_state.current_speed_percent = 0U;
        FanApp_ApplyHardware(0U);
        taskEXIT_CRITICAL();
        return;
    }

    target = FanApp_ModeTarget(now);
    scaled_target = (uint8_t)(((uint16_t)target * s_state.intensity_percent) / 100U);

    if ((s_boost_until_tick != 0U) && (now < s_boost_until_tick) &&
        (scaled_target > 0U) && (scaled_target < FAN_APP_START_BOOST_PERCENT))
    {
        scaled_target = FAN_APP_START_BOOST_PERCENT;
    }

    s_state.target_speed_percent = scaled_target;
    current = s_state.current_speed_percent;

    if (current < scaled_target)
    {
        uint16_t next = (uint16_t)current + 3U;
        current = (next > scaled_target) ? scaled_target : (uint8_t)next;
    }
    else if (current > scaled_target)
    {
        current = (current > (scaled_target + 4U)) ? (uint8_t)(current - 4U) : scaled_target;
    }

    s_state.current_speed_percent = current;
    FanApp_ApplyHardware(current);
    taskEXIT_CRITICAL();
}
