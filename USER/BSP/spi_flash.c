#include "spi_flash.h"

#include <string.h>

#include "log.h"

#define SPI_FLASH_SPI_TIMEOUT_MS          100U
#define SPI_FLASH_LOCK_TIMEOUT_MS         1000U
#define SPI_FLASH_SYNC_TIMEOUT_MS         5000U
#define SPI_FLASH_PAGE_TIMEOUT_MS         20U
#define SPI_FLASH_SECTOR_TIMEOUT_MS       3000U
#define SPI_FLASH_BLOCK_TIMEOUT_MS        5000U
#define SPI_FLASH_CHIP_TIMEOUT_MS         300000U

static void flash_delay_ms(uint32_t delay_ms)
{
#if FLASH_USE_FREERTOS
    TickType_t ticks = pdMS_TO_TICKS(delay_ms);

    if ((delay_ms != 0U) && (ticks == 0U))
    {
        ticks = 1U;
    }
    vTaskDelay(ticks);
#else
    HAL_Delay(delay_ms);
#endif
}

static int flash_lock(spi_flash_t *f)
{
    if (f == NULL)
    {
        return SPI_FLASH_ERR_INVALID_ARGUMENT;
    }

#if FLASH_USE_FREERTOS
    TickType_t ticks;

    if (f->lock == NULL)
    {
        return SPI_FLASH_ERR_NOT_READY;
    }

    ticks = pdMS_TO_TICKS(SPI_FLASH_LOCK_TIMEOUT_MS);
    if (ticks == 0U)
    {
        ticks = 1U;
    }
    if (xSemaphoreTake(f->lock, ticks) != pdTRUE)
    {
        return SPI_FLASH_ERR_LOCK_TIMEOUT;
    }
#endif

    return SPI_FLASH_OK;
}

static void flash_unlock(spi_flash_t *f)
{
#if FLASH_USE_FREERTOS
    if ((f != NULL) && (f->lock != NULL))
    {
        (void)xSemaphoreGive(f->lock);
    }
#else
    (void)f;
#endif
}

static void flash_destroy_lock(spi_flash_t *f)
{
#if FLASH_USE_FREERTOS
    if ((f != NULL) && (f->lock != NULL))
    {
        vSemaphoreDelete(f->lock);
        f->lock = NULL;
    }
#else
    (void)f;
#endif
}

static inline void cs_low(spi_flash_t *f)
{
    HAL_GPIO_WritePin(f->cs_port, f->cs_pin, GPIO_PIN_RESET);
}

static inline void cs_high(spi_flash_t *f)
{
    HAL_GPIO_WritePin(f->cs_port, f->cs_pin, GPIO_PIN_SET);
}

static int flash_status_from_hal(HAL_StatusTypeDef status)
{
    if (status == HAL_OK)
    {
        return SPI_FLASH_OK;
    }
    if (status == HAL_TIMEOUT)
    {
        return SPI_FLASH_ERR_TIMEOUT;
    }
    return SPI_FLASH_ERR_IO;
}

static int flash_spi_transmit(spi_flash_t *f, const uint8_t *data, uint32_t len)
{
    while (len != 0U)
    {
        uint16_t chunk = (len > UINT16_MAX) ? UINT16_MAX : (uint16_t)len;
        HAL_StatusTypeDef status = HAL_SPI_Transmit(f->hspi,
                                                    (uint8_t *)data,
                                                    chunk,
                                                    SPI_FLASH_SPI_TIMEOUT_MS);
        int ret = flash_status_from_hal(status);

        if (ret != SPI_FLASH_OK)
        {
            return ret;
        }
        data += chunk;
        len -= chunk;
    }

    return SPI_FLASH_OK;
}

