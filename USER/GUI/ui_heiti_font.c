#include "ui_heiti_font.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "core/egui_common.h"
#include "heiti_font_drv.h"
#include "log.h"
#include "resource/egui_resource.h"

typedef struct
{
    egui_font_t base;
    HeitiFont_Context_t ctx;
    const char *path;
    uint8_t default_line_h;
    bool open_tried;
    bool ready;
    const egui_font_t *fallback;
    uint8_t glyph_bitmap[HEITI_FONT_GLYPH_RAW_MAX];
} ui_heiti_font_t;

/* 32 槽 × 256B 位图：在用字号最大 20px/4bpp（约 210B/字），256B 足够。
 * 相比旧的 16 槽 × 512B，槽数翻倍（一屏诗词不再 LRU 抖动、避免每帧重读
 * SPI Flash），总 RAM 仅增加约 0.6KB。超过 256B 的字形直接绘制不进缓存。 */
#define UI_HEITI_GLYPH_CACHE_SLOTS      32U
#define UI_HEITI_GLYPH_CACHE_BITMAP_MAX 256U
#define UI_HEITI_MISSING_GLYPH_CP  0x00B7U

typedef struct
{
    const ui_heiti_font_t *font;
    uint32_t cp;
    HeitiFont_GlyphDsc_t dsc;
    uint8_t bpp;
    uint8_t valid;
    uint32_t age;
    uint8_t bitmap[UI_HEITI_GLYPH_CACHE_BITMAP_MAX];
} ui_heiti_glyph_cache_t;

static uint32_t ui_heiti_glyph_bitmap_bytes(const HeitiFont_GlyphDsc_t *dsc, uint8_t bpp)
{
    return (((uint32_t)dsc->box_w * dsc->box_h * bpp) + 7U) / 8U;
}

static int ui_heiti_font_draw_string(const egui_font_t *self,
                                     egui_canvas_t *canvas,
                                     const void *string,
                                     egui_dim_t x,
                                     egui_dim_t y,
                                     egui_color_t color,
                                     egui_alpha_t alpha);
static int ui_heiti_font_get_str_size(const egui_font_t *self,
                                      const void *string,
                                      uint8_t is_multi_line,
                                      egui_dim_t line_space,
                                      egui_dim_t *width,
                                      egui_dim_t *height);

static const egui_font_api_t s_ui_heiti_font_api = {
    .draw_string = ui_heiti_font_draw_string,
    .get_str_size = ui_heiti_font_get_str_size,
};

static ui_heiti_glyph_cache_t s_heiti_glyph_cache[UI_HEITI_GLYPH_CACHE_SLOTS];
static uint32_t s_heiti_glyph_cache_age = 0U;

static ui_heiti_font_t s_heiti_fonts[] = {
    {
        .base = { .res = NULL, .api = &s_ui_heiti_font_api },
        .path = UI_HEITI_FONT_12_PATH,
        .default_line_h = 12U,
        .fallback = EGUI_FONT_OF(&egui_res_font_montserrat_12_4),
    },
    {
        .base = { .res = NULL, .api = &s_ui_heiti_font_api },
        .path = UI_HEITI_FONT_16_PATH,
        .default_line_h = 16U,
        .fallback = EGUI_FONT_OF(&egui_res_font_montserrat_12_4),
    },
    {
        .base = { .res = NULL, .api = &s_ui_heiti_font_api },
        .path = UI_HEITI_FONT_18_PATH,
        .default_line_h = 18U,
        .fallback = EGUI_FONT_OF(&egui_res_font_montserrat_16_4),
    },
    {
        .base = { .res = NULL, .api = &s_ui_heiti_font_api },
        .path = UI_HEITI_FONT_20_PATH,
        .default_line_h = 20U,
        .fallback = EGUI_FONT_OF(&egui_res_font_montserrat_16_4),
    },
};

static ui_heiti_glyph_cache_t *ui_heiti_glyph_cache_find(const ui_heiti_font_t *font, uint32_t cp)
{
    for (uint8_t i = 0U; i < UI_HEITI_GLYPH_CACHE_SLOTS; i++)
    {
        ui_heiti_glyph_cache_t *entry = &s_heiti_glyph_cache[i];

        if ((entry->valid != 0U) && (entry->font == font) && (entry->cp == cp))
        {
            entry->age = ++s_heiti_glyph_cache_age;
            return entry;
        }
    }

    return NULL;
}

