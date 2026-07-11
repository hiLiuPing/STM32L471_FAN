#include "ui_SettingPage.h"

#include <stdio.h>

#include "FreeRTOS.h"
#include "core/egui_timer.h"
#include "egui_port_stm32l471_fan.h"
#include "page_manager.h"
#include "ui_common.h"

typedef struct
{
    egui_view_t base;
    egui_timer_t timer;
} ui_setting_page_t;

static ui_setting_page_t s_setting_page;
static egui_view_api_t s_setting_api;

egui_view_t *ui_SettingPage = NULL;

static void ui_SettingPage_on_draw(egui_view_t *self);
static void ui_SettingPage_timer_cb(egui_timer_t *timer);

void ui_SettingPage_screen_init(void)
{
    egui_view_t *view = EGUI_VIEW_OF(&s_setting_page.base);

    ui_SettingPage = view;
    egui_view_init(view, egui_port_get_core());
    egui_view_copy_api(view, &s_setting_api);
    s_setting_api.on_draw = ui_SettingPage_on_draw;
    view->api = &s_setting_api;
    egui_view_set_position(view, 0, 0);
    egui_view_set_size(view, UI_SCREEN_W, UI_SCREEN_H);
    egui_view_set_visible(view, 1);
    egui_view_start_periodic(view, &s_setting_page.timer, view, ui_SettingPage_timer_cb, 2000U);
}

void ui_SettingPage_screen_destroy(void)
{
}

bool ui_SettingPage_key_handler(void *key_event)
{
    return ui_page_consume_nav_key_event(key_event);
}

static void ui_SettingPage_timer_cb(egui_timer_t *timer)
{
    egui_view_t *view = (egui_view_t *)timer->user_data;

    if ((view != NULL) && egui_view_get_visible(view))
    {
        egui_view_invalidate_full(view);
    }
}

static void ui_SettingPage_on_draw(egui_view_t *self)
{
    char line[48];
    egui_canvas_t *canvas = egui_view_get_canvas(self);

    ui_draw_header(canvas, "Setting", "N/R switch pages", 0xEAB308);

    ui_draw_panel(canvas, 18, 48, 188, 72, 0x19180D, 0xEAB308);
    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_16_4), "Display Stack", 30, 56, 150, 20,
                 EGUI_ALIGN_LEFT | EGUI_ALIGN_VCENTER, 0xFEF9C3);
    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_12_4), "EmbeddedGUI RGB565", 30, 80, 150, 14,
                 EGUI_ALIGN_LEFT | EGUI_ALIGN_VCENTER, 0xFDE68A);
    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_12_4), "LCD 428x142  PFB 428x10", 30, 96, 160, 14,
                 EGUI_ALIGN_LEFT | EGUI_ALIGN_VCENTER, 0xFDE68A);

    ui_draw_panel(canvas, 224, 48, 184, 72, 0x111827, 0x334155);
    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_16_4), "Runtime", 236, 56, 132, 20,
                 EGUI_ALIGN_LEFT | EGUI_ALIGN_VCENTER, 0xF8FAFC);
    (void)snprintf(line, sizeof(line), "Heap %lu bytes", (unsigned long)xPortGetFreeHeapSize());
    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_12_4), line, 236, 80, 146, 14, EGUI_ALIGN_LEFT | EGUI_ALIGN_VCENTER, 0xCBD5E1);
    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_12_4), "Poetry popup: Heiti 16", 236, 96, 146, 14,
                 EGUI_ALIGN_LEFT | EGUI_ALIGN_VCENTER, 0xCBD5E1);
}
