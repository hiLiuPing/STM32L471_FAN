#include "ui_shutdown_popup.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/egui_timer.h"
#include "egui_port_stm32l471_fan.h"
#include "key.h"
#include "system_power.h"
#include "ui_common.h"
#include "ui_poetry_popup.h"
#include "ui_system_popup.h"
#include "widget/egui_view.h"

#define UI_SHUTDOWN_POPUP_TIMER_MS       50U
#define UI_SHUTDOWN_POPUP_CANCEL_MS      200U
#define UI_SHUTDOWN_POPUP_PANEL_X        54
#define UI_SHUTDOWN_POPUP_PANEL_Y        0
#define UI_SHUTDOWN_POPUP_PANEL_W        320
#define UI_SHUTDOWN_POPUP_PANEL_H        UI_SCREEN_H
#define UI_SHUTDOWN_POPUP_CORE_X         (UI_SHUTDOWN_POPUP_PANEL_X + 78)
#define UI_SHUTDOWN_POPUP_CORE_Y         66
#define UI_SHUTDOWN_POPUP_PARTICLE_COUNT 10U

typedef enum
{
    UI_SHUTDOWN_PHASE_IDLE = 0,
    UI_SHUTDOWN_PHASE_HOLDING,
    UI_SHUTDOWN_PHASE_CANCELING,
    UI_SHUTDOWN_PHASE_COMMITTED
} ui_shutdown_phase_t;

typedef struct
{
    egui_view_t base;
    egui_timer_t timer;
    ui_shutdown_phase_t phase;
    uint32_t shown_at_ms;
    uint32_t cancel_at_ms;
    uint8_t progress;
    uint8_t cancel_start_progress;
    uint8_t frame;
} ui_shutdown_popup_state_t;

static const int8_t s_particle_dx[UI_SHUTDOWN_POPUP_PARTICLE_COUNT] = {
    -62, -52, -42, -24, 8, 28, 48, 62, 54, -56,
};
static const int8_t s_particle_dy[UI_SHUTDOWN_POPUP_PARTICLE_COUNT] = {
    -36, 28, -48, 50, -54, 44, -32, 16, 40, -8,
};

static ui_shutdown_popup_state_t s_popup;
static egui_view_api_t s_popup_api;

static void ui_shutdown_popup_timer_cb(egui_timer_t *timer);
static void ui_shutdown_popup_on_draw(egui_view_t *self);
static void ui_shutdown_popup_hide(void);
static void ui_shutdown_popup_begin_cancel(uint32_t now);
static void ui_shutdown_popup_draw_particles(egui_canvas_t *canvas);
static void ui_shutdown_popup_draw_core(egui_canvas_t *canvas);

void ui_shutdown_popup_init(void)
{
    egui_view_t *view = EGUI_VIEW_OF(&s_popup.base);

    memset(&s_popup, 0, sizeof(s_popup));
    egui_view_init(view, egui_port_get_core());
    egui_view_copy_api(view, &s_popup_api);
    s_popup_api.on_draw = ui_shutdown_popup_on_draw;
    view->api = &s_popup_api;
    egui_view_set_position(view, 0, 0);
    egui_view_set_size(view, UI_SCREEN_W, UI_SCREEN_H);
    egui_view_set_visible(view, 0);
    egui_core_add_user_root_view(view);
    egui_view_start_periodic(view, &s_popup.timer, &s_popup,
                             ui_shutdown_popup_timer_cb,
                             UI_SHUTDOWN_POPUP_TIMER_MS);
}

void ui_shutdown_popup_start(void)
{
    egui_view_t *view = EGUI_VIEW_OF(&s_popup.base);

    if (s_popup.phase != UI_SHUTDOWN_PHASE_IDLE)
    {
        return;
    }

    ui_poetry_popup_dismiss();
    ui_system_popup_dismiss_immediate();
    s_popup.phase = UI_SHUTDOWN_PHASE_HOLDING;
    s_popup.shown_at_ms = egui_timer_get_current_time();
    s_popup.cancel_at_ms = 0U;
    s_popup.progress = 0U;
    s_popup.cancel_start_progress = 0U;
    s_popup.frame = 0U;

    egui_view_remove_from_user_root(view);
    egui_core_add_user_root_view(view);
    egui_view_set_visible(view, 1);
    egui_view_invalidate_full(view);
    egui_core_force_refresh(egui_port_get_core());
}

bool ui_shutdown_popup_is_active(void)
{
    return (s_popup.phase != UI_SHUTDOWN_PHASE_IDLE);
}