static ui_heiti_glyph_cache_t *ui_heiti_glyph_cache_alloc(void)
{
    ui_heiti_glyph_cache_t *oldest = &s_heiti_glyph_cache[0];

    for (uint8_t i = 0U; i < UI_HEITI_GLYPH_CACHE_SLOTS; i++)
    {
        ui_heiti_glyph_cache_t *entry = &s_heiti_glyph_cache[i];

        if (entry->valid == 0U)
        {
            return entry;
        }

        if (entry->age < oldest->age)
        {
            oldest = entry;
        }
    }

    return oldest;
}

static const ui_heiti_glyph_cache_t *ui_heiti_glyph_cache_put(const ui_heiti_font_t *font,
                                                              uint32_t cp,
                                                              const HeitiFont_GlyphDsc_t *dsc,
                                                              uint8_t bpp,
                                                              const uint8_t *bitmap)
{
    ui_heiti_glyph_cache_t *entry;
    uint32_t bitmap_bytes;

    if ((font == NULL) || (dsc == NULL) || (bitmap == NULL))
    {
        return NULL;
    }

    bitmap_bytes = ui_heiti_glyph_bitmap_bytes(dsc, bpp);
    if (bitmap_bytes > UI_HEITI_GLYPH_CACHE_BITMAP_MAX)
    {
        /* 字形超出槽容量：调用方直接用驱动缓冲绘制，不缓存 */
        return NULL;
    }

    entry = ui_heiti_glyph_cache_alloc();
    entry->font = font;
    entry->cp = cp;
    entry->dsc = *dsc;
    entry->bpp = bpp;
    entry->valid = 1U;
    entry->age = ++s_heiti_glyph_cache_age;
    memcpy(entry->bitmap, bitmap, bitmap_bytes);

    return entry;
}

static ui_heiti_font_t *ui_heiti_font_find(uint8_t size)
{
    switch (size)
    {
    case 12U:
        return &s_heiti_fonts[0];
    case 16U:
        return &s_heiti_fonts[1];
    case 18U:
        return &s_heiti_fonts[2];
    case 20U:
        return &s_heiti_fonts[3];
    default:
        return NULL;
    }
}

static int ui_heiti_utf8_decode(const char *s, uint32_t *out_cp)
{
    const uint8_t *p = (const uint8_t *)s;

    if ((s == NULL) || (out_cp == NULL) || (p[0] == 0U))
    {
        return 0;
    }

    if ((p[0] & 0x80U) == 0U)
    {
        *out_cp = p[0];
        return 1;
    }

    if (((p[0] & 0xE0U) == 0xC0U) && ((p[1] & 0xC0U) == 0x80U))
    {
        *out_cp = ((uint32_t)(p[0] & 0x1FU) << 6) |
                  (uint32_t)(p[1] & 0x3FU);
        return 2;
    }

    if (((p[0] & 0xF0U) == 0xE0U) &&
        ((p[1] & 0xC0U) == 0x80U) &&
        ((p[2] & 0xC0U) == 0x80U))
    {
        *out_cp = ((uint32_t)(p[0] & 0x0FU) << 12) |
                  ((uint32_t)(p[1] & 0x3FU) << 6) |
                  (uint32_t)(p[2] & 0x3FU);
        return 3;
    }

    if (((p[0] & 0xF8U) == 0xF0U) &&
        ((p[1] & 0xC0U) == 0x80U) &&
        ((p[2] & 0xC0U) == 0x80U) &&
        ((p[3] & 0xC0U) == 0x80U))
    {
        *out_cp = ((uint32_t)(p[0] & 0x07U) << 18) |
                  ((uint32_t)(p[1] & 0x3FU) << 12) |
                  ((uint32_t)(p[2] & 0x3FU) << 6) |
                  (uint32_t)(p[3] & 0x3FU);
        return 4;
    }

    *out_cp = p[0];
    return 1;
}

static uint8_t ui_heiti_bitmap_read(const uint8_t *bitmap, uint32_t pixel_index, uint8_t bpp)
{
    uint32_t bit_pos;
    uint8_t value = 0U;

    if (bpp == 3U)
    {
        bpp = 4U;
    }

    bit_pos = pixel_index * bpp;

    for (uint8_t i = 0U; i < bpp; i++)
    {
        uint32_t byte_index = bit_pos >> 3;
        uint8_t shift = (uint8_t)(7U - (bit_pos & 0x07U));

        value = (uint8_t)((value << 1) | ((bitmap[byte_index] >> shift) & 0x01U));
        bit_pos++;
    }

    return value;
}

