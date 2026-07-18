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
void I2C_Bus_Lock(I2C_Bus_t *bus)
{
    if (bus == NULL)
        return;

#ifdef USE_FREERTOS
    if (bus->mutex)
    {
        if (xSemaphoreTake(bus->mutex, portMAX_DELAY) == pdTRUE)
            bus->locked = 1U;
    }
    else
    {
        bus->locked = 1U;
    }
#else
    bus->locked = 1U;
#endif
}

void I2C_Bus_Unlock(I2C_Bus_t *bus)
{
    if (bus == NULL || bus->locked == 0U)
        return;

#ifdef USE_FREERTOS
    if (bus->mutex)
    {
        /* Clear ownership before waking the next waiter. */
        bus->locked = 0U;
        xSemaphoreGive(bus->mutex);
        return;
    }
#endif
    bus->locked = 0U;
}

void I2C_Bus_UnlockFromISR(I2C_Bus_t *bus)
{
    if (bus == NULL || bus->locked == 0U)
        return;

#ifdef USE_FREERTOS
    if (bus->mutex)
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        /* A woken task may take the semaphore immediately after the give. */
        bus->locked = 0U;
        xSemaphoreGiveFromISR(bus->mutex, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        return;
    }
#endif

    bus->locked = 0U;
}

uint8_t I2C_Bus_IsLocked(const I2C_Bus_t *bus)
{
    if (bus == NULL)
        return 0U;

    return bus->locked;
}

I2C_HandleTypeDef *I2C_Bus_GetHandle(I2C_Bus_t *bus)
{
    if (bus == NULL)
        return NULL;

    return bus->hi2c;
}

/* ========================= */
void I2C_Bus_Init(I2C_Bus_t *bus)
{
    if (bus == NULL)
        return;

#ifdef USE_FREERTOS
    if (bus->mutex == NULL)
    {
        bus->mutex = xSemaphoreCreateBinary();
        if (bus->mutex != NULL)
            xSemaphoreGive(bus->mutex);
    }
#endif
    bus->locked = 0U;
}

HAL_StatusTypeDef I2C_IsDeviceReady(I2C_Bus_t *bus, uint8_t dev_addr,
                                    uint32_t trials, uint32_t timeout)
{
    HAL_StatusTypeDef ret;

    if (bus == NULL || bus->hi2c == NULL)
        return HAL_ERROR;

    I2C_Bus_Lock(bus);
    ret = HAL_I2C_IsDeviceReady(bus->hi2c, I2C_ADDR(dev_addr), trials, timeout);
    I2C_Bus_Unlock(bus);
    return ret;
}

HAL_StatusTypeDef I2C_Write_DMA_Start(I2C_Bus_t *bus, uint8_t dev_addr,
                                      uint8_t *data, uint16_t len)
{
    HAL_StatusTypeDef ret;

    if (bus == NULL || bus->hi2c == NULL)
        return HAL_ERROR;

    I2C_Bus_Lock(bus);
    ret = HAL_I2C_Master_Transmit_DMA(bus->hi2c, I2C_ADDR(dev_addr), data, len);
    if (ret != HAL_OK)
        I2C_Bus_Unlock(bus);

    return ret;
}

/* ========================= */
HAL_StatusTypeDef I2C_Write(I2C_Bus_t *bus, uint8_t dev_addr,
                            const uint8_t *data, uint16_t len)
{
    if (bus == NULL || bus->hi2c == NULL)
        return HAL_ERROR;

    I2C_Bus_Lock(bus);

    HAL_StatusTypeDef ret =
        HAL_I2C_Master_Transmit(bus->hi2c,
                                 I2C_ADDR(dev_addr),
                                 (uint8_t *)data,
                                 len,
                                 100);

    I2C_Bus_Unlock(bus);
    return ret;
}

/* ========================= */
HAL_StatusTypeDef I2C_Read(I2C_Bus_t *bus, uint8_t dev_addr,
                           uint8_t *data, uint16_t len)
{
    if (bus == NULL || bus->hi2c == NULL)
        return HAL_ERROR;

    I2C_Bus_Lock(bus);

    HAL_StatusTypeDef ret =
        HAL_I2C_Master_Receive(bus->hi2c,
                                I2C_ADDR(dev_addr),
                                data,
                                len,
                                100);

    I2C_Bus_Unlock(bus);
    return ret;
}

/* ========================= */
HAL_StatusTypeDef I2C_Mem_Write(I2C_Bus_t *bus, uint8_t dev_addr,
                                uint8_t reg, uint8_t *data, uint16_t len)
{
    if (bus == NULL || bus->hi2c == NULL)
        return HAL_ERROR;

    I2C_Bus_Lock(bus);

    HAL_StatusTypeDef ret =
        HAL_I2C_Mem_Write(bus->hi2c,
                          I2C_ADDR(dev_addr),
                          reg,
                          I2C_MEMADD_SIZE_8BIT,
                          data,
                          len,
                          100);

    I2C_Bus_Unlock(bus);
    return ret;
}

/* ========================= */
HAL_StatusTypeDef I2C_Mem_Read(I2C_Bus_t *bus, uint8_t dev_addr,
                               uint8_t reg, uint8_t *data, uint16_t len)
{
    if (bus == NULL || bus->hi2c == NULL)
        return HAL_ERROR;

    I2C_Bus_Lock(bus);

    HAL_StatusTypeDef ret =
        HAL_I2C_Mem_Read(bus->hi2c,
                         I2C_ADDR(dev_addr),
                         reg,
                         I2C_MEMADD_SIZE_8BIT,
                         data,
                         len,
                         100);

    I2C_Bus_Unlock(bus);
    return ret;
}
