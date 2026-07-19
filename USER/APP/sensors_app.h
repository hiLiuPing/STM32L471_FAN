#ifndef __SENSORS_APP_H
#define __SENSORS_APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "bq24295_reg.h"
#include "i2c_bus.h"
#include "ina226_reg.h"
#include "lis3dh_reg.h"
#include "max17048_reg.h"
#include "sht40_reg.h"
#include "FreeRTOS.h"

typedef struct
{
    I2C_Bus_t *bus;
    uint16_t dev_addr;
} generic_i2c_t;

typedef enum
{
    BQ_CHG_IDLE = 0,
    BQ_CHG_PRECHARGE,
    BQ_CHG_FAST_CHARGE,
    BQ_CHG_DONE,
    BQ_CHG_SUSPEND,
    BQ_CHG_FAULT
} bq24295_state_t;

typedef struct
{
    uint8_t reg_status;
    uint8_t reg_fault;
    bq24295_state_t state;
    bool charging;
    bool power_good;
    bool input_present;
    uint16_t vin_limit_mA;
    uint16_t ichg_mA;
    uint16_t vreg_mV;
    bool fault_thermal;
    bool fault_timer;
    bool fault_watchdog;
    bool fault_input;
    bq24295_ctx_t ctx;
} bq24295_module_t;

typedef struct
{
    float current;
    float voltage;
    float power;
    ina226_ctx_t ctx;
} ina226_module_t;

typedef struct
{
    float soc;
    float voltage;
    max17048_ctx_t ctx;
} battery_module_t;

typedef struct
{
    float temp;
    float hum;
    sht40_ctx_t ctx;
} env_module_t;

typedef struct
{
    int16_t x[2];
    int16_t y[2];
    int16_t z[2];
    uint8_t buf_idx;
    lis3dh_ctx_t ctx;
} motion_module_t;

extern I2C_Bus_t i2c_bus_1;
extern I2C_Bus_t i2c_bus_2;

typedef struct
{
    uint8_t valid;
    uint8_t stale;
    uint16_t consecutive_failures;
    TickType_t last_success_tick;
} sensor_health_t;

typedef struct
{
    float temp;
    float hum;
} sensor_environment_value_t;

typedef struct
{
    float soc;
    float voltage;
} sensor_battery_value_t;

typedef struct
{
    bq24295_state_t state;
    uint8_t charging;
    uint8_t power_good;
    uint8_t input_present;
    uint8_t fault_thermal;
    uint8_t fault_timer;
    uint8_t fault_watchdog;
    uint8_t fault_input;
} sensor_charger_value_t;

typedef struct
{
    float current;
    float voltage;
    float power;
} sensor_ina226_value_t;

typedef struct
{
    int16_t x;
    int16_t y;
    int16_t z;
} motion_sample_t;

typedef struct
{
    sensor_environment_value_t value;
    sensor_health_t health;
} sensor_environment_snapshot_t;

typedef struct
{
    sensor_battery_value_t value;
    sensor_health_t health;
} sensor_battery_snapshot_t;

typedef struct
{
    sensor_charger_value_t value;
    sensor_health_t health;
} sensor_charger_snapshot_t;

typedef struct
{
    sensor_ina226_value_t value;
    sensor_health_t health;
} sensor_ina226_snapshot_t;

typedef struct
{
    motion_sample_t value;
    sensor_health_t health;
} sensor_motion_snapshot_t;

typedef struct
{
    sensor_environment_snapshot_t environment;
    sensor_battery_snapshot_t battery;
    sensor_charger_snapshot_t charger;
    sensor_ina226_snapshot_t ina226;
    sensor_motion_snapshot_t motion;
} sensors_snapshot_t;

#define SENSORS_INIT_MOTION_OK  (1UL << 0)
#define SENSORS_INIT_ENV_OK     (1UL << 1)
#define SENSORS_INIT_BATTERY_OK (1UL << 2)
#define SENSORS_INIT_CHARGER_OK (1UL << 3)
#define SENSORS_INIT_INA226_OK  (1UL << 4)
#define SENSORS_INIT_ALL_OK     (SENSORS_INIT_MOTION_OK | SENSORS_INIT_ENV_OK | \
                                 SENSORS_INIT_BATTERY_OK | SENSORS_INIT_CHARGER_OK | \
                                 SENSORS_INIT_INA226_OK)

uint32_t APP_Sensors_Init(void);
int32_t APP_Storage_Init(void);
int32_t APP_Sensors_Update(void);

bool SensorsApp_UpdateEnvironment(void);
bool SensorsApp_UpdateMotion(void);
bool SensorsApp_UpdateBattery(void);
bool SensorsApp_UpdateCharger(void);
bool SensorsApp_UpdateINA226(void);
void SensorsApp_GetSnapshot(sensors_snapshot_t *out);

#ifdef __cplusplus
}
#endif

#endif
