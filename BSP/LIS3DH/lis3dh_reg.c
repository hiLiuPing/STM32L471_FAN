#include "lis3dh_reg.h"

/* ================= 寄存器读写封装 ================= */

int32_t lis3dh_write_reg(lis3dh_ctx_t *ctx, uint8_t reg, const uint8_t *data, uint16_t len) {
    return ctx->write_reg(ctx->handle, reg, data, len);
}

int32_t lis3dh_read_reg(lis3dh_ctx_t *ctx, uint8_t reg, uint8_t *data, uint16_t len) {
    return ctx->read_reg(ctx->handle, reg, data, len);
}

int32_t lis3dh_acceleration_raw_get(lis3dh_ctx_t *ctx, int16_t raw[3]) {
    uint8_t buf[6];
    // 使用 LIS3DH_ADDR_INC (0x80) 确保一次性读取 6 个字节
    if (lis3dh_read_reg(ctx, LIS3DH_OUT_X_L | LIS3DH_ADDR_INC, buf, 6) != 0) return -1;

    /* 关键修正：LIS3DH 在高分辨率模式下数据是左对齐的，必须右移 4 位 */
    raw[0] = ((int16_t)((buf[1] << 8) | buf[0])) >> 4;
    raw[1] = ((int16_t)((buf[3] << 8) | buf[2])) >> 4;
    raw[2] = ((int16_t)((buf[5] << 8) | buf[4])) >> 4;
    // log_printf("Raw: %d, %d, %d\n", raw[0], raw[1], raw[2]);
    return 0;
}