static void ui_shutdown_popup_timer_cb(egui_timer_t *timer)
{
    uint32_t now;
    uint32_t elapsed;

    (void)timer;

    if (s_popup.phase == UI_SHUTDOWN_PHASE_IDLE)
    {
        return;
    }

    now = egui_timer_get_current_time();
    s_popup.frame++;

    if (s_popup.phase == UI_SHUTDOWN_PHASE_HOLDING)
    {
        if ((Key_GetPressedMask() & KEY_BIT(KEY_ID_PWR)) == 0U)
        {
            ui_shutdown_popup_begin_cancel(now);
        }
        else
        {
            elapsed = (uint32_t)(now - s_popup.shown_at_ms);
            if (elapsed >= UI_SHUTDOWN_POPUP_CONFIRM_MS)
            {
                s_popup.progress = 100U;
                s_popup.phase = UI_SHUTDOWN_PHASE_COMMITTED;
                egui_view_invalidate_full(EGUI_VIEW_OF(&s_popup.base));
                egui_core_force_refresh(egui_port_get_core());
                SystemPower_RequestShutdown();
                return;
            }

            s_popup.progress = (uint8_t)((elapsed * 100U) / UI_SHUTDOWN_POPUP_CONFIRM_MS);
        }
    }
    else if (s_popup.phase == UI_SHUTDOWN_PHASE_CANCELING)
    {
        elapsed = (uint32_t)(now - s_popup.cancel_at_ms);
        if (elapsed >= UI_SHUTDOWN_POPUP_CANCEL_MS)
        {
            ui_shutdown_popup_hide();
            return;
        }

        s_popup.progress = (uint8_t)(((uint32_t)s_popup.cancel_start_progress *
                                      (UI_SHUTDOWN_POPUP_CANCEL_MS - elapsed)) /
                                     UI_SHUTDOWN_POPUP_CANCEL_MS);
    }

    egui_view_invalidate_full(EGUI_VIEW_OF(&s_popup.base));
}

static void ui_shutdown_popup_begin_cancel(uint32_t now)
{
    if (s_popup.phase != UI_SHUTDOWN_PHASE_HOLDING)
    {
        return;
    }

    s_popup.phase = UI_SHUTDOWN_PHASE_CANCELING;
    s_popup.cancel_at_ms = now;
    s_popup.cancel_start_progress = s_popup.progress;
}

static void ui_shutdown_popup_hide(void)
{
    egui_view_t *view = EGUI_VIEW_OF(&s_popup.base);

    s_popup.phase = UI_SHUTDOWN_PHASE_IDLE;
    s_popup.shown_at_ms = 0U;
    s_popup.cancel_at_ms = 0U;
    s_popup.progress = 0U;
    s_popup.cancel_start_progress = 0U;
    s_popup.frame = 0U;
    egui_view_set_visible(view, 0);
    egui_view_invalidate_full(view);
    egui_core_force_refresh(egui_port_get_core());
}

static void ui_shutdown_popup_draw_particles(egui_canvas_t *canvas)
{
    uint8_t remaining = (uint8_t)(100U - s_popup.progress);

    for (uint8_t i = 0U; i < UI_SHUTDOWN_POPUP_PARTICLE_COUNT; i++)
    {
        int16_t drift = (int16_t)((int16_t)((uint16_t)(s_popup.frame + i * 3U) % 9U) - 4);
        int16_t x = UI_SHUTDOWN_POPUP_CORE_X +
                    ((int16_t)s_particle_dx[i] * remaining) / 100 + drift;
        int16_t y = UI_SHUTDOWN_POPUP_CORE_Y +
                    ((int16_t)s_particle_dy[i] * remaining) / 100;
        uint32_t color = ((i & 1U) == 0U) ? 0x22D3EE : 0xFBBF24;
        egui_dim_t radius = (egui_dim_t)(1U + ((s_popup.frame + i) & 1U));

        egui_canvas_draw_circle_fill_basic(canvas, x, y, radius,
                                           ui_color(color), EGUI_ALPHA_80);
    }
}

