#include "heiti_font_drv.h"

#include <stdio.h>
#include <string.h>

#include "lfs_port.h"
#include "log.h"

#define HEITI_FONT_MAGIC       0x0000071CL
#define HEITI_FONT_CMAP_TAG    0x50414D43UL   /* "cmap" LE */
#define HEITI_FONT_CMAP_OFFSET 0x3CUL
#define HEITI_FONT_DATA_OFFSET 0x74CUL

static bool s_initialized = false;
static lfs_t *s_lfs = NULL;

/* ---------- helpers ---------- */

static uint16_t read_le16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t read_le32(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

/* ---------- utf-8 decoder ---------- */

static uint32_t utf8_decode(const char **pp)
{
    const uint8_t *p = (const uint8_t *)*pp;
    uint32_t cp;

    if ((p[0] & 0x80) == 0)
    {
        cp = p[0];
        *pp = (const char *)(p + 1);
    }
    else if ((p[0] & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80)
    {
        cp = ((uint32_t)(p[0] & 0x1F) << 6) | (uint32_t)(p[1] & 0x3F);
        *pp = (const char *)(p + 2);
    }
    else if ((p[0] & 0xF0) == 0xE0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80)
    {
        cp = ((uint32_t)(p[0] & 0x0F) << 12) |
             ((uint32_t)(p[1] & 0x3F) << 6) |
             (uint32_t)(p[2] & 0x3F);
        *pp = (const char *)(p + 3);
    }
    else if ((p[0] & 0xF8) == 0xF0 && (p[1] & 0xC0) == 0x80 &&
             (p[2] & 0xC0) == 0x80 && (p[3] & 0xC0) == 0x80)
    {
        cp = ((uint32_t)(p[0] & 0x07) << 18) |
             ((uint32_t)(p[1] & 0x3F) << 12) |
             ((uint32_t)(p[2] & 0x3F) << 6) |
             (uint32_t)(p[3] & 0x3F);
        *pp = (const char *)(p + 4);
    }
    else
    {
        cp = 0xFFFDUL;
        *pp = (const char *)(p + 1);
    }

    return cp;
}

/* ---------- public API ---------- */

void HeitiFont_Init(void)
{
    s_lfs = lfs_port_get();
    s_initialized = (s_lfs != NULL);
    log_printf("[HEITI] init %s", s_initialized ? "OK" : "FAIL (no lfs)");
}

HeitiFont_Result_t HeitiFont_Open(HeitiFont_Context_t *ctx, HeitiFont_Size_t size)
{
    char path[32];
    uint8_t head[HEITI_FONT_HEAD_SIZE];
    int ret;

    if (ctx == NULL || !s_initialized)
    {
        return HEITI_FONT_ERR_PARAM;
    }

    memset(ctx, 0, sizeof(*ctx));

    (void)snprintf(path, sizeof(path), "heiti_4_%d.bin", (int)size);

    ctx->file_cfg.buffer       = ctx->file_buf;
    ctx->file_cfg.attrs        = NULL;
    ctx->file_cfg.attr_count   = 0;

    ret = lfs_file_opencfg(s_lfs, &ctx->file, path, LFS_O_RDONLY, &ctx->file_cfg);
    if (ret != 0)
    {
        log_printf("[HEITI] open FAIL %s ret=%d lfs=%p", path, ret, (void*)s_lfs);
        return HEITI_FONT_ERR_IO;
    }

    /* read head (48 bytes at offset 0) */
    if (lfs_file_read(s_lfs, &ctx->file, head, sizeof(head)) != sizeof(head))
    {
        log_printf("[HEITI] read head FAIL");
        lfs_file_close(s_lfs, &ctx->file);
        return HEITI_FONT_ERR_IO;
    }

    /* validate head */
    log_printf("[HEITI] head=%02x%02x%02x%02x%02x%02x%02x%02x",
               head[0],head[1],head[2],head[3],head[4],head[5],head[6],head[7]);
    if (memcmp(head + 4, "head", 4) != 0)
    {
        lfs_file_close(s_lfs, &ctx->file);
        return HEITI_FONT_ERR_FORMAT;
    }

    ctx->bpp    = read_le16(head + 12);
    ctx->height = read_le16(head + 14);

    /* cmap entries: 113 × 16B starting at 0x3C, up to data_offset */
    ctx->cmap_count = (HEITI_FONT_DATA_OFFSET - HEITI_FONT_CMAP_OFFSET) / 16U;
    if (ctx->cmap_count > HEITI_FONT_MAX_CMAP_ENTRY)
    {
        ctx->cmap_count = HEITI_FONT_MAX_CMAP_ENTRY;
    }

    /* seek to first cmap entry at 0x3C */
    if (lfs_file_seek(s_lfs, &ctx->file, HEITI_FONT_CMAP_OFFSET, LFS_SEEK_SET) < 0)
    {
        lfs_file_close(s_lfs, &ctx->file);
        return HEITI_FONT_ERR_IO;
    }

    /* read cmap entries (16 bytes each) */
    for (uint16_t i = 0; i < ctx->cmap_count; i++)
    {
        uint8_t raw[16];
        HeitiFont_CmapEntry_t *e = &ctx->cmap[i];

        if (lfs_file_read(s_lfs, &ctx->file, raw, sizeof(raw)) != sizeof(raw))
        {
            lfs_file_close(s_lfs, &ctx->file);
            return HEITI_FONT_ERR_IO;
        }

        e->magic       = read_le16(raw + 0);
        e->padding     = read_le16(raw + 2);
        e->cp_start    = read_le32(raw + 4);
        e->count       = read_le16(raw + 8);
        e->glyph_start = read_le16(raw + 10);
        e->count_dup   = read_le16(raw + 12);
        e->flags       = read_le16(raw + 14);
    }

    ctx->data_offset = HEITI_FONT_DATA_OFFSET;
    ctx->is_open = true;

    log_printf("[HEITI] open %s: %dx%d %dbpp, %u cmap entries",
               path, (int)ctx->bpp, (int)ctx->height, (int)ctx->bpp,
               (unsigned)ctx->cmap_count);

    return HEITI_FONT_OK;
}

void HeitiFont_Close(HeitiFont_Context_t *ctx)
{
    if (ctx != NULL && ctx->is_open)
    {
        lfs_file_close(s_lfs, &ctx->file);
        ctx->is_open = false;
    }
}

HeitiFont_Result_t HeitiFont_Lookup(HeitiFont_Context_t *ctx,
                                    uint32_t unicode_cp,
                                    uint32_t *out_glyph_index)
{
    if (ctx == NULL || !ctx->is_open || out_glyph_index == NULL)
    {
        return HEITI_FONT_ERR_PARAM;
    }

    for (uint16_t i = 0; i < ctx->cmap_count; i++)
    {
        const HeitiFont_CmapEntry_t *e = &ctx->cmap[i];

        if (unicode_cp >= e->cp_start &&
            unicode_cp < e->cp_start + e->count)
        {
            *out_glyph_index = e->glyph_start + (unicode_cp - e->cp_start);
            return HEITI_FONT_OK;
        }
    }

    return HEITI_FONT_ERR_NOT_FOUND;
}

HeitiFont_Result_t HeitiFont_GetGlyphBlock(HeitiFont_Context_t *ctx,
                                           uint32_t glyph_index,
                                           uint8_t block[HEITI_FONT_BLOCK_SIZE])
{
    uint32_t offset;

    if (ctx == NULL || !ctx->is_open || block == NULL)
    {
        return HEITI_FONT_ERR_PARAM;
    }

    offset = ctx->data_offset + glyph_index * HEITI_FONT_BLOCK_SIZE;

    if (lfs_file_seek(s_lfs, &ctx->file, (lfs_off_t)offset, LFS_SEEK_SET) < 0)
    {
        return HEITI_FONT_ERR_IO;
    }

    if (lfs_file_read(s_lfs, &ctx->file, block, HEITI_FONT_BLOCK_SIZE) != HEITI_FONT_BLOCK_SIZE)
    {
        return HEITI_FONT_ERR_IO;
    }

    return HEITI_FONT_OK;
}

/* ========== LVGL compressed-bitmap RLE decoder ==========
 *
 * The 48-byte glyph block stores LVGL RLE-compressed pixel data
 * (output of lvgl_conv_tool.exe --compress --bpp 4).
 *
 * At 4bpp, each pixel is a 4-bit grayscale value (0=background, 15=foreground).
 * The RLE bitstream is read MSB-first within each byte.
 *
 * RLE states:
 *   SINGLE   – read bpp bits as a pixel.  If == previous pixel, switch to REPEATE.
 *   REPEATE  – read 1-bit continue flags.  1 = same pixel continues;
 *              after 11 consecutive 1s, read 6-bit extended count → COUNTER.
 *              0 = run ended, read new pixel → SINGLE.
 *   COUNTER  – output previous pixel for remaining count, then → SINGLE.
 *
 * Prefilter (default with --compress): each row after the first is
 * XOR'd with the previous row (delta encoding).
 */

#define HEITI_RLE_STATE_SINGLE   0
#define HEITI_RLE_STATE_REPEATE  1
#define HEITI_RLE_STATE_COUNTER  2

typedef struct
{
    const uint8_t *data;
    uint32_t bit_pos;
    uint8_t  bpp;
    uint8_t  state;
    uint8_t  prev_v;
    uint16_t cnt;
} HeitiRleCtx_t;

/* Extract up to 8 bits from a byte array at an arbitrary bit offset */
static uint32_t rle_get_bits(const uint8_t *in, uint32_t bit_pos, uint8_t len)
{
    uint32_t byte_pos = bit_pos >> 3;
    uint32_t bit_off  = bit_pos & 0x7;
    uint32_t mask     = (1UL << len) - 1UL;

    if (bit_off + len >= 8)
    {
        uint16_t v = ((uint16_t)in[byte_pos] << 8) | in[byte_pos + 1];
        return (v >> (16 - bit_off - len)) & mask;
    }
    return (in[byte_pos] >> (8 - bit_off - len)) & mask;
}

static void rle_init(HeitiRleCtx_t *ctx, const uint8_t *data, uint8_t bpp)
{
    ctx->data    = data;
    ctx->bpp     = bpp;
    ctx->bit_pos = 0;
    ctx->state   = HEITI_RLE_STATE_SINGLE;
    ctx->prev_v  = 0;
    ctx->cnt     = 0;
}

static uint8_t rle_next(HeitiRleCtx_t *ctx)
{
    uint8_t v;

    if (ctx->state == HEITI_RLE_STATE_SINGLE)
    {
        uint8_t ret = (uint8_t)rle_get_bits(ctx->data, ctx->bit_pos, ctx->bpp);
        ctx->bit_pos += ctx->bpp;
        if (ctx->bit_pos != ctx->bpp && ctx->prev_v == ret)
        {
            ctx->cnt   = 0;
            ctx->state = HEITI_RLE_STATE_REPEATE;
        }
        ctx->prev_v = ret;
        return ret;
    }

    if (ctx->state == HEITI_RLE_STATE_REPEATE)
    {
        v = (uint8_t)rle_get_bits(ctx->data, ctx->bit_pos, 1);
        ctx->bit_pos += 1;
        ctx->cnt++;
        if (v == 1)
        {
            /* same value continues */
            if (ctx->cnt == 11)
            {
                ctx->cnt = (uint16_t)rle_get_bits(ctx->data, ctx->bit_pos, 6);
                ctx->bit_pos += 6;
                if (ctx->cnt != 0)
                {
                    ctx->state = HEITI_RLE_STATE_COUNTER;
                }
                else
                {
                    /* count == 0 means exit repeat & read new value */
                    uint8_t ret = (uint8_t)rle_get_bits(ctx->data, ctx->bit_pos, ctx->bpp);
                    ctx->prev_v = ret;
                    ctx->bit_pos += ctx->bpp;
                    ctx->state = HEITI_RLE_STATE_SINGLE;
                    return ret;
                }
            }
            return ctx->prev_v;
        }
        else
        {
            /* 0 bit – run ended, read next pixel value */
            uint8_t ret = (uint8_t)rle_get_bits(ctx->data, ctx->bit_pos, ctx->bpp);
            ctx->prev_v = ret;
            ctx->bit_pos += ctx->bpp;
            ctx->state = HEITI_RLE_STATE_SINGLE;
            return ret;
        }
    }

    /* HEITI_RLE_STATE_COUNTER */
    ctx->cnt--;
    if (ctx->cnt == 0)
    {
        ctx->state = HEITI_RLE_STATE_SINGLE;
        uint8_t ret = (uint8_t)rle_get_bits(ctx->data, ctx->bit_pos, ctx->bpp);
        ctx->prev_v = ret;
        ctx->bit_pos += ctx->bpp;
        return ret;
    }
    return ctx->prev_v;
}

/* Write a bpp-bit pixel value at pixel-index `pix` into a packed bitmap.
 * At 4bpp: pix 0 → high nibble of byte 0, pix 1 → low nibble, etc. */
static void pixel_write(uint8_t *bitmap, uint32_t pix, uint8_t val, uint8_t bpp)
{
    uint32_t bit_pos = pix * bpp;
    uint32_t byte_pos = bit_pos >> 3;
    uint32_t bit_off  = bit_pos & 0x7;
    bit_off = 8 - bit_off - bpp;
    uint8_t mask = (uint8_t)((1U << bpp) - 1U);
    bitmap[byte_pos] &= (uint8_t)(~(mask << bit_off));
    bitmap[byte_pos] |= (val << bit_off);
}

/**
 * Decode a 48-byte LVGL compressed glyph block into a packed 4bpp bitmap.
 *
 * The glyph is assumed to be square: width == height (standard for CJK
 * bitmap fonts).
 *
 * The 48-byte block contains an LVGL RLE-compressed pixel stream (as
 * output by lvgl_conv_tool.exe with --compress --bpp 4, prefilter
 * enabled).
 */
HeitiFont_Result_t HeitiFont_DecodeBlock(const uint8_t block[HEITI_FONT_BLOCK_SIZE],
                                         uint16_t height,
                                         uint16_t bpp,
                                         uint8_t *out_bitmap,
                                         uint32_t buf_size,
                                         uint32_t *out_width,
                                         uint32_t *out_height,
                                         uint32_t *out_bytes)
{
    uint32_t w, h;
    uint32_t row_bytes, total_bytes;
    HeitiRleCtx_t rle;
    uint8_t line_buf[256];        /* one decompressed row, 1 byte per pixel */

    if (block == NULL || out_bitmap == NULL)
    {
        return HEITI_FONT_ERR_PARAM;
    }

    h = height;
    w = height;                   /* square CJK glyphs */

    row_bytes   = (w * bpp + 7U) / 8U;
    total_bytes = row_bytes * h;

    *out_width  = (uint32_t)w;
    *out_height = (uint32_t)h;
    *out_bytes  = total_bytes;

    if (buf_size < total_bytes)
    {
        return HEITI_FONT_ERR_NO_MEM;
    }

    /* clear output (important: prefilter XOR reads previous row) */
    memset(out_bitmap, 0, total_bytes);

    rle_init(&rle, block, (uint8_t)bpp);

    /* ---- row 0: no prefilter ---- */
    for (uint16_t x = 0; x < w; x++)
    {
        line_buf[x] = rle_next(&rle);
        pixel_write(out_bitmap, x, line_buf[x], (uint8_t)bpp);
    }

    /* ---- rows 1..h-1: prefilter XOR ---- */
    for (uint16_t y = 1; y < h; y++)
    {
        for (uint16_t x = 0; x < w; x++)
        {
            uint8_t px = rle_next(&rle);
            line_buf[x] ^= px;                      /* XOR with previous row */
            pixel_write(out_bitmap, y * w + x, line_buf[x], (uint8_t)bpp);
        }
    }

    return HEITI_FONT_OK;
}

uint16_t HeitiFont_GetHeight(const HeitiFont_Context_t *ctx)
{
    return (ctx != NULL) ? ctx->height : 0;
}

uint16_t HeitiFont_GetBpp(const HeitiFont_Context_t *ctx)
{
    return (ctx != NULL) ? ctx->bpp : 0;
}
