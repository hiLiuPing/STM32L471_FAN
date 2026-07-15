#include "ina226_reg.h"

/* ================= 16bit 写 ================= */
static int32_t ina226_write16(ina226_ctx_t *ctx, uint8_t reg, uint16_t val)
{
    uint8_t buf[2];

    buf[0] = (uint8_t)(val >> 8);
    buf[1] = (uint8_t)(val & 0xFF);

    return ctx->write_reg(ctx->handle, reg, buf, 2);
}

/* ================= 16bit 读 ================= */
static int32_t ina226_read16(ina226_ctx_t *ctx, uint8_t reg, uint16_t *val)
{
    uint8_t buf[2];

    if (ctx->read_reg(ctx->handle, reg, buf, 2) != 0)
        return -1;

    *val = ((uint16_t)buf[0] << 8) | buf[1];
    return 0;
}

/* ================= 自动计算校准 ================= */
static void ina226_calc_calibration(ina226_ctx_t *ctx)
{
    /* ===== 设定最大电流 ===== */
    float max_current = 2.0f;   // 2A full scale

    /* ===== CURRENT_LSB (A/bit) ===== */
    ctx->current_lsb = max_current / 32768.0f;

    /* ===== POWER LSB ===== */
    ctx->power_lsb = 25.0f * ctx->current_lsb;

    /* ===== CALIBRATION ===== */
    ctx->calibration_val =
        (uint16_t)(0.00512f / (ctx->current_lsb * ctx->r_shunt));
}

/* ================= INIT ================= */
int32_t ina226_init(ina226_ctx_t *ctx)
{
    if (!ctx)
        return -1;

    /* 默认分流电阻（必须设置，否则不准） */
    if (ctx->r_shunt == 0)
        ctx->r_shunt = 0.01f; // 10mΩ默认

    /* 自动计算 calibration + LSB */
    ina226_calc_calibration(ctx);

    /* Average across multiple PWM periods before publishing a measurement. */
    if (ina226_write16(ctx, INA226_REG_CONFIG, INA226_CONFIG_STABLE_POWER) != 0)
        return -1;

    /* 写校准值 */
    if (ina226_write16(ctx, INA226_REG_CALIBRATION,
                       ctx->calibration_val) != 0)
        return -1;

    return 0;
}

/* ================= 电压 ================= */
int32_t ina226_get_bus_voltage_mv(ina226_ctx_t *ctx, float *mv)
{
    uint16_t raw;

    if (ina226_read16(ctx, INA226_REG_BUS_VOLT, &raw) != 0)
        return -1;

    *mv = raw * 1.25f;
    return 0;
}

/* ================= 电流（关键修复点） ================= */
int32_t ina226_get_current_ma(ina226_ctx_t *ctx, float *ma)
{
    uint16_t raw;

    if (ina226_read16(ctx, INA226_REG_CURRENT, &raw) != 0)
        return -1;

    int16_t sraw = (int16_t)raw;

    /* ===== 标准换算 ===== */
    float current_a = sraw * ctx->current_lsb;

    *ma = current_a * 1000.0f;

    return 0;
}