static egui_alpha_t ui_heiti_alpha_from_value(uint8_t value, uint8_t bpp)
{
    uint16_t max_value;

    if (value == 0U)
    {
        return EGUI_ALPHA_0;
    }

    if (bpp == 3U)
    {
        bpp = 4U;
    }

    max_value = (uint16_t)((1U << bpp) - 1U);
    if (value >= max_value)
    {
        return EGUI_ALPHA_100;
    }

    return (egui_alpha_t)(((uint16_t)value * EGUI_ALPHA_100 + (max_value / 2U)) / max_value);
}

static bool ui_heiti_font_ensure_open(ui_heiti_font_t *font)
{
    if (font == NULL)
    {
        return false;
    }

    if (font->ready)
    {
        return true;
    }

    if (font->open_tried)
    {
        return false;
    }

    font->open_tried = true;
    if (HeitiFont_Open(&font->ctx, font->path) == HEITI_FONT_OK)
    {
        font->ready = true;
        return true;
    }

    log_printf("[UI_FONT] open %s FAIL", font->path);
    return false;
}

static int ui_heiti_font_draw_fallback(const ui_heiti_font_t *font,
                                       egui_canvas_t *canvas,
                                       const char *s,
                                       int bytes,
                                       egui_dim_t x,
                                       egui_dim_t y,
                                       egui_color_t color,
                                       egui_alpha_t alpha,
                                       egui_dim_t *adv)
{
    char ch[5] = {0};
    egui_dim_t w = 0;
    egui_dim_t h = 0;

    if ((font == NULL) || (font->fallback == NULL) || (bytes <= 0))
    {
        if (adv != NULL)
        {
            *adv = 8;
        }
        return bytes;
    }

    if (bytes > 4)
    {
        bytes = 4;
    }

    memcpy(ch, s, (size_t)bytes);
    if ((canvas != NULL) && (font->fallback->api != NULL) && (font->fallback->api->draw_string != NULL))
    {
        (void)font->fallback->api->draw_string(font->fallback, canvas, ch, x, y, color, alpha);
    }

    (void)egui_font_get_str_size_with_canvas(font->fallback, canvas, ch, 0, 0, &w, &h);
    if (adv != NULL)
    {
        *adv = (w > 0) ? w : 8;
    }

    return bytes;
}

static int ui_heiti_font_draw_glyph(ui_heiti_font_t *font,
                                    egui_canvas_t *canvas,
                                    uint32_t cp,
                                    egui_dim_t x,
                                    egui_dim_t y,
                                    egui_color_t color,
                                    egui_alpha_t alpha,
                                    egui_dim_t *adv)
{
    uint32_t glyph_index;
    HeitiFont_GlyphDsc_t dsc;
    const ui_heiti_glyph_cache_t *cache;
    const uint8_t *bitmap;
    uint8_t bpp;
    egui_dim_t baseline;
    egui_dim_t draw_x;
    egui_dim_t draw_y;

    if (!ui_heiti_font_ensure_open(font))
    {
        return 0;
    }

    cache = ui_heiti_glyph_cache_find(font, cp);
    if (cache != NULL)
    {
        dsc = cache->dsc;
        bpp = cache->bpp;
        bitmap = cache->bitmap;
    }
    else
    {
        if ((HeitiFont_Lookup(&font->ctx, cp, &glyph_index) != HEITI_FONT_OK) ||
            (HeitiFont_GetGlyph(&font->ctx, glyph_index, &dsc, font->glyph_bitmap, sizeof(font->glyph_bitmap)) != HEITI_FONT_OK))
        {
            return 0;
        }

        bpp = (uint8_t)HeitiFont_GetBpp(&font->ctx);
        cache = ui_heiti_glyph_cache_put(font, cp, &dsc, bpp, font->glyph_bitmap);
        bitmap = (cache != NULL) ? cache->bitmap : font->glyph_bitmap;
    }

    if (adv != NULL)
    {
        *adv = (egui_dim_t)dsc.adv_w;
    }

    if ((canvas == NULL) || (dsc.box_w == 0U) || (dsc.box_h == 0U))
    {
        return 1;
    }

    baseline = (egui_dim_t)(y + HeitiFont_GetBaseLine(&font->ctx));
    draw_x = (egui_dim_t)(x + dsc.ofs_x);
    draw_y = (egui_dim_t)(baseline - dsc.ofs_y - dsc.box_h);

    for (uint16_t gy = 0U; gy < dsc.box_h; gy++)
    {
        for (uint16_t gx = 0U; gx < dsc.box_w; gx++)
        {
            uint32_t pixel_index = (uint32_t)gy * dsc.box_w + gx;
            uint8_t value = ui_heiti_bitmap_read(bitmap, pixel_index, bpp);
            egui_alpha_t pixel_alpha = ui_heiti_alpha_from_value(value, bpp);

            if (pixel_alpha != EGUI_ALPHA_0)
            {
                egui_canvas_draw_point(canvas,
                                       (egui_dim_t)(draw_x + (egui_dim_t)gx),
                                       (egui_dim_t)(draw_y + (egui_dim_t)gy),
                                       color,
                                       egui_color_alpha_mix(alpha, pixel_alpha));
            }
        }
    }

    return 1;
}

