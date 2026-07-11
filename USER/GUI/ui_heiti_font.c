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
    bool open_tried;
    bool ready;
    const egui_font_t *fallback;
    uint8_t glyph_bitmap[HEITI_FONT_GLYPH_RAW_MAX];
} ui_heiti_font_t;

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

static ui_heiti_font_t s_heiti_16 = {
    .base = { .res = NULL, .api = &s_ui_heiti_font_api },
    .fallback = EGUI_FONT_OF(&egui_res_font_montserrat_12_4),
};

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
    if (HeitiFont_Open(&font->ctx, UI_HEITI_FONT_16_PATH) == HEITI_FONT_OK)
    {
        font->ready = true;
        return true;
    }

    log_printf("[UI_FONT] open %s FAIL", UI_HEITI_FONT_16_PATH);
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
    uint8_t bpp;
    egui_dim_t baseline;
    egui_dim_t draw_x;
    egui_dim_t draw_y;

    if (!ui_heiti_font_ensure_open(font) ||
        (HeitiFont_Lookup(&font->ctx, cp, &glyph_index) != HEITI_FONT_OK) ||
        (HeitiFont_GetGlyph(&font->ctx, glyph_index, &dsc, font->glyph_bitmap, sizeof(font->glyph_bitmap)) != HEITI_FONT_OK))
    {
        return 0;
    }

    if (adv != NULL)
    {
        *adv = (egui_dim_t)dsc.adv_w;
    }

    if ((canvas == NULL) || (dsc.box_w == 0U) || (dsc.box_h == 0U))
    {
        return 1;
    }

    bpp = (uint8_t)HeitiFont_GetBpp(&font->ctx);
    baseline = (egui_dim_t)(y + HeitiFont_GetBaseLine(&font->ctx));
    draw_x = (egui_dim_t)(x + dsc.ofs_x);
    draw_y = (egui_dim_t)(baseline - dsc.ofs_y - dsc.box_h);

    for (uint16_t gy = 0U; gy < dsc.box_h; gy++)
    {
        for (uint16_t gx = 0U; gx < dsc.box_w; gx++)
        {
            uint32_t pixel_index = (uint32_t)gy * dsc.box_w + gx;
            uint8_t value = ui_heiti_bitmap_read(font->glyph_bitmap, pixel_index, bpp);
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

        if ((cp < 0x80U) || !ui_heiti_font_draw_glyph(font, canvas, cp, pen_x, y, color, alpha, &adv))
        {
            (void)ui_heiti_font_draw_fallback(font, canvas, s, bytes, pen_x, y, color, alpha, &adv);
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

    if ((font == NULL) || (adv == NULL) || !ui_heiti_font_ensure_open(font))
    {
        return 0;
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
    egui_dim_t line_h = 16;
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

        if ((cp >= 0x80U) && ui_heiti_font_measure_glyph(font, cp, &adv))
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

const egui_font_t *ui_heiti_font_get_16(void)
{
    (void)ui_heiti_font_ensure_open(&s_heiti_16);
    return &s_heiti_16.base;
}

bool ui_heiti_font_16_is_ready(void)
{
    return ui_heiti_font_ensure_open(&s_heiti_16);
}
