#include "ui_StartPage.h"

#include <stdio.h>
#include <string.h>

#include "core/egui_timer.h"
#include "egui_port_stm32l471_fan.h"
#include "fan_anim_res.h"
#include "page_manager.h"
#include "ui_common.h"

#define UI_START_DURATION_MS       3000U
#define UI_START_FRAME_MS            50U
#define UI_START_ENTER_MS           400U
#define UI_START_READY_MS           250U
#define UI_START_PARTICLE_COUNT       8U
#define UI_START_ORBIT_POINT_COUNT   16U
#define UI_START_PROGRESS_SEGMENTS   12U

typedef struct
{
    egui_view_t base;
    egui_timer_t timer;
    const egui_image_t **fan_frames;
    uint32_t enter_tick;
    uint32_t frame_tick;
    uint16_t fan_interval_ms;
    uint8_t fan_frame_count;
    uint8_t progress;
    uint8_t transitioning;
} ui_start_page_t;

static const uint32_t s_start_palette[] = {
    0x22D3EE,
    0x38BDF8,
    0x818CF8,
    0xC084FC,
    0xF472B6,
    0xFB7185,
    0xFBBF24,
    0x34D399,
};

static const int16_t s_particle_base_x[UI_START_PARTICLE_COUNT] = {13, 31, 52, 75, 103, 21, 67, 112};
static const int16_t s_particle_base_y[UI_START_PARTICLE_COUNT] = {14, 35, 20, 42, 27, 66, 82, 63};
static const uint8_t s_particle_size[UI_START_PARTICLE_COUNT] = {1, 2, 1, 2, 1, 1, 2, 1};

static const int8_t s_orbit_x[UI_START_ORBIT_POINT_COUNT] = {
    31, 29, 22, 12, 0, -12, -22, -29, -31, -29, -22, -12, 0, 12, 22, 29,
};
static const int8_t s_orbit_y[UI_START_ORBIT_POINT_COUNT] = {
    0, 12, 22, 29, 31, 29, 22, 12, 0, -12, -22, -29, -31, -29, -22, -12,
};

static ui_start_page_t s_start_page;
static egui_view_api_t s_start_api;

egui_view_t *ui_StartPage = NULL;

static void ui_StartPage_on_draw(egui_view_t *self);
static void ui_StartPage_timer_cb(egui_timer_t *timer);

static uint32_t ui_start_palette_color(uint32_t phase, uint8_t offset)
{
    uint8_t count = (uint8_t)(sizeof(s_start_palette) / sizeof(s_start_palette[0]));
    uint8_t index = (uint8_t)(((phase / 90U) + offset) % count);

    return s_start_palette[index];
}

static uint32_t ui_start_elapsed(const ui_start_page_t *page)
{
    uint32_t elapsed = page->frame_tick - page->enter_tick;

    return (elapsed > UI_START_DURATION_MS) ? UI_START_DURATION_MS : elapsed;
}

void ui_StartPage_screen_init(void)
{
    egui_view_t *view;

    memset(&s_start_page, 0, sizeof(s_start_page));
    view = EGUI_VIEW_OF(&s_start_page.base);
    ui_StartPage = view;

    egui_view_init(view, egui_port_get_core());
    egui_view_copy_api(view, &s_start_api);
    s_start_api.on_draw = ui_StartPage_on_draw;
    view->api = &s_start_api;
    egui_view_set_position(view, 0, 0);
    egui_view_set_size(view, UI_SCREEN_W, UI_SCREEN_H);
    egui_view_set_visible(view, 1);

    s_start_page.fan_frames = FanAnimRes_GetFrames(FAN_MODE_MANUAL,
                                                   &s_start_page.fan_frame_count,
                                                   &s_start_page.fan_interval_ms);
    s_start_page.enter_tick = egui_timer_get_current_time();
    s_start_page.frame_tick = s_start_page.enter_tick;
    egui_view_start_periodic(view, &s_start_page.timer, &s_start_page, ui_StartPage_timer_cb, UI_START_FRAME_MS);
}

void ui_StartPage_screen_destroy(void)
{
    s_start_page.transitioning = 1U;
    if (ui_StartPage != NULL)
    {
        egui_view_stop_periodic(ui_StartPage, &s_start_page.timer);
    }
}

bool ui_StartPage_key_handler(void *key_event)
{
    return ui_page_consume_nav_key_event(key_event);
}

