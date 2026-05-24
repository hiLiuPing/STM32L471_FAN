#ifndef __INA226_REG_H
#define __INA226_REG_H

#include <stdint.h>

/* INA226 常用寄存器地址 */
#define INA226_REG_CONFIG       0x00
#define INA226_REG_SHUNT_VOLT   0x01
#define INA226_REG_BUS_VOLT     0x02
#define INA226_REG_POWER        0x03
#define INA226_REG_CURRENT      0x04
#define INA226_REG_CALIBRATION  0x05

typedef struct {
    void *handle;
    int32_t (*write_reg)(void *handle, uint8_t reg, const uint8_t *data, uint16_t len);
    int32_t (*read_reg)(void *handle, uint8_t reg, uint8_t *data, uint16_t len);
} ina226_ctx_t;

int32_t ina226_init(ina226_ctx_t *ctx, uint16_t calibration_val);
int32_t ina226_read_current(ina226_ctx_t *ctx, float *current_ma);
int32_t ina226_read_bus_voltage(ina226_ctx_t *ctx, float *bus_mv);

#endif