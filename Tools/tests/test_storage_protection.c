#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Keep the production modules under test while replacing hardware services. */
#define __LOG_H__
#define log_printf(...) ((void)0)

/* ================= EEPROM application stubs ================= */
#define __EEPROM_APP_H

typedef struct
{
    uint8_t placeholder;
} EE24_HandleTypeDef;

typedef struct
{
    uint8_t placeholder;
} I2C_Bus_t;

typedef struct
{
    EE24_HandleTypeDef handle;
    uint8_t initialized;
} eeprom_ctx_t;

typedef enum
{
    APP_CONFIG_LOAD_OK = 0,
    APP_CONFIG_LOAD_INVALID_DATA,
    APP_CONFIG_LOAD_IO_ERROR,
    APP_CONFIG_LOAD_NOT_READY,
    APP_CONFIG_LOAD_INVALID_ARGUMENT
} AppConfig_LoadResult_t;

bool AppConfig_Init(eeprom_ctx_t *ctx, I2C_Bus_t *bus, uint8_t dev_addr);
AppConfig_LoadResult_t AppConfig_Load(uint32_t offset, void *data, uint16_t size);
bool AppConfig_Save(uint32_t offset, void *data, uint16_t size);
uint16_t AppConfig_CRC16(uint8_t *ptr, uint16_t len);

static uint8_t s_eeprom_data[256];
static bool s_eeprom_read_ok = true;
static bool s_eeprom_write_ok = true;

bool EE24_Init(EE24_HandleTypeDef *handle, I2C_Bus_t *bus, uint8_t address)
{
    (void)handle;
    (void)bus;
    (void)address;
    return true;
}

bool EE24_Read(EE24_HandleTypeDef *handle, uint32_t address,
               uint8_t *data, size_t len, uint32_t timeout)
{
    (void)handle;
    (void)timeout;
    if (!s_eeprom_read_ok || ((address + len) > sizeof(s_eeprom_data)))
    {
        return false;
    }
    memcpy(data, &s_eeprom_data[address], len);
    return true;
}

bool EE24_Write(EE24_HandleTypeDef *handle, uint32_t address,
                const uint8_t *data, size_t len, uint32_t timeout)
{
    (void)handle;
    (void)timeout;
    if (!s_eeprom_write_ok || ((address + len) > sizeof(s_eeprom_data)))
    {
        return false;
    }
    memcpy(&s_eeprom_data[address], data, len);
    return true;
}

#include "../../USER/APP/eeprom_app.c"

/* ================= LittleFS port stubs ================= */
#define __LFS_PORT_H__
#define FLASH_USE_FREERTOS 0
#define LFS_ERR_IO          (-5)
#define LFS_ERR_INVAL       (-22)
#define LFS_ERR_CORRUPT     (-84)
#define FLASH_TOTAL_SIZE    (32U * 1024U * 1024U)
#define LFS_FLASH_OFFSET    (1U * 1024U * 1024U)
#define LFS_READ_SIZE       16U
#define LFS_PROG_SIZE       256U
#define LFS_BLOCK_SIZE      4096U
#define LFS_CACHE_SIZE      1024U
#define LFS_LOOKAHEAD_SIZE  64U
#define LFS_BLOCK_COUNT     ((FLASH_TOTAL_SIZE - LFS_FLASH_OFFSET) / LFS_BLOCK_SIZE)

typedef uint32_t lfs_block_t;
typedef uint32_t lfs_off_t;
typedef uint32_t lfs_size_t;

typedef struct
{
    uint8_t placeholder;
} lfs_t;

typedef struct
{
    uint8_t placeholder;
} spi_flash_t;

struct lfs_config
{
    void *context;
    int (*read)(const struct lfs_config *, lfs_block_t, lfs_off_t,
                void *, lfs_size_t);
    int (*prog)(const struct lfs_config *, lfs_block_t, lfs_off_t,
                const void *, lfs_size_t);
    int (*erase)(const struct lfs_config *, lfs_block_t);
    int (*sync)(const struct lfs_config *);
    lfs_size_t read_size;
    lfs_size_t prog_size;
    lfs_size_t block_size;
    lfs_size_t block_count;
    int32_t block_cycles;
    lfs_size_t cache_size;
    lfs_size_t lookahead_size;
    void *read_buffer;
    void *prog_buffer;
    void *lookahead_buffer;
};

static int s_mount_result;
static int s_format_result;
static int s_unmount_result;
static unsigned s_mount_calls;
static unsigned s_format_calls;
static unsigned s_unmount_calls;

int lfs_mount(lfs_t *lfs, const struct lfs_config *config)
{
    (void)lfs;
    (void)config;
    s_mount_calls++;
    return s_mount_result;
}

int lfs_format(lfs_t *lfs, const struct lfs_config *config)
{
    (void)lfs;
    (void)config;
    s_format_calls++;
    return s_format_result;
}