static void ui_StartPage_timer_cb(egui_timer_t *timer)
{
    ui_start_page_t *page = (ui_start_page_t *)timer->user_data;
    uint32_t elapsed;

    if ((page == NULL) || (page->transitioning != 0U))
    {
        return;
    }

    page->frame_tick = egui_timer_get_current_time();
    elapsed = page->frame_tick - page->enter_tick;
    page->progress = (elapsed >= UI_START_DURATION_MS)
                         ? 100U
                         : (uint8_t)((elapsed * 100U) / UI_START_DURATION_MS);

    if ((ui_StartPage != NULL) && egui_view_get_visible(ui_StartPage))
    {
        egui_view_invalidate_full(ui_StartPage);
    }

    if (elapsed >= UI_START_DURATION_MS)
    {
        ui_page_t *current = ui_page_manager_get_current();

        if ((current != NULL) && (current->page_view == &ui_StartPage))
        {
            page->transitioning = 1U;
            egui_timer_stop_timer(egui_port_get_core(), &page->timer);
            ui_page_manager_goto("HOME", 1U);
        }
    }
}

static void ui_start_draw_background(egui_canvas_t *canvas, const ui_start_page_t *page)
{
    uint32_t phase = page->frame_tick - page->enter_tick;
    uint16_t scan_phase = (uint16_t)((phase / 18U) % (UI_SCREEN_W + 70U));

    ui_draw_rect(canvas, 0, 0, UI_SCREEN_W, UI_SCREEN_H, 0x07111F);
    ui_draw_rect(canvas, 0, 0, 128, UI_SCREEN_H, 0x0B1F33);
    ui_draw_rect(canvas, 126, 0, 2, UI_SCREEN_H, 0x123451);

    for (uint8_t i = 0U; i < 6U; i++)
    {
        int16_t x = (int16_t)(((scan_phase + (uint16_t)i * 86U) % (UI_SCREEN_W + 70U)) - 35);
        uint32_t color = ui_start_palette_color(phase, i);

        egui_canvas_draw_line(canvas, x, 0, x + 36, UI_SCREEN_H, 2, ui_color(color), EGUI_ALPHA_20);
    }

    for (uint8_t i = 0U; i < UI_START_PARTICLE_COUNT; i++)
    {
        int16_t drift = (int16_t)(((phase / (34U + (uint32_t)i * 4U)) + (uint32_t)i * 8U) % 30U);
        int16_t y = (int16_t)((s_particle_base_y[i] + drift) % 103);

        egui_canvas_draw_circle_fill_basic(canvas,
                                           s_particle_base_x[i],
                                           y + 5,
                                           s_particle_size[i],
                                           ui_color(ui_start_palette_color(phase, (uint8_t)(i + 2U))),
                                           EGUI_ALPHA_80);
    }
}

static void ui_start_draw_fan_core(egui_canvas_t *canvas, const ui_start_page_t *page)
{
    uint32_t elapsed = ui_start_elapsed(page);
    uint8_t pulse = (uint8_t)((elapsed / 55U) % 12U);
    uint8_t orbit_index = (uint8_t)((elapsed / 65U) % UI_START_ORBIT_POINT_COUNT);
    uint32_t accent = ui_start_palette_color(elapsed, 0U);
    uint32_t accent_alt = ui_start_palette_color(elapsed, 3U);
    uint8_t fan_index = 0U;

    if (pulse > 6U)
    {
        pulse = (uint8_t)(12U - pulse);
    }

    egui_canvas_draw_circle_fill_basic(canvas, 64, 52, 27, ui_color(0x071827), EGUI_ALPHA_100);
    egui_canvas_draw_circle_basic(canvas, 64, 52, (egui_dim_t)(31 + (pulse / 2U)), 2,
                                  ui_color(accent), EGUI_ALPHA_80);
    egui_canvas_draw_circle_basic(canvas, 64, 52, 25, 1, ui_color(0xE2E8F0), EGUI_ALPHA_40);

    for (uint8_t i = 0U; i < 3U; i++)
    {
        uint8_t index = (uint8_t)((orbit_index + (uint8_t)(i * 5U)) % UI_START_ORBIT_POINT_COUNT);
        uint32_t color = (i == 1U) ? accent_alt : ui_start_palette_color(elapsed, (uint8_t)(i * 2U));

        egui_canvas_draw_circle_fill_basic(canvas,
                                           (int16_t)(64 + s_orbit_x[index]),
                                           (int16_t)(52 + s_orbit_y[index]),
                                           (i == 0U) ? 3 : 2,
                                           ui_color(color),
                                           EGUI_ALPHA_90);
    }

    if ((page->fan_frames != NULL) && (page->fan_frame_count > 0U) && (page->fan_interval_ms > 0U))
    {
        fan_index = (uint8_t)((elapsed / page->fan_interval_ms) % page->fan_frame_count);
        egui_canvas_draw_image(canvas, page->fan_frames[fan_index], 44, 32);
    }
    else
    {
        egui_canvas_draw_line(canvas, 52, 40, 76, 64, 5, ui_color(accent), EGUI_ALPHA_100);
        egui_canvas_draw_line(canvas, 76, 40, 52, 64, 5, ui_color(accent_alt), EGUI_ALPHA_100);
        egui_canvas_draw_circle_fill_basic(canvas, 64, 52, 4, ui_color(0xF8FAFC), EGUI_ALPHA_100);
    }

    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_18_4), "FAN", 34, 87, 60, 22,
                 EGUI_ALIGN_CENTER, 0xF8FAFC);
    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_10_4), "AIR SYSTEM", 17, 111, 94, 14,
                 EGUI_ALIGN_CENTER, accent);
}

