#include "lfs_port.h"
#include "log.h"
#include <string.h>

/* ================= 全局 ================= */
spi_flash_t g_spi_flash = {0};
lfs_t g_lfs;
struct lfs_config g_cfg;
#if FLASH_USE_FREERTOS
SemaphoreHandle_t g_lfs_lock = NULL;
#endif

/* ================= buffer ================= */
static uint8_t read_buf[LFS_CACHE_SIZE];
static uint8_t prog_buf[LFS_CACHE_SIZE];
static uint8_t lookahead_buf[LFS_LOOKAHEAD_SIZE];
static uint8_t s_lfs_configured = 0U;
static uint8_t s_lfs_mounted = 0U;

/* =========================================================
 * ⭐ 关键：统一地址映射（修复核心）
 * ========================================================= */
static inline uint32_t lfs_to_flash_addr(lfs_block_t block, lfs_off_t off)
{
    return LFS_FLASH_OFFSET + block * LFS_BLOCK_SIZE + off;
}

/* ================= READ ================= */
int lfs_read_cb(const struct lfs_config *c,
                lfs_block_t block,
                lfs_off_t off,
                void *buffer,
                lfs_size_t size)
{
    spi_flash_t *flash = (spi_flash_t *)c->context;

    uint32_t addr = lfs_to_flash_addr(block, off);

    return spi_flash_read(flash, addr, buffer, size);
}

/* ================= PROGRAM ================= */
int lfs_prog_cb(const struct lfs_config *c,
                lfs_block_t block,
                lfs_off_t off,
                const void *buffer,
                lfs_size_t size)
{
    spi_flash_t *flash = (spi_flash_t *)c->context;

    uint32_t addr = lfs_to_flash_addr(block, off);

    return spi_flash_write(flash, addr, buffer, size);
}

/* ================= ERASE ================= */
int lfs_erase_cb(const struct lfs_config *c,
                lfs_block_t block)
{
    spi_flash_t *flash = (spi_flash_t *)c->context;

    uint32_t addr = LFS_FLASH_OFFSET + block * LFS_BLOCK_SIZE;

    return spi_flash_erase(flash, addr, LFS_BLOCK_SIZE);
}

/* ================= SYNC ================= */
int lfs_sync_cb(const struct lfs_config *c)
{
    spi_flash_t *flash = (spi_flash_t *)c->context;

    return spi_flash_sync(flash);
}

#if FLASH_USE_FREERTOS
static int lfs_cfg_lock_cb(const struct lfs_config *c)
{
    (void)c;

    if (g_lfs_lock == NULL)
    {
        return 0;
    }

    return (xSemaphoreTakeRecursive(g_lfs_lock, portMAX_DELAY) == pdTRUE) ? 0 : -1;
}

static int lfs_cfg_unlock_cb(const struct lfs_config *c)
{
    (void)c;

    if (g_lfs_lock == NULL)
    {
        return 0;
    }

    return (xSemaphoreGiveRecursive(g_lfs_lock) == pdTRUE) ? 0 : -1;
}

void lfs_port_lock(void)
{
    if (g_lfs_lock != NULL)
    {
        (void)xSemaphoreTakeRecursive(g_lfs_lock, portMAX_DELAY);
    }
}

void lfs_port_unlock(void)
{
    if (g_lfs_lock != NULL)
    {
        (void)xSemaphoreGiveRecursive(g_lfs_lock);
    }
}
#else
void lfs_port_lock(void) {}
void lfs_port_unlock(void) {}
#endif

/* ================= INIT ================= */
int lfs_port_init(spi_flash_t *flash)
{
    int ret;

    if (flash == NULL)
    {
        log_printf("[LFS] invalid flash context");
        return LFS_ERR_INVAL;
    }

    if (s_lfs_mounted != 0U)
    {
        return 0;
    }

    memset(&g_cfg, 0, sizeof(g_cfg));
    s_lfs_configured = 0U;
    s_lfs_mounted = 0U;

#if FLASH_USE_FREERTOS
    if (g_lfs_lock == NULL)
    {
        g_lfs_lock = xSemaphoreCreateRecursiveMutex();
        if (g_lfs_lock == NULL)
        {
            log_printf("[LFS] mutex create FAIL\r\n");
            return -1;
        }
    }
#endif

    g_cfg.context = flash;

    g_cfg.read  = lfs_read_cb;
    g_cfg.prog  = lfs_prog_cb;
    g_cfg.erase = lfs_erase_cb;
    g_cfg.sync  = lfs_sync_cb;
#if FLASH_USE_FREERTOS
    g_cfg.lock  = lfs_cfg_lock_cb;
    g_cfg.unlock = lfs_cfg_unlock_cb;
#endif

    g_cfg.read_size = LFS_READ_SIZE;
    g_cfg.prog_size = LFS_PROG_SIZE;
    g_cfg.block_size = LFS_BLOCK_SIZE;
    g_cfg.block_count = LFS_BLOCK_COUNT;

    g_cfg.cache_size = LFS_CACHE_SIZE;
    g_cfg.lookahead_size = LFS_LOOKAHEAD_SIZE;
    g_cfg.block_cycles = 500;

    g_cfg.read_buffer = read_buf;
    g_cfg.prog_buffer = prog_buf;
    g_cfg.lookahead_buffer = lookahead_buf;
    s_lfs_configured = 1U;

    log_printf("[LFS] mount start");
    ret = lfs_mount(&g_lfs, &g_cfg);
    log_printf("[LFS] mount ret=%d", ret);

    if (ret == 0)
    {
        s_lfs_mounted = 1U;
        log_printf("[LFS] mount OK\r\n");
    }
    else
    {
        log_printf("[LFS] mount FAIL ret=%d, filesystem preserved\r\n", ret);
    }

    return ret;
}

int lfs_port_format_and_mount(void)
{
    int ret;

    if ((s_lfs_configured == 0U) || (g_cfg.context == NULL))
    {
        log_printf("[LFS] format rejected: port not configured");
        return LFS_ERR_INVAL;
    }

    if (s_lfs_mounted != 0U)
    {
        ret = lfs_unmount(&g_lfs);
        if (ret != 0)
        {
            log_printf("[LFS] unmount before format FAIL ret=%d", ret);
            return ret;
        }
        s_lfs_mounted = 0U;
    }

    log_printf("[LFS] explicit format start: all filesystem data will be erased");
    ret = lfs_format(&g_lfs, &g_cfg);
    log_printf("[LFS] explicit format ret=%d", ret);
    if (ret != 0)
    {
        return ret;
    }

    ret = lfs_mount(&g_lfs, &g_cfg);
    log_printf("[LFS] mount after format ret=%d", ret);
    if (ret == 0)
    {
        s_lfs_mounted = 1U;
    }

    return ret;
}

/* ================= handle ================= */
lfs_t* lfs_port_get(void)
{
    return (s_lfs_mounted != 0U) ? &g_lfs : NULL;
}
