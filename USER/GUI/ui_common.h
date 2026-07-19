#ifndef __GUI_UI_COMMON_H__
#define __GUI_UI_COMMON_H__

#include "canvas/egui_canvas.h"
#include "resource/egui_resource.h"
#include "time_utils.h"

#define UI_SCREEN_W 428
#define UI_SCREEN_H 142

static inline egui_color_t ui_color(uint32_t rgb)
{
    return EGUI_COLOR_HEX(rgb);
}

static inline void ui_draw_rect(egui_canvas_t *canvas,
                                egui_dim_t x,
                                egui_dim_t y,
                                egui_dim_t w,
                                egui_dim_t h,
                                uint32_t rgb)
{
    egui_canvas_draw_rectangle_fill(canvas, x, y, w, h, ui_color(rgb), EGUI_ALPHA_100);
}

static inline void ui_draw_panel(egui_canvas_t *canvas,
                                 egui_dim_t x,
                                 egui_dim_t y,
                                 egui_dim_t w,
                                 egui_dim_t h,
                                 uint32_t fill_rgb,
                                 uint32_t border_rgb)
{
    egui_canvas_draw_rectangle_fill(canvas, x, y, w, h, ui_color(fill_rgb), EGUI_ALPHA_100);
    egui_canvas_draw_rectangle(canvas, x, y, w, h, 1, ui_color(border_rgb), EGUI_ALPHA_100);
}

static inline void ui_draw_round_panel(egui_canvas_t *canvas,
                                       egui_dim_t x,
                                       egui_dim_t y,
                                       egui_dim_t w,
                                       egui_dim_t h,
                                       egui_dim_t r,
                                       uint32_t fill_rgb,
                                       uint32_t border_rgb)
{
    egui_canvas_draw_round_rectangle_fill(canvas, x, y, w, h, r, ui_color(fill_rgb), EGUI_ALPHA_100);
    egui_canvas_draw_round_rectangle(canvas, x, y, w, h, r, 1, ui_color(border_rgb), EGUI_ALPHA_100);
}

static inline void ui_draw_text(egui_canvas_t *canvas,
                                const egui_font_t *font,
                                const char *text,
                                egui_dim_t x,
                                egui_dim_t y,
                                egui_dim_t w,
                                egui_dim_t h,
                                uint8_t align,
                                uint32_t rgb)
{
    egui_region_t rect = {{x, y}, {w, h}};

    egui_canvas_draw_text_in_rect(canvas, font, text, &rect, align, ui_color(rgb), EGUI_ALPHA_100);
}

static inline void ui_draw_progress(egui_canvas_t *canvas,
                                    egui_dim_t x,
                                    egui_dim_t y,
                                    egui_dim_t w,
                                    egui_dim_t h,
                                    uint8_t percent,
                                    uint32_t back_rgb,
                                    uint32_t fore_rgb)
{
    egui_dim_t fill_w;

    if (percent > 100U)
    {
        percent = 100U;
    }

    fill_w = (egui_dim_t)(((uint32_t)w * percent) / 100U);
    ui_draw_rect(canvas, x, y, w, h, back_rgb);
    if (fill_w > 0)
    {
        ui_draw_rect(canvas, x, y, fill_w, h, fore_rgb);
    }
}

static inline void ui_draw_header(egui_canvas_t *canvas, const char *title, const char *hint, uint32_t accent_rgb)
{
    ui_draw_rect(canvas, 0, 0, UI_SCREEN_W, UI_SCREEN_H, 0x0B1020);
    ui_draw_rect(canvas, 0, 0, 4, UI_SCREEN_H, accent_rgb);
    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_24_4), title, 18, 10, 230, 34, EGUI_ALIGN_LEFT | EGUI_ALIGN_VCENTER, 0xF8FAFC);
    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_12_4), hint, 18, 118, 390, 18, EGUI_ALIGN_LEFT | EGUI_ALIGN_VCENTER, 0x94A3B8);
}

#endif /* __GUI_UI_COMMON_H__ */
