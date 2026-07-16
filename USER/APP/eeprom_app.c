#include "eeprom_app.h"

#include <string.h>

#include "log.h"

#define APP_CONFIG_MAGIC 0x55AA55AAUL

eeprom_ctx_t g_ee_ctx = {0};

bool AppConfig_Init(eeprom_ctx_t *ctx, I2C_Bus_t *bus, uint8_t dev_addr)
{
    I2C_HandleTypeDef *hi2c;

    if ((ctx == NULL) || (bus == NULL))
    {
        return false;
    }

    hi2c = I2C_Bus_GetHandle(bus);
    if (hi2c == NULL)
    {
        return false;
    }

    ctx->bus = bus;
    if (EE24_Init(&ctx->handle, hi2c, dev_addr))
    {
        ctx->initialized = 1U;
        log_printf("[EEPROM] init ok");
        return true;
    }

    ctx->initialized = 0U;
    log_printf("[EEPROM] init failed");
    return false;
}

bool AppConfig_Load(uint32_t offset, void *data, uint16_t size)
{
    uint8_t *buf = (uint8_t *)data;
    uint32_t magic;
    uint16_t read_crc;
    uint16_t calc_crc;

    if ((data == NULL) || (size < 6U) ||
        (g_ee_ctx.initialized == 0U) || (g_ee_ctx.bus == NULL))
    {
        return false;
    }

    I2C_Bus_Lock(g_ee_ctx.bus);
    if (!EE24_Read(&g_ee_ctx.handle, offset, buf, size, 1000U))
    {
        I2C_Bus_Unlock(g_ee_ctx.bus);
        log_printf("[EEPROM] read failed");
        return false;
    }
    I2C_Bus_Unlock(g_ee_ctx.bus);

    memcpy(&magic, buf, sizeof(magic));
    if (magic != APP_CONFIG_MAGIC)
    {
        return false;
    }

    memcpy(&read_crc, &buf[size - sizeof(read_crc)], sizeof(read_crc));
    calc_crc = AppConfig_CRC16(buf, (uint16_t)(size - sizeof(read_crc)));

    return (read_crc == calc_crc);
}

bool AppConfig_Save(uint32_t offset, void *data, uint16_t size)
{
    uint8_t *buf = (uint8_t *)data;
    uint32_t magic = APP_CONFIG_MAGIC;
    uint16_t crc;

    if ((data == NULL) || (size < 6U) ||
        (g_ee_ctx.initialized == 0U) || (g_ee_ctx.bus == NULL))
    {
        return false;
    }

    memcpy(buf, &magic, sizeof(magic));
    crc = AppConfig_CRC16(buf, (uint16_t)(size - sizeof(crc)));
    memcpy(&buf[size - sizeof(crc)], &crc, sizeof(crc));

    I2C_Bus_Lock(g_ee_ctx.bus);
    if (!EE24_Write(&g_ee_ctx.handle, offset, buf, size, 1000U))
    {
        I2C_Bus_Unlock(g_ee_ctx.bus);
        log_printf("[EEPROM] write failed");
        return false;
    }
    I2C_Bus_Unlock(g_ee_ctx.bus);

    return true;
}

uint16_t AppConfig_CRC16(uint8_t *ptr, uint16_t len)
{
    uint16_t crc = 0xFFFFU;

    while (len-- != 0U)
    {
        crc ^= *ptr++;
        for (uint8_t i = 0U; i < 8U; i++)
        {
            if ((crc & 0x0001U) != 0U)
            {
                crc >>= 1;
                crc ^= 0xA001U;
            }
            else
            {
                crc >>= 1;
            }
        }
    }

    return crc;
}
