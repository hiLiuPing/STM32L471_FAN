#ifndef __LIS3DH_REG_H
#define __LIS3DH_REG_H

#include <stdint.h>

/* ================= 寄存器地址（保留最小集合） ================= */

#define LIS3DH_WHO_AM_I      0x0F
#define LIS3DH_ADDR          0x19 

#define LIS3DH_CTRL_REG1 0x20
#define LIS3DH_CTRL_REG2 0x21
#define LIS3DH_CTRL_REG3 0x22
#define LIS3DH_CTRL_REG4 0x23
#define LIS3DH_CTRL_REG5 0x24
#define LIS3DH_INT1_CFG  0x30
#define LIS3DH_INT1_THS  0x32
#define LIS3DH_INT1_DUR  0x33

#define LIS3DH_OUT_X_L       0x28
#define LIS3DH_OUT_Y_L       0x2A
#define LIS3DH_OUT_Z_L       0x2C

/* ================= 设备上下文（来自 ST 官方思想） ================= */

/* 自动增量标志位 (I2C) */
#define LIS3DH_ADDR_INC      0x80 



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
} lis3dh_ctx_t;

int32_t lis3dh_acceleration_raw_get(lis3dh_ctx_t *ctx, int16_t raw[3]);
int32_t lis3dh_write_reg(lis3dh_ctx_t *ctx, uint8_t reg, const uint8_t *data, uint16_t len);
int32_t lis3dh_read_reg(lis3dh_ctx_t *ctx, uint8_t reg, uint8_t *data, uint16_t len);

#endif /* __LIS3DH_REG_H */


