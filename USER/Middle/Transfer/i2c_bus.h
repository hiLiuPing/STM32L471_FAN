#ifndef __I2C_BUS_H
#define __I2C_BUS_H

#include "main.h"

#define USE_FREERTOS 1 
/*
 * ======================================================
 * 统一规则：
 * dev_addr 必须是 7bit 地址（例如 0x40）
 * bus 层统一左移，不允许 driver 再 shift
 * ======================================================
*/

#ifdef USE_FREERTOS
#include "FreeRTOS.h"
#include "semphr.h"
#endif

typedef struct {
    I2C_HandleTypeDef *hi2c;

#ifdef USE_FREERTOS
    SemaphoreHandle_t mutex;
#endif
    volatile uint8_t locked;

} I2C_Bus_t;

void I2C_Bus_Init(I2C_Bus_t *bus);
void I2C_Bus_Lock(I2C_Bus_t *bus);
void I2C_Bus_Unlock(I2C_Bus_t *bus);
uint8_t I2C_Bus_IsLocked(const I2C_Bus_t *bus);
I2C_HandleTypeDef *I2C_Bus_GetHandle(I2C_Bus_t *bus);

HAL_StatusTypeDef I2C_IsDeviceReady(I2C_Bus_t *bus, uint8_t dev_addr,
                                    uint32_t trials, uint32_t timeout);

HAL_StatusTypeDef I2C_Write(I2C_Bus_t *bus, uint8_t dev_addr,
                            const uint8_t *data, uint16_t len);

HAL_StatusTypeDef I2C_Read(I2C_Bus_t *bus, uint8_t dev_addr,
                           uint8_t *data, uint16_t len);

HAL_StatusTypeDef I2C_Mem_Write(I2C_Bus_t *bus, uint8_t dev_addr,
                                uint8_t reg, uint8_t *data, uint16_t len);

HAL_StatusTypeDef I2C_Mem_Read(I2C_Bus_t *bus, uint8_t dev_addr,
                               uint8_t reg, uint8_t *data, uint16_t len);

#endif
