#include "sensors_app.h"

#include "eeprom_app.h"
#include "FreeRTOS.h"
#include "i2c.h"
#include "task.h"

static int32_t generic_i2c_write(void *handle, uint8_t reg, const uint8_t *data, uint16_t len)
{
    generic_i2c_t *dev = (generic_i2c_t *)handle;

    return (I2C_Mem_Write(dev->bus, (uint8_t)dev->dev_addr, reg, (uint8_t *)data, len) == HAL_OK) ? 0 : -1;
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

bq24295_module_t g_sensors_charger = {0};
ina226_module_t g_sensors_ina226 = {0};
battery_module_t g_sensors_battery = {0};
env_module_t g_sensors_environment = {0};
motion_module_t g_sensors_motion = {0};

static void lis3dh_manual_init(lis3dh_ctx_t *ctx)
{
    uint8_t data;

    data = 0x80U;
    (void)ctx->write_reg(ctx->handle, LIS3DH_CTRL_REG5, &data, 1U);
    vTaskDelay(pdMS_TO_TICKS(10U));

    data = 0x67U;
    (void)ctx->write_reg(ctx->handle, LIS3DH_CTRL_REG1, &data, 1U);

    data = 0x80U;
    (void)ctx->write_reg(ctx->handle, LIS3DH_CTRL_REG4, &data, 1U);

    data = 0x01U;
    (void)ctx->write_reg(ctx->handle, LIS3DH_CTRL_REG2, &data, 1U);

    data = 0x7FU;
    (void)ctx->write_reg(ctx->handle, LIS3DH_INT1_CFG, &data, 1U);

    data = 8U;
    (void)ctx->write_reg(ctx->handle, LIS3DH_INT1_THS, &data, 1U);

    data = 1U;
    (void)ctx->write_reg(ctx->handle, LIS3DH_INT1_DUR, &data, 1U);

    data = 0x40U;
    (void)ctx->write_reg(ctx->handle, LIS3DH_CTRL_REG3, &data, 1U);
}

static void Init_Motion(motion_module_t *m)
{
    m->ctx.handle = &dev_lis3dh;
    m->ctx.write_reg = generic_i2c_write;
    m->ctx.read_reg = generic_i2c_read;
    lis3dh_manual_init(&m->ctx);
}

static void Init_Env(env_module_t *m)
{
    m->ctx.handle = &dev_sht40;
    m->ctx.write_reg = sht40_i2c_write;
    m->ctx.read_reg = sht40_i2c_read;
    (void)sht40_soft_reset(&m->ctx);
    vTaskDelay(pdMS_TO_TICKS(2U));
}

static void Init_Battery(battery_module_t *m)
{
    m->ctx.handle = &dev_max17048;
    m->ctx.write_reg = generic_i2c_write;
    m->ctx.read_reg = generic_i2c_read;
    (void)max17048_init(&m->ctx);
}

static void Init_Charger(bq24295_module_t *m)
{
    m->ctx.handle = &dev_bq24295;
    m->ctx.write_reg = generic_i2c_write;
    m->ctx.read_reg = generic_i2c_read;
    (void)bq24295_init(&m->ctx);
}

static void Init_INA226(ina226_module_t *m)
{
    m->ctx.handle = &dev_ina226;
    m->ctx.write_reg = generic_i2c_write;
    m->ctx.read_reg = generic_i2c_read;
    m->ctx.r_shunt = 0.01f;
    (void)ina226_init(&m->ctx);
}

void Update_Motion(motion_module_t *m)
{
    int16_t raw[3];

    if ((m != NULL) && (lis3dh_acceleration_raw_get(&m->ctx, raw) == 0))
    {
        uint8_t back_buf = (uint8_t)(1U - m->buf_idx);

        m->x[back_buf] = raw[0];
        m->y[back_buf] = raw[1];
        m->z[back_buf] = raw[2];
    }
}

void Motion_SwapBuffer(motion_module_t *m)
{
    if (m != NULL)
    {
        m->buf_idx = (uint8_t)(1U - m->buf_idx);
    }
}

void Update_Env(env_module_t *m)
{
    uint16_t raw_t;
    uint16_t raw_h;

    if ((m != NULL) && (sht40_read_raw(&m->ctx, &raw_t, &raw_h) == 0))
    {
        m->temp = sht40_convert_temp(raw_t);
        m->hum = sht40_convert_hum(raw_h);
    }
}

void Update_Battery(battery_module_t *m)
{
    float soc;
    float voltage_mv;

    if (m == NULL)
    {
        return;
    }

    if (max17048_get_soc(&m->ctx, &soc) == 0)
    {
        if (soc < 0.0f)
        {
            soc = 0.0f;
        }
        else if (soc > 100.0f)
        {
            soc = 100.0f;
        }
        m->soc = soc;
    }

    if (max17048_get_vcell(&m->ctx, &voltage_mv) == 0)
    {
        m->voltage = voltage_mv / 1000.0f;
    }
}

void Update_Charger(bq24295_module_t *m)
{
    uint8_t status;
    uint8_t fault;
    uint8_t chg;

    if (m == NULL)
    {
        return;
    }

    if (bq24295_get_status(&m->ctx, &status) != 0)
    {
        return;
    }

    if (bq24295_get_fault(&m->ctx, &fault) != 0)
    {
        return;
    }

    m->reg_status = status;
    m->reg_fault = fault;
    m->input_present = ((status & 0xC0U) != 0U);
    m->power_good = ((status & 0x04U) != 0U);

    chg = (uint8_t)(status & 0x30U);
    switch (chg)
    {
    case 0x10U:
        m->state = BQ_CHG_PRECHARGE;
        m->charging = true;
        break;
    case 0x20U:
        m->state = BQ_CHG_FAST_CHARGE;
        m->charging = true;
        break;
    case 0x30U:
        m->state = BQ_CHG_DONE;
        m->charging = false;
        break;
    default:
        m->state = BQ_CHG_IDLE;
        m->charging = false;
        break;
    }

    m->fault_thermal = ((fault & (1U << 0)) != 0U);
    m->fault_timer = ((fault & (1U << 1)) != 0U);
    m->fault_watchdog = ((fault & (1U << 2)) != 0U);
    m->fault_input = ((fault & (1U << 3)) != 0U);
}

void Update_INA226(ina226_module_t *m)
{
    float voltage_mv;
    float current_ma;

    if (m == NULL)
    {
        return;
    }

    if (ina226_get_bus_voltage_mv(&m->ctx, &voltage_mv) == 0)
    {
        m->voltage = voltage_mv;
    }

    if (ina226_get_current_ma(&m->ctx, &current_ma) == 0)
    {
        m->current = current_ma;
    }

    m->power = (m->voltage * m->current) / 1000.0f;
}

int32_t APP_Sensors_Init(void)
{
    I2C_Bus_Init(&i2c_bus_1);
    I2C_Bus_Init(&i2c_bus_2);

    (void)AppConfig_Init(&g_ee_ctx, &i2c_bus_1, EE24_ADDRESS_DEFAULT);
    Init_Motion(&g_sensors_motion);
    Init_Env(&g_sensors_environment);
    Init_Battery(&g_sensors_battery);
    Init_Charger(&g_sensors_charger);
    Init_INA226(&g_sensors_ina226);

    return 0;
}

int32_t APP_Sensors_Update(void)
{
    Update_Motion(&g_sensors_motion);
    Motion_SwapBuffer(&g_sensors_motion);
    Update_Env(&g_sensors_environment);
    Update_Battery(&g_sensors_battery);
    Update_Charger(&g_sensors_charger);
    Update_INA226(&g_sensors_ina226);

    return 0;
}
