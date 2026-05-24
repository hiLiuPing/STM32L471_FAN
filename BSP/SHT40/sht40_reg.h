#ifndef __SHT40_REG_H
#define __SHT40_REG_H

#include <stdint.h>

/* ================= 命令定义 ================= */
#define SHT40_MEAS_HIGH_PRECISION   0xFD
#define SHT40_MEAS_MED_PRECISION    0xF6
#define SHT40_MEAS_LOW_PRECISION    0xE0
#define SHT40_READ_SERIAL_NUMBER    0x89
#define SHT40_SOFT_RESET            0x94
#define SHT40_ADDR            0x44 << 1

/* ================= 上下文结构体 ================= */
typedef struct {
    void *handle; // 指向 I2C 总线对象
    int32_t (*write_reg)(void *handle, uint8_t reg, const uint8_t *data, uint16_t len);
    int32_t (*read_reg)(void *handle, uint8_t reg, uint8_t *data, uint16_t len);
} sht40_ctx_t;

typedef struct {
    float temperature;
    float humidity;
} SHT40_Data_t;

/* ================= API 接口 ================= */
int32_t sht40_read_data(sht40_ctx_t *ctx, SHT40_Data_t *data);
int32_t sht40_soft_reset(sht40_ctx_t *ctx);

#endif /* __SHT40_REG_H */