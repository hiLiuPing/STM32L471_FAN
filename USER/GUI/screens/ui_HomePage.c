#include "ui_HomePage.h"

#include "core/egui_timer.h"
#include "data_app.h"
#include "egui_port_stm32l471_fan.h"
#include "page_manager.h"
#include "ui_common.h"

typedef struct
{
    egui_view_t base;
    egui_timer_t timer;
} ui_home_page_t;

static ui_home_page_t s_home_page;
static egui_view_api_t s_home_api;

egui_view_t *ui_HomePage = NULL;

static void ui_HomePage_on_draw(egui_view_t *self);
static void ui_HomePage_timer_cb(egui_timer_t *timer);

void ui_HomePage_screen_init(void)
{
    egui_view_t *view = EGUI_VIEW_OF(&s_home_page.base);

    ui_HomePage = view;
    egui_view_init(view, egui_port_get_core());
    egui_view_copy_api(view, &s_home_api);
    s_home_api.on_draw = ui_HomePage_on_draw;
    view->api = &s_home_api;
    egui_view_set_position(view, 0, 0);
    egui_view_set_size(view, UI_SCREEN_W, UI_SCREEN_H);
    egui_view_set_visible(view, 1);
    egui_view_start_periodic(view, &s_home_page.timer, view, ui_HomePage_timer_cb, 1000U);
}

void ui_HomePage_screen_destroy(void)
{
}

void ui_HomePage_key_handler(void *key_event)
{
    ui_page_handle_default_key_event(key_event);
}

static void ui_HomePage_timer_cb(egui_timer_t *timer)
{
    egui_view_t *view = (egui_view_t *)timer->user_data;

    if ((view != NULL) && egui_view_get_visible(view))
    {
        egui_view_invalidate_full(view);
    }
}

static void ui_HomePage_on_draw(egui_view_t *self)
{
    char time_text[16];
    egui_canvas_t *canvas = egui_view_get_canvas(self);

    Time_Format(time_text);

    ui_draw_header(canvas, "Home", "N/R switch pages  L/B page actions", 0x22C55E);
    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_48_4), time_text, 228, 12, 180, 54,
                 EGUI_ALIGN_RIGHT | EGUI_ALIGN_VCENTER, 0xF8FAFC);

    ui_draw_panel(canvas, 18, 52, 118, 54, 0x12321F, 0x22C55E);
    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_18_4), "Fan", 30, 60, 90, 22, EGUI_ALIGN_LEFT | EGUI_ALIGN_VCENTER, 0xDCFCE7);
    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_12_4), "Power / mode", 30, 84, 90, 14, EGUI_ALIGN_LEFT | EGUI_ALIGN_VCENTER, 0x86EFAC);

    ui_draw_panel(canvas, 154, 52, 118, 54, 0x1E293B, 0x38BDF8);
    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_18_4), "Weather", 166, 60, 90, 22, EGUI_ALIGN_LEFT | EGUI_ALIGN_VCENTER, 0xE0F2FE);
    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_12_4), "Now / air", 166, 84, 90, 14, EGUI_ALIGN_LEFT | EGUI_ALIGN_VCENTER, 0xBAE6FD);

    ui_draw_panel(canvas, 290, 52, 118, 54, 0x312E11, 0xEAB308);
    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_18_4), "Setting", 302, 60, 90, 22, EGUI_ALIGN_LEFT | EGUI_ALIGN_VCENTER, 0xFEF9C3);
    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_12_4), "System info", 302, 84, 90, 14, EGUI_ALIGN_LEFT | EGUI_ALIGN_VCENTER, 0xFDE68A);
}
