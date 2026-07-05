#ifndef __BQ24295_REG_H
#define __BQ24295_REG_H

#include <stdint.h>

#include <stdint.h>

/* BQ24295 地址定义 */
#define BQ24295_ADDR     0x6B

/* 寄存器映射 */
#define BQ_REG00_INPUT_SRC        0x00
#define BQ_REG01_PWR_ON_CFG       0x01
#define BQ_REG02_CHG_CURR         0x02
#define BQ_REG03_PRECHG_TERM      0x03
#define BQ_REG04_CHG_VOLT         0x04
#define BQ_REG05_CHG_TERM_TIMER   0x05
#define BQ_REG06_BOOST_CFG        0x06
#define BQ_REG07_MISC_CTRL        0x07
#define BQ_REG08_STATUS           0x08
#define BQ_REG09_FAULT            0x09
#define BQ_REG0A_VENDER_PART      0x0A

/* 配置掩码与常量 */
#define BQ_WATCHDOG_DISABLE       0x00 
#define BQ_CHG_ENABLE             0x10 



typedef struct {
    void *handle;
    uint16_t dev_addr;

    int32_t (*write_reg)(
        void *handle,
        uint8_t reg,
        const uint8_t *data,
        uint16_t len);

    int32_t (*read_reg)(
        void *handle,
        uint8_t reg,
        uint8_t *data,
        uint16_t len);
} bq24295_ctx_t;

int32_t bq24295_init(bq24295_ctx_t *ctx);
int32_t bq24295_get_status(bq24295_ctx_t *ctx, uint8_t *status);
int32_t bq24295_get_fault(bq24295_ctx_t *ctx, uint8_t *fault);
#endif