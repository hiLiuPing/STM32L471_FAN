#include "settings_app.h"

#include <stddef.h>
#include <string.h>

#include "data_app.h"
#include "eeprom_app.h"
#include "lcd_init.h"
#include "led_app.h"
#include "log.h"
#include "systemMonitor_app.h"
#include "task.h"
#include "time_utils.h"
#include "ui_poetry_popup.h"
#include "weather_app.h"

#define SETTINGS_APP_STORAGE_VERSION 6U
#define SETTINGS_APP_SAVE_DELAY_MS   1000U

#pragma pack(push, 1)
typedef struct
{
    uint32_t magic;
    uint8_t version;
    uint8_t poetry_popup_enabled;
    uint16_t poetry_popup_interval_min;
    uint16_t weather_time_sync_interval_min;
    uint8_t rgb_pwr_enabled;
    uint8_t reserved_0;
    uint16_t screen_idle_timeout_min;
    uint16_t poetry_popup_duration_s;
    uint16_t system_auto_off_min;
    uint8_t home_theme;
    uint8_t reserved[235];
    uint16_t crc;
} SettingsApp_Storage_t;
#pragma pack(pop)

_Static_assert(sizeof(SettingsApp_Storage_t) == EE_BLOCK_SIZE,
               "Settings storage must remain one EEPROM block");
_Static_assert(offsetof(SettingsApp_Storage_t, poetry_popup_enabled) == 5U,
               "Settings V6 poetry enable offset changed");
_Static_assert(offsetof(SettingsApp_Storage_t, poetry_popup_interval_min) == 6U,
               "Settings V6 poetry interval offset changed");
_Static_assert(offsetof(SettingsApp_Storage_t, poetry_popup_duration_s) == 14U,
               "Settings V6 poetry duration offset changed");
_Static_assert(offsetof(SettingsApp_Storage_t, system_auto_off_min) == 16U,
               "Settings V6 system auto-off offset changed");
_Static_assert(offsetof(SettingsApp_Storage_t, home_theme) == 18U,
               "Settings V6 home theme offset changed");
_Static_assert(offsetof(SettingsApp_Storage_t, crc) == (EE_BLOCK_SIZE - sizeof(uint16_t)),
               "Settings CRC must remain at the end of the EEPROM block");

static AppSettings_t s_settings;
static uint8_t s_settings_initialized = 0U;
static TickType_t s_save_deadline_tick = 0U;
static uint8_t s_save_dirty = 0U;

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

static uint16_t SettingsApp_ClampOptionalU16(uint16_t value,
                                             uint16_t disabled_value,
                                             uint16_t min_value,
                                             uint16_t max_value)
{
    if (value == disabled_value)
    {
        return disabled_value;
    }
    return SettingsApp_ClampU16(value, min_value, max_value);
}

static void SettingsApp_LoadDefaults(AppSettings_t *settings)
{
    if (settings == NULL)
    {
        return;
    }

    settings->poetry_popup_interval_min = SETTINGS_APP_POETRY_INTERVAL_MIN_DEFAULT;
    settings->poetry_popup_duration_s = SETTINGS_APP_POETRY_DURATION_S_DEFAULT;
    settings->weather_time_sync_interval_min = SETTINGS_APP_WEATHER_SYNC_INTERVAL_MIN_DEFAULT;
    settings->rgb_pwr_enabled = SETTINGS_APP_RGB_PWR_ENABLED_DEFAULT;
    settings->screen_idle_timeout_min = SETTINGS_APP_SCREEN_IDLE_TIMEOUT_MIN_DEFAULT;
    settings->system_auto_off_min = SETTINGS_APP_SYSTEM_AUTO_OFF_MIN_DEFAULT;
    settings->home_theme = SETTINGS_APP_HOME_THEME_DEFAULT;
}