static void ui_shutdown_popup_draw_core(egui_canvas_t *canvas)
{
    int16_t rotation = (int16_t)((uint16_t)s_popup.frame * 13U % 360U);
    egui_dim_t pulse = (egui_dim_t)((s_popup.frame / 2U) % 4U);

    egui_canvas_draw_circle_fill_basic(canvas,
                                       UI_SHUTDOWN_POPUP_CORE_X,
                                       UI_SHUTDOWN_POPUP_CORE_Y,
                                       (egui_dim_t)(22 + pulse),
                                       ui_color(0x082F49), EGUI_ALPHA_90);
    egui_canvas_draw_circle_basic(canvas,
                                  UI_SHUTDOWN_POPUP_CORE_X,
                                  UI_SHUTDOWN_POPUP_CORE_Y,
                                  (egui_dim_t)(27 + pulse), 1,
                                  ui_color(0x22D3EE), EGUI_ALPHA_40);

    for (uint8_t i = 0U; i < 3U; i++)
    {
        egui_canvas_draw_arc_sweep(canvas,
                                   UI_SHUTDOWN_POPUP_CORE_X,
                                   UI_SHUTDOWN_POPUP_CORE_Y,
                                   34,
                                   (int16_t)(rotation + i * 120),
                                   52, 2,
                                   ui_color(0x22D3EE), EGUI_ALPHA_90);
        egui_canvas_draw_arc_sweep(canvas,
                                   UI_SHUTDOWN_POPUP_CORE_X,
                                   UI_SHUTDOWN_POPUP_CORE_Y,
                                   40,
                                   (int16_t)(-rotation + i * 120),
                                   34, 1,
                                   ui_color(0xFBBF24), EGUI_ALPHA_80);
    }

    egui_canvas_draw_arc_sweep(canvas,
                               UI_SHUTDOWN_POPUP_CORE_X,
                               UI_SHUTDOWN_POPUP_CORE_Y,
                               12, -48, 276, 3,
                               ui_color(0xF8FAFC), EGUI_ALPHA_100);
    egui_canvas_draw_line(canvas,
                          UI_SHUTDOWN_POPUP_CORE_X,
                          UI_SHUTDOWN_POPUP_CORE_Y - 16,
                          UI_SHUTDOWN_POPUP_CORE_X,
                          UI_SHUTDOWN_POPUP_CORE_Y - 1,
                          3, ui_color(0xF8FAFC), EGUI_ALPHA_100);
}

static void ui_shutdown_popup_on_draw(egui_view_t *self)
{
    egui_canvas_t *canvas;
    char percent_text[8];
    int16_t beam_x;
    uint32_t status_color;

    if (s_popup.phase == UI_SHUTDOWN_PHASE_IDLE)
    {
        return;
    }

    canvas = egui_view_get_canvas(self);
    beam_x = (int16_t)(UI_SHUTDOWN_POPUP_PANEL_X +
                       ((uint16_t)s_popup.frame * 11U) % UI_SHUTDOWN_POPUP_PANEL_W);
    status_color = (s_popup.phase == UI_SHUTDOWN_PHASE_CANCELING) ? 0xFBBF24 : 0x22D3EE;

    ui_draw_round_panel(canvas,
                        UI_SHUTDOWN_POPUP_PANEL_X,
                        UI_SHUTDOWN_POPUP_PANEL_Y,
                        UI_SHUTDOWN_POPUP_PANEL_W,
                        UI_SHUTDOWN_POPUP_PANEL_H,
                        8, 0x030712, 0x164E63);
    egui_canvas_draw_line(canvas, beam_x, 5, beam_x, UI_SHUTDOWN_POPUP_PANEL_H - 6,
                          1, ui_color(0x22D3EE), EGUI_ALPHA_20);

    ui_shutdown_popup_draw_particles(canvas);
    ui_shutdown_popup_draw_core(canvas);

    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_18_4),
                 "POWERING DOWN", 182, 20, 174, 24,
                 EGUI_ALIGN_LEFT | EGUI_ALIGN_VCENTER, 0xF8FAFC);
    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_10_4),
                 (s_popup.phase == UI_SHUTDOWN_PHASE_CANCELING) ? "CANCELING" : "KEEP HOLDING",
                 182, 47, 116, 16,
                 EGUI_ALIGN_LEFT | EGUI_ALIGN_VCENTER, status_color);

    (void)snprintf(percent_text, sizeof(percent_text), "%u%%", (unsigned)s_popup.progress);
    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_14_4),
                 percent_text, 310, 45, 48, 19,
                 EGUI_ALIGN_RIGHT | EGUI_ALIGN_VCENTER, 0xE2E8F0);

    ui_draw_progress(canvas, 182, 76, 174, 8, s_popup.progress,
                     0x164E63, status_color);
    egui_canvas_draw_line(canvas, 182, 91, 356, 91, 1,
                          ui_color(0x164E63), EGUI_ALPHA_80);
    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_10_4),
                 "FAN SAFE STOP  /  SYSTEM OFF", 182, 99, 174, 17,
                 EGUI_ALIGN_LEFT | EGUI_ALIGN_VCENTER, 0x94A3B8);
}
