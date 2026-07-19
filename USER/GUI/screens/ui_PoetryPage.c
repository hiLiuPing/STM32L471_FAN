#include "ui_PoetryPage.h"

#include <stdint.h>

#include "egui_port_stm32l471_fan.h"
#include "key.h"
#include "page_manager.h"
#include "ui_common.h"
#include "ui_heiti_font.h"
#include "ui_poetry_popup.h"

#define UI_POETRY_CARD_X  48
#define UI_POETRY_CARD_Y  2
#define UI_POETRY_CARD_W  332
#define UI_POETRY_CARD_H  138

typedef struct
{
    egui_view_t base;
    uint8_t content_ready;
} ui_poetry_page_t;

static ui_poetry_page_t s_poetry_page;
static egui_view_api_t s_poetry_api;

egui_view_t *ui_PoetryPage = NULL;

static void ui_PoetryPage_on_draw(egui_view_t *self);

static void ui_PoetryPage_draw_background(egui_canvas_t *canvas)
{
    egui_gradient_stop_t stops[2];
    egui_gradient_t gradient;

    stops[0].position = 0U;
    stops[0].color = ui_color(0x07111F);
    stops[1].position = 255U;
    stops[1].color = ui_color(0x0B1F33);
    gradient.type = EGUI_GRADIENT_TYPE_LINEAR_VERTICAL;
    gradient.stop_count = 2U;
    gradient.alpha = EGUI_ALPHA_100;
    gradient.stops = stops;
    gradient.center_x = 0;
    gradient.center_y = 0;
    gradient.radius = 0;
    egui_canvas_draw_rectangle_fill_gradient(canvas, 0, 0,
                                             UI_SCREEN_W, UI_SCREEN_H,
                                             &gradient);

    for (uint8_t i = 0U; i < 6U; i++)
    {
        int16_t x = (int16_t)((int16_t)i * 86 - 30);
        egui_canvas_draw_line(canvas, x, 0, x + 34, UI_SCREEN_H, 2,
                              ui_color((i & 1U) ? 0x38BDF8 : 0xF59E0B),
                              EGUI_ALPHA_20);
    }

    egui_canvas_draw_circle_basic(canvas, 24, 27, 19, 1,
                                  ui_color(0xF59E0B), EGUI_ALPHA_40);
    egui_canvas_draw_circle_basic(canvas, 404, 112, 24, 1,
                                  ui_color(0x38BDF8), EGUI_ALPHA_40);
    egui_canvas_draw_round_rectangle_fill(canvas,
                                          UI_POETRY_CARD_X,
                                          UI_POETRY_CARD_Y,
                                          UI_POETRY_CARD_W,
                                          UI_POETRY_CARD_H,
                                          10,
                                          ui_color(0x0D2134),
                                          EGUI_ALPHA_90);
    egui_canvas_draw_round_rectangle(canvas,
                                     UI_POETRY_CARD_X,
                                     UI_POETRY_CARD_Y,
                                     UI_POETRY_CARD_W,
                                     UI_POETRY_CARD_H,
                                     10,
                                     1,
                                     ui_color(0x1E3A50),
                                     EGUI_ALPHA_100);
    egui_canvas_draw_line(canvas, UI_POETRY_CARD_X, 20,
                          UI_POETRY_CARD_X, 122, 2,
                          ui_color(0xF59E0B), EGUI_ALPHA_80);
    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_10_4),
                 "SONG", 3, 5, 42, 14, EGUI_ALIGN_CENTER, 0xF8FAFC);
    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_10_4),
                 "OK", 387, 121, 38, 14, EGUI_ALIGN_CENTER, 0x7DD3FC);
}

void ui_PoetryPage_screen_init(void)
{
    egui_view_t *view = EGUI_VIEW_OF(&s_poetry_page.base);

    s_poetry_page.content_ready = 0U;
    ui_PoetryPage = view;
    egui_view_init(view, egui_port_get_core());
    egui_view_copy_api(view, &s_poetry_api);
    s_poetry_api.on_draw = ui_PoetryPage_on_draw;
    view->api = &s_poetry_api;
    egui_view_set_position(view, 0, 0);
    egui_view_set_size(view, UI_SCREEN_W, UI_SCREEN_H);
    egui_view_set_visible(view, 1);
}

void ui_PoetryPage_screen_destroy(void)
{
}

void ui_PoetryPage_on_enter(void)
{
    s_poetry_page.content_ready = ui_poetry_popup_refresh_cached() ? 1U : 0U;
    if (ui_PoetryPage != NULL)
    {
        egui_view_invalidate_full(ui_PoetryPage);
    }
}

bool ui_PoetryPage_key_handler(void *key_event)
{
    const key_event_t *event = (const key_event_t *)key_event;

    if ((event != NULL) &&
        (event->id == KEY_ID_OK) &&
        (event->type == KEY_EVT_CLICK))
    {
        s_poetry_page.content_ready = ui_poetry_popup_refresh_cached() ? 1U : 0U;
        if (ui_PoetryPage != NULL)
        {
            egui_view_invalidate_full(ui_PoetryPage);
        }
        return true;
    }

    return ui_page_consume_nav_key_event(key_event);
}

static void ui_PoetryPage_on_draw(egui_view_t *self)
{
    egui_canvas_t *canvas = egui_view_get_canvas(self);

    ui_PoetryPage_draw_background(canvas);
    if ((s_poetry_page.content_ready != 0U) &&
        ui_poetry_popup_draw_cached(canvas))
    {
        return;
    }

    ui_draw_text(canvas, ui_heiti_font_get_16(),
                 "宋词加载失败", 66, 47, 296, 22,
                 EGUI_ALIGN_CENTER, 0xCBD5E1);
    ui_draw_text(canvas, ui_heiti_font_get_12(),
                 "按 OK 重试", 66, 75, 296, 18,
                 EGUI_ALIGN_CENTER, 0x7DD3FC);
}
