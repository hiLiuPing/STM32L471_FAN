#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define __SPI_FLASH_H__
#define __LOG_H__
#define FLASH_USE_FREERTOS 0
#define log_printf(...) ((void)0)

#define FLASH_CMD_READ_ID        0x9FU
#define FLASH_CMD_READ           0x03U
#define FLASH_CMD_FAST_READ      0x0BU
#define FLASH_CMD_PAGE_PROGRAM   0x02U
#define FLASH_CMD_SECTOR_ERASE   0x20U
#define FLASH_CMD_BLOCK_ERASE    0xD8U
#define FLASH_CMD_CHIP_ERASE     0xC7U
#define FLASH_CMD_WREN           0x06U
#define FLASH_CMD_RDSR           0x05U
#define FLASH_CMD_RESET_EN       0x66U
#define FLASH_CMD_RESET          0x99U
#define FLASH_CMD_ENTER_4B       0xB7U
#define FLASH_SR_BUSY            0x01U
#define FLASH_SR_WEL             0x02U

typedef enum
{
    HAL_OK = 0,
    HAL_ERROR,
    HAL_BUSY,
    HAL_TIMEOUT
} HAL_StatusTypeDef;

typedef struct
{
    uint8_t placeholder;
} SPI_HandleTypeDef;

typedef struct
{
    uint8_t placeholder;
} GPIO_TypeDef;

typedef enum
{
    GPIO_PIN_RESET = 0,
    GPIO_PIN_SET
} GPIO_PinState;

typedef enum
{
    FLASH_T_PAGE = 0,
    FLASH_T_SECTOR,
    FLASH_T_BLOCK,
    FLASH_T_CHIP
} flash_wait_type_t;

typedef enum
{
    FLASH_TYPE_SPI = 0,
    FLASH_TYPE_INTERNAL
} flash_type_t;

typedef enum
{
    SPI_FLASH_OK = 0,
    SPI_FLASH_ERR_INVALID_ARGUMENT = -1,
    SPI_FLASH_ERR_RANGE = -2,
    SPI_FLASH_ERR_ALIGNMENT = -3,
    SPI_FLASH_ERR_IO = -4,
    SPI_FLASH_ERR_TIMEOUT = -5,
    SPI_FLASH_ERR_NOT_READY = -6,
    SPI_FLASH_ERR_LOCK_TIMEOUT = -7,
    SPI_FLASH_ERR_UNSUPPORTED = -8,
    SPI_FLASH_ERR_WRITE_ENABLE = -9
} spi_flash_status_t;

typedef struct
{
    SPI_HandleTypeDef *hspi;
    GPIO_TypeDef *cs_port;
    uint16_t cs_pin;
    uint32_t flash_size;
    uint32_t page_size;
    uint32_t sector_size;
    uint32_t block_size;
    uint8_t addr_len;
    flash_type_t type;
} spi_flash_t;

int spi_flash_read_id(spi_flash_t *f, uint32_t *id);

static uint32_t s_tick;
static uint32_t s_flash_id = 0xEF4019U;
static uint8_t s_status_register;
static uint8_t s_last_command;
static unsigned s_tx_calls;
static unsigned s_rx_calls;
static unsigned s_tx_fail_call;
static unsigned s_rx_fail_call;
static HAL_StatusTypeDef s_tx_failure = HAL_ERROR;
static HAL_StatusTypeDef s_rx_failure = HAL_ERROR;
static uint16_t s_largest_tx;
static uint16_t s_largest_rx;
static uint32_t s_largest_timeout;
static GPIO_PinState s_cs_state = GPIO_PIN_SET;

uint32_t HAL_GetTick(void)
{
    return s_tick;
}

void HAL_Delay(uint32_t delay)
{
    s_tick += delay;
}

void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState state)
{
    (void)port;
    (void)pin;
    s_cs_state = state;
}

HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef *hspi,
                                   uint8_t *data,
                                   uint16_t size,
                                   uint32_t timeout)
{
    (void)hspi;
    s_tx_calls++;
    if (size > s_largest_tx)
    {
        s_largest_tx = size;
    }
    if (timeout > s_largest_timeout)
    {
        s_largest_timeout = timeout;
    }
    if ((s_tx_fail_call != 0U) && (s_tx_calls == s_tx_fail_call))
    {
        return s_tx_failure;
    }
    if (size == 1U)
    {
        s_last_command = data[0];
    }
    return HAL_OK;
}

HAL_StatusTypeDef HAL_SPI_Receive(SPI_HandleTypeDef *hspi,
                                  uint8_t *data,
                                  uint16_t size,
                                  uint32_t timeout)
{
    (void)hspi;
    s_rx_calls++;
    if (size > s_largest_rx)
    {
        s_largest_rx = size;
    }
    if (timeout > s_largest_timeout)
    {
        s_largest_timeout = timeout;
    }
    if ((s_rx_fail_call != 0U) && (s_rx_calls == s_rx_fail_call))
    {
        return s_rx_failure;
    }

    if (s_last_command == FLASH_CMD_RDSR)
    {
        memset(data, s_status_register, size);
    }
    else if ((s_last_command == FLASH_CMD_READ_ID) && (size == 3U))
    {
        data[0] = (uint8_t)(s_flash_id >> 16);
        data[1] = (uint8_t)(s_flash_id >> 8);
        data[2] = (uint8_t)s_flash_id;
    }
    else
    {
        memset(data, 0xA5, size);
    }
    return HAL_OK;
}

