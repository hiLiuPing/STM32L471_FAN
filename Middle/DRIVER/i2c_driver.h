#ifndef __I2C_DRIVER_H
#define __I2C_DRIVER_H

#include "main.h"
#include "i2c_bus.h"
#include "sht40_reg.h"
#include "lis3dh_reg.h"
#include "ee24.h"
#include "bq24295_reg.h"
// EEPROM 适配函数
extern EE24_HandleTypeDef h_eeprom; 

int32_t sht40_i2c_write(void *handle, uint8_t reg, const uint8_t *data, uint16_t len);
int32_t sht40_i2c_read(void *handle, uint8_t reg, uint8_t *data, uint16_t len);
int32_t lis3dh_i2c_write(void *handle, uint8_t reg, const uint8_t *data, uint16_t len);
int32_t lis3dh_i2c_read(void *handle, uint8_t reg, uint8_t *data, uint16_t len);
int32_t ee24_i2c_write(void *handle, uint8_t reg, const uint8_t *data, uint16_t len);
int32_t ee24_i2c_read(void *handle, uint8_t reg, uint8_t *data, uint16_t len);
int32_t bq24295_i2c_write(void *handle, uint8_t reg, const uint8_t *data, uint16_t len);
int32_t bq24295_i2c_read(void *handle, uint8_t reg, uint8_t *data, uint16_t len);

#endif