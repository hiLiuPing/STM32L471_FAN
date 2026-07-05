#ifndef __MAX17048_REG_H
#define __MAX17048_REG_H

#include <stdint.h>

#define MAX17048_ADDR    0x36 

// 核心寄存器
#define MAX17048_REG_VCELL   0x02  // 电池电压 (16位)
#define MAX17048_REG_SOC     0x04  // 剩余电量百分比 (16位)
#define MAX17048_REG_MODE    0x06  // 模式控制
#define MAX17048_REG_CONFIG  0x0C  // 配置寄存器
#define MAX17048_REG_COMMAND 0xFE  // 软件复位命令



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
} max17048_ctx_t;

int32_t max17048_init(max17048_ctx_t *ctx);
int32_t max17048_get_soc(max17048_ctx_t *ctx, float *soc);
int32_t max17048_get_vcell(max17048_ctx_t *ctx, float *voltage_mv);

#endif