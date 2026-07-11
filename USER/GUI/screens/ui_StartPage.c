#include "ui_StartPage.h"

#include "core/egui_timer.h"
#include "egui_port_stm32l471_fan.h"
#include "page_manager.h"
#include "ui_common.h"

typedef struct
{
    egui_view_t base;
    egui_timer_t timer;
    uint32_t enter_tick;
    uint8_t progress;
} ui_start_page_t;

static ui_start_page_t s_start_page;
static egui_view_api_t s_start_api;

egui_view_t *ui_StartPage = NULL;

static void ui_StartPage_on_draw(egui_view_t *self);
static void ui_StartPage_timer_cb(egui_timer_t *timer);

void ui_StartPage_screen_init(void)
{
    egui_view_t *view = EGUI_VIEW_OF(&s_start_page.base);

    ui_StartPage = view;
    egui_view_init(view, egui_port_get_core());
    egui_view_copy_api(view, &s_start_api);
    s_start_api.on_draw = ui_StartPage_on_draw;
    view->api = &s_start_api;
    egui_view_set_position(view, 0, 0);
    egui_view_set_size(view, UI_SCREEN_W, UI_SCREEN_H);
    egui_view_set_visible(view, 1);

    s_start_page.enter_tick = egui_timer_get_current_time();
    s_start_page.progress = 0U;
    egui_view_start_periodic(view, &s_start_page.timer, &s_start_page, ui_StartPage_timer_cb, 100U);
}

void ui_StartPage_screen_destroy(void)
{
    if (ui_StartPage != NULL)
    {
        egui_view_stop_periodic(ui_StartPage, &s_start_page.timer);
    }
}

void ui_StartPage_key_handler(void *key_event)
{
    ui_page_handle_default_key_event(key_event);
}

static void ui_StartPage_timer_cb(egui_timer_t *timer)
{
    ui_start_page_t *page = (ui_start_page_t *)timer->user_data;
    uint32_t elapsed;

    if (page == NULL)
    {
        return;
    }

    elapsed = egui_timer_get_current_time() - page->enter_tick;
    page->progress = (elapsed >= 2000U) ? 100U : (uint8_t)((elapsed * 100U) / 2000U);

    if (ui_StartPage != NULL)
    {
        egui_view_invalidate_full(ui_StartPage);
    }

    if (elapsed >= 2000U)
    {
        ui_page_t *current = ui_page_manager_get_current();

        if ((current != NULL) && (current->page_view == &ui_StartPage))
        {
            egui_timer_stop_timer(egui_port_get_core(), &page->timer);
            ui_page_manager_goto("HOME", 1U);
        }
    }
}

static void ui_StartPage_on_draw(egui_view_t *self)
{
    EGUI_LOCAL_INIT(ui_start_page_t);
    egui_canvas_t *canvas = egui_view_get_canvas(self);

    ui_draw_header(canvas, "Plain UI", "EmbeddedGUI boot sequence", 0x38BDF8);
    ui_draw_panel(canvas, 18, 48, 392, 54, 0x111827, 0x334155);
    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_16_4), "Starting system modules", 34, 58, 250, 20,
                 EGUI_ALIGN_LEFT | EGUI_ALIGN_VCENTER, 0xE2E8F0);
    ui_draw_progress(canvas, 34, 82, 360, 12, local->progress, 0x1F2937, 0x38BDF8);
    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_12_4), "Auto jump to HOME in 2s", 34, 98, 200, 16,
                 EGUI_ALIGN_LEFT | EGUI_ALIGN_VCENTER, 0x94A3B8);
    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_12_4), "Keys: N/R page switch", 270, 98, 112, 16,
                 EGUI_ALIGN_RIGHT | EGUI_ALIGN_VCENTER, 0x94A3B8);
}
