#include "sensors_app.h"

#include <string.h>

#include "eeprom_app.h"
#include "FreeRTOS.h"
#include "i2c.h"
#include "task.h"

#define SENSOR_DATA_STALE_MS   5000U
#define SENSOR_MOTION_STALE_MS 300U

static int32_t generic_i2c_write(void *handle, uint8_t reg, const uint8_t *data, uint16_t len)
{
    generic_i2c_t *dev = (generic_i2c_t *)handle;

    return (I2C_Mem_Write(dev->bus, (uint8_t)dev->dev_addr, reg, data, len) == HAL_OK) ? 0 : -1;
}

static int32_t generic_i2c_read(void *handle, uint8_t reg, uint8_t *data, uint16_t len)
{
    generic_i2c_t *dev = (generic_i2c_t *)handle;

    return (I2C_Mem_Read(dev->bus, (uint8_t)dev->dev_addr, reg, data, len) == HAL_OK) ? 0 : -1;
}

static int32_t sht40_i2c_write(void *handle, uint8_t reg, const uint8_t *data, uint16_t len)
{
    generic_i2c_t *dev = (generic_i2c_t *)handle;

    (void)reg;
    return (I2C_Write(dev->bus, (uint8_t)dev->dev_addr, data, len) == HAL_OK) ? 0 : -1;
}

static int32_t sht40_i2c_read(void *handle, uint8_t reg, uint8_t *data, uint16_t len)
{
    generic_i2c_t *dev = (generic_i2c_t *)handle;

    (void)reg;
    return (I2C_Read(dev->bus, (uint8_t)dev->dev_addr, data, len) == HAL_OK) ? 0 : -1;
}

I2C_Bus_t i2c_bus_1 = { .hi2c = &hi2c1 };
I2C_Bus_t i2c_bus_2 = { .hi2c = &hi2c2 };

static generic_i2c_t dev_lis3dh = {
    .bus = &i2c_bus_1,
    .dev_addr = LIS3DH_ADDR,
};

static generic_i2c_t dev_sht40 = {
    .bus = &i2c_bus_2,
    .dev_addr = SHT40_ADDR,
};

static generic_i2c_t dev_max17048 = {
    .bus = &i2c_bus_2,
    .dev_addr = MAX17048_ADDR,
};

static generic_i2c_t dev_bq24295 = {
    .bus = &i2c_bus_2,
    .dev_addr = BQ24295_ADDR,
};

static generic_i2c_t dev_ina226 = {
    .bus = &i2c_bus_2,
    .dev_addr = INA226_ADDR,
};

static bq24295_module_t s_charger = {0};
static ina226_module_t s_ina226 = {0};
static battery_module_t s_battery = {0};
static env_module_t s_environment = {0};
static motion_module_t s_motion = {0};
static sensors_snapshot_t s_snapshot = {0};

static void SensorsApp_RecordResult(sensor_health_t *health, bool success, TickType_t now)
{
    if (success)
    {
        health->valid = 1U;
        health->stale = 0U;
        health->consecutive_failures = 0U;
        health->last_success_tick = now;
    }
    else if (health->consecutive_failures < UINT16_MAX)
    {
        health->consecutive_failures++;
    }
}

static void SensorsApp_UpdateStale(sensor_health_t *health,
                                   TickType_t now,
                                   TickType_t stale_ticks)
{
    if ((health->valid != 0U) &&
        ((TickType_t)(now - health->last_success_tick) >= stale_ticks))
    {
        health->stale = 1U;
    }
}

