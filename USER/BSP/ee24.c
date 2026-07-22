#include "ee24.h"

#include <limits.h>

#include "task.h"

#if (EE24_SIZE <= 2)
#define EE24_PSIZE 8U
#elif (EE24_SIZE <= 16)
#define EE24_PSIZE 16U
#elif (EE24_SIZE <= 64)
#define EE24_PSIZE 32U
#else
#define EE24_PSIZE 64U
#endif

#define EE24_CAPACITY_BYTES ((uint32_t)EE24_SIZE * 128U)
#define EE24_READY_POLL_TIMEOUT_MS 2U

static TickType_t EE24_MillisecondsToTicks(uint32_t milliseconds)
{
    TickType_t ticks = pdMS_TO_TICKS(milliseconds);

    if ((milliseconds != 0U) && (ticks == 0U))
    {
        ticks = 1U;
    }
    return ticks;
}

static bool EE24_GetRemainingTimeout(uint32_t start_tick,
                                     uint32_t timeout,
                                     uint32_t *remaining)
{
    uint32_t elapsed;

    if (remaining == NULL)
    {
        return false;
    }

    elapsed = HAL_GetTick() - start_tick;
    if (elapsed >= timeout)
    {
        *remaining = 0U;
        return false;
    }

    *remaining = timeout - elapsed;
    return true;
}

static bool EE24_IsRangeValid(uint32_t address, size_t len)
{
    if ((len == 0U) || (address >= EE24_CAPACITY_BYTES))
    {
        return false;
    }

    return len <= (size_t)(EE24_CAPACITY_BYTES - address);
}

static uint8_t EE24_GetDeviceAddress(const EE24_HandleTypeDef *handle,
                                     uint32_t address)
{
#if ((EE24_SIZE == EE24_1KBIT) || (EE24_SIZE == EE24_2KBIT))
    (void)address;
    return handle->Address;
#elif (EE24_SIZE == EE24_4KBIT)
    return (uint8_t)(handle->Address | ((address >> 8) & 0x01U));
#elif (EE24_SIZE == EE24_8KBIT)
    return (uint8_t)(handle->Address | ((address >> 8) & 0x03U));
#elif (EE24_SIZE == EE24_16KBIT)
    return (uint8_t)(handle->Address | ((address >> 8) & 0x07U));
#else
    (void)address;
    return handle->Address;
#endif
}

static uint16_t EE24_GetMemoryAddress(uint32_t address)
{
#if (EE24_SIZE <= EE24_16KBIT)
    return (uint16_t)(address & 0x00FFU);
#else
    return (uint16_t)(address & 0xFFFFU);
#endif
}

static uint16_t EE24_GetMemoryAddressSize(void)
{
#if (EE24_SIZE <= EE24_16KBIT)
    return I2C_MEMADD_SIZE_8BIT;
#else
    return I2C_MEMADD_SIZE_16BIT;
#endif
}

static uint16_t EE24_GetReadChunk(uint32_t address, size_t len)
{
    size_t chunk = len;

    if (chunk > UINT16_MAX)
    {
        chunk = UINT16_MAX;
    }

#if ((EE24_SIZE >= EE24_4KBIT) && (EE24_SIZE <= EE24_16KBIT))
    {
        size_t bank_remaining = 0x100U - (address & 0xFFU);

        if (chunk > bank_remaining)
        {
            chunk = bank_remaining;
        }
    }
#else
    (void)address;
#endif

    return (uint16_t)chunk;
}

static bool EE24_TakeMutex(EE24_HandleTypeDef *handle, uint32_t timeout)
{
    if ((handle == NULL) || (handle->Mutex == NULL))
    {
        return false;
    }

    return xSemaphoreTake(handle->Mutex,
                          EE24_MillisecondsToTicks(timeout)) == pdTRUE;
}

static void EE24_GiveMutex(EE24_HandleTypeDef *handle)
{
    if ((handle != NULL) && (handle->Mutex != NULL))
    {
        (void)xSemaphoreGive(handle->Mutex);
    }
}

static bool EE24_WaitReady(EE24_HandleTypeDef *handle,
                           uint8_t device_address,
                           uint32_t start_tick,
                           uint32_t timeout)
{
    uint32_t remaining;
    uint32_t poll_timeout;

    while (EE24_GetRemainingTimeout(start_tick, timeout, &remaining))
    {
        poll_timeout = remaining;
        if (poll_timeout > EE24_READY_POLL_TIMEOUT_MS)
        {
            poll_timeout = EE24_READY_POLL_TIMEOUT_MS;
        }

        if (I2C_IsDeviceReady(handle->Bus, device_address, 1U,
                              poll_timeout) == HAL_OK)
        {
            return true;
        }

        if (!EE24_GetRemainingTimeout(start_tick, timeout, &remaining))
        {
            break;
        }
        vTaskDelay(EE24_MillisecondsToTicks(1U));
    }

    return false;
}

