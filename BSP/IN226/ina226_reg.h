#ifndef __INA226_REG_H
#define __INA226_REG_H

#include <stdint.h>

/* 寄存器 */
#define INA226_REG_CONFIG       0x00
#define INA226_REG_SHUNT_VOLT   0x01
#define INA226_REG_BUS_VOLT     0x02
#define INA226_REG_POWER        0x03
#define INA226_REG_CURRENT      0x04
#define INA226_REG_CALIBRATION  0x05

#define INA226_ADDR 0x40

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

    /* ===== INA226 核心参数 ===== */

    float r_shunt;            // 分流电阻（Ω）

    float current_lsb;        // 每bit电流 (mA/bit) ⭐核心

    uint16_t calibration_val; // INA226 校准寄存器

    float power_lsb;          // 可选：一般 = 25 * current_lsb

} ina226_ctx_t;

/* API */
int32_t ina226_init(ina226_ctx_t *ctx);
int32_t ina226_get_bus_voltage_mv(ina226_ctx_t *ctx, float *mv);
int32_t ina226_get_current_ma(ina226_ctx_t *ctx, float *ma);

#endif