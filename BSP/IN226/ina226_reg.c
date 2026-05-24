#include "ina226_reg.h"

static int32_t ina226_write_16(ina226_ctx_t *ctx, uint8_t reg, uint16_t val) {
    uint8_t data[2];
    data[0] = (val >> 8) & 0xFF; // 大端模式
    data[1] = val & 0xFF;
    return ctx->write_reg(ctx->handle, reg, data, 2);
}

static int32_t ina226_read_16(ina226_ctx_t *ctx, uint8_t reg, uint16_t *val) {
    uint8_t data[2];
    if (ctx->read_reg(ctx->handle, reg, data, 2) != 0) return -1;
    *val = (data[0] << 8) | data[1];
    return 0;
}

int32_t ina226_init(ina226_ctx_t *ctx, uint16_t calibration_val) {
    // 1. 设置配置寄存器: 连续模式, 转换时间 1.1ms, 平均采样 1
    ina226_write_16(ctx, INA226_REG_CONFIG, 0x4127);
    // 2. 写入校准寄存器 (根据你的分流电阻计算得出)
    return ina226_write_16(ctx, INA226_REG_CALIBRATION, calibration_val);
}

int32_t ina226_read_current(ina226_ctx_t *ctx, float *current_ma) {
    uint16_t raw;
    if (ina226_read_16(ctx, INA226_REG_CURRENT, &raw) != 0) return -1;
    // LSB 默认是 1mA (取决于校准寄存器配置)
    *current_ma = (float)((int16_t)raw); 
    return 0;
}

int32_t ina226_read_bus_voltage(ina226_ctx_t *ctx, float *bus_mv) {
    uint16_t raw;
    if (ina226_read_16(ctx, INA226_REG_BUS_VOLT, &raw) != 0) return -1;
    *bus_mv = (float)raw * 1.25f; // INA226 Bus Voltage LSB = 1.25mV
    return 0;
}