static void SettingsApp_Normalize(AppSettings_t *settings)
{
    if (settings == NULL)
    {
        return;
    }

    settings->poetry_popup_interval_min =
        SettingsApp_ClampOptionalU16(settings->poetry_popup_interval_min,
                                     SETTINGS_APP_POETRY_INTERVAL_MIN_DISABLED,
                                     SETTINGS_APP_POETRY_INTERVAL_MIN_MIN,
                                     SETTINGS_APP_POETRY_INTERVAL_MIN_MAX);
    settings->poetry_popup_duration_s = SettingsApp_ClampU16(settings->poetry_popup_duration_s,
                                                              SETTINGS_APP_POETRY_DURATION_S_MIN,
                                                              SETTINGS_APP_POETRY_DURATION_S_MAX);
    settings->weather_time_sync_interval_min =
        SettingsApp_ClampOptionalU16(settings->weather_time_sync_interval_min,
                                     SETTINGS_APP_WEATHER_SYNC_INTERVAL_MIN_DISABLED,
                                     SETTINGS_APP_WEATHER_SYNC_INTERVAL_MIN_MIN,
                                     SETTINGS_APP_WEATHER_SYNC_INTERVAL_MIN_MAX);
    settings->rgb_pwr_enabled = (settings->rgb_pwr_enabled != 0U) ? 1U : 0U;
    settings->screen_idle_timeout_min =
        SettingsApp_ClampOptionalU16(settings->screen_idle_timeout_min,
                                     SETTINGS_APP_SCREEN_IDLE_TIMEOUT_MIN_DISABLED,
                                     SETTINGS_APP_SCREEN_IDLE_TIMEOUT_MIN_MIN,
                                     SETTINGS_APP_SCREEN_IDLE_TIMEOUT_MIN_MAX);
    settings->system_auto_off_min =
        SettingsApp_ClampOptionalU16(settings->system_auto_off_min,
                                     SETTINGS_APP_SYSTEM_AUTO_OFF_MIN_DISABLED,
                                     SETTINGS_APP_SYSTEM_AUTO_OFF_MIN_MIN,
                                     SETTINGS_APP_SYSTEM_AUTO_OFF_MIN_MAX);
    settings->home_theme = (settings->home_theme == SETTINGS_APP_HOME_THEME_2) ?
                               SETTINGS_APP_HOME_THEME_2 :
                               SETTINGS_APP_HOME_THEME_1;
}

static void SettingsApp_FromStorage(const SettingsApp_Storage_t *storage, AppSettings_t *settings)
{
    if ((storage == NULL) || (settings == NULL))
    {
        return;
    }

    settings->poetry_popup_interval_min = storage->poetry_popup_interval_min;
    if (storage->poetry_popup_enabled == 0U)
    {
        settings->poetry_popup_interval_min = SETTINGS_APP_POETRY_INTERVAL_MIN_DISABLED;
    }
    settings->poetry_popup_duration_s = storage->poetry_popup_duration_s;
    settings->weather_time_sync_interval_min = storage->weather_time_sync_interval_min;
    settings->rgb_pwr_enabled = storage->rgb_pwr_enabled;
    settings->screen_idle_timeout_min = storage->screen_idle_timeout_min;
    settings->system_auto_off_min = storage->system_auto_off_min;
    settings->home_theme = storage->home_theme;
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
    storage->poetry_popup_enabled =
        (settings->poetry_popup_interval_min != SETTINGS_APP_POETRY_INTERVAL_MIN_DISABLED) ? 1U : 0U;
    storage->poetry_popup_interval_min = settings->poetry_popup_interval_min;
    storage->poetry_popup_duration_s = settings->poetry_popup_duration_s;
    storage->weather_time_sync_interval_min = settings->weather_time_sync_interval_min;
    storage->rgb_pwr_enabled = settings->rgb_pwr_enabled;
    storage->screen_idle_timeout_min = settings->screen_idle_timeout_min;
    storage->system_auto_off_min = settings->system_auto_off_min;
    storage->home_theme = settings->home_theme;
}

static bool SettingsApp_StorageIsCanonical(const SettingsApp_Storage_t *storage,
                                           const AppSettings_t *settings)
{
    SettingsApp_Storage_t canonical;
    const size_t payload_offset = offsetof(SettingsApp_Storage_t, version);
    const size_t payload_size = offsetof(SettingsApp_Storage_t, crc) - payload_offset;

    if ((storage == NULL) || (settings == NULL))
    {
        return false;
    }

    SettingsApp_ToStorage(settings, &canonical);
    return memcmp((const uint8_t *)storage + payload_offset,
                  (const uint8_t *)&canonical + payload_offset,
                  payload_size) == 0;
}

