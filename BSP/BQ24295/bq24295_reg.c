#include "bq24295_reg.h"

/**
 * @brief 初始化 BQ24295
 * 1. 关闭看门狗防止自动复位
 * 2. 开启充电使能
 * 3. 设置默认输入限流为 1.5A (0x30)
 */
int32_t bq24295_init(bq24295_ctx_t *ctx) {
    uint8_t data;

    // 1. 关闭看门狗 (REG05 Bit 5:4 = 00)
    data = BQ_WATCHDOG_DISABLE;
    if (ctx->write_reg(ctx->handle, BQ24295_REG05_CHARGE_TERM, &data, 1) != 0) return -1;

    // 2. 使能充电 (读取 REG01，修改 Bit 4)
    if (ctx->read_reg(ctx->handle, BQ24295_REG01_POWER_ON_CONFIG, &data, 1) != 0) return -1;
    data |= BQ_CHG_ENABLE;
    if (ctx->write_reg(ctx->handle, BQ24295_REG01_POWER_ON_CONFIG, &data, 1) != 0) return -1;

    // 3. 设置输入电流限制为 1.5A (REG00 = 0x30)
    data = 0x30;
    if (ctx->write_reg(ctx->handle, BQ24295_REG00_INPUT_SRC, &data, 1) != 0) return -1;

    return 0;
}

/**
 * @brief 获取充电状态 (REG08)
 */
int32_t bq24295_get_status(bq24295_ctx_t *ctx, uint8_t *status) {
    return ctx->read_reg(ctx->handle, BQ24295_REG08_STATUS, status, 1);
}