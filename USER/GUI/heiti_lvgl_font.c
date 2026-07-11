/**
 * @file heiti_lvgl_font.c
 * @brief Register the on-demand Heiti font reader as an LVGL custom font.
 */

#include "heiti_lvgl_font.h"

#include <string.h>

#define GLYPH_BUF_SIZE 512U

static HeitiFont_Context_t *s_font_ctx = NULL;
static uint8_t s_glyph_buf[GLYPH_BUF_SIZE];
static lv_font_t s_lv_font;
static HeitiFont_Context_t s_default_ctx;
static bool s_default_open = false;
static uint32_t s_last_unicode = 0U;
static uint32_t s_last_glyph_idx = 0U;
static HeitiFont_GlyphDsc_t s_last_dsc;
static bool s_last_valid = false;

static bool get_glyph_dsc_cb(const lv_font_t *font,
                             lv_font_glyph_dsc_t *dsc,
                             uint32_t unicode_letter,
                             uint32_t unicode_letter_next)
{
    HeitiFont_Context_t *ctx = s_font_ctx;
    uint32_t glyph_idx;
    HeitiFont_GlyphDsc_t gdsc;

    (void)font;
    (void)unicode_letter_next;

    if ((ctx == NULL) || !ctx->is_open)
    {
        return false;
    }

    if (HeitiFont_Lookup(ctx, unicode_letter, &glyph_idx) != HEITI_FONT_OK)
    {
        return false;
    }

    if (HeitiFont_GetGlyphDsc(ctx, glyph_idx, &gdsc) != HEITI_FONT_OK)
    {
        return false;
    }

    if ((gdsc.adv_w == 0U) && (gdsc.box_w == 0U) && (gdsc.box_h == 0U))
    {
        return false;
    }

    dsc->adv_w = gdsc.adv_w;
    dsc->box_w = gdsc.box_w;
    dsc->box_h = gdsc.box_h;
    dsc->ofs_x = gdsc.ofs_x;
    dsc->ofs_y = gdsc.ofs_y;
    dsc->bpp = (uint8_t)ctx->bpp;
    dsc->is_placeholder = 0;

    s_last_unicode = unicode_letter;
    s_last_glyph_idx = glyph_idx;
    s_last_dsc = gdsc;
    s_last_valid = true;

    return true;
}

static const uint8_t *get_glyph_bitmap_cb(const lv_font_t *font, uint32_t unicode_letter)
{
    HeitiFont_Context_t *ctx = s_font_ctx;
    uint32_t glyph_idx;
    HeitiFont_GlyphDsc_t gdsc;

    (void)font;

    if ((ctx == NULL) || !ctx->is_open)
    {
        return NULL;
    }

    if (s_last_valid && (s_last_unicode == unicode_letter))
    {
        glyph_idx = s_last_glyph_idx;
        if ((s_last_dsc.box_w == 0U) || (s_last_dsc.box_h == 0U))
        {
            return NULL;
        }
    }
    else
    {
        if (HeitiFont_Lookup(ctx, unicode_letter, &glyph_idx) != HEITI_FONT_OK)
        {
            return NULL;
        }
    }

    if (HeitiFont_GetGlyph(ctx, glyph_idx, &gdsc, s_glyph_buf, sizeof(s_glyph_buf)) != HEITI_FONT_OK)
    {
        return NULL;
    }

    if ((gdsc.box_w == 0U) || (gdsc.box_h == 0U))
    {
        return NULL;
    }

    s_last_unicode = unicode_letter;
    s_last_glyph_idx = glyph_idx;
    s_last_dsc = gdsc;
    s_last_valid = true;

    return s_glyph_buf;
}

const lv_font_t *HeitiLvgl_Register(HeitiFont_Context_t *ctx)
{
    if ((ctx == NULL) || !ctx->is_open)
    {
        return NULL;
    }

    s_font_ctx = ctx;
    s_last_unicode = 0U;
    s_last_glyph_idx = 0U;
    memset(&s_last_dsc, 0, sizeof(s_last_dsc));
    s_last_valid = false;

    memset(&s_lv_font, 0, sizeof(s_lv_font));
    s_lv_font.line_height = HeitiFont_GetLineHeight(ctx);
    s_lv_font.base_line = HeitiFont_GetBaseLine(ctx);
    s_lv_font.subpx = LV_FONT_SUBPX_NONE;
    s_lv_font.get_glyph_dsc = get_glyph_dsc_cb;
    s_lv_font.get_glyph_bitmap = get_glyph_bitmap_cb;
    s_lv_font.fallback = &lv_font_montserrat_16;

    return &s_lv_font;
}

const lv_font_t *HeitiLvgl_OpenDefault(const char *path)
{
    if (path == NULL)
    {
        return NULL;
    }

    if (!s_default_open)
    {
        if (HeitiFont_Open(&s_default_ctx, path) != HEITI_FONT_OK)
        {
            return NULL;
        }
        s_default_open = true;
    }

    return HeitiLvgl_Register(&s_default_ctx);
}
