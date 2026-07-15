#include "settings_app.h"

#include <string.h>

#include "data_app.h"
#include "eeprom_app.h"
#include "lcd_init.h"
#include "log.h"
#include "ui_poetry_popup.h"
#include "weather_app.h"

#define SETTINGS_APP_STORAGE_VERSION     2U
#define SETTINGS_APP_STORAGE_VERSION_OLD 1U

#pragma pack(push, 1)
typedef struct
{
    uint32_t magic;
    uint8_t version;
    uint8_t poetry_popup_enabled;
    uint16_t poetry_popup_interval_min;
    uint16_t weather_time_sync_interval_min;
    uint8_t brightness_day_percent;
    uint8_t brightness_night_percent;
    uint16_t screen_idle_timeout_min;
    uint8_t reserved[240];
    uint16_t crc;
} SettingsApp_Storage_t;
#pragma pack(pop)

static AppSettings_t s_settings;
static uint8_t s_settings_initialized = 0U;

static uint16_t SettingsApp_ClampU16(uint16_t value, uint16_t min_value, uint16_t max_value)
{
    if (value < min_value)
    {
        return min_value;
    }
    if (value > max_value)
    {
        return max_value;
    }
    return value;
}

static uint8_t SettingsApp_ClampU8(uint8_t value, uint8_t min_value, uint8_t max_value)
{
    if (value < min_value)
    {
        return min_value;
    }
    if (value > max_value)
    {
        return max_value;
    }
    return value;
}

static void SettingsApp_LoadDefaults(AppSettings_t *settings)
{
    if (settings == NULL)
    {
        return;
    }

    settings->poetry_popup_enabled = 1U;
    settings->poetry_popup_interval_min = SETTINGS_APP_POETRY_INTERVAL_MIN_DEFAULT;
    settings->weather_time_sync_interval_min = SETTINGS_APP_WEATHER_SYNC_INTERVAL_MIN_DEFAULT;
    settings->brightness_day_percent = SETTINGS_APP_BRIGHTNESS_DAY_DEFAULT;
    settings->brightness_night_percent = SETTINGS_APP_BRIGHTNESS_NIGHT_DEFAULT;
    settings->screen_idle_timeout_min = SETTINGS_APP_SCREEN_IDLE_TIMEOUT_MIN_DEFAULT;
}

static void SettingsApp_Normalize(AppSettings_t *settings)
{
    if (settings == NULL)
    {
        return;
    }

    settings->poetry_popup_enabled = (settings->poetry_popup_enabled != 0U) ? 1U : 0U;
    settings->poetry_popup_interval_min = SettingsApp_ClampU16(settings->poetry_popup_interval_min,
                                                               SETTINGS_APP_POETRY_INTERVAL_MIN_MIN,
                                                               SETTINGS_APP_POETRY_INTERVAL_MIN_MAX);
    settings->weather_time_sync_interval_min = SettingsApp_ClampU16(settings->weather_time_sync_interval_min,
                                                                     SETTINGS_APP_WEATHER_SYNC_INTERVAL_MIN_MIN,
                                                                     SETTINGS_APP_WEATHER_SYNC_INTERVAL_MIN_MAX);
    settings->brightness_day_percent = SettingsApp_ClampU8(settings->brightness_day_percent,
                                                           SETTINGS_APP_BRIGHTNESS_MIN,
                                                           SETTINGS_APP_BRIGHTNESS_MAX);
    settings->brightness_night_percent = SettingsApp_ClampU8(settings->brightness_night_percent,
                                                             SETTINGS_APP_BRIGHTNESS_MIN,
                                                             SETTINGS_APP_BRIGHTNESS_MAX);
    settings->screen_idle_timeout_min = SettingsApp_ClampU16(settings->screen_idle_timeout_min,
                                                             SETTINGS_APP_SCREEN_IDLE_TIMEOUT_MIN_MIN,
                                                             SETTINGS_APP_SCREEN_IDLE_TIMEOUT_MIN_MAX);
}

static void SettingsApp_FromStorage(const SettingsApp_Storage_t *storage, AppSettings_t *settings)
{
    if ((storage == NULL) || (settings == NULL))
    {
        return;
    }

    settings->poetry_popup_enabled = storage->poetry_popup_enabled;
    settings->poetry_popup_interval_min = storage->poetry_popup_interval_min;
    settings->weather_time_sync_interval_min = storage->weather_time_sync_interval_min;
    settings->brightness_day_percent = storage->brightness_day_percent;
    settings->brightness_night_percent = storage->brightness_night_percent;
    settings->screen_idle_timeout_min = storage->screen_idle_timeout_min;
    SettingsApp_Normalize(settings);
}

static void SettingsApp_ToStorage(const AppSettings_t *settings, SettingsApp_Storage_t *storage)
{
    if ((settings == NULL) || (storage == NULL))
    {
        return;
    }

    memset(storage, 0, sizeof(*storage));
    storage->version = SETTINGS_APP_STORAGE_VERSION;
    storage->poetry_popup_enabled = settings->poetry_popup_enabled;
    storage->poetry_popup_interval_min = settings->poetry_popup_interval_min;
    storage->weather_time_sync_interval_min = settings->weather_time_sync_interval_min;
    storage->brightness_day_percent = settings->brightness_day_percent;
    storage->brightness_night_percent = settings->brightness_night_percent;
    storage->screen_idle_timeout_min = settings->screen_idle_timeout_min;
}

