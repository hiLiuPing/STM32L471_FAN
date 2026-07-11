/**
 * @file heiti_font_drv.c
 * @brief On-demand reader for LVGL binary bitmap fonts stored in LittleFS.
 */

#include "heiti_font_drv.h"
#include "lfs_port.h"
#include "log.h"

#include <string.h>

#define HEAD_DATA_SIZE 40U

#define CMAP_FORMAT0_FULL 0U
#define CMAP_SPARSE_FULL  1U
#define CMAP_FORMAT0_TINY 2U
#define CMAP_SPARSE_TINY  3U

static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static int16_t rd16s(const uint8_t *p)
{
    return (int16_t)rd16(p);
}

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static lfs_t *get_lfs(void)
{
    return lfs_port_get();
}

static int f_read_at(HeitiFont_Context_t *ctx, uint32_t offset, void *buf, uint32_t len)
{
    lfs_t *lfs = get_lfs();

    if (lfs == NULL)
    {
        return -1;
    }

    if (lfs_file_seek(lfs, &ctx->file, (lfs_off_t)offset, LFS_SEEK_SET) < 0)
    {
        return -1;
    }

    return (lfs_file_read(lfs, &ctx->file, buf, len) == (lfs_ssize_t)len) ? 0 : -1;
}

static int read_section_header(HeitiFont_Context_t *ctx,
                               uint32_t offset,
                               const char label[4],
                               uint32_t *out_len)
{
    uint8_t hdr[8];

    if (f_read_at(ctx, offset, hdr, sizeof(hdr)) != 0)
    {
        return -1;
    }

    if (memcmp(&hdr[4], label, 4U) != 0)
    {
        return -1;
    }

    *out_len = rd32(hdr);
    return 0;
}

static uint32_t bit_read(const uint8_t *data, uint32_t *bit_pos, uint8_t n_bits)
{
    uint32_t value = 0U;

    while (n_bits-- != 0U)
    {
        uint32_t byte_idx = *bit_pos >> 3;
        uint8_t shift = (uint8_t)(7U - (*bit_pos & 7U));

        value <<= 1;
        value |= (uint32_t)((data[byte_idx] >> shift) & 0x01U);
        (*bit_pos)++;
    }

    return value;
}

static int16_t bit_read_signed(const uint8_t *data, uint32_t *bit_pos, uint8_t n_bits)
{
    uint16_t value = (uint16_t)bit_read(data, bit_pos, n_bits);

    if ((n_bits != 0U) && ((value & (uint16_t)(1U << (n_bits - 1U))) != 0U))
    {
        value |= (uint16_t)(0xFFFFU << n_bits);
    }

    return (int16_t)value;
}

static void bit_write(uint8_t *data, uint32_t *bit_pos, uint8_t value, uint8_t n_bits)
{
    while (n_bits-- != 0U)
    {
        uint32_t byte_idx = *bit_pos >> 3;
        uint8_t shift = (uint8_t)(7U - (*bit_pos & 7U));
        uint8_t bit = (uint8_t)((value >> n_bits) & 0x01U);

        data[byte_idx] &= (uint8_t)~(uint8_t)(1U << shift);
        data[byte_idx] |= (uint8_t)(bit << shift);
        (*bit_pos)++;
    }
}

static uint16_t adv_to_px(uint16_t adv_w_8_4)
{
    return (uint16_t)((adv_w_8_4 + 8U) >> 4);
}

static HeitiFont_Result_t parse_head(const uint8_t raw[HEAD_DATA_SIZE],
                                     HeitiFont_Context_t *ctx)
{
    int16_t ascent = rd16s(&raw[8]);
    int16_t descent = rd16s(&raw[10]);

    ctx->line_height = (uint16_t)(ascent - descent);
    ctx->base_line = (int16_t)(-descent);
    ctx->default_advance_width = rd16(&raw[22]);
    ctx->loca_format = raw[26];
    ctx->aw_format = raw[28];
    ctx->bpp = raw[29];
    ctx->xy_bits = raw[30];
    ctx->wh_bits = raw[31];
    ctx->aw_bits = raw[32];
    ctx->compression = raw[33];

    if ((ctx->bpp != 1U) && (ctx->bpp != 2U) &&
        (ctx->bpp != 3U) && (ctx->bpp != 4U) && (ctx->bpp != 8U))
    {
        return HEITI_FONT_ERR_FORMAT;
    }

    if (ctx->loca_format > 1U)
    {
        return HEITI_FONT_ERR_FORMAT;
    }

    return HEITI_FONT_OK;
}