#if EE24_USE_WP_PIN == false
bool EE24_Init(EE24_HandleTypeDef *Handle, I2C_Bus_t *Bus, uint8_t I2CAddress)
#else
bool EE24_Init(EE24_HandleTypeDef *Handle, I2C_Bus_t *Bus, uint8_t I2CAddress,
               GPIO_TypeDef *WpGpio, uint16_t WpPin)
#endif
{
    if ((Handle == NULL) || (Bus == NULL) || (Bus->hi2c == NULL) ||
        (Bus->mutex == NULL) || (I2CAddress > 0x7FU))
    {
        return false;
    }

#if EE24_USE_WP_PIN == true
    if (WpGpio == NULL)
    {
        return false;
    }
    Handle->WpGpio = WpGpio;
    Handle->WpPin = WpPin;
    HAL_GPIO_WritePin(Handle->WpGpio, Handle->WpPin, GPIO_PIN_SET);
#endif

    Handle->Bus = Bus;
    Handle->Address = I2CAddress;
    if (Handle->Mutex == NULL)
    {
        Handle->Mutex = xSemaphoreCreateMutexStatic(&Handle->MutexStorage);
    }
    if ((Handle->Mutex == NULL) ||
        (EE24_GetDeviceAddress(Handle, EE24_CAPACITY_BYTES - 1U) > 0x7FU))
    {
        return false;
    }

    return I2C_IsDeviceReady(Handle->Bus, Handle->Address, 2U, 100U) == HAL_OK;
}

bool EE24_Read(EE24_HandleTypeDef *Handle, uint32_t Address,
               uint8_t *Data, size_t Len, uint32_t Timeout)
{
    uint32_t start_tick;
    uint32_t remaining;
    uint16_t chunk;
    bool answer = false;

    if ((Handle == NULL) || (Handle->Bus == NULL) || (Data == NULL) ||
        (Timeout == 0U) || !EE24_IsRangeValid(Address, Len))
    {
        return false;
    }

    start_tick = HAL_GetTick();
    if (!EE24_TakeMutex(Handle, Timeout))
    {
        return false;
    }

    while (Len != 0U)
    {
        if (!EE24_GetRemainingTimeout(start_tick, Timeout, &remaining))
        {
            break;
        }

        chunk = EE24_GetReadChunk(Address, Len);
        if (I2C_Mem_ReadEx(Handle->Bus,
                           EE24_GetDeviceAddress(Handle, Address),
                           EE24_GetMemoryAddress(Address),
                           EE24_GetMemoryAddressSize(),
                           Data,
                           chunk,
                           remaining) != HAL_OK)
        {
            break;
        }

        Address += chunk;
        Data += chunk;
        Len -= chunk;
    }

    answer = (Len == 0U);
    EE24_GiveMutex(Handle);
    return answer;
}

bool EE24_Write(EE24_HandleTypeDef *Handle, uint32_t Address,
                const uint8_t *Data, size_t Len, uint32_t Timeout)
{
    uint32_t start_tick;
    uint32_t remaining;
    uint16_t chunk;
    uint8_t device_address;
    bool answer = false;

    if ((Handle == NULL) || (Handle->Bus == NULL) || (Data == NULL) ||
        (Timeout == 0U) || !EE24_IsRangeValid(Address, Len))
    {
        return false;
    }

    start_tick = HAL_GetTick();
    if (!EE24_TakeMutex(Handle, Timeout))
    {
        return false;
    }

#if EE24_USE_WP_PIN == true
    HAL_GPIO_WritePin(Handle->WpGpio, Handle->WpPin, GPIO_PIN_RESET);
#endif

    while (Len != 0U)
    {
        if (!EE24_GetRemainingTimeout(start_tick, Timeout, &remaining))
        {
            break;
        }

        chunk = (uint16_t)(EE24_PSIZE - (Address % EE24_PSIZE));
        if ((size_t)chunk > Len)
        {
            chunk = (uint16_t)Len;
        }
        device_address = EE24_GetDeviceAddress(Handle, Address);

        if (I2C_Mem_WriteEx(Handle->Bus,
                            device_address,
                            EE24_GetMemoryAddress(Address),
                            EE24_GetMemoryAddressSize(),
                            Data,
                            chunk,
                            remaining) != HAL_OK)
        {
            break;
        }
        if (!EE24_WaitReady(Handle, device_address, start_tick, Timeout))
        {
            break;
        }

        Address += chunk;
        Data += chunk;
        Len -= chunk;
    }

    answer = (Len == 0U);

#if EE24_USE_WP_PIN == true
    HAL_GPIO_WritePin(Handle->WpGpio, Handle->WpPin, GPIO_PIN_SET);
#endif
    EE24_GiveMutex(Handle);
    return answer;
}