static int flash_spi_receive(spi_flash_t *f, uint8_t *data, uint32_t len)
{
    while (len != 0U)
    {
        uint16_t chunk = (len > UINT16_MAX) ? UINT16_MAX : (uint16_t)len;
        HAL_StatusTypeDef status = HAL_SPI_Receive(f->hspi,
                                                   data,
                                                   chunk,
                                                   SPI_FLASH_SPI_TIMEOUT_MS);
        int ret = flash_status_from_hal(status);

        if (ret != SPI_FLASH_OK)
        {
            return ret;
        }
        data += chunk;
        len -= chunk;
    }

    return SPI_FLASH_OK;
}

static int flash_send_command(spi_flash_t *f, uint8_t command)
{
    int ret;

    cs_low(f);
    ret = flash_spi_transmit(f, &command, 1U);
    cs_high(f);
    return ret;
}

static int flash_read_status(spi_flash_t *f, uint8_t *status)
{
    uint8_t command = FLASH_CMD_RDSR;
    int ret;

    if (status == NULL)
    {
        return SPI_FLASH_ERR_INVALID_ARGUMENT;
    }

    *status = 0U;
    cs_low(f);
    ret = flash_spi_transmit(f, &command, 1U);
    if (ret == SPI_FLASH_OK)
    {
        ret = flash_spi_receive(f, status, 1U);
    }
    cs_high(f);

    if ((ret == SPI_FLASH_OK) && (*status == 0xFFU))
    {
        return SPI_FLASH_ERR_IO;
    }
    return ret;
}

static int flash_write_enable(spi_flash_t *f)
{
    uint8_t status;
    int ret = flash_send_command(f, FLASH_CMD_WREN);

    if (ret != SPI_FLASH_OK)
    {
        return ret;
    }

    ret = flash_read_status(f, &status);
    if (ret != SPI_FLASH_OK)
    {
        return ret;
    }
    if ((status & FLASH_SR_WEL) == 0U)
    {
        return SPI_FLASH_ERR_WRITE_ENABLE;
    }
    return SPI_FLASH_OK;
}

static int flash_reset(spi_flash_t *f)
{
    int ret = flash_send_command(f, FLASH_CMD_RESET_EN);

    if (ret != SPI_FLASH_OK)
    {
        return ret;
    }
    flash_delay_ms(1U);

    ret = flash_send_command(f, FLASH_CMD_RESET);
    if (ret != SPI_FLASH_OK)
    {
        return ret;
    }
    flash_delay_ms(5U);
    return SPI_FLASH_OK;
}

static uint32_t flash_wait_timeout(flash_wait_type_t type)
{
    switch (type)
    {
    case FLASH_T_PAGE:
        return SPI_FLASH_PAGE_TIMEOUT_MS;
    case FLASH_T_SECTOR:
        return SPI_FLASH_SECTOR_TIMEOUT_MS;
    case FLASH_T_BLOCK:
        return SPI_FLASH_BLOCK_TIMEOUT_MS;
    case FLASH_T_CHIP:
        return SPI_FLASH_CHIP_TIMEOUT_MS;
    default:
        return SPI_FLASH_SYNC_TIMEOUT_MS;
    }
}

static int flash_wait_ready_ms(spi_flash_t *f, uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();

    for (;;)
    {
        uint8_t status;
        int ret = flash_read_status(f, &status);

        if (ret != SPI_FLASH_OK)
        {
            return ret;
        }
        if ((status & FLASH_SR_BUSY) == 0U)
        {
            return SPI_FLASH_OK;
        }
        if ((HAL_GetTick() - start) >= timeout_ms)
        {
            log_printf("[FLASH] ready timeout status=0x%02X", status);
            return SPI_FLASH_ERR_TIMEOUT;
        }
        flash_delay_ms(1U);
    }
}

static int flash_wait_ready(spi_flash_t *f, flash_wait_type_t type)
{
    return flash_wait_ready_ms(f, flash_wait_timeout(type));
}

