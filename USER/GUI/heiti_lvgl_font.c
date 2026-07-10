#include "heiti_lvgl_font.h"

#include <string.h>

/*
 * Static buffer for one glyph's decoded bitmap.
 * Square glyph assumption: w == h == font height.
 * Max needed: 20×20×4bpp = 200 bytes.  Generous 256 bytes.
 */
#define GLYPH_BUF_SIZE 256U

static HeitiFont_Context_t *s_font_ctx = NULL;
static uint8_t s_glyph_buf[GLYPH_BUF_SIZE];
static lv_font_t s_lv_font;

static bool get_glyph_dsc_cb(const lv_font_t *font,
                              lv_font_glyph_dsc_t *dsc,
                              uint32_t unicode_letter,
                              uint32_t unicode_letter_next)
{
    HeitiFont_Context_t *ctx = s_font_ctx;
    uint32_t glyph_idx;
    uint8_t block[HEITI_FONT_BLOCK_SIZE];
    uint32_t w, h, bytes;
    (void)font;
    (void)unicode_letter_next;

    if (ctx == NULL || !ctx->is_open)
    {
        return false;
    }

    /* Look up the codepoint */
    if (HeitiFont_Lookup(ctx, unicode_letter, &glyph_idx) != HEITI_FONT_OK)
    {
        /* character not in font -> use glyph 0 (notdef) */
        glyph_idx = 0;
    }

    /* Read the 48-byte glyph block */
    if (HeitiFont_GetGlyphBlock(ctx, glyph_idx, block) != HEITI_FONT_OK)
    {
        return false;
    }

    /* Decode the block into the temporary bitmap buffer */
    if (HeitiFont_DecodeBlock(block, ctx->height, ctx->bpp,
                               s_glyph_buf, sizeof(s_glyph_buf),
                               &w, &h, &bytes) != HEITI_FONT_OK)
    {
        return false;
    }

    dsc->adv_w = (int16_t)w;
    dsc->box_w = (uint16_t)w;
    dsc->box_h = (uint16_t)h;
    dsc->ofs_x = 0;
    dsc->ofs_y = (int16_t)h;    /* baseline at bottom: glyph fills box_h above baseline */
    dsc->bpp   = (uint8_t)ctx->bpp;

    return true;
}

static const uint8_t *get_glyph_bitmap_cb(const lv_font_t *font, uint32_t unicode_letter)
{
    (void)font;
    (void)unicode_letter;

    /*
     * The bitmap was already decoded in get_glyph_dsc_cb and stored
     * in s_glyph_buf.  Return that pointer.
     *
     * LVGL calls get_glyph_dsc first, then get_glyph_bitmap during
     * the same render pass, so s_glyph_buf is still valid.
     */
    return s_glyph_buf;
}

const lv_font_t *HeitiLvgl_Register(HeitiFont_Context_t *ctx)
{
    if (ctx == NULL || !ctx->is_open)
    {
        return NULL;
    }

    s_font_ctx = ctx;

    memset(&s_lv_font, 0, sizeof(s_lv_font));
    s_lv_font.line_height      = ctx->height;
    s_lv_font.base_line        = ctx->height;   /* baseline at bottom of line */
    s_lv_font.subpx            = LV_FONT_SUBPX_NONE;
    s_lv_font.get_glyph_dsc    = get_glyph_dsc_cb;
    s_lv_font.get_glyph_bitmap = get_glyph_bitmap_cb;
    s_lv_font.dsc              = NULL;           /* no built-in dsc */

    return &s_lv_font;
}