static HeitiFont_Result_t parse_cmap(HeitiFont_Context_t *ctx, uint32_t cmap_start)
{
    uint8_t count_raw[4];

    if (f_read_at(ctx, cmap_start + 8U, count_raw, sizeof(count_raw)) != 0)
    {
        return HEITI_FONT_ERR_IO;
    }

    uint32_t count = rd32(count_raw);
    if ((count == 0U) || (count > HEITI_FONT_MAX_CMAP_ENTRY))
    {
        log_printf("[FONT] bad cmap count=%u", (unsigned)count);
        return HEITI_FONT_ERR_FORMAT;
    }

    ctx->cmap_count = (uint16_t)count;
    ctx->cmap_section_start = cmap_start;

    for (uint16_t i = 0U; i < ctx->cmap_count; i++)
    {
        uint8_t raw[16];
        HeitiFont_CmapEntry_t *entry = &ctx->cmap[i];

        if (f_read_at(ctx, cmap_start + 12U + (uint32_t)i * 16U, raw, sizeof(raw)) != 0)
        {
            return HEITI_FONT_ERR_IO;
        }

        entry->data_offset = rd32(&raw[0]);
        entry->range_start = rd32(&raw[4]);
        entry->range_length = rd16(&raw[8]);
        entry->glyph_id_start = rd16(&raw[10]);
        entry->data_entries_count = rd16(&raw[12]);
        entry->format_type = raw[14];

        if (entry->format_type > CMAP_SPARSE_TINY)
        {
            log_printf("[FONT] bad cmap type=%u", entry->format_type);
            return HEITI_FONT_ERR_FORMAT;
        }
    }

    return HEITI_FONT_OK;
}

static HeitiFont_Result_t parse_loca_info(HeitiFont_Context_t *ctx, uint32_t loca_start)
{
    uint8_t count_raw[4];

    if (f_read_at(ctx, loca_start + 8U, count_raw, sizeof(count_raw)) != 0)
    {
        return HEITI_FONT_ERR_IO;
    }

    ctx->loca_count = rd32(count_raw);
    ctx->loca_data_start = loca_start + 12U;

    return (ctx->loca_count > 0U) ? HEITI_FONT_OK : HEITI_FONT_ERR_FORMAT;
}

static HeitiFont_Result_t read_loca_offset(HeitiFont_Context_t *ctx,
                                           uint32_t glyph_index,
                                           uint32_t *out_offset)
{
    uint8_t raw[4];
    uint32_t entry_size = (ctx->loca_format == 1U) ? 4U : 2U;

    if (glyph_index >= ctx->loca_count)
    {
        return HEITI_FONT_ERR_NOT_FOUND;
    }

    if (f_read_at(ctx, ctx->loca_data_start + glyph_index * entry_size, raw, entry_size) != 0)
    {
        return HEITI_FONT_ERR_IO;
    }

    *out_offset = (entry_size == 4U) ? rd32(raw) : rd16(raw);
    return HEITI_FONT_OK;
}

static HeitiFont_Result_t read_u16_at(HeitiFont_Context_t *ctx, uint32_t offset, uint16_t *out_value)
{
    uint8_t raw[2];

    if (f_read_at(ctx, offset, raw, sizeof(raw)) != 0)
    {
        return HEITI_FONT_ERR_IO;
    }

    *out_value = rd16(raw);
    return HEITI_FONT_OK;
}