static int32_t lis3dh_manual_init(lis3dh_ctx_t *ctx)
{
    uint8_t data;
    int32_t status = 0;

    data = 0x80U;
    status |= ctx->write_reg(ctx->handle, LIS3DH_CTRL_REG5, &data, 1U);
    vTaskDelay(pdMS_TO_TICKS(10U));

    data = 0x67U;
    status |= ctx->write_reg(ctx->handle, LIS3DH_CTRL_REG1, &data, 1U);

    data = 0x80U;
    status |= ctx->write_reg(ctx->handle, LIS3DH_CTRL_REG4, &data, 1U);

    data = 0x01U;
    status |= ctx->write_reg(ctx->handle, LIS3DH_CTRL_REG2, &data, 1U);

    data = 0x7FU;
    status |= ctx->write_reg(ctx->handle, LIS3DH_INT1_CFG, &data, 1U);

    data = 8U;
    status |= ctx->write_reg(ctx->handle, LIS3DH_INT1_THS, &data, 1U);

    data = 1U;
    status |= ctx->write_reg(ctx->handle, LIS3DH_INT1_DUR, &data, 1U);

    data = 0x40U;
    status |= ctx->write_reg(ctx->handle, LIS3DH_CTRL_REG3, &data, 1U);
    return status;
}

static bool Init_Motion(motion_module_t *m)
{
    m->ctx.handle = &dev_lis3dh;
    m->ctx.write_reg = generic_i2c_write;
    m->ctx.read_reg = generic_i2c_read;
    return (I2C_IsDeviceReady(&i2c_bus_1, LIS3DH_ADDR, 2U, 100U) == HAL_OK) &&
           (lis3dh_manual_init(&m->ctx) == 0);
}

static bool Init_Env(env_module_t *m)
{
    m->ctx.handle = &dev_sht40;
    m->ctx.write_reg = sht40_i2c_write;
    m->ctx.read_reg = sht40_i2c_read;
    if ((I2C_IsDeviceReady(&i2c_bus_2, SHT40_ADDR, 2U, 100U) != HAL_OK) ||
        (sht40_soft_reset(&m->ctx) != 0))
    {
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(2U));
    return true;
}

static bool Init_Battery(battery_module_t *m)
{
    m->ctx.handle = &dev_max17048;
    m->ctx.write_reg = generic_i2c_write;
    m->ctx.read_reg = generic_i2c_read;
    return (I2C_IsDeviceReady(&i2c_bus_2, MAX17048_ADDR, 2U, 100U) == HAL_OK) &&
           (max17048_init(&m->ctx) == 0);
}

static bool Init_Charger(bq24295_module_t *m)
{
    m->ctx.handle = &dev_bq24295;
    m->ctx.write_reg = generic_i2c_write;
    m->ctx.read_reg = generic_i2c_read;
    return (I2C_IsDeviceReady(&i2c_bus_2, BQ24295_ADDR, 2U, 100U) == HAL_OK) &&
           (bq24295_init(&m->ctx) == 0);
}

static bool Init_INA226(ina226_module_t *m)
{
    m->ctx.handle = &dev_ina226;
    m->ctx.write_reg = generic_i2c_write;
    m->ctx.read_reg = generic_i2c_read;
    m->ctx.r_shunt = 0.01f;
    return (I2C_IsDeviceReady(&i2c_bus_2, INA226_ADDR, 2U, 100U) == HAL_OK) &&
           (ina226_init(&m->ctx) == 0);
}

bool SensorsApp_UpdateMotion(void)
{
    int16_t raw[3];
    bool success;
    TickType_t now = xTaskGetTickCount();

    success = (lis3dh_acceleration_raw_get(&s_motion.ctx, raw) == 0);
    if (success)
    {
        uint8_t back_buf = (uint8_t)(1U - s_motion.buf_idx);

        s_motion.x[back_buf] = raw[0];
        s_motion.y[back_buf] = raw[1];
        s_motion.z[back_buf] = raw[2];
        s_motion.buf_idx = back_buf;
    }

    taskENTER_CRITICAL();
    if (success)
    {
        s_snapshot.motion.value.x = raw[0];
        s_snapshot.motion.value.y = raw[1];
        s_snapshot.motion.value.z = raw[2];
    }
    SensorsApp_RecordResult(&s_snapshot.motion.health, success, now);
    taskEXIT_CRITICAL();
    return success;
}

