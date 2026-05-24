#ifndef __SHT40_REG_H
#define __SHT40_REG_H

#include <stdint.h>

/* ================= 命令定义 ================= */
#define SHT40_MEAS_HIGH_PRECISION   0xFD
#define SHT40_MEAS_MED_PRECISION    0xF6
#define SHT40_MEAS_LOW_PRECISION    0xE0
#define SHT40_READ_SERIAL_NUMBER    0x89
#define SHT40_SOFT_RESET            0x94
#define SHT40_ADDR            0x44

/* ================= 上下文结构体 ================= */

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
} sht40_ctx_t;



/* ================= API 接口 ================= */

int32_t sht40_soft_reset(sht40_ctx_t *ctx);
int32_t sht40_read_raw(sht40_ctx_t *ctx, uint16_t *rawT, uint16_t *rawH);
float sht40_convert_temp(uint16_t rawT);
float sht40_convert_hum(uint16_t rawH);
uint8_t SHT40_CRC8(uint8_t *data, uint8_t len);
#endif /* __SHT40_REG_H */