static inline void pack_addr(spi_flash_t *f, uint32_t addr, uint8_t *bytes)
{
    if (f->addr_len == 3U)
    {
        bytes[0] = (uint8_t)(addr >> 16);
        bytes[1] = (uint8_t)(addr >> 8);
        bytes[2] = (uint8_t)addr;
    }
    else
    {
        bytes[0] = (uint8_t)(addr >> 24);
        bytes[1] = (uint8_t)(addr >> 16);
        bytes[2] = (uint8_t)(addr >> 8);
        bytes[3] = (uint8_t)addr;
    }
}

static int flash_validate(const spi_flash_t *f)
{
    if ((f == NULL) || (f->hspi == NULL) || (f->cs_port == NULL))
    {
        return SPI_FLASH_ERR_INVALID_ARGUMENT;
    }
    if ((f->flash_size == 0U) ||
        ((f->addr_len != 3U) && (f->addr_len != 4U)))
    {
        return SPI_FLASH_ERR_NOT_READY;
    }
    return SPI_FLASH_OK;
}

static int flash_validate_range(const spi_flash_t *f, uint32_t addr, uint32_t len)
{
    int ret = flash_validate(f);

    if (ret != SPI_FLASH_OK)
    {
        return ret;
    }
    if (len == 0U)
    {
        return SPI_FLASH_OK;
    }
    if ((addr >= f->flash_size) || (len > (f->flash_size - addr)))
    {
        return SPI_FLASH_ERR_RANGE;
    }
    return SPI_FLASH_OK;
}

int spi_flash_init(spi_flash_t *f,
                   SPI_HandleTypeDef *hspi,
                   GPIO_TypeDef *cs_port,
                   uint16_t cs_pin)
{
    uint32_t id;
    int ret;

    if ((f == NULL) || (hspi == NULL) || (cs_port == NULL))
    {
        return SPI_FLASH_ERR_INVALID_ARGUMENT;
    }

    memset(f, 0, sizeof(*f));
    f->hspi = hspi;
    f->cs_port = cs_port;
    f->cs_pin = cs_pin;
    f->page_size = 256U;
    f->sector_size = 4096U;
    f->block_size = 65536U;
    f->addr_len = 3U;
    f->type = FLASH_TYPE_SPI;

#if FLASH_USE_FREERTOS
    f->lock = xSemaphoreCreateMutex();
    if (f->lock == NULL)
    {
        return SPI_FLASH_ERR_NOT_READY;
    }
#endif

    cs_high(f);
    ret = flash_reset(f);
    if (ret != SPI_FLASH_OK)
    {
        log_printf("[FLASH] reset failed ret=%d", ret);
        goto fail;
    }

    ret = spi_flash_read_id(f, &id);
    if (ret != SPI_FLASH_OK)
    {
        log_printf("[FLASH] read ID failed ret=%d", ret);
        goto fail;
    }
    log_printf("Flash ID: 0x%06lX", (unsigned long)id);

    if ((id >> 16) != 0xEFU)
    {
        ret = SPI_FLASH_ERR_UNSUPPORTED;
        goto fail;
    }

    switch (id & 0xFFU)
    {
    case 0x17U:
        f->flash_size = 8U * 1024U * 1024U;
        break;
    case 0x18U:
        f->flash_size = 16U * 1024U * 1024U;
        break;
    case 0x19U:
        f->flash_size = 32U * 1024U * 1024U;
        f->addr_len = 4U;
        break;
    default:
        ret = SPI_FLASH_ERR_UNSUPPORTED;
        goto fail;
    }

    if (f->addr_len == 4U)
    {
        ret = flash_lock(f);
        if (ret == SPI_FLASH_OK)
        {
            ret = flash_send_command(f, FLASH_CMD_ENTER_4B);
            flash_unlock(f);
        }
        if (ret != SPI_FLASH_OK)
        {
            log_printf("[FLASH] enter 4-byte mode failed ret=%d", ret);
            goto fail;
        }
    }

    return SPI_FLASH_OK;

fail:
    f->flash_size = 0U;
    flash_destroy_lock(f);
    return ret;
}

