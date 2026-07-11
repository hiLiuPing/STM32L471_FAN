#ifndef __HEITI_FONT_DRV_H__
#define __HEITI_FONT_DRV_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "lfs.h"

#define HEITI_FONT_MAX_CMAP_ENTRY 128U
#define HEITI_FONT_GLYPH_RAW_MAX  512U

typedef enum
{
    HEITI_FONT_OK = 0,
    HEITI_FONT_ERR_PARAM = -1,
    HEITI_FONT_ERR_NOT_FOUND = -2,
    HEITI_FONT_ERR_IO = -3,
    HEITI_FONT_ERR_FORMAT = -4,
    HEITI_FONT_ERR_NO_MEM = -6,
} HeitiFont_Result_t;

typedef struct
{
    uint32_t data_offset;
    uint32_t range_start;
    uint16_t range_length;
    uint16_t glyph_id_start;
    uint16_t data_entries_count;
    uint8_t format_type;
} HeitiFont_CmapEntry_t;

typedef struct
{
    uint16_t adv_w;
    uint16_t box_w;
    uint16_t box_h;
    int16_t ofs_x;
    int16_t ofs_y;
} HeitiFont_GlyphDsc_t;

typedef struct HeitiFont_Context
{
    lfs_file_t file;
    bool is_open;

    uint8_t bpp;
    uint16_t line_height;
    int16_t base_line;
    uint16_t default_advance_width;
    uint8_t aw_bits;
    uint8_t xy_bits;
    uint8_t wh_bits;
    uint8_t loca_format;
    uint8_t compression;
    uint8_t aw_format;

    uint32_t cmap_section_start;
    uint32_t loca_data_start;
    uint32_t loca_count;
    uint32_t glyf_section_start;
    uint32_t glyf_section_len;

    uint16_t cmap_count;
    HeitiFont_CmapEntry_t cmap[HEITI_FONT_MAX_CMAP_ENTRY];

    struct lfs_file_config file_cfg;
    uint8_t file_buf[1024];
} HeitiFont_Context_t;

HeitiFont_Result_t HeitiFont_Open(HeitiFont_Context_t *ctx, const char *path);
void HeitiFont_Close(HeitiFont_Context_t *ctx);

HeitiFont_Result_t HeitiFont_Lookup(HeitiFont_Context_t *ctx,
                                    uint32_t unicode_cp,
                                    uint32_t *out_glyph_index);

HeitiFont_Result_t HeitiFont_GetGlyph(HeitiFont_Context_t *ctx,
                                      uint32_t glyph_index,
                                      HeitiFont_GlyphDsc_t *dsc,
                                      uint8_t *bitmap_buf,
                                      uint32_t buf_size);

uint16_t HeitiFont_GetLineHeight(const HeitiFont_Context_t *ctx);
int16_t HeitiFont_GetBaseLine(const HeitiFont_Context_t *ctx);
uint16_t HeitiFont_GetBpp(const HeitiFont_Context_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* __HEITI_FONT_DRV_H__ */