static bool SettingsApp_SaveCurrent(void)
{
    SettingsApp_Storage_t storage;

    SettingsApp_ToStorage(&s_settings, &storage);
    return AppConfig_Save(OFF_APP_SETTINGS, &storage, (uint16_t)sizeof(storage));
}

void SettingsApp_Init(void)
{
    SettingsApp_Storage_t storage;
    bool need_save = false;

    SettingsApp_LoadDefaults(&s_settings);

    if (AppConfig_Load(OFF_APP_SETTINGS, &storage, (uint16_t)sizeof(storage)) &&
        ((storage.version == SETTINGS_APP_STORAGE_VERSION) ||
         (storage.version == SETTINGS_APP_STORAGE_VERSION_OLD)))
    {
        SettingsApp_FromStorage(&storage, &s_settings);
        if (storage.version == SETTINGS_APP_STORAGE_VERSION_OLD)
        {
            s_settings.poetry_popup_interval_min = SETTINGS_APP_POETRY_INTERVAL_MIN_DEFAULT;
            SettingsApp_Normalize(&s_settings);
            need_save = true;
        }
        log_printf("[Settings] load ok");
    }
    else
    {
        SettingsApp_Normalize(&s_settings);
        need_save = true;
        log_printf("[Settings] use defaults");
    }

    s_settings_initialized = 1U;
    if (need_save)
    {
        (void)SettingsApp_SaveCurrent();
    }
}

void SettingsApp_Get(AppSettings_t *out)
{
    if (out == NULL)
    {
        return;
    }

    if (s_settings_initialized == 0U)
    {
        SettingsApp_LoadDefaults(&s_settings);
        SettingsApp_Normalize(&s_settings);
    }

    *out = s_settings;
}

bool SettingsApp_Update(const AppSettings_t *next)
{
    AppSettings_t normalized;

    if (next == NULL)
    {
        return false;
    }

    normalized = *next;
    SettingsApp_Normalize(&normalized);
    s_settings = normalized;
    s_settings_initialized = 1U;

    return SettingsApp_SaveCurrent();
}

uint8_t SettingsApp_GetActiveBrightnessPercent(void)
{
    uint8_t base_percent;
    uint8_t weather_factor;
    uint16_t adjusted_percent;

    if (s_settings_initialized == 0U)
    {
        SettingsApp_LoadDefaults(&s_settings);
        SettingsApp_Normalize(&s_settings);
    }

    base_percent = (Time_IsDaytime() != 0U) ?
                       s_settings.brightness_day_percent :
                       s_settings.brightness_night_percent;

    switch (Weather_GetScene())
    {
    case WEATHER_SCENE_CLOUDY:
    case WEATHER_SCENE_SNOW:
        weather_factor = 90U;
        break;
    case WEATHER_SCENE_LIGHT_RAIN:
        weather_factor = 85U;
        break;
    case WEATHER_SCENE_MODERATE_RAIN:
        weather_factor = 75U;
        break;
    case WEATHER_SCENE_HEAVY_RAIN:
        weather_factor = 65U;
        break;
    case WEATHER_SCENE_CLEAR:
    case WEATHER_SCENE_UNKNOWN:
    default:
        weather_factor = 100U;
        break;
    }

    adjusted_percent = (uint16_t)(((uint16_t)base_percent * weather_factor + 50U) / 100U);
    if (adjusted_percent < SETTINGS_APP_BRIGHTNESS_MIN)
    {
        adjusted_percent = SETTINGS_APP_BRIGHTNESS_MIN;
    }

    return (uint8_t)adjusted_percent;
}

void SettingsApp_PreviewBrightnessPercent(uint8_t percent)
{
    LCD_SetBacklightPercent(SettingsApp_ClampU8(percent,
                                                SETTINGS_APP_BRIGHTNESS_MIN,
                                                SETTINGS_APP_BRIGHTNESS_MAX));
}

void SettingsApp_ApplyActiveBrightness(void)
{
    LCD_SetBacklightPercent(SettingsApp_GetActiveBrightnessPercent());
}

void SettingsApp_Apply(void)
{
    uint32_t interval_s;

    if (s_settings_initialized == 0U)
    {
        SettingsApp_LoadDefaults(&s_settings);
        SettingsApp_Normalize(&s_settings);
    }

    interval_s = (uint32_t)s_settings.poetry_popup_interval_min * 60U;
    if (interval_s > 0xFFFFU)
    {
        interval_s = 0xFFFFU;
    }

    ui_poetry_popup_set_timing((uint16_t)interval_s, UI_POETRY_POPUP_DEFAULT_DURATION_S);
    ui_poetry_popup_set_enabled(s_settings.poetry_popup_enabled != 0U);
    SettingsApp_ApplyActiveBrightness();
}

uint16_t SettingsApp_GetWeatherTimeSyncIntervalMin(void)
{
    if (s_settings_initialized == 0U)
    {
        SettingsApp_LoadDefaults(&s_settings);
        SettingsApp_Normalize(&s_settings);
    }

    return s_settings.weather_time_sync_interval_min;
}

uint16_t SettingsApp_GetScreenIdleTimeoutMin(void)
{
    if (s_settings_initialized == 0U)
    {
        SettingsApp_LoadDefaults(&s_settings);
        SettingsApp_Normalize(&s_settings);
    }

    return s_settings.screen_idle_timeout_min;
}
