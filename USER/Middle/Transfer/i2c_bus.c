#include "i2c_bus.h"

/* =========================
 * 内部：I2C 地址统一转换
 * ========================= */
static inline uint16_t I2C_ADDR(uint8_t dev_addr)
{
    return (uint16_t)(dev_addr << 1);
}

static uint8_t I2C_Bus_IsValid(const I2C_Bus_t *bus, uint8_t dev_addr)
{
    if ((bus == NULL) || (bus->hi2c == NULL) || (dev_addr > 0x7FU))
        return 0U;

#ifdef USE_FREERTOS
    if (bus->mutex == NULL)
        return 0U;
#endif

    return 1U;
}

static TickType_t I2C_Bus_MillisecondsToTicks(uint32_t timeout)
{
    TickType_t ticks;

    if (timeout == HAL_MAX_DELAY)
        return portMAX_DELAY;

    ticks = pdMS_TO_TICKS(timeout);
    if ((timeout != 0U) && (ticks == 0U))
        ticks = 1U;

    return ticks;
}

static uint8_t I2C_Bus_LockFor(I2C_Bus_t *bus, uint32_t timeout)
{
    if (bus == NULL)
        return 0U;

#ifdef USE_FREERTOS
    if ((bus->mutex == NULL) ||
        (xSemaphoreTake(bus->mutex,
                        I2C_Bus_MillisecondsToTicks(timeout)) != pdTRUE))
        return 0U;
#endif

    bus->locked = 1U;
    return 1U;
}

static uint8_t I2C_Bus_GetRemainingTimeout(uint32_t start_tick,
                                           uint32_t timeout,
                                           uint32_t *remaining)
{
    uint32_t elapsed;

    if (remaining == NULL)
        return 0U;

    if (timeout == HAL_MAX_DELAY)
    {
        *remaining = HAL_MAX_DELAY;
        return 1U;
    }

    elapsed = HAL_GetTick() - start_tick;
    if (elapsed >= timeout)
    {
        *remaining = 0U;
        return 0U;
    }

    *remaining = timeout - elapsed;
    return 1U;
}

/* =========================
 * Lock
 * ========================= */
uint8_t I2C_Bus_Lock(I2C_Bus_t *bus)
{
    return I2C_Bus_LockFor(bus, HAL_MAX_DELAY);
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
        if (xSemaphoreGive(bus->mutex) != pdTRUE)
            bus->locked = 1U;
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
        bus->mutex = xSemaphoreCreateMutexStatic(&bus->mutex_storage);
        bus->locked = 0U;
    }
#else
    bus->locked = 0U;
#endif
}

HAL_StatusTypeDef I2C_IsDeviceReady(I2C_Bus_t *bus, uint8_t dev_addr,
                                    uint32_t trials, uint32_t timeout)
{
    HAL_StatusTypeDef ret;
    uint32_t start_tick;
    uint32_t remaining;

    if ((I2C_Bus_IsValid(bus, dev_addr) == 0U) || (timeout == 0U))
        return HAL_ERROR;

    start_tick = HAL_GetTick();
    if (I2C_Bus_LockFor(bus, timeout) == 0U)
        return HAL_TIMEOUT;
    if (I2C_Bus_GetRemainingTimeout(start_tick, timeout, &remaining) == 0U)
    {
        I2C_Bus_Unlock(bus);
        return HAL_TIMEOUT;
    }

    ret = HAL_I2C_IsDeviceReady(bus->hi2c, I2C_ADDR(dev_addr), trials, remaining);
    I2C_Bus_Unlock(bus);
    return ret;
}

/* ========================= */
HAL_StatusTypeDef I2C_Write(I2C_Bus_t *bus, uint8_t dev_addr,
                            const uint8_t *data, uint16_t len)
{
    HAL_StatusTypeDef ret;
    uint32_t start_tick;
    uint32_t remaining;

    if ((I2C_Bus_IsValid(bus, dev_addr) == 0U) ||
        ((data == NULL) && (len != 0U)))
        return HAL_ERROR;

    start_tick = HAL_GetTick();
    if (I2C_Bus_LockFor(bus, 100U) == 0U)
        return HAL_TIMEOUT;
    if (I2C_Bus_GetRemainingTimeout(start_tick, 100U, &remaining) == 0U)
    {
        I2C_Bus_Unlock(bus);
        return HAL_TIMEOUT;
    }

    ret = HAL_I2C_Master_Transmit(bus->hi2c,
                                 I2C_ADDR(dev_addr),
                                 (uint8_t *)data,
                                 len,
                                 remaining);

    I2C_Bus_Unlock(bus);
    return ret;
}