static HeitiFont_Result_t parse_glyph_header(HeitiFont_Context_t *ctx,
                                             const uint8_t *raw,
                                             HeitiFont_GlyphDsc_t *dsc)
{
    uint32_t bit_pos = 0U;
    uint16_t adv_raw;

    memset(dsc, 0, sizeof(*dsc));

    if (ctx->aw_bits == 0U)
    {
        dsc->adv_w = adv_to_px(ctx->default_advance_width);
    }
    else
    {
        adv_raw = (uint16_t)bit_read(raw, &bit_pos, ctx->aw_bits);
        dsc->adv_w = (ctx->aw_format == 0U) ? adv_raw : adv_to_px(adv_raw);
    }

    dsc->ofs_x = bit_read_signed(raw, &bit_pos, ctx->xy_bits);
    dsc->ofs_y = bit_read_signed(raw, &bit_pos, ctx->xy_bits);
    dsc->box_w = (uint16_t)bit_read(raw, &bit_pos, ctx->wh_bits);
    dsc->box_h = (uint16_t)bit_read(raw, &bit_pos, ctx->wh_bits);

    return HEITI_FONT_OK;
}

static HeitiFont_Result_t copy_plain_bitmap(const uint8_t *glyph_raw,
                                            uint32_t start_bit,
                                            uint32_t bit_count,
                                            uint8_t *bitmap_buf,
                                            uint32_t bitmap_bytes)
{
    uint32_t src_bit = start_bit;
    uint32_t dst_bit = 0U;

    memset(bitmap_buf, 0, bitmap_bytes);

    while (bit_count != 0U)
    {
        uint8_t chunk = (bit_count > 8U) ? 8U : (uint8_t)bit_count;
        uint8_t value = (uint8_t)bit_read(glyph_raw, &src_bit, chunk);

        bit_write(bitmap_buf, &dst_bit, value, chunk);
        bit_count -= chunk;
    }

    return HEITI_FONT_OK;
}

typedef enum
{
    RLE_STATE_SINGLE = 0,
    RLE_STATE_REPEATE,
    RLE_STATE_COUNTER,
} RleState_t;

typedef struct
{
    const uint8_t *data;
    uint32_t start_bit;
    uint32_t bit_pos;
    uint8_t bpp;
    uint8_t prev_v;
    uint8_t cnt;
    RleState_t state;
} RleCtx_t;

static void rle_init(RleCtx_t *rle, const uint8_t *data, uint32_t start_bit, uint8_t bpp)
{
    rle->data = data;
    rle->start_bit = start_bit;
    rle->bit_pos = start_bit;
    rle->bpp = bpp;
    rle->prev_v = 0U;
    rle->cnt = 0U;
    rle->state = RLE_STATE_SINGLE;
}

static uint8_t rle_next(RleCtx_t *rle)
{
    uint8_t value;
    uint8_t ret = 0U;

    if (rle->state == RLE_STATE_SINGLE)
    {
        ret = (uint8_t)bit_read(rle->data, &rle->bit_pos, rle->bpp);
        if ((rle->bit_pos != (rle->start_bit + rle->bpp)) && (rle->prev_v == ret))
        {
            rle->cnt = 0U;
            rle->state = RLE_STATE_REPEATE;
        }
        rle->prev_v = ret;
    }
    else if (rle->state == RLE_STATE_REPEATE)
    {
        value = (uint8_t)bit_read(rle->data, &rle->bit_pos, 1U);
        rle->cnt++;

        if (value == 1U)
        {
            ret = rle->prev_v;
            if (rle->cnt == 11U)
            {
                rle->cnt = (uint8_t)bit_read(rle->data, &rle->bit_pos, 6U);
                if (rle->cnt != 0U)
                {
                    rle->state = RLE_STATE_COUNTER;
                }
                else
                {
                    ret = (uint8_t)bit_read(rle->data, &rle->bit_pos, rle->bpp);
                    rle->prev_v = ret;
                    rle->state = RLE_STATE_SINGLE;
                }
            }
        }
        else
        {
            ret = (uint8_t)bit_read(rle->data, &rle->bit_pos, rle->bpp);
            rle->prev_v = ret;
            rle->state = RLE_STATE_SINGLE;
        }
    }
    else
    {
        ret = rle->prev_v;
        rle->cnt--;
        if (rle->cnt == 0U)
        {
            ret = (uint8_t)bit_read(rle->data, &rle->bit_pos, rle->bpp);
            rle->prev_v = ret;
            rle->state = RLE_STATE_SINGLE;
        }
    }

    return ret;
}