int spi_flash_read(spi_flash_t *f, uint32_t addr, uint8_t *buf, uint32_t len)
{
    uint8_t command;
    uint8_t address[4];
    int ret = flash_validate_range(f, addr, len);

    if (ret != SPI_FLASH_OK)
    {
        return ret;
    }
    if (len == 0U)
    {
        return SPI_FLASH_OK;
    }
    if (buf == NULL)
    {
        return SPI_FLASH_ERR_INVALID_ARGUMENT;
    }

    ret = flash_lock(f);
    if (ret != SPI_FLASH_OK)
    {
        return ret;
    }

    command = (len > 512U) ? FLASH_CMD_FAST_READ : FLASH_CMD_READ;
    pack_addr(f, addr, address);
    cs_low(f);
    ret = flash_spi_transmit(f, &command, 1U);
    if (ret == SPI_FLASH_OK)
    {
        ret = flash_spi_transmit(f, address, f->addr_len);
    }
    if ((ret == SPI_FLASH_OK) && (command == FLASH_CMD_FAST_READ))
    {
        uint8_t dummy = 0U;
        ret = flash_spi_transmit(f, &dummy, 1U);
    }
    if (ret == SPI_FLASH_OK)
    {
        ret = flash_spi_receive(f, buf, len);
    }
    cs_high(f);
    flash_unlock(f);

    if (ret != SPI_FLASH_OK)
    {
        log_printf("[FLASH] read failed addr=0x%08lX len=%lu ret=%d",
                   (unsigned long)addr, (unsigned long)len, ret);
    }
    return ret;
}

int spi_flash_write(spi_flash_t *f, uint32_t addr, const uint8_t *buf, uint32_t len)
{
    uint32_t remaining = len;
    int ret = flash_validate_range(f, addr, len);

    if (ret != SPI_FLASH_OK)
    {
        return ret;
    }
    if (len == 0U)
    {
        return SPI_FLASH_OK;
    }
    if (buf == NULL)
    {
        return SPI_FLASH_ERR_INVALID_ARGUMENT;
    }

    ret = flash_lock(f);
    if (ret != SPI_FLASH_OK)
    {
        return ret;
    }

    while (remaining != 0U)
    {
        uint8_t command = FLASH_CMD_PAGE_PROGRAM;
        uint8_t address[4];
        uint32_t page_offset = addr % f->page_size;
        uint32_t chunk = f->page_size - page_offset;

        if (chunk > remaining)
        {
            chunk = remaining;
        }

        ret = flash_write_enable(f);
        if (ret != SPI_FLASH_OK)
        {
            break;
        }

        pack_addr(f, addr, address);
        cs_low(f);
        ret = flash_spi_transmit(f, &command, 1U);
        if (ret == SPI_FLASH_OK)
        {
            ret = flash_spi_transmit(f, address, f->addr_len);
        }
        if (ret == SPI_FLASH_OK)
        {
            ret = flash_spi_transmit(f, buf, chunk);
        }
        cs_high(f);
        if (ret != SPI_FLASH_OK)
        {
            break;
        }

        ret = flash_wait_ready(f, FLASH_T_PAGE);
        if (ret != SPI_FLASH_OK)
        {
            break;
        }

        addr += chunk;
        buf += chunk;
        remaining -= chunk;
    }

    flash_unlock(f);
    if (ret != SPI_FLASH_OK)
    {
        log_printf("[FLASH] write failed addr=0x%08lX remaining=%lu ret=%d",
                   (unsigned long)addr, (unsigned long)remaining, ret);
    }
    return ret;
}