/* ========================= */
HAL_StatusTypeDef I2C_Read(I2C_Bus_t *bus, uint8_t dev_addr,
                           uint8_t *data, uint16_t len)
{
    HAL_StatusTypeDef ret;
    uint32_t start_tick;
    uint32_t remaining;

    if ((I2C_Bus_IsValid(bus, dev_addr) == 0U) ||
        ((data == NULL) && (len != 0U)))
        return HAL_ERROR;

    start_tick = HAL_GetTick();
    if (I2C_Bus_LockFor(bus, 100U) == 0U)
        return HAL_TIMEOUT;
    if (I2C_Bus_GetRemainingTimeout(start_tick, 100U, &remaining) == 0U)
    {
        I2C_Bus_Unlock(bus);
        return HAL_TIMEOUT;
    }

    ret = HAL_I2C_Master_Receive(bus->hi2c,
                                I2C_ADDR(dev_addr),
                                data,
                                len,
                                remaining);

    I2C_Bus_Unlock(bus);
    return ret;
}

/* ========================= */
HAL_StatusTypeDef I2C_Mem_Write(I2C_Bus_t *bus, uint8_t dev_addr,
                                uint8_t reg, const uint8_t *data, uint16_t len)
{
    return I2C_Mem_WriteEx(bus, dev_addr, reg, I2C_MEMADD_SIZE_8BIT,
                           data, len, 100U);
}

/* ========================= */
HAL_StatusTypeDef I2C_Mem_Read(I2C_Bus_t *bus, uint8_t dev_addr,
                               uint8_t reg, uint8_t *data, uint16_t len)
{
    return I2C_Mem_ReadEx(bus, dev_addr, reg, I2C_MEMADD_SIZE_8BIT,
                          data, len, 100U);
}

HAL_StatusTypeDef I2C_Mem_WriteEx(I2C_Bus_t *bus, uint8_t dev_addr,
                                  uint16_t mem_addr, uint16_t mem_addr_size,
                                  const uint8_t *data, uint16_t len,
                                  uint32_t timeout)
{
    HAL_StatusTypeDef ret;
    uint32_t start_tick;
    uint32_t remaining;

    if ((I2C_Bus_IsValid(bus, dev_addr) == 0U) ||
        ((data == NULL) && (len != 0U)) ||
        (timeout == 0U) ||
        ((mem_addr_size != I2C_MEMADD_SIZE_8BIT) &&
         (mem_addr_size != I2C_MEMADD_SIZE_16BIT)))
        return HAL_ERROR;

    start_tick = HAL_GetTick();
    if (I2C_Bus_LockFor(bus, timeout) == 0U)
        return HAL_TIMEOUT;
    if (I2C_Bus_GetRemainingTimeout(start_tick, timeout, &remaining) == 0U)
    {
        I2C_Bus_Unlock(bus);
        return HAL_TIMEOUT;
    }

    ret = HAL_I2C_Mem_Write(bus->hi2c,
                            I2C_ADDR(dev_addr),
                            mem_addr,
                            mem_addr_size,
                            (uint8_t *)data,
                            len,
                            remaining);

    I2C_Bus_Unlock(bus);
    return ret;
}

HAL_StatusTypeDef I2C_Mem_ReadEx(I2C_Bus_t *bus, uint8_t dev_addr,
                                 uint16_t mem_addr, uint16_t mem_addr_size,
                                 uint8_t *data, uint16_t len,
                                 uint32_t timeout)
{
    HAL_StatusTypeDef ret;
    uint32_t start_tick;
    uint32_t remaining;

    if ((I2C_Bus_IsValid(bus, dev_addr) == 0U) ||
        ((data == NULL) && (len != 0U)) ||
        (timeout == 0U) ||
        ((mem_addr_size != I2C_MEMADD_SIZE_8BIT) &&
         (mem_addr_size != I2C_MEMADD_SIZE_16BIT)))
        return HAL_ERROR;

    start_tick = HAL_GetTick();
    if (I2C_Bus_LockFor(bus, timeout) == 0U)
        return HAL_TIMEOUT;
    if (I2C_Bus_GetRemainingTimeout(start_tick, timeout, &remaining) == 0U)
    {
        I2C_Bus_Unlock(bus);
        return HAL_TIMEOUT;
    }

    ret = HAL_I2C_Mem_Read(bus->hi2c,
                           I2C_ADDR(dev_addr),
                           mem_addr,
                           mem_addr_size,
                           data,
                           len,
                           remaining);

    I2C_Bus_Unlock(bus);
    return ret;
}
