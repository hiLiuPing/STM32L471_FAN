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

extern bq24295_module_t g_sensors_charger;
extern ina226_module_t g_sensors_ina226;
extern battery_module_t g_sensors_battery;
extern env_module_t g_sensors_environment;
extern motion_module_t g_sensors_motion;

int32_t APP_Sensors_Init(void);
int32_t APP_Storage_Init(void);
int32_t APP_Sensors_Update(void);

void Update_Env(env_module_t *m);
void Update_Motion(motion_module_t *m);
void Update_Battery(battery_module_t *m);
void Update_Charger(bq24295_module_t *m);
void Update_INA226(ina226_module_t *m);
void Motion_SwapBuffer(motion_module_t *m);

#ifdef __cplusplus
}
#endif

#endif
