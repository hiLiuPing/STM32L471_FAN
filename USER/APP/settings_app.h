#ifndef __SETTINGS_APP_H__
#define __SETTINGS_APP_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"

#define SETTINGS_APP_POETRY_INTERVAL_MIN_DEFAULT       1U
#define SETTINGS_APP_POETRY_INTERVAL_MIN_MIN           1U
#define SETTINGS_APP_POETRY_INTERVAL_MIN_MAX           60U

#define SETTINGS_APP_POETRY_DURATION_S_DEFAULT         30U
#define SETTINGS_APP_POETRY_DURATION_S_MIN             30U
#define SETTINGS_APP_POETRY_DURATION_S_MAX             300U

#define SETTINGS_APP_WEATHER_SYNC_INTERVAL_MIN_DISABLED 0U
#define SETTINGS_APP_WEATHER_SYNC_INTERVAL_MIN_DEFAULT 60U
#define SETTINGS_APP_WEATHER_SYNC_INTERVAL_MIN_MIN     30U
#define SETTINGS_APP_WEATHER_SYNC_INTERVAL_MIN_MAX     300U

#define SETTINGS_APP_BRIGHTNESS_DAY_PERCENT            95U
#define SETTINGS_APP_BRIGHTNESS_NIGHT_PERCENT          50U
#define SETTINGS_APP_BRIGHTNESS_MIN                    1U
#define SETTINGS_APP_BRIGHTNESS_MAX                    100U

#define SETTINGS_APP_RGB_PWR_ENABLED_DEFAULT           1U

#define SETTINGS_APP_HOME_THEME_DEFAULT                SETTINGS_APP_HOME_THEME_1

typedef enum
{
    SETTINGS_APP_HOME_THEME_1 = 1,
    SETTINGS_APP_HOME_THEME_2 = 2
} SettingsApp_HomeTheme_t;

#define SETTINGS_APP_SCREEN_IDLE_TIMEOUT_MIN_DISABLED  0U
#define SETTINGS_APP_SCREEN_IDLE_TIMEOUT_MIN_DEFAULT   5U
#define SETTINGS_APP_SCREEN_IDLE_TIMEOUT_MIN_MIN       1U
#define SETTINGS_APP_SCREEN_IDLE_TIMEOUT_MIN_MAX       360U

#define SETTINGS_APP_SYSTEM_AUTO_OFF_MIN_DISABLED      0U
#define SETTINGS_APP_SYSTEM_AUTO_OFF_MIN_DEFAULT       480U
#define SETTINGS_APP_SYSTEM_AUTO_OFF_MIN_MIN           30U
#define SETTINGS_APP_SYSTEM_AUTO_OFF_MIN_MAX           480U

typedef struct
{
    uint8_t poetry_popup_enabled;
    uint16_t poetry_popup_interval_min;
    uint16_t poetry_popup_duration_s;
    uint16_t weather_time_sync_interval_min;
    uint8_t rgb_pwr_enabled;
    uint16_t screen_idle_timeout_min;
    uint16_t system_auto_off_min;
    uint8_t home_theme;
} AppSettings_t;

void SettingsApp_Init(void);
void SettingsApp_Get(AppSettings_t *out);
bool SettingsApp_Update(const AppSettings_t *next);
void SettingsApp_PersistPending(TickType_t now);
bool SettingsApp_PersistNow(void);
void SettingsApp_Apply(void);
uint16_t SettingsApp_GetWeatherTimeSyncIntervalMin(void);
uint16_t SettingsApp_GetScreenIdleTimeoutMin(void);
uint16_t SettingsApp_GetSystemAutoOffMin(void);
uint8_t SettingsApp_GetHomeTheme(void);
uint8_t SettingsApp_GetActiveBrightnessPercent(void);
void SettingsApp_ApplyActiveBrightness(void);

#ifdef __cplusplus
}
#endif

#endif /* __SETTINGS_APP_H__ */
