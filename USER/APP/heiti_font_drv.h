#ifndef __HEITI_FONT_DRV_H__
#define __HEITI_FONT_DRV_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#include "lfs.h"

#define HEITI_FONT_HEAD_SIZE       48U
#define HEITI_FONT_BLOCK_SIZE      48U
#define HEITI_FONT_MAX_CMAP_ENTRY  113U

typedef enum
{
    HEITI_FONT_12 = 12,
    HEITI_FONT_16 = 16,
    HEITI_FONT_18 = 18,
    HEITI_FONT_20 = 20,
} HeitiFont_Size_t;

typedef enum
{
    HEITI_FONT_OK = 0,
    HEITI_FONT_ERR_PARAM = -1,
    HEITI_FONT_ERR_NOT_FOUND = -2,
    HEITI_FONT_ERR_IO = -3,
    HEITI_FONT_ERR_FORMAT = -4,
    HEITI_FONT_ERR_NOT_OPEN = -5,
    HEITI_FONT_ERR_NO_MEM = -6,
} HeitiFont_Result_t;

typedef struct
{
    uint16_t magic;
    uint16_t padding;
    uint32_t cp_start;
    uint16_t count;
    uint16_t glyph_start;
    uint16_t count_dup;
    uint16_t flags;
} HeitiFont_CmapEntry_t;

typedef struct
{
    uint32_t block_size;
    uint8_t  tag[4];
    uint32_t version;
    uint16_t bpp;
    uint16_t height;
    uint8_t  reserved[32];
} HeitiFont_Head_t;

typedef struct HeitiFont_Context
{
    lfs_file_t file;            /* LittleFS file handle */
    bool       is_open;
    uint16_t   bpp;
    uint16_t   height;
    uint32_t   data_offset;
    uint16_t   cmap_count;
    HeitiFont_CmapEntry_t cmap[HEITI_FONT_MAX_CMAP_ENTRY];

    /* LittleFS file config with static buffer (LFS_NO_MALLOC) */
    struct lfs_file_config file_cfg;
    uint8_t               file_buf[1024];   /* LFS_CACHE_SIZE */
} HeitiFont_Context_t;

void HeitiFont_Init(void);

HeitiFont_Result_t HeitiFont_Open(HeitiFont_Context_t *ctx, HeitiFont_Size_t size);
void HeitiFont_Close(HeitiFont_Context_t *ctx);

HeitiFont_Result_t HeitiFont_Lookup(HeitiFont_Context_t *ctx,
                                    uint32_t unicode_cp,
                                    uint32_t *out_glyph_index);

HeitiFont_Result_t HeitiFont_GetGlyphBlock(HeitiFont_Context_t *ctx,
                                           uint32_t glyph_index,
                                           uint8_t block[HEITI_FONT_BLOCK_SIZE]);

HeitiFont_Result_t HeitiFont_DecodeBlock(const uint8_t block[HEITI_FONT_BLOCK_SIZE],
                                         uint16_t height,
                                         uint16_t bpp,
                                         uint8_t *out_bitmap,
                                         uint32_t buf_size,
                                         uint32_t *out_width,
                                         uint32_t *out_height,
                                         uint32_t *out_bytes);

uint16_t HeitiFont_GetHeight(const HeitiFont_Context_t *ctx);
uint16_t HeitiFont_GetBpp(const HeitiFont_Context_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* __HEITI_FONT_DRV_H__ */