static void write_pixel(uint8_t *bitmap_buf, uint32_t pixel_index, uint8_t bpp, uint8_t value)
{
    uint32_t bit_pos;

    if (bpp == 3U)
    {
        bpp = 4U;
        value = (uint8_t)((value * 15U) / 7U);
    }

    bit_pos = pixel_index * bpp;
    bit_write(bitmap_buf, &bit_pos, value, bpp);
}

static HeitiFont_Result_t decompress_bitmap(const uint8_t *glyph_raw,
                                            uint32_t start_bit,
                                            uint16_t width,
                                            uint16_t height,
                                            uint8_t bpp,
                                            uint8_t prefilter,
                                            uint8_t *bitmap_buf,
                                            uint32_t bitmap_bytes)
{
    RleCtx_t rle;
    uint8_t prev_line[32];
    uint8_t cur_line[32];
    uint32_t pixel = 0U;

    if (width > sizeof(prev_line))
    {
        return HEITI_FONT_ERR_NO_MEM;
    }

    memset(bitmap_buf, 0, bitmap_bytes);
    memset(prev_line, 0, sizeof(prev_line));
    rle_init(&rle, glyph_raw, start_bit, bpp);

    for (uint16_t y = 0U; y < height; y++)
    {
        for (uint16_t x = 0U; x < width; x++)
        {
            cur_line[x] = rle_next(&rle);
            if ((prefilter != 0U) && (y != 0U))
            {
                cur_line[x] ^= prev_line[x];
            }
            write_pixel(bitmap_buf, pixel++, bpp, cur_line[x]);
        }
        memcpy(prev_line, cur_line, width);
    }

    return HEITI_FONT_OK;
}