static void SettingsApp_MarkDirty(TickType_t now)
{
    s_save_dirty = 1U;
    s_save_deadline_tick = now + pdMS_TO_TICKS(SETTINGS_APP_SAVE_DELAY_MS);
}

static void SettingsApp_EnsureInitializedUnlocked(void)
{
    if (s_settings_initialized == 0U)
    {
        SettingsApp_LoadDefaults(&s_settings);
        SettingsApp_Normalize(&s_settings);
        s_settings_initialized = 1U;
    }
}

static void SettingsApp_CopySnapshot(AppSettings_t *out)
{
    taskENTER_CRITICAL();
    SettingsApp_EnsureInitializedUnlocked();
    *out = s_settings;
    taskEXIT_CRITICAL();
}

static bool SettingsApp_SaveSnapshot(const AppSettings_t *settings)
{
    SettingsApp_Storage_t storage;

    SettingsApp_ToStorage(settings, &storage);
    return AppConfig_Save(OFF_APP_SETTINGS, &storage, (uint16_t)sizeof(storage));
}

static bool SettingsApp_SaveCurrent(void)
{
    AppSettings_t snapshot;

    SettingsApp_CopySnapshot(&snapshot);
    return SettingsApp_SaveSnapshot(&snapshot);
}

void SettingsApp_Init(void)
{
    SettingsApp_Storage_t storage;
    AppConfig_LoadResult_t load_result;
    bool need_save = false;

    SettingsApp_LoadDefaults(&s_settings);
    load_result = AppConfig_Load(OFF_APP_SETTINGS, &storage, (uint16_t)sizeof(storage));

    if ((load_result == APP_CONFIG_LOAD_OK) &&
        (storage.version == SETTINGS_APP_STORAGE_VERSION))
    {
        SettingsApp_FromStorage(&storage, &s_settings);
        need_save = !SettingsApp_StorageIsCanonical(&storage, &s_settings);
        log_printf("[Settings] load ok");
    }
    else if ((load_result == APP_CONFIG_LOAD_INVALID_DATA) ||
             ((load_result == APP_CONFIG_LOAD_OK) &&
              (storage.version != SETTINGS_APP_STORAGE_VERSION)))
    {
        SettingsApp_Normalize(&s_settings);
        need_save = true;
        log_printf("[Settings] invalid data, reset to defaults");
    }
    else
    {
        SettingsApp_Normalize(&s_settings);
        log_printf("[Settings] storage unavailable result=%d, defaults not persisted",
                   (int)load_result);
    }

    s_settings_initialized = 1U;
    s_save_dirty = 0U;
    s_save_deadline_tick = 0U;
    if (need_save)
    {
        if (!SettingsApp_SaveCurrent())
        {
            taskENTER_CRITICAL();
            SettingsApp_MarkDirty(xTaskGetTickCount());
            taskEXIT_CRITICAL();
            log_printf("[Settings] save pending");
        }
    }
}

void SettingsApp_Get(AppSettings_t *out)
{
    if (out == NULL)
    {
        return;
    }

    SettingsApp_CopySnapshot(out);
}

bool SettingsApp_Update(const AppSettings_t *next)
{
    AppSettings_t normalized;
    uint8_t changed;

    if (next == NULL)
    {
        return false;
    }

    normalized = *next;
    SettingsApp_Normalize(&normalized);
    taskENTER_CRITICAL();
    SettingsApp_EnsureInitializedUnlocked();
    changed = (uint8_t)(memcmp(&s_settings, &normalized, sizeof(s_settings)) != 0);
    s_settings = normalized;
    s_settings_initialized = 1U;
    if (changed != 0U)
    {
        SettingsApp_MarkDirty(xTaskGetTickCount());
    }
    taskEXIT_CRITICAL();

    return true;
}

