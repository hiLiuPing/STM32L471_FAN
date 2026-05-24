#include "bq24295_reg.h"


 /* @brief 初始化 BQ24295
 * 1. 关闭看门狗防止自动复位
 * 2. 开启充电使能
 * 3. 设置输入限流为 1.5A
 */
int32_t bq24295_init(bq24295_ctx_t *ctx)
{
    uint8_t data;

    // 1. 关闭看门狗
    if (ctx->read_reg(ctx->handle,
                      BQ_REG05_CHG_TERM_TIMER,
                      &data, 1) != 0)
        return -1;

    data &= ~(0x30);

    if (ctx->write_reg(ctx->handle,
                       BQ_REG05_CHG_TERM_TIMER,
                       &data, 1) != 0)
        return -1;

    // 2. 使能充电
    if (ctx->read_reg(ctx->handle,
                      BQ_REG01_PWR_ON_CFG,
                      &data, 1) != 0)
        return -1;

    data |= BQ_CHG_ENABLE;

    if (ctx->write_reg(ctx->handle,
                       BQ_REG01_PWR_ON_CFG,
                       &data, 1) != 0)
        return -1;

    // 3. 输入限流
    if (ctx->read_reg(ctx->handle,
                      BQ_REG00_INPUT_SRC,
                      &data, 1) != 0)
        return -1;

    data &= 0xF8;
    data |= 0x05;

    if (ctx->write_reg(ctx->handle,
                       BQ_REG00_INPUT_SRC,
                       &data, 1) != 0)
        return -1;

    return 0;
}
int32_t bq24295_get_status(bq24295_ctx_t *ctx, uint8_t *status)
{
    return ctx->read_reg(ctx->handle,
                         BQ_REG08_STATUS,
                         status,
                         1);
}
int32_t bq24295_get_fault(bq24295_ctx_t *ctx, uint8_t *fault)
{
    return ctx->read_reg(ctx->handle,
                         BQ_REG09_FAULT,
                         fault,
                         1);
}