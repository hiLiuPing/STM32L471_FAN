#ifndef __FAN_APP_H__
#define __FAN_APP_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

typedef enum {
    FAN_MODE_OFF = 0,
    FAN_MODE_MANUAL,
    FAN_MODE_NATURAL,
    FAN_MODE_SMART,
    FAN_MODE_RANDOM,
    FAN_MODE_SLEEP,
    FAN_MODE_TURBO,
    FAN_MODE_ECO,
    FAN_MODE_COUNT
} fan_mode_t;

typedef enum {
    FAN_CMD_SET_POWER = 0,
    FAN_CMD_SET_MODE,
    FAN_CMD_SET_SPEED,
    FAN_CMD_SET_INTENSITY,
    FAN_CMD_SET_SMART_TEMP,
    FAN_CMD_SET_SMART_HUM,
    FAN_CMD_SET_NATURAL_AMPLITUDE,
    FAN_CMD_SET_RANDOM_AMPLITUDE,
    FAN_CMD_SET_AUTO_OFF_MIN,
    FAN_CMD_RESET_DEFAULTS,
} fan_cmd_type_t;

typedef struct {
    fan_cmd_type_t type;
    uint16_t value;
} fan_cmd_t;

typedef struct {
    uint8_t power_on;
    fan_mode_t mode;
    fan_mode_t last_mode;
    uint8_t base_speed_percent;
    uint8_t target_speed_percent;
    uint8_t current_speed_percent;
    uint8_t intensity_percent;
    uint16_t smart_temp_threshold_x10;
    uint8_t smart_hum_threshold_percent;
    uint8_t natural_amplitude_percent;
    uint8_t random_amplitude_percent;
    uint16_t auto_off_min;
    uint16_t auto_off_remaining_min;
    int16_t current_temp_x10;
    uint8_t current_hum_percent;
    uint16_t pwm_compare;
} fan_state_t;

void FanApp_Init(void);
void FanApp_AttachCommandQueue(QueueHandle_t queue);
bool FanApp_SendCommand(const fan_cmd_t *cmd, TickType_t timeout);

bool FanApp_SetPower(bool enable, TickType_t timeout);
bool FanApp_SetMode(fan_mode_t mode, TickType_t timeout);
bool FanApp_SetSpeed(uint8_t percent, TickType_t timeout);
bool FanApp_SetIntensity(uint8_t percent, TickType_t timeout);
bool FanApp_SetSmartTempThreshold(uint16_t temp_x10, TickType_t timeout);
bool FanApp_SetSmartHumidityThreshold(uint8_t percent, TickType_t timeout);
bool FanApp_SetNaturalAmplitude(uint8_t percent, TickType_t timeout);
bool FanApp_SetRandomAmplitude(uint8_t percent, TickType_t timeout);
bool FanApp_SetAutoOffMinutes(uint16_t minutes, TickType_t timeout);

void FanApp_GetState(fan_state_t *out_state);
uint8_t FanApp_GetModeCount(void);
const char *FanApp_GetModeName(fan_mode_t mode);
fan_mode_t FanApp_ModeFromIndex(uint8_t index);
uint8_t FanApp_ModeToIndex(fan_mode_t mode);

/* FanTask-only executors. Other tasks should use FanApp_Set... or FanApp_SendCommand. */
void FanApp_HandleCommand(const fan_cmd_t *cmd);
void FanApp_Service(TickType_t now);

#ifdef __cplusplus
}
#endif

#endif /* __FAN_APP_H__ */