static void ui_start_draw_progress(egui_canvas_t *canvas,
                                   const ui_start_page_t *page,
                                   int16_t content_x,
                                   uint32_t accent)
{
    const int16_t segment_w = 17;
    const int16_t segment_gap = 3;
    uint8_t filled_segments = (uint8_t)(((uint16_t)page->progress * UI_START_PROGRESS_SEGMENTS + 99U) / 100U);
    char percent_text[8];

    for (uint8_t i = 0U; i < UI_START_PROGRESS_SEGMENTS; i++)
    {
        int16_t x = (int16_t)(content_x + (int16_t)i * (segment_w + segment_gap));
        uint32_t fill = (i < filled_segments) ? ui_start_palette_color(page->frame_tick - page->enter_tick, i) : 0x193047;

        egui_canvas_draw_round_rectangle_fill(canvas, x, 90, segment_w, 7, 3,
                                              ui_color(fill), (i < filled_segments) ? EGUI_ALPHA_100 : EGUI_ALPHA_80);
    }

    (void)snprintf(percent_text, sizeof(percent_text), "%u%%", (unsigned)page->progress);
    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_10_4), percent_text,
                 content_x + 241, 85, 39, 17, EGUI_ALIGN_RIGHT | EGUI_ALIGN_VCENTER, accent);
}

static void ui_start_draw_content(egui_canvas_t *canvas, const ui_start_page_t *page)
{
    uint32_t elapsed = ui_start_elapsed(page);
    uint32_t enter_progress = (elapsed >= UI_START_ENTER_MS) ? UI_START_ENTER_MS : elapsed;
    uint32_t ready_start = UI_START_DURATION_MS - UI_START_READY_MS;
    uint32_t accent = ui_start_palette_color(elapsed, 0U);
    int16_t slide = (int16_t)(34U - ((enter_progress * 34U) / UI_START_ENTER_MS));
    int16_t content_x = (int16_t)(144 + slide);
    const char *status = (elapsed >= ready_start) ? "READY" : "WELCOME";

    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_30_4), "SMART FAN",
                 content_x, 13, 264, 38, EGUI_ALIGN_LEFT | EGUI_ALIGN_VCENTER, 0xF8FAFC);
    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_10_4), "INTELLIGENT AIR CONTROL",
                 content_x + 2, 51, 220, 15, EGUI_ALIGN_LEFT | EGUI_ALIGN_VCENTER, 0x94A3B8);

    egui_canvas_draw_line(canvas, content_x, 72, content_x + 280, 72, 1, ui_color(0x1E3A50), EGUI_ALPHA_100);
    egui_canvas_draw_line(canvas, content_x, 72, content_x + (int16_t)(((uint32_t)page->progress * 280U) / 100U), 72,
                          2, ui_color(accent), EGUI_ALPHA_90);

    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_10_4), status,
                 content_x, 73, 120, 15, EGUI_ALIGN_LEFT | EGUI_ALIGN_VCENTER, accent);
    ui_start_draw_progress(canvas, page, content_x, accent);

    if (elapsed >= ready_start)
    {
        uint32_t ready_elapsed = elapsed - ready_start;
        uint8_t pulse = (uint8_t)((ready_elapsed / 45U) % 6U);
        egui_alpha_t alpha = (pulse < 3U) ? EGUI_ALPHA_20 : EGUI_ALPHA_10;

        egui_canvas_draw_round_rectangle(canvas, content_x - 4, 106, 284, 24, 5, 2,
                                         ui_color(accent), EGUI_ALPHA_80);
        egui_canvas_draw_round_rectangle_fill(canvas, content_x - 3, 107, 282, 22, 4,
                                              ui_color(accent), alpha);
        ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_12_4), "SYSTEM READY",
                     content_x + 8, 108, 258, 19, EGUI_ALIGN_CENTER, 0xF8FAFC);
    }
    else
    {
        ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_10_4), "SMART  /  QUIET  /  EFFICIENT",
                     content_x, 108, 280, 18, EGUI_ALIGN_LEFT | EGUI_ALIGN_VCENTER, 0x64748B);
    }
}

static void ui_StartPage_on_draw(egui_view_t *self)
{
    EGUI_LOCAL_INIT(ui_start_page_t);
    egui_canvas_t *canvas = egui_view_get_canvas(self);

    ui_start_draw_background(canvas, local);
    ui_start_draw_fan_core(canvas, local);
    ui_start_draw_content(canvas, local);
}
