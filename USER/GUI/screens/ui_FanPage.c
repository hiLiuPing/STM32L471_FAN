#include "ui_FanPage.h"

#include <stdio.h>

#include "core/egui_timer.h"
#include "egui_port_stm32l471_fan.h"
#include "fan_app.h"
#include "key.h"
#include "page_manager.h"
#include "ui_common.h"

typedef struct
{
    egui_view_t base;
    egui_timer_t timer;
} ui_fan_page_t;

static ui_fan_page_t s_fan_page;
static egui_view_api_t s_fan_api;

egui_view_t *ui_FanPage = NULL;

static void ui_FanPage_on_draw(egui_view_t *self);
static void ui_FanPage_timer_cb(egui_timer_t *timer);

void ui_FanPage_screen_init(void)
{
    egui_view_t *view = EGUI_VIEW_OF(&s_fan_page.base);

    ui_FanPage = view;
    egui_view_init(view, egui_port_get_core());
    egui_view_copy_api(view, &s_fan_api);
    s_fan_api.on_draw = ui_FanPage_on_draw;
    view->api = &s_fan_api;
    egui_view_set_position(view, 0, 0);
    egui_view_set_size(view, UI_SCREEN_W, UI_SCREEN_H);
    egui_view_set_visible(view, 1);
    egui_view_start_periodic(view, &s_fan_page.timer, view, ui_FanPage_timer_cb, 300U);
}

void ui_FanPage_screen_destroy(void)
{
}

void ui_FanPage_key_handler(void *key_event)
{
    const key_event_t *event = (const key_event_t *)key_event;

    if (event == NULL)
    {
        return;
    }

    if (event->type == KEY_EVT_CLICK)
    {
        fan_state_t state;

        FanApp_GetState(&state);
        if (event->id == KEY_ID_L)
        {
            (void)FanApp_SetPower(state.power_on == 0U, 0U);
            egui_view_invalidate_full(ui_FanPage);
            return;
        }
        if (event->id == KEY_ID_B)
        {
            uint8_t index = (uint8_t)((FanApp_ModeToIndex(state.mode) + 1U) % FanApp_GetModeCount());

            (void)FanApp_SetMode(FanApp_ModeFromIndex(index), 0U);
            egui_view_invalidate_full(ui_FanPage);
            return;
        }
    }

    ui_page_handle_default_key_event(key_event);
}

static void ui_FanPage_timer_cb(egui_timer_t *timer)
{
    egui_view_t *view = (egui_view_t *)timer->user_data;

    if ((view != NULL) && egui_view_get_visible(view))
    {
        egui_view_invalidate_full(view);
    }
}

static void ui_FanPage_draw_metric(egui_canvas_t *canvas,
                                   egui_dim_t x,
                                   egui_dim_t y,
                                   const char *name,
                                   const char *value,
                                   uint8_t percent,
                                   uint32_t accent_rgb)
{
    ui_draw_panel(canvas, x, y, 126, 32, 0x111827, 0x334155);
    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_12_4), name, (egui_dim_t)(x + 8), (egui_dim_t)(y + 4), 54, 13,
                 EGUI_ALIGN_LEFT | EGUI_ALIGN_VCENTER, 0xCBD5E1);
    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_12_4), value, (egui_dim_t)(x + 68), (egui_dim_t)(y + 4), 48, 13,
                 EGUI_ALIGN_RIGHT | EGUI_ALIGN_VCENTER, 0xF8FAFC);
    ui_draw_progress(canvas, (egui_dim_t)(x + 8), (egui_dim_t)(y + 22), 110, 5, percent, 0x1F2937, accent_rgb);
}

static void ui_FanPage_on_draw(egui_view_t *self)
{
    fan_state_t state;
    char value[24];
    char temp[24];
    egui_canvas_t *canvas = egui_view_get_canvas(self);

    FanApp_GetState(&state);

    ui_draw_header(canvas, "Fan", "L power  B mode  N/R switch", 0x14B8A6);

    ui_draw_panel(canvas, 18, 48, 118, 58, state.power_on ? 0x12312B : 0x2B1720, state.power_on ? 0x14B8A6 : 0xFB7185);
    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_12_4), "POWER", 30, 56, 50, 14, EGUI_ALIGN_LEFT | EGUI_ALIGN_VCENTER, 0x99F6E4);
    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_30_4), state.power_on ? "ON" : "OFF", 30, 70, 92, 30,
                 EGUI_ALIGN_LEFT | EGUI_ALIGN_VCENTER, state.power_on ? 0xF0FDFA : 0xFFE4E6);

    ui_draw_panel(canvas, 154, 48, 254, 32, 0x111827, 0x334155);
    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_12_4), "MODE", 166, 54, 46, 16, EGUI_ALIGN_LEFT | EGUI_ALIGN_VCENTER, 0x94A3B8);
    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_18_4), FanApp_GetModeName(state.mode), 218, 51, 176, 22,
                 EGUI_ALIGN_LEFT | EGUI_ALIGN_VCENTER, 0xF8FAFC);

    (void)snprintf(value, sizeof(value), "%u%%", state.current_speed_percent);
    ui_FanPage_draw_metric(canvas, 154, 88, "Speed", value, state.current_speed_percent, 0x38BDF8);
    (void)snprintf(value, sizeof(value), "%u%%", state.intensity_percent);
    ui_FanPage_draw_metric(canvas, 290, 88, "Intensity", value, state.intensity_percent, 0xA78BFA);

    (void)snprintf(temp, sizeof(temp), "%d.%dC", state.current_temp_x10 / 10, state.current_temp_x10 % 10);
    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_12_4), temp, 30, 108, 80, 14, EGUI_ALIGN_LEFT | EGUI_ALIGN_VCENTER, 0xF8FAFC);
    (void)snprintf(value, sizeof(value), "Hum %u%%", state.current_hum_percent);
    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_12_4), value, 30, 122, 80, 14, EGUI_ALIGN_LEFT | EGUI_ALIGN_VCENTER, 0xCBD5E1);
}
