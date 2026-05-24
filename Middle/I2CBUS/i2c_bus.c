#include "i2c_bus.h"

/* =========================
 * 内部：I2C 地址统一转换
 * ========================= */
static inline uint16_t I2C_ADDR(uint8_t dev_addr)
{
    return (uint16_t)(dev_addr << 1);
}

/* =========================
 * Lock
 * ========================= */
static void I2C_Lock(I2C_Bus_t *bus)
{
#ifdef USE_FREERTOS
    if (bus->mutex)
        xSemaphoreTake(bus->mutex, portMAX_DELAY);
#endif
}

static void I2C_Unlock(I2C_Bus_t *bus)
{
#ifdef USE_FREERTOS
    if (bus->mutex)
        xSemaphoreGive(bus->mutex);
#endif
}

/* ========================= */
void I2C_Bus_Init(I2C_Bus_t *bus)
{
#ifdef USE_FREERTOS
    if (bus->mutex == NULL)
        bus->mutex = xSemaphoreCreateMutex();
#endif
}

/* ========================= */
HAL_StatusTypeDef I2C_Write(I2C_Bus_t *bus, uint8_t dev_addr,
                            const uint8_t *data, uint16_t len)
{
    I2C_Lock(bus);

    HAL_StatusTypeDef ret =
        HAL_I2C_Master_Transmit(bus->hi2c,
                                 I2C_ADDR(dev_addr),
                                 (uint8_t *)data,
                                 len,
                                 100);

    I2C_Unlock(bus);
    return ret;
}

/* ========================= */
HAL_StatusTypeDef I2C_Read(I2C_Bus_t *bus, uint8_t dev_addr,
                           uint8_t *data, uint16_t len)
{
    I2C_Lock(bus);

    HAL_StatusTypeDef ret =
        HAL_I2C_Master_Receive(bus->hi2c,
                                I2C_ADDR(dev_addr),
                                data,
                                len,
                                100);

    I2C_Unlock(bus);
    return ret;
}

/* ========================= */
HAL_StatusTypeDef I2C_Mem_Write(I2C_Bus_t *bus, uint8_t dev_addr,
                                uint8_t reg, uint8_t *data, uint16_t len)
{
    I2C_Lock(bus);

    HAL_StatusTypeDef ret =
        HAL_I2C_Mem_Write(bus->hi2c,
                          I2C_ADDR(dev_addr),
                          reg,
                          I2C_MEMADD_SIZE_8BIT,
                          data,
                          len,
                          100);

    I2C_Unlock(bus);
    return ret;
}

/* ========================= */
HAL_StatusTypeDef I2C_Mem_Read(I2C_Bus_t *bus, uint8_t dev_addr,
                               uint8_t reg, uint8_t *data, uint16_t len)
{
    I2C_Lock(bus);

    HAL_StatusTypeDef ret =
        HAL_I2C_Mem_Read(bus->hi2c,
                         I2C_ADDR(dev_addr),
                         reg,
                         I2C_MEMADD_SIZE_8BIT,
                         data,
                         len,
                         100);

    I2C_Unlock(bus);
    return ret;
}