static int ui_heiti_font_draw_string(const egui_font_t *self,
                                     egui_canvas_t *canvas,
                                     const void *string,
                                     egui_dim_t x,
                                     egui_dim_t y,
                                     egui_color_t color,
                                     egui_alpha_t alpha)
{
    ui_heiti_font_t *font = (ui_heiti_font_t *)self;
    const char *s = (const char *)string;
    int consumed = 0;
    egui_dim_t pen_x = x;

    if ((font == NULL) || (s == NULL))
    {
        return 0;
    }

    while (*s != '\0')
    {
        uint32_t cp;
        int bytes;
        egui_dim_t adv = 0;

        if (*s == '\r')
        {
            s++;
            consumed++;
            continue;
        }

        if (*s == '\n')
        {
            consumed++;
            break;
        }

        bytes = ui_heiti_utf8_decode(s, &cp);
        if (bytes <= 0)
        {
            break;
        }

        if (cp < 0x80U)
        {
            (void)ui_heiti_font_draw_fallback(font, canvas, s, bytes, pen_x, y, color, alpha, &adv);
        }
        else if (!ui_heiti_font_draw_glyph(font, canvas, cp, pen_x, y, color, alpha, &adv))
        {
            /*
             * Poetry resources can contain uncommon characters that are not
             * present in the offline Heiti font.  Use the same middle dot as
             * the poem titles so a missing glyph remains visible and keeps a
             * deterministic advance width.
             */
            if ((cp == UI_HEITI_MISSING_GLYPH_CP) ||
                !ui_heiti_font_draw_glyph(font,
                                          canvas,
                                          UI_HEITI_MISSING_GLYPH_CP,
                                          pen_x,
                                          y,
                                          color,
                                          alpha,
                                          &adv))
            {
                (void)ui_heiti_font_draw_fallback(font, canvas, s, bytes, pen_x, y, color, alpha, &adv);
            }
        }

        pen_x = (egui_dim_t)(pen_x + adv);
        s += bytes;
        consumed += bytes;
    }

    return consumed;
}

static int ui_heiti_font_measure_glyph(ui_heiti_font_t *font, uint32_t cp, egui_dim_t *adv)
{
    uint32_t glyph_index;
    HeitiFont_GlyphDsc_t dsc;
    const ui_heiti_glyph_cache_t *cache;

    if ((font == NULL) || (adv == NULL) || !ui_heiti_font_ensure_open(font))
    {
        return 0;
    }

    /* 已绘制过的字直接用缓存的 dsc，测量不再触发 Flash 读取 */
    cache = ui_heiti_glyph_cache_find(font, cp);
    if (cache != NULL)
    {
        *adv = (egui_dim_t)cache->dsc.adv_w;
        return 1;
    }

    if ((HeitiFont_Lookup(&font->ctx, cp, &glyph_index) != HEITI_FONT_OK) ||
        (HeitiFont_GetGlyphDsc(&font->ctx, glyph_index, &dsc) != HEITI_FONT_OK))
    {
        return 0;
    }

    *adv = (egui_dim_t)dsc.adv_w;
    return 1;
}

