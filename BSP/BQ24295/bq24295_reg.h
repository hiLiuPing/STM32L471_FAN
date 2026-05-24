#ifndef __BQ24295_REG_H
#define __BQ24295_REG_H

#include <stdint.h>

/* BQ24295 I2C 地址 (7位地址 0x6B) */
#define BQ24295_I2C_ADDR_7BIT          0x6B

/* 寄存器地址 */
#define BQ24295_REG00_INPUT_SRC        0x00
#define BQ24295_REG01_POWER_ON_CONFIG  0x01
#define BQ24295_REG02_CHARGE_CURR      0x02
#define BQ24295_REG03_PRECHG_TERM_CURR 0x03
#define BQ24295_REG04_VOLT_LIMIT       0x04
#define BQ24295_REG05_CHARGE_TERM      0x05
#define BQ24295_REG06_CHARGE_TIMER     0x06
#define BQ24295_REG07_THERMAL_REG      0x07
#define BQ24295_REG08_STATUS           0x08
#define BQ24295_REG09_FAULT            0x09

/* 常用控制位 */
#define BQ_CHG_ENABLE                  (0x01 << 4) // REG01 Bit 4
#define BQ_WATCHDOG_DISABLE            (0x00 << 4) // REG05 Bit 5:4

typedef struct {
    void *handle;
    int32_t (*write_reg)(void *handle, uint8_t reg, const uint8_t *data, uint16_t len);
    int32_t (*read_reg)(void *handle, uint8_t reg, uint8_t *data, uint16_t len);
} bq24295_ctx_t;

int32_t bq24295_init(bq24295_ctx_t *ctx);
int32_t bq24295_get_status(bq24295_ctx_t *ctx, uint8_t *status);

#endif