int spi_flash_erase(spi_flash_t *f, uint32_t addr, uint32_t len)
{
    uint32_t end;
    int ret = flash_validate_range(f, addr, len);

    if (ret != SPI_FLASH_OK)
    {
        return ret;
    }
    if (len == 0U)
    {
        return SPI_FLASH_OK;
    }
    if ((f->sector_size == 0U) ||
        ((addr % f->sector_size) != 0U) ||
        ((len % f->sector_size) != 0U))
    {
        return SPI_FLASH_ERR_ALIGNMENT;
    }

    ret = flash_lock(f);
    if (ret != SPI_FLASH_OK)
    {
        return ret;
    }

    end = addr + len;
    while (addr < end)
    {
        uint8_t command;
        uint8_t address[4];
        uint32_t erase_size;
        flash_wait_type_t wait_type;

        if ((f->block_size != 0U) &&
            ((addr % f->block_size) == 0U) &&
            ((end - addr) >= f->block_size))
        {
            command = FLASH_CMD_BLOCK_ERASE;
            erase_size = f->block_size;
            wait_type = FLASH_T_BLOCK;
        }
        else
        {
            command = FLASH_CMD_SECTOR_ERASE;
            erase_size = f->sector_size;
            wait_type = FLASH_T_SECTOR;
        }

        ret = flash_write_enable(f);
        if (ret != SPI_FLASH_OK)
        {
            break;
        }

        pack_addr(f, addr, address);
        cs_low(f);
        ret = flash_spi_transmit(f, &command, 1U);
        if (ret == SPI_FLASH_OK)
        {
            ret = flash_spi_transmit(f, address, f->addr_len);
        }
        cs_high(f);
        if (ret != SPI_FLASH_OK)
        {
            break;
        }

        ret = flash_wait_ready(f, wait_type);
        if (ret != SPI_FLASH_OK)
        {
            break;
        }
        addr += erase_size;
    }

    flash_unlock(f);
    if (ret != SPI_FLASH_OK)
    {
        log_printf("[FLASH] erase failed addr=0x%08lX ret=%d",
                   (unsigned long)addr, ret);
    }
    return ret;
}

int spi_flash_chip_erase(spi_flash_t *f)
{
    int ret = flash_validate(f);

    if (ret != SPI_FLASH_OK)
    {
        return ret;
    }

    ret = flash_lock(f);
    if (ret != SPI_FLASH_OK)
    {
        return ret;
    }

    ret = flash_write_enable(f);
    if (ret == SPI_FLASH_OK)
    {
        ret = flash_send_command(f, FLASH_CMD_CHIP_ERASE);
    }
    if (ret == SPI_FLASH_OK)
    {
        ret = flash_wait_ready(f, FLASH_T_CHIP);
    }

    flash_unlock(f);
    if (ret != SPI_FLASH_OK)
    {
        log_printf("[FLASH] chip erase failed ret=%d", ret);
    }
    return ret;
}

int spi_flash_read_id(spi_flash_t *f, uint32_t *id)
{
    uint8_t command = FLASH_CMD_READ_ID;
    uint8_t id_bytes[3] = {0U};
    int ret;

    if ((f == NULL) || (f->hspi == NULL) ||
        (f->cs_port == NULL) || (id == NULL))
    {
        return SPI_FLASH_ERR_INVALID_ARGUMENT;
    }

    *id = 0U;
    ret = flash_lock(f);
    if (ret != SPI_FLASH_OK)
    {
        return ret;
    }

    cs_low(f);
    ret = flash_spi_transmit(f, &command, 1U);
    if (ret == SPI_FLASH_OK)
    {
        ret = flash_spi_receive(f, id_bytes, sizeof(id_bytes));
    }
    cs_high(f);
    flash_unlock(f);

    if (ret == SPI_FLASH_OK)
    {
        *id = ((uint32_t)id_bytes[0] << 16) |
              ((uint32_t)id_bytes[1] << 8) |
              (uint32_t)id_bytes[2];
    }
    return ret;
}

int spi_flash_sync(spi_flash_t *f)
{
    int ret = flash_validate(f);

    if (ret != SPI_FLASH_OK)
    {
        return ret;
    }

    ret = flash_lock(f);
    if (ret != SPI_FLASH_OK)
    {
        return ret;
    }
    ret = flash_wait_ready_ms(f, SPI_FLASH_SYNC_TIMEOUT_MS);
    flash_unlock(f);
    return ret;
}
