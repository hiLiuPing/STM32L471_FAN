#ifndef UI_MODEL_H
#define UI_MODEL_H

#include <stdint.h>

#include "ui_property_store.h"

typedef enum
{
  UI_FAN_MODE_AUTO = 0,
  UI_FAN_MODE_SLEEP,
  UI_FAN_MODE_MANUAL,
  UI_FAN_MODE_BOOST
} ui_fan_mode_t;

typedef enum
{
  UI_WEATHER_CLEAR = 0,
  UI_WEATHER_RAIN,
  UI_WEATHER_SNOW,
  UI_WEATHER_CLOUD
} ui_weather_mode_t;

typedef struct
{
  uint8_t enabled;
  uint8_t speed_percent;
  uint16_t rpm;
  int16_t temperature_c;
  ui_fan_mode_t mode;
  uint8_t has_fault;
} ui_fan_state_t;

typedef struct
{
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  int8_t timezone_offset;
  uint8_t use_24h;
  uint16_t anim_phase;
  char city[16];
} ui_clock_state_t;

typedef struct
{
  uint8_t brightness_percent;
  uint8_t sound_enabled;
} ui_setting_state_t;

void UI_Model_Init(void);
void UI_Model_Sync(void);
void UI_Model_Tick(uint32_t elapsed_ms);
void UI_Model_SetFanState(const ui_fan_state_t *state);
void UI_Model_GetFanState(ui_fan_state_t *state);
void UI_Model_SetClockState(const ui_clock_state_t *state);
void UI_Model_GetClockState(ui_clock_state_t *state);
void UI_Model_SetSettings(const ui_setting_state_t *state);
void UI_Model_GetSettings(ui_setting_state_t *state);
void UI_Model_SetWeatherMode(ui_weather_mode_t mode);
ui_weather_mode_t UI_Model_GetWeatherMode(void);

#endif /* UI_MODEL_H */