void SettingsApp_PersistPending(TickType_t now)
{
    AppSettings_t snapshot;
    TickType_t save_deadline = 0U;
    uint8_t save_due = 0U;

    taskENTER_CRITICAL();
    if ((s_save_dirty != 0U) &&
        Time32_Reached((uint32_t)now, (uint32_t)s_save_deadline_tick))
    {
        SettingsApp_EnsureInitializedUnlocked();
        snapshot = s_settings;
        save_deadline = s_save_deadline_tick;
        save_due = 1U;
    }
    taskEXIT_CRITICAL();

    if (save_due == 0U)
    {
        return;
    }

    if (SettingsApp_SaveSnapshot(&snapshot))
    {
        taskENTER_CRITICAL();
        if ((s_save_deadline_tick == save_deadline) &&
            (memcmp(&s_settings, &snapshot, sizeof(s_settings)) == 0))
        {
            s_save_dirty = 0U;
        }
        taskEXIT_CRITICAL();
        log_printf("[Settings] saved");
    }
    else
    {
        taskENTER_CRITICAL();
        SettingsApp_MarkDirty(now);
        taskEXIT_CRITICAL();
        log_printf("[Settings] save retry");
    }
}

bool SettingsApp_PersistNow(void)
{
    AppSettings_t snapshot;
    uint8_t dirty;

    taskENTER_CRITICAL();
    SettingsApp_EnsureInitializedUnlocked();
    dirty = s_save_dirty;
    snapshot = s_settings;
    taskEXIT_CRITICAL();

    if (dirty == 0U)
    {
        return true;
    }
    if (!SettingsApp_SaveSnapshot(&snapshot))
    {
        return false;
    }

    taskENTER_CRITICAL();
    if (memcmp(&s_settings, &snapshot, sizeof(snapshot)) == 0)
    {
        s_save_dirty = 0U;
    }
    taskEXIT_CRITICAL();
    return true;
}

uint8_t SettingsApp_GetActiveBrightnessPercent(void)
{
    uint8_t base_percent;
    uint8_t weather_factor;
    uint16_t adjusted_percent;

    base_percent = (Time_IsDaytime() != 0U) ?
                       SETTINGS_APP_BRIGHTNESS_DAY_PERCENT :
                       SETTINGS_APP_BRIGHTNESS_NIGHT_PERCENT;

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

void SettingsApp_ApplyActiveBrightness(void)
{
    LCD_SetBacklightPercent(SettingsApp_GetActiveBrightnessPercent());
}

void SettingsApp_Apply(void)
{
    AppSettings_t snapshot;
    uint32_t interval_s;

    SettingsApp_CopySnapshot(&snapshot);

    interval_s = (uint32_t)snapshot.poetry_popup_interval_min * 60U;
    if (interval_s > 0xFFFFU)
    {
        interval_s = 0xFFFFU;
    }

    if (snapshot.poetry_popup_interval_min != SETTINGS_APP_POETRY_INTERVAL_MIN_DISABLED)
    {
        ui_poetry_popup_set_timing((uint16_t)interval_s, snapshot.poetry_popup_duration_s);
        ui_poetry_popup_set_enabled(true);
    }
    else
    {
        ui_poetry_popup_set_enabled(false);
    }
    LED_CMD_OnEvent(LED_TARGET_PWR,
                    (snapshot.rgb_pwr_enabled != 0U) ? LED_EVT_RAINBOW : LED_EVT_STOP);
    SettingsApp_ApplyActiveBrightness();
    UserMonitor_ApplySettings();
}

uint16_t SettingsApp_GetWeatherTimeSyncIntervalMin(void)
{
    AppSettings_t snapshot;

    SettingsApp_CopySnapshot(&snapshot);
    return snapshot.weather_time_sync_interval_min;
}

uint16_t SettingsApp_GetScreenIdleTimeoutMin(void)
{
    AppSettings_t snapshot;

    SettingsApp_CopySnapshot(&snapshot);
    return snapshot.screen_idle_timeout_min;
}

uint16_t SettingsApp_GetSystemAutoOffMin(void)
{
    AppSettings_t snapshot;

    SettingsApp_CopySnapshot(&snapshot);
    return snapshot.system_auto_off_min;
}

uint8_t SettingsApp_GetHomeTheme(void)
{
    AppSettings_t snapshot;

    SettingsApp_CopySnapshot(&snapshot);
    return snapshot.home_theme;
}
