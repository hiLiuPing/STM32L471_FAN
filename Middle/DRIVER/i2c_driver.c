
#include "i2c_driver.h"
EE24_HandleTypeDef h_eeprom; 

// --- SHT40 适配 ---
int32_t sht40_i2c_write(void *handle, uint8_t reg, const uint8_t *data, uint16_t len) {
    // SHT40 测量命令直接发，不需要 reg 地址
    return (int32_t)I2C_Write((I2C_Bus_t*)handle, SHT40_ADDR, data, len);
}
int32_t sht40_i2c_read(void *handle, uint8_t reg, uint8_t *data, uint16_t len) {
    return (int32_t)I2C_Read((I2C_Bus_t*)handle, SHT40_ADDR, data, len);
}

// --- LIS3DH 适配 ---
int32_t lis3dh_i2c_write(void *handle, uint8_t reg, const uint8_t *data, uint16_t len) {
    // LIS3DH 需要写寄存器，直接调用 I2C_Mem_Write
    return (int32_t)I2C_Mem_Write((I2C_Bus_t*)handle, LIS3DH_ADDR, reg, (uint8_t*)data, len);
}
int32_t lis3dh_i2c_read(void *handle, uint8_t reg, uint8_t *data, uint16_t len) {
    return (int32_t)I2C_Mem_Read((I2C_Bus_t*)handle, LIS3DH_ADDR, reg, data, len);
}



int32_t ee24_i2c_write(void *handle, uint8_t reg, const uint8_t *data, uint16_t len) {
    // 适配库函数参数：(句柄, 地址, 数据, 长度, 超时时间)
    // 假设超时设为 100ms
    if (EE24_Write(&h_eeprom, (uint32_t)reg, (uint8_t*)data, (size_t)len, 100) == true) {
        return 0; // 成功返回 0
    }
    return -1; // 失败返回 -1
}

int32_t ee24_i2c_read(void *handle, uint8_t reg, uint8_t *data, uint16_t len) {
    // 适配库函数参数：(句柄, 地址, 数据, 长度, 超时时间)
    if (EE24_Read(&h_eeprom, (uint32_t)reg, data, (size_t)len, 100) == true) {
        return 0; // 成功返回 0
    }
    return -1; // 失败返回 -1
}


// 1. 定义实例
bq24295_ctx_t bq_ctx;

// 2. 适配器 (连接到你的 I2C 驱动)
int32_t bq_write(void *h, uint8_t reg, const uint8_t *data, uint16_t len) {
    // 假设你的 I2C_Mem_Write 参数为: (句柄, 地址, 寄存器, 数据, 长度)
    return I2C_Mem_Write((I2C_Bus_t*)h, BQ24295_I2C_ADDR_7BIT << 1, reg, (uint8_t*)data, len);
}

int32_t bq_read(void *h, uint8_t reg, uint8_t *data, uint16_t len) {
    return I2C_Mem_Read((I2C_Bus_t*)h, BQ24295_I2C_ADDR_7BIT << 1, reg, data, len);
}