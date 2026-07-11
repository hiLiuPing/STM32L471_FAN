/**
 * @file lv_port_fs.c
 * @brief LVGL 文件系统适配层 —— 对接项目已有的 LittleFS (lfs_port)
 *
 * 兼容 LFS_NO_MALLOC：每个文件句柄内嵌静态 buffer，
 * 通过 lfs_file_opencfg() 代替 lfs_file_open()。
 */

#include "lv_port_fs.h"
#include "lfs_port.h"

/*********************
 *      DEFINES
 *********************/
#define FS_LETTER       'L'
#define FS_FILE_CNT     2       /* 同时打开的最大文件数 */
#define FS_CACHE_SIZE   1024    /* 与 LFS_CACHE_SIZE 对齐 */

/**********************
 *      TYPEDEFS
 **********************/
typedef struct {
    lfs_file_t         file;
    struct lfs_file_config cfg;
    uint8_t            buf[FS_CACHE_SIZE];
    bool               in_use;
} file_slot_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void * fs_open(lv_fs_drv_t * drv, const char * path, lv_fs_mode_t mode);
static lv_fs_res_t fs_close(lv_fs_drv_t * drv, void * file_p);
static lv_fs_res_t fs_read(lv_fs_drv_t * drv, void * file_p,
                           void * buf, uint32_t btr, uint32_t * br);
static lv_fs_res_t fs_write(lv_fs_drv_t * drv, void * file_p,
                            const void * buf, uint32_t btw, uint32_t * bw);
static lv_fs_res_t fs_seek(lv_fs_drv_t * drv, void * file_p,
                           uint32_t pos, lv_fs_whence_t whence);
static lv_fs_res_t fs_tell(lv_fs_drv_t * drv, void * file_p, uint32_t * pos_p);

/**********************
 *  STATIC VARIABLES
 **********************/
static file_slot_t s_slots[FS_FILE_CNT];

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
void lv_port_fs_init(void)
{
    static lv_fs_drv_t fs_drv;
    lv_fs_drv_init(&fs_drv);

    fs_drv.letter    = FS_LETTER;
    fs_drv.cache_size = FS_CACHE_SIZE;

    fs_drv.open_cb   = fs_open;
    fs_drv.close_cb  = fs_close;
    fs_drv.read_cb   = fs_read;
    fs_drv.write_cb  = fs_write;
    fs_drv.seek_cb   = fs_seek;
    fs_drv.tell_cb   = fs_tell;

    /* user_data 指向全局 lfs 实例 */
    fs_drv.user_data = (void *)lfs_port_get();

    lv_fs_drv_register(&fs_drv);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
static file_slot_t * alloc_slot(void)
{
    for (int i = 0; i < FS_FILE_CNT; i++) {
        if (!s_slots[i].in_use) {
            s_slots[i].in_use = true;
            return &s_slots[i];
        }
    }
    return NULL;
}

static void free_slot(file_slot_t * s)
{
    s->in_use = false;
}

static void * fs_open(lv_fs_drv_t * drv, const char * path, lv_fs_mode_t mode)
{
    lfs_t * lfs_p = (lfs_t *)drv->user_data;

    int flags = (mode == LV_FS_MODE_RD) ? LFS_O_RDONLY
              : (mode == LV_FS_MODE_WR) ? LFS_O_WRONLY
              : (mode == (LV_FS_MODE_WR | LV_FS_MODE_RD)) ? LFS_O_RDWR
              : 0;

    file_slot_t * slot = alloc_slot();
    if (slot == NULL) return NULL;

    slot->cfg.buffer      = slot->buf;
    slot->cfg.attrs       = NULL;
    slot->cfg.attr_count  = 0;

    int ret = lfs_file_opencfg(lfs_p, &slot->file, path, flags, &slot->cfg);
    if (ret != 0) {
        free_slot(slot);
        return NULL;
    }

    return (void *)slot;
}

static lv_fs_res_t fs_close(lv_fs_drv_t * drv, void * file_p)
{
    lfs_t * lfs_p = (lfs_t *)drv->user_data;
    file_slot_t * slot = (file_slot_t *)file_p;

    int ret = lfs_file_close(lfs_p, &slot->file);
    free_slot(slot);

    return (ret == 0) ? LV_FS_RES_OK : LV_FS_RES_UNKNOWN;
}

static lv_fs_res_t fs_read(lv_fs_drv_t * drv, void * file_p,
                           void * buf, uint32_t btr, uint32_t * br)
{
    lfs_t * lfs_p = (lfs_t *)drv->user_data;
    file_slot_t * slot = (file_slot_t *)file_p;

    lfs_ssize_t n = lfs_file_read(lfs_p, &slot->file, buf, btr);
    if (n < 0) return LV_FS_RES_UNKNOWN;

    *br = (uint32_t)n;
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_write(lv_fs_drv_t * drv, void * file_p,
                            const void * buf, uint32_t btw, uint32_t * bw)
{
    lfs_t * lfs_p = (lfs_t *)drv->user_data;
    file_slot_t * slot = (file_slot_t *)file_p;

    lfs_ssize_t n = lfs_file_write(lfs_p, &slot->file, buf, btw);
    if (n < 0) return LV_FS_RES_UNKNOWN;

    *bw = (uint32_t)n;
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_seek(lv_fs_drv_t * drv, void * file_p,
                           uint32_t pos, lv_fs_whence_t whence)
{
    lfs_t * lfs_p = (lfs_t *)drv->user_data;
    file_slot_t * slot = (file_slot_t *)file_p;

    int lfs_whence = (whence == LV_FS_SEEK_SET) ? LFS_SEEK_SET
                   : (whence == LV_FS_SEEK_CUR) ? LFS_SEEK_CUR
                   : (whence == LV_FS_SEEK_END) ? LFS_SEEK_END
                   : 0;

    lfs_soff_t ret = lfs_file_seek(lfs_p, &slot->file, (lfs_off_t)pos, lfs_whence);
    return (ret >= 0) ? LV_FS_RES_OK : LV_FS_RES_UNKNOWN;
}

static lv_fs_res_t fs_tell(lv_fs_drv_t * drv, void * file_p, uint32_t * pos_p)
{
    lfs_t * lfs_p = (lfs_t *)drv->user_data;
    file_slot_t * slot = (file_slot_t *)file_p;

    lfs_soff_t ret = lfs_file_tell(lfs_p, &slot->file);
    if (ret < 0) return LV_FS_RES_UNKNOWN;

    *pos_p = (uint32_t)ret;
    return LV_FS_RES_OK;
}