#include "../../USER/BSP/spi_flash.c"

static SPI_HandleTypeDef s_spi;
static GPIO_TypeDef s_gpio;
static spi_flash_t s_flash;
static uint8_t s_large_read[70000];

static void reset_bus(void)
{
    s_tick = 0U;
    s_flash_id = 0xEF4019U;
    s_status_register = 0U;
    s_last_command = 0U;
    s_tx_calls = 0U;
    s_rx_calls = 0U;
    s_tx_fail_call = 0U;
    s_rx_fail_call = 0U;
    s_tx_failure = HAL_ERROR;
    s_rx_failure = HAL_ERROR;
    s_largest_tx = 0U;
    s_largest_rx = 0U;
    s_largest_timeout = 0U;
    s_cs_state = GPIO_PIN_SET;
}

static void init_flash(void)
{
    reset_bus();
    assert(spi_flash_init(&s_flash, &s_spi, &s_gpio, 1U) == SPI_FLASH_OK);
    assert(s_flash.flash_size == (32U * 1024U * 1024U));
    assert(s_flash.addr_len == 4U);
    assert(s_cs_state == GPIO_PIN_SET);
    assert(s_largest_timeout == 100U);
}

static void test_init_errors(void)
{
    reset_bus();
    s_tx_fail_call = 1U;
    assert(spi_flash_init(&s_flash, &s_spi, &s_gpio, 1U) == SPI_FLASH_ERR_IO);
    assert(s_cs_state == GPIO_PIN_SET);

    reset_bus();
    s_rx_fail_call = 1U;
    s_rx_failure = HAL_TIMEOUT;
    assert(spi_flash_init(&s_flash, &s_spi, &s_gpio, 1U) == SPI_FLASH_ERR_TIMEOUT);
    assert(s_cs_state == GPIO_PIN_SET);
}

static void test_read_errors_and_chunking(void)
{
    uint8_t data[16];

    init_flash();
    reset_bus();
    s_tx_fail_call = 1U;
    assert(spi_flash_read(&s_flash, 0U, data, sizeof(data)) == SPI_FLASH_ERR_IO);
    assert(s_cs_state == GPIO_PIN_SET);

    reset_bus();
    s_rx_fail_call = 1U;
    s_rx_failure = HAL_TIMEOUT;
    assert(spi_flash_read(&s_flash, 0U, data, sizeof(data)) == SPI_FLASH_ERR_TIMEOUT);
    assert(s_cs_state == GPIO_PIN_SET);

    reset_bus();
    assert(spi_flash_read(&s_flash, 0U, s_large_read, sizeof(s_large_read)) == SPI_FLASH_OK);
    assert(s_rx_calls == 2U);
    assert(s_largest_rx == UINT16_MAX);
    assert(s_largest_timeout == 100U);
}

static void test_write_error_propagation(void)
{
    uint8_t data[16] = {0U};

    init_flash();
    reset_bus();
    s_status_register = 0U;
    assert(spi_flash_write(&s_flash, 0U, data, sizeof(data)) ==
           SPI_FLASH_ERR_WRITE_ENABLE);
    assert(s_cs_state == GPIO_PIN_SET);

    reset_bus();
    s_tx_fail_call = 1U;
    assert(spi_flash_write(&s_flash, 0U, data, sizeof(data)) == SPI_FLASH_ERR_IO);
    assert(s_cs_state == GPIO_PIN_SET);

    reset_bus();
    s_status_register = FLASH_SR_WEL | FLASH_SR_BUSY;
    assert(spi_flash_write(&s_flash, 0U, data, sizeof(data)) == SPI_FLASH_ERR_TIMEOUT);
    assert(s_tick >= 20U);
    assert(s_cs_state == GPIO_PIN_SET);

    reset_bus();
    s_status_register = FLASH_SR_WEL;
    assert(spi_flash_write(&s_flash, 0U, data, sizeof(data)) == SPI_FLASH_OK);
}

static void test_erase_and_sync_errors(void)
{
    init_flash();
    assert(spi_flash_erase(&s_flash, 1U, 4096U) == SPI_FLASH_ERR_ALIGNMENT);
    assert(spi_flash_erase(&s_flash, s_flash.flash_size - 4096U, 8192U) ==
           SPI_FLASH_ERR_RANGE);

    reset_bus();
    s_status_register = FLASH_SR_WEL;
    assert(spi_flash_erase(&s_flash, 0U, 4096U) == SPI_FLASH_OK);

    reset_bus();
    s_rx_fail_call = 1U;
    assert(spi_flash_sync(&s_flash) == SPI_FLASH_ERR_IO);
    assert(s_cs_state == GPIO_PIN_SET);
}

int main(void)
{
    test_init_errors();
    test_read_errors_and_chunking();
    test_write_error_propagation();
    test_erase_and_sync_errors();
    puts("spi flash host tests: PASS");
    return 0;
}
