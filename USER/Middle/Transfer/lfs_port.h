#ifndef __LFS_PORT_H__
#define __LFS_PORT_H__

#include "lfs.h"
#include "spi_flash.h"
#if FLASH_USE_FREERTOS
#include "semphr.h"
#endif

/* ================= Flash分区（关键） ================= */
#define FLASH_TOTAL_SIZE     (32 * 1024 * 1024)

#define BOOT_SIZE            (1 * 1024 * 1024)

/* ================= FS起始地址（关键修复点） ================= */
#define LFS_FLASH_OFFSET     (BOOT_SIZE)

/* ================= LittleFS配置 ================= */
#define LFS_READ_SIZE        16
#define LFS_PROG_SIZE        256
#define LFS_BLOCK_SIZE       4096

// #define LFS_CACHE_SIZE       256
// #define LFS_LOOKAHEAD_SIZE   32

/* ================= 关键修复：将缓存扩大到1K 包的大小 */
#define LFS_CACHE_SIZE       1024   // 必须 >= 1024，建议与 YModem 包大小对齐
/* ================= 提高元数据查找速度 */
#define LFS_LOOKAHEAD_SIZE   64     // 适当增大，减少在大容量 Flash (32MB) 中寻找空闲块的时间

/* ================= 只给FS区域 */
#define LFS_BLOCK_COUNT   ((FLASH_TOTAL_SIZE - LFS_FLASH_OFFSET) / LFS_BLOCK_SIZE)

/* ================= 全局 ================= */
extern lfs_t g_lfs;
extern struct lfs_config g_cfg;
#if FLASH_USE_FREERTOS
extern SemaphoreHandle_t g_lfs_lock;
#endif

/* ================= API ================= */
int lfs_port_init(spi_flash_t *flash);
lfs_t* lfs_port_get(void);
void lfs_port_lock(void);
void lfs_port_unlock(void);

/* ================= callbacks ================= */
int lfs_read_cb (const struct lfs_config *c,
                 lfs_block_t block,
                 lfs_off_t off,
                 void *buffer,
                 lfs_size_t size);

int lfs_prog_cb (const struct lfs_config *c,
                 lfs_block_t block,
                 lfs_off_t off,
                 const void *buffer,
                 lfs_size_t size);

int lfs_erase_cb(const struct lfs_config *c,
                 lfs_block_t block);

int lfs_sync_cb (const struct lfs_config *c);

#endif