bool SensorsApp_UpdateEnvironment(void)
{
    uint16_t raw_t;
    uint16_t raw_h;
    float temp = 0.0f;
    float hum = 0.0f;
    bool success;
    TickType_t now = xTaskGetTickCount();

    success = (sht40_read_raw(&s_environment.ctx, &raw_t, &raw_h) == 0);
    if (success)
    {
        temp = sht40_convert_temp(raw_t);
        hum = sht40_convert_hum(raw_h);
        s_environment.temp = temp;
        s_environment.hum = hum;
    }

    taskENTER_CRITICAL();
    if (success)
    {
        s_snapshot.environment.value.temp = temp;
        s_snapshot.environment.value.hum = hum;
    }
    SensorsApp_RecordResult(&s_snapshot.environment.health, success, now);
    taskEXIT_CRITICAL();
    return success;
}

bool SensorsApp_UpdateBattery(void)
{
    float soc;
    float voltage_mv;
    bool success;
    TickType_t now = xTaskGetTickCount();

    success = (max17048_get_soc(&s_battery.ctx, &soc) == 0) &&
              (max17048_get_vcell(&s_battery.ctx, &voltage_mv) == 0);
    if (success)
    {
        if (soc < 0.0f)
        {
            soc = 0.0f;
        }
        else if (soc > 100.0f)
        {
            soc = 100.0f;
        }
        s_battery.soc = soc;
        s_battery.voltage = voltage_mv / 1000.0f;
    }

    taskENTER_CRITICAL();
    if (success)
    {
        s_snapshot.battery.value.soc = s_battery.soc;
        s_snapshot.battery.value.voltage = s_battery.voltage;
    }
    SensorsApp_RecordResult(&s_snapshot.battery.health, success, now);
    taskEXIT_CRITICAL();
    return success;
}

bool SensorsApp_UpdateCharger(void)
{
    uint8_t status;
    uint8_t fault;
    uint8_t chg;

    bool success;
    TickType_t now = xTaskGetTickCount();

    success = (bq24295_get_status(&s_charger.ctx, &status) == 0) &&
              (bq24295_get_fault(&s_charger.ctx, &fault) == 0);
    if (!success)
    {
        taskENTER_CRITICAL();
        SensorsApp_RecordResult(&s_snapshot.charger.health, false, now);
        taskEXIT_CRITICAL();
        return false;
    }

    s_charger.reg_status = status;
    s_charger.reg_fault = fault;
    s_charger.input_present = ((status & 0xC0U) != 0U);
    s_charger.power_good = ((status & 0x04U) != 0U);

    chg = (uint8_t)(status & 0x30U);
    switch (chg)
    {
    case 0x10U:
        s_charger.state = BQ_CHG_PRECHARGE;
        s_charger.charging = true;
        break;
    case 0x20U:
        s_charger.state = BQ_CHG_FAST_CHARGE;
        s_charger.charging = true;
        break;
    case 0x30U:
        s_charger.state = BQ_CHG_DONE;
        s_charger.charging = false;
        break;
    default:
        s_charger.state = BQ_CHG_IDLE;
        s_charger.charging = false;
        break;
    }

    s_charger.fault_thermal = ((fault & (1U << 0)) != 0U);
    s_charger.fault_timer = ((fault & (1U << 1)) != 0U);
    s_charger.fault_watchdog = ((fault & (1U << 2)) != 0U);
    s_charger.fault_input = ((fault & (1U << 3)) != 0U);

    taskENTER_CRITICAL();
    s_snapshot.charger.value.state = s_charger.state;
    s_snapshot.charger.value.charging = s_charger.charging ? 1U : 0U;
    s_snapshot.charger.value.power_good = s_charger.power_good ? 1U : 0U;
    s_snapshot.charger.value.input_present = s_charger.input_present ? 1U : 0U;
    s_snapshot.charger.value.fault_thermal = s_charger.fault_thermal ? 1U : 0U;
    s_snapshot.charger.value.fault_timer = s_charger.fault_timer ? 1U : 0U;
    s_snapshot.charger.value.fault_watchdog = s_charger.fault_watchdog ? 1U : 0U;
    s_snapshot.charger.value.fault_input = s_charger.fault_input ? 1U : 0U;
    SensorsApp_RecordResult(&s_snapshot.charger.health, true, now);
    taskEXIT_CRITICAL();
    return true;
}

