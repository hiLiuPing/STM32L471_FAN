#include "i2c_bus.h"

void I2C_Bus_Init(I2C_Bus_t *bus)
{
#ifdef USE_FREERTOS
    if (bus->mutex == NULL) {
        bus->mutex = xSemaphoreCreateMutex();
    }
#endif
}

/* 内部辅助函数：处理加锁逻辑 */
static void I2C_Lock(I2C_Bus_t *bus)
{
#ifdef USE_FREERTOS
    if (bus->mutex != NULL) {
        xSemaphoreTake(bus->mutex, portMAX_DELAY);
    }
#endif
}

/* 内部辅助函数：处理解锁逻辑 */
static void I2C_Unlock(I2C_Bus_t *bus)
{
#ifdef USE_FREERTOS
    if (bus->mutex != NULL) {
        xSemaphoreGive(bus->mutex);
    }
#endif
}

HAL_StatusTypeDef I2C_Write(I2C_Bus_t *bus, uint8_t dev_addr, const uint8_t *data, uint16_t len)
{
    I2C_Lock(bus);
    HAL_StatusTypeDef ret = HAL_I2C_Master_Transmit(bus->hi2c, dev_addr, (uint8_t *)data, len, 100);
    I2C_Unlock(bus);
    return ret;
}

HAL_StatusTypeDef I2C_Read(I2C_Bus_t *bus, uint8_t dev_addr, uint8_t *data, uint16_t len)
{
    I2C_Lock(bus);
    HAL_StatusTypeDef ret = HAL_I2C_Master_Receive(bus->hi2c, dev_addr, data, len, 100);
    I2C_Unlock(bus);
    return ret;
}

HAL_StatusTypeDef I2C_Mem_Write(I2C_Bus_t *bus, uint8_t dev_addr, uint8_t reg, uint8_t *data, uint16_t len)
{
    I2C_Lock(bus);
    HAL_StatusTypeDef ret = HAL_I2C_Mem_Write(bus->hi2c, dev_addr, reg, I2C_MEMADD_SIZE_8BIT, data, len, 100);
    I2C_Unlock(bus);
    return ret;
}

HAL_StatusTypeDef I2C_Mem_Read(I2C_Bus_t *bus, uint8_t dev_addr, uint8_t reg, uint8_t *data, uint16_t len)
{
    I2C_Lock(bus);
    HAL_StatusTypeDef ret = HAL_I2C_Mem_Read(bus->hi2c, dev_addr, reg, I2C_MEMADD_SIZE_8BIT, data, len, 100);
    I2C_Unlock(bus);
    return ret;
}