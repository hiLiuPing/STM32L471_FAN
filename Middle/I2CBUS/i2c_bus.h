#ifndef __I2C_BUS_H
#define __I2C_BUS_H

#include "main.h"

/* --- 配置开关 --- */
// 取消下方注释以开启 FreeRTOS 支持
// #define USE_FREERTOS 

#ifdef USE_FREERTOS
#include "FreeRTOS.h"
#include "semphr.h"
#endif

/* I2C 总线结构体：包含硬件句柄和对应的互斥锁 */
typedef struct {
    I2C_HandleTypeDef *hi2c;
#ifdef USE_FREERTOS
    SemaphoreHandle_t mutex;
#endif
} I2C_Bus_t;

/* 初始化函数：为指定的总线创建锁 */
void I2C_Bus_Init(I2C_Bus_t *bus);

/* I2C 接口函数 */
HAL_StatusTypeDef I2C_Write(I2C_Bus_t *bus, uint8_t dev_addr, const uint8_t *data, uint16_t len);
HAL_StatusTypeDef I2C_Read(I2C_Bus_t *bus, uint8_t dev_addr, uint8_t *data, uint16_t len);
HAL_StatusTypeDef I2C_Mem_Write(I2C_Bus_t *bus, uint8_t dev_addr, uint8_t reg, uint8_t *data, uint16_t len);
HAL_StatusTypeDef I2C_Mem_Read(I2C_Bus_t *bus, uint8_t dev_addr, uint8_t reg, uint8_t *data, uint16_t len);

#endif