static int ui_heiti_font_get_str_size(const egui_font_t *self,
                                      const void *string,
                                      uint8_t is_multi_line,
                                      egui_dim_t line_space,
                                      egui_dim_t *width,
                                      egui_dim_t *height)
{
    ui_heiti_font_t *font = (ui_heiti_font_t *)self;
    const char *s = (const char *)string;
    egui_dim_t line_w = 0;
    egui_dim_t max_w = 0;
    egui_dim_t line_h = (font != NULL) ? font->default_line_h : 16;
    egui_dim_t total_h;

    if ((font != NULL) && ui_heiti_font_ensure_open(font))
    {
        line_h = (egui_dim_t)HeitiFont_GetLineHeight(&font->ctx);
    }

    total_h = line_h;

    if ((width == NULL) || (height == NULL))
    {
        return -1;
    }

    if (s == NULL)
    {
        *width = 0;
        *height = 0;
        return -1;
    }

    while (*s != '\0')
    {
        uint32_t cp;
        int bytes;
        egui_dim_t adv = 0;

        if (*s == '\r')
        {
            s++;
            continue;
        }

        if (*s == '\n')
        {
            if (max_w < line_w)
            {
                max_w = line_w;
            }

            if (is_multi_line == 0U)
            {
                break;
            }

            line_w = 0;
            total_h = (egui_dim_t)(total_h + line_h + line_space);
            s++;
            continue;
        }

        bytes = ui_heiti_utf8_decode(s, &cp);
        if (bytes <= 0)
        {
            break;
        }

        if ((cp >= 0x80U) &&
            (ui_heiti_font_measure_glyph(font, cp, &adv) ||
             ((cp != UI_HEITI_MISSING_GLYPH_CP) &&
              ui_heiti_font_measure_glyph(font, UI_HEITI_MISSING_GLYPH_CP, &adv))))
        {
            line_w = (egui_dim_t)(line_w + adv);
        }
        else if ((font != NULL) && (font->fallback != NULL))
        {
            char ch[5] = {0};
            egui_dim_t fw = 0;
            egui_dim_t fh = 0;

            if (bytes > 4)
            {
                bytes = 4;
            }
            memcpy(ch, s, (size_t)bytes);
            (void)egui_font_get_str_size_with_core(font->fallback, NULL, ch, 0, 0, &fw, &fh);
            line_w = (egui_dim_t)(line_w + ((fw > 0) ? fw : 8));
        }
        else
        {
            line_w = (egui_dim_t)(line_w + 8);
        }

        s += bytes;
    }

    if (max_w < line_w)
    {
        max_w = line_w;
    }

    *width = max_w;
    *height = total_h;
    return 0;
}

const egui_font_t *ui_heiti_font_get(uint8_t size)
{
    ui_heiti_font_t *font = ui_heiti_font_find(size);

    if (font == NULL)
    {
        font = ui_heiti_font_find(16U);
    }

    (void)ui_heiti_font_ensure_open(font);
    return &font->base;
}

bool ui_heiti_font_is_ready(uint8_t size)
{
    ui_heiti_font_t *font = ui_heiti_font_find(size);

    return (font != NULL) ? ui_heiti_font_ensure_open(font) : false;
}

const char *ui_heiti_font_get_path(uint8_t size)
{
    ui_heiti_font_t *font = ui_heiti_font_find(size);

    return (font != NULL) ? font->path : UI_HEITI_FONT_16_PATH;
}

const egui_font_t *ui_heiti_font_get_12(void)
{
    return ui_heiti_font_get(12U);
}

const egui_font_t *ui_heiti_font_get_16(void)
{
    return ui_heiti_font_get(16U);
}

const egui_font_t *ui_heiti_font_get_18(void)
{
    return ui_heiti_font_get(18U);
}

const egui_font_t *ui_heiti_font_get_20(void)
{
    return ui_heiti_font_get(20U);
}

bool ui_heiti_font_12_is_ready(void)
{
    return ui_heiti_font_is_ready(12U);
}

bool ui_heiti_font_16_is_ready(void)
{
    return ui_heiti_font_is_ready(16U);
}

bool ui_heiti_font_18_is_ready(void)
{
    return ui_heiti_font_is_ready(18U);
}

bool ui_heiti_font_20_is_ready(void)
{
    return ui_heiti_font_is_ready(20U);
}
