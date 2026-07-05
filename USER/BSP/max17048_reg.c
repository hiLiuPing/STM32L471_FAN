#include "max17048_reg.h"

// 内部辅助：读取 16 位寄存器
static int32_t max17048_read_16(max17048_ctx_t *ctx, uint8_t reg, uint16_t *val) {
    uint8_t buf[2];
    if (ctx->read_reg(ctx->handle, reg, buf, 2) != 0) return -1;
    *val = (buf[0] << 8) | buf[1];
    return 0;
}

int32_t max17048_init(max17048_ctx_t *ctx) {
    // 软件复位：写入 0x0054 到 COMMAND 寄存器 (0xFE)
    uint8_t cmd[2] = {0x00, 0x54};
    return ctx->write_reg(ctx->handle, MAX17048_REG_COMMAND, cmd, 2);
}

// 获取 SOC (高字节为整数百分比，低字节为小数部分)
int32_t max17048_get_soc(max17048_ctx_t *ctx, float *soc) {
    uint16_t raw;
    if (max17048_read_16(ctx, MAX17048_REG_SOC, &raw) != 0) return -1;
    *soc = (float)raw / 256.0f; // 前 8 位是整数，后 8 位是小数
    return 0;
}

// 获取电压 (单位：mV，分辨率 1.25mV)
int32_t max17048_get_vcell(max17048_ctx_t *ctx, float *voltage_mv) {
    uint16_t raw;
    if (max17048_read_16(ctx, MAX17048_REG_VCELL, &raw) != 0) return -1;
    // 电压寄存器有效位是高 12 位
    *voltage_mv = (float)(raw >> 4) * 1.25f; 
    return 0;
}