int lfs_unmount(lfs_t *lfs)
{
    (void)lfs;
    s_unmount_calls++;
    return s_unmount_result;
}

int spi_flash_read(spi_flash_t *flash, uint32_t address,
                   uint8_t *data, uint32_t len)
{
    (void)flash;
    (void)address;
    (void)data;
    (void)len;
    return 0;
}

int spi_flash_write(spi_flash_t *flash, uint32_t address,
                    const uint8_t *data, uint32_t len)
{
    (void)flash;
    (void)address;
    (void)data;
    (void)len;
    return 0;
}

int spi_flash_erase(spi_flash_t *flash, uint32_t address, uint32_t len)
{
    (void)flash;
    (void)address;
    (void)len;
    return 0;
}

int spi_flash_sync(spi_flash_t *flash)
{
    (void)flash;
    return 0;
}

#include "../../USER/Middle/Transfer/lfs_port.c"

static void test_eeprom_load_results(void)
{
    uint8_t source[256] = {0};
    uint8_t loaded[256];

    g_ee_ctx.initialized = 0U;
    assert(AppConfig_Load(0U, loaded, sizeof(loaded)) == APP_CONFIG_LOAD_NOT_READY);
    assert(AppConfig_Load(0U, NULL, sizeof(loaded)) == APP_CONFIG_LOAD_INVALID_ARGUMENT);

    g_ee_ctx.initialized = 1U;
    s_eeprom_read_ok = true;
    s_eeprom_write_ok = true;
    source[8] = 0x5AU;
    assert(AppConfig_Save(0U, source, sizeof(source)));
    assert(AppConfig_Load(0U, loaded, sizeof(loaded)) == APP_CONFIG_LOAD_OK);
    assert(loaded[8] == 0x5AU);

    s_eeprom_data[0] ^= 0x01U;
    assert(AppConfig_Load(0U, loaded, sizeof(loaded)) == APP_CONFIG_LOAD_INVALID_DATA);
    s_eeprom_data[0] ^= 0x01U;

    s_eeprom_data[8] ^= 0x01U;
    assert(AppConfig_Load(0U, loaded, sizeof(loaded)) == APP_CONFIG_LOAD_INVALID_DATA);
    s_eeprom_data[8] ^= 0x01U;

    s_eeprom_read_ok = false;
    assert(AppConfig_Load(0U, loaded, sizeof(loaded)) == APP_CONFIG_LOAD_IO_ERROR);
    s_eeprom_read_ok = true;
}

static void reset_lfs_state(void)
{
    memset(&g_lfs, 0, sizeof(g_lfs));
    memset(&g_cfg, 0, sizeof(g_cfg));
    s_lfs_configured = 0U;
    s_lfs_mounted = 0U;
    s_mount_result = 0;
    s_format_result = 0;
    s_unmount_result = 0;
    s_mount_calls = 0U;
    s_format_calls = 0U;
    s_unmount_calls = 0U;
}

static void test_lfs_mount_never_formats(void)
{
    spi_flash_t flash = {0};

    reset_lfs_state();
    s_mount_result = LFS_ERR_IO;
    assert(lfs_port_init(&flash) == LFS_ERR_IO);
    assert(s_mount_calls == 1U);
    assert(s_format_calls == 0U);
    assert(lfs_port_get() == NULL);

    reset_lfs_state();
    s_mount_result = LFS_ERR_CORRUPT;
    assert(lfs_port_init(&flash) == LFS_ERR_CORRUPT);
    assert(s_mount_calls == 1U);
    assert(s_format_calls == 0U);
    assert(lfs_port_get() == NULL);
}

static void test_lfs_explicit_format(void)
{
    spi_flash_t flash = {0};

    reset_lfs_state();
    assert(lfs_port_format_and_mount() == LFS_ERR_INVAL);
    assert(s_format_calls == 0U);

    s_mount_result = LFS_ERR_IO;
    assert(lfs_port_init(&flash) == LFS_ERR_IO);

    s_format_result = LFS_ERR_IO;
    assert(lfs_port_format_and_mount() == LFS_ERR_IO);
    assert(s_format_calls == 1U);
    assert(lfs_port_get() == NULL);

    s_format_result = 0;
    s_mount_result = LFS_ERR_CORRUPT;
    assert(lfs_port_format_and_mount() == LFS_ERR_CORRUPT);
    assert(lfs_port_get() == NULL);

    s_mount_result = 0;
    assert(lfs_port_format_and_mount() == 0);
    assert(lfs_port_get() == &g_lfs);

    assert(lfs_port_format_and_mount() == 0);
    assert(s_unmount_calls == 1U);
    assert(lfs_port_get() == &g_lfs);
}

int main(void)
{
    test_eeprom_load_results();
    test_lfs_mount_never_formats();
    test_lfs_explicit_format();
    puts("storage protection host tests: PASS");
    return 0;
}