bool SensorsApp_UpdateINA226(void)
{
    float voltage_mv;
    float current_ma;

    bool success;
    TickType_t now = xTaskGetTickCount();

    success = (ina226_get_bus_voltage_mv(&s_ina226.ctx, &voltage_mv) == 0) &&
              (ina226_get_current_ma(&s_ina226.ctx, &current_ma) == 0);
    if (success)
    {
        s_ina226.voltage = voltage_mv;
        s_ina226.current = current_ma;
        s_ina226.power = (voltage_mv * current_ma) / 1000.0f;
    }

    taskENTER_CRITICAL();
    if (success)
    {
        s_snapshot.ina226.value.voltage = s_ina226.voltage;
        s_snapshot.ina226.value.current = s_ina226.current;
        s_snapshot.ina226.value.power = s_ina226.power;
    }
    SensorsApp_RecordResult(&s_snapshot.ina226.health, success, now);
    taskEXIT_CRITICAL();
    return success;
}

uint32_t APP_Sensors_Init(void)
{
    uint32_t result = 0U;

    I2C_Bus_Init(&i2c_bus_1);
    I2C_Bus_Init(&i2c_bus_2);
    memset(&s_snapshot, 0, sizeof(s_snapshot));

    if (Init_Motion(&s_motion)) result |= SENSORS_INIT_MOTION_OK;
    if (Init_Env(&s_environment)) result |= SENSORS_INIT_ENV_OK;
    if (Init_Battery(&s_battery)) result |= SENSORS_INIT_BATTERY_OK;
    if (Init_Charger(&s_charger)) result |= SENSORS_INIT_CHARGER_OK;
    if (Init_INA226(&s_ina226)) result |= SENSORS_INIT_INA226_OK;

    return result;
}

int32_t APP_Storage_Init(void)
{
    I2C_Bus_Init(&i2c_bus_1);

    return AppConfig_Init(&g_ee_ctx, &i2c_bus_1, EE24_ADDRESS_DEFAULT) ? 0 : -1;
}

int32_t APP_Sensors_Update(void)
{
    (void)SensorsApp_UpdateMotion();
    (void)SensorsApp_UpdateEnvironment();
    (void)SensorsApp_UpdateBattery();
    (void)SensorsApp_UpdateCharger();
    (void)SensorsApp_UpdateINA226();

    return 0;
}

void SensorsApp_GetSnapshot(sensors_snapshot_t *out)
{
    TickType_t now;

    if (out == NULL)
    {
        return;
    }

    now = xTaskGetTickCount();
    taskENTER_CRITICAL();
    *out = s_snapshot;
    taskEXIT_CRITICAL();

    SensorsApp_UpdateStale(&out->environment.health, now,
                           pdMS_TO_TICKS(SENSOR_DATA_STALE_MS));
    SensorsApp_UpdateStale(&out->battery.health, now,
                           pdMS_TO_TICKS(SENSOR_DATA_STALE_MS));
    SensorsApp_UpdateStale(&out->charger.health, now,
                           pdMS_TO_TICKS(SENSOR_DATA_STALE_MS));
    SensorsApp_UpdateStale(&out->ina226.health, now,
                           pdMS_TO_TICKS(SENSOR_DATA_STALE_MS));
    SensorsApp_UpdateStale(&out->motion.health, now,
                           pdMS_TO_TICKS(SENSOR_MOTION_STALE_MS));
}