HeitiFont_Result_t HeitiFont_Open(HeitiFont_Context_t *ctx, const char *path)
{
    uint8_t head_raw[8U + HEAD_DATA_SIZE];
    uint32_t head_len;
    uint32_t cmap_len;
    uint32_t loca_len;
    uint32_t glyf_len;
    uint32_t cmap_start;
    uint32_t loca_start;
    uint32_t glyf_start;
    lfs_t *lfs;

    if ((ctx == NULL) || (path == NULL))
    {
        return HEITI_FONT_ERR_PARAM;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->file_cfg.buffer = ctx->file_buf;
    ctx->file_cfg.attrs = NULL;
    ctx->file_cfg.attr_count = 0;

    lfs = get_lfs();
    if (lfs == NULL)
    {
        return HEITI_FONT_ERR_NOT_FOUND;
    }

    if (lfs_file_opencfg(lfs, &ctx->file, path, LFS_O_RDONLY, &ctx->file_cfg) != 0)
    {
        log_printf("[FONT] open %s FAIL", path);
        return HEITI_FONT_ERR_NOT_FOUND;
    }

    if (f_read_at(ctx, 0U, head_raw, sizeof(head_raw)) != 0)
    {
        goto fail;
    }

    head_len = rd32(&head_raw[0]);
    if ((memcmp(&head_raw[4], "head", 4U) != 0) || (head_len < (8U + HEAD_DATA_SIZE)))
    {
        goto fail;
    }

    if (parse_head(&head_raw[8], ctx) != HEITI_FONT_OK)
    {
        goto fail;
    }

    cmap_start = head_len;
    if (read_section_header(ctx, cmap_start, "cmap", &cmap_len) != 0)
    {
        goto fail;
    }

    loca_start = cmap_start + cmap_len;
    if (read_section_header(ctx, loca_start, "loca", &loca_len) != 0)
    {
        goto fail;
    }

    glyf_start = loca_start + loca_len;
    if (read_section_header(ctx, glyf_start, "glyf", &glyf_len) != 0)
    {
        goto fail;
    }

    if ((parse_cmap(ctx, cmap_start) != HEITI_FONT_OK) ||
        (parse_loca_info(ctx, loca_start) != HEITI_FONT_OK))
    {
        goto fail;
    }

    ctx->glyf_section_start = glyf_start;
    ctx->glyf_section_len = glyf_len;
    ctx->is_open = true;

    log_printf("[FONT] open %s OK bpp=%u line=%u comp=%u",
               path,
               (unsigned)ctx->bpp,
               (unsigned)ctx->line_height,
               (unsigned)ctx->compression);

    return HEITI_FONT_OK;

fail:
    (void)lfs_file_close(lfs, &ctx->file);
    memset(ctx, 0, sizeof(*ctx));
    log_printf("[FONT] bad LVGL font %s", path);
    return HEITI_FONT_ERR_FORMAT;
}

void HeitiFont_Close(HeitiFont_Context_t *ctx)
{
    lfs_t *lfs;

    if ((ctx == NULL) || !ctx->is_open)
    {
        return;
    }

    lfs = get_lfs();
    if (lfs != NULL)
    {
        (void)lfs_file_close(lfs, &ctx->file);
    }

    memset(ctx, 0, sizeof(*ctx));
}

HeitiFont_Result_t HeitiFont_Lookup(HeitiFont_Context_t *ctx,
                                    uint32_t unicode_cp,
                                    uint32_t *out_glyph_index)
{
    if ((ctx == NULL) || !ctx->is_open || (out_glyph_index == NULL))
    {
        return HEITI_FONT_ERR_PARAM;
    }

    if (unicode_cp == 0U)
    {
        return HEITI_FONT_ERR_NOT_FOUND;
    }

    for (uint16_t i = 0U; i < ctx->cmap_count; i++)
    {
        const HeitiFont_CmapEntry_t *entry = &ctx->cmap[i];
        uint32_t rcp;
        uint32_t data_start;

        if ((unicode_cp < entry->range_start) ||
            (unicode_cp >= (entry->range_start + entry->range_length)))
        {
            continue;
        }

        rcp = unicode_cp - entry->range_start;
        data_start = ctx->cmap_section_start + entry->data_offset;

        if (entry->format_type == CMAP_FORMAT0_TINY)
        {
            *out_glyph_index = (uint32_t)entry->glyph_id_start + rcp;
            return HEITI_FONT_OK;
        }

        if (entry->format_type == CMAP_FORMAT0_FULL)
        {
            uint8_t glyph_ofs;

            if (rcp >= entry->data_entries_count)
            {
                return HEITI_FONT_ERR_NOT_FOUND;
            }

            if (f_read_at(ctx, data_start + rcp, &glyph_ofs, 1U) != 0)
            {
                return HEITI_FONT_ERR_IO;
            }

            *out_glyph_index = (uint32_t)entry->glyph_id_start + glyph_ofs;
            return HEITI_FONT_OK;
        }

        if ((entry->format_type == CMAP_SPARSE_TINY) ||
            (entry->format_type == CMAP_SPARSE_FULL))
        {
            uint32_t left = 0U;
            uint32_t right = entry->data_entries_count;

            while (left < right)
            {
                uint32_t mid = left + ((right - left) >> 1);
                uint16_t listed_rcp;

                if (read_u16_at(ctx, data_start + mid * 2U, &listed_rcp) != HEITI_FONT_OK)
                {
                    return HEITI_FONT_ERR_IO;
                }

                if ((uint32_t)listed_rcp == rcp)
                {
                    if (entry->format_type == CMAP_SPARSE_TINY)
                    {
                        *out_glyph_index = (uint32_t)entry->glyph_id_start + mid;
                    }
                    else
                    {
                        uint16_t glyph_ofs;
                        uint32_t ofs_start = data_start + (uint32_t)entry->data_entries_count * 2U;

                        if (read_u16_at(ctx, ofs_start + mid * 2U, &glyph_ofs) != HEITI_FONT_OK)
                        {
                            return HEITI_FONT_ERR_IO;
                        }
                        *out_glyph_index = (uint32_t)entry->glyph_id_start + glyph_ofs;
                    }
                    return HEITI_FONT_OK;
                }

                if ((uint32_t)listed_rcp < rcp)
                {
                    left = mid + 1U;
                }
                else
                {
                    right = mid;
                }
            }
        }
    }

    return HEITI_FONT_ERR_NOT_FOUND;
}

HeitiFont_Result_t HeitiFont_GetGlyph(HeitiFont_Context_t *ctx,
                                      uint32_t glyph_index,
                                      HeitiFont_GlyphDsc_t *dsc,
                                      uint8_t *bitmap_buf,
                                      uint32_t buf_size)
{
    static uint8_t glyph_raw[HEITI_FONT_GLYPH_RAW_MAX];
    uint32_t offset_curr;
    uint32_t offset_next;
    uint32_t total_size;
    uint32_t header_bits;
    uint32_t pixel_count;
    uint32_t bitmap_bits;
    uint32_t bitmap_bytes;
    HeitiFont_Result_t res;

    if ((ctx == NULL) || !ctx->is_open || (dsc == NULL) ||
        (bitmap_buf == NULL) || (buf_size == 0U))
    {
        return HEITI_FONT_ERR_PARAM;
    }

    if (glyph_index == 0U)
    {
        memset(dsc, 0, sizeof(*dsc));
        return HEITI_FONT_OK;
    }

    res = read_loca_offset(ctx, glyph_index, &offset_curr);
    if (res != HEITI_FONT_OK)
    {
        return res;
    }

    if ((glyph_index + 1U) < ctx->loca_count)
    {
        res = read_loca_offset(ctx, glyph_index + 1U, &offset_next);
        if (res != HEITI_FONT_OK)
        {
            return res;
        }
    }
    else
    {
        offset_next = ctx->glyf_section_len;
    }

    if ((offset_next < offset_curr) || (offset_next > ctx->glyf_section_len))
    {
        return HEITI_FONT_ERR_FORMAT;
    }

    total_size = offset_next - offset_curr;
    if (total_size == 0U)
    {
        memset(dsc, 0, sizeof(*dsc));
        return HEITI_FONT_OK;
    }

    if (total_size > HEITI_FONT_GLYPH_RAW_MAX)
    {
        log_printf("[FONT] glyph %u too large: %u",
                   (unsigned)glyph_index,
                   (unsigned)total_size);
        return HEITI_FONT_ERR_NO_MEM;
    }

    if (f_read_at(ctx, ctx->glyf_section_start + offset_curr, glyph_raw, total_size) != 0)
    {
        return HEITI_FONT_ERR_IO;
    }

    res = parse_glyph_header(ctx, glyph_raw, dsc);
    if (res != HEITI_FONT_OK)
    {
        return res;
    }

    if ((dsc->box_w == 0U) || (dsc->box_h == 0U))
    {
        return HEITI_FONT_OK;
    }

    pixel_count = (uint32_t)dsc->box_w * dsc->box_h;
    bitmap_bits = pixel_count * ((ctx->bpp == 3U) ? 4U : ctx->bpp);
    bitmap_bytes = (bitmap_bits + 7U) >> 3;
    header_bits = (uint32_t)ctx->aw_bits + 2U * ctx->xy_bits + 2U * ctx->wh_bits;

    if ((total_size * 8U) < header_bits)
    {
        return HEITI_FONT_ERR_FORMAT;
    }

    if (bitmap_bytes > buf_size)
    {
        return HEITI_FONT_ERR_NO_MEM;
    }

    if (ctx->compression == 0U)
    {
        if (bitmap_bits > ((total_size * 8U) - header_bits))
        {
            return HEITI_FONT_ERR_FORMAT;
        }
        return copy_plain_bitmap(glyph_raw, header_bits, bitmap_bits, bitmap_buf, bitmap_bytes);
    }

    return decompress_bitmap(glyph_raw,
                             header_bits,
                             dsc->box_w,
                             dsc->box_h,
                             ctx->bpp,
                             (ctx->compression == 1U) ? 1U : 0U,
                             bitmap_buf,
                             bitmap_bytes);
}

uint16_t HeitiFont_GetLineHeight(const HeitiFont_Context_t *ctx)
{
    return (ctx != NULL) ? ctx->line_height : 0U;
}

int16_t HeitiFont_GetBaseLine(const HeitiFont_Context_t *ctx)
{
    return (ctx != NULL) ? ctx->base_line : 0;
}

uint16_t HeitiFont_GetBpp(const HeitiFont_Context_t *ctx)
{
    return (ctx != NULL) ? ctx->bpp : 0U;
}
