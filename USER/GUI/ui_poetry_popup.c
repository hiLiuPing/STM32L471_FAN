#include "ui_poetry_popup.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/egui_timer.h"
#include "egui_port_stm32l471_fan.h"
#include "log.h"
#include "page_manager.h"
#include "poetry_app.h"
#include "ui_common.h"
#include "ui_heiti_font.h"
#include "widget/egui_view.h"

#define UI_POETRY_POPUP_TIMER_MS       50U
#define UI_POETRY_POPUP_PANEL_X        54
#define UI_POETRY_POPUP_PANEL_Y        6
#define UI_POETRY_POPUP_PANEL_W        320
#define UI_POETRY_POPUP_PANEL_H        130
#define UI_POETRY_POPUP_TEXT_PAD_X     12
#define UI_POETRY_POPUP_TEXT_PAD_Y     8
#define UI_POETRY_POPUP_FONT_SIZE      18U
#define UI_POETRY_POPUP_LINE_STEP      22
#define UI_POETRY_POPUP_FONT_H         UI_POETRY_POPUP_FONT_SIZE
#define UI_POETRY_POPUP_ANIM_MS        300U
#define UI_POETRY_POPUP_TEXT_AREA_H    (UI_POETRY_POPUP_PANEL_H - 2 * UI_POETRY_POPUP_TEXT_PAD_Y)
#define UI_POETRY_POPUP_MAX_VISIBLE_LINES \
    (((UI_POETRY_POPUP_TEXT_AREA_H - UI_POETRY_POPUP_FONT_H) / UI_POETRY_POPUP_LINE_STEP) + 1)

typedef enum
{
    UI_POETRY_POPUP_PHASE_WAITING = 0,
    UI_POETRY_POPUP_PHASE_ENTERING,
    UI_POETRY_POPUP_PHASE_VISIBLE,
    UI_POETRY_POPUP_PHASE_LEAVING
} ui_poetry_popup_phase_t;

typedef struct
{
    egui_view_t base;
    egui_timer_t timer;
    uint16_t interval_s;
    uint16_t duration_s;
    uint32_t next_show_ms;
    uint32_t shown_at_ms;
    uint32_t anim_start_ms;
    ui_poetry_popup_phase_t phase;
    int16_t panel_y;
    int16_t target_panel_y;
    int16_t hidden_panel_y;
    uint16_t text_total_h;
    uint8_t line_count;
    uint8_t poem_buf[POETRY_APP_MAX_TEXT_SIZE];
    const char *lines[POETRY_APP_MAX_POEM_LINES];
    egui_dim_t line_widths[POETRY_APP_MAX_POEM_LINES];
    char text[1024];
} ui_poetry_popup_state_t;

static ui_poetry_popup_state_t s_popup;
static egui_view_api_t s_popup_api;

static void ui_poetry_popup_timer_cb(egui_timer_t *timer);
static bool ui_poetry_popup_time_reached(uint32_t now, uint32_t target);
static bool ui_poetry_popup_prepare_text(void);
static void ui_poetry_popup_append_line(const char *line, uint16_t *pos);
static void ui_poetry_popup_show(void);
static void ui_poetry_popup_start_leave(void);
static void ui_poetry_popup_hide(void);
static uint32_t ui_poetry_popup_elapsed(uint32_t now, uint32_t since);
static int16_t ui_poetry_popup_lerp_i16(int16_t from, int16_t to, uint32_t elapsed, uint32_t duration);
static void ui_poetry_popup_invalidate_panel(egui_dim_t panel_y);
static void ui_poetry_popup_invalidate_panel_sweep(egui_dim_t old_y, egui_dim_t new_y);
static void ui_poetry_popup_collect_lines(void);
static void ui_poetry_popup_draw_lines(egui_canvas_t *canvas, int16_t panel_y);
static void ui_poetry_popup_on_draw(egui_view_t *self);

void ui_poetry_popup_init(void)
{
    egui_view_t *view = EGUI_VIEW_OF(&s_popup.base);

    memset(&s_popup, 0, sizeof(s_popup));
    s_popup.interval_s = UI_POETRY_POPUP_DEFAULT_INTERVAL_S;
    s_popup.duration_s = UI_POETRY_POPUP_DEFAULT_DURATION_S;
    s_popup.next_show_ms = egui_timer_get_current_time() + (uint32_t)s_popup.interval_s * 1000U;
    s_popup.phase = UI_POETRY_POPUP_PHASE_WAITING;
    s_popup.target_panel_y = UI_POETRY_POPUP_PANEL_Y;
    s_popup.hidden_panel_y = -UI_POETRY_POPUP_PANEL_H;
    s_popup.panel_y = s_popup.hidden_panel_y;

    egui_view_init(view, egui_port_get_core());
    egui_view_copy_api(view, &s_popup_api);
    s_popup_api.on_draw = ui_poetry_popup_on_draw;
    view->api = &s_popup_api;
    egui_view_set_position(view, 0, 0);
    egui_view_set_size(view, UI_SCREEN_W, UI_SCREEN_H);
    egui_view_set_visible(view, 0);
    egui_core_add_user_root_view(view);
    egui_view_start_periodic(view, &s_popup.timer, &s_popup, ui_poetry_popup_timer_cb, UI_POETRY_POPUP_TIMER_MS);
}

void ui_poetry_popup_set_timing(uint16_t interval_s, uint16_t duration_s)
{
    if (interval_s == 0U)
    {
        interval_s = 1U;
    }
    if (duration_s == 0U)
    {
        duration_s = 1U;
    }

    s_popup.interval_s = interval_s;
    s_popup.duration_s = duration_s;

    if (s_popup.phase == UI_POETRY_POPUP_PHASE_WAITING)
    {
        s_popup.next_show_ms = egui_timer_get_current_time() + (uint32_t)s_popup.interval_s * 1000U;
    }
}

static void ui_poetry_popup_timer_cb(egui_timer_t *timer)
{
    uint32_t now = egui_timer_get_current_time();

    (void)timer;

    if (s_popup.phase == UI_POETRY_POPUP_PHASE_WAITING)
    {
        if (ui_poetry_popup_time_reached(now, s_popup.next_show_ms))
        {
            ui_poetry_popup_show();
        }
        return;
    }

    if (s_popup.phase == UI_POETRY_POPUP_PHASE_ENTERING)
    {
        uint32_t elapsed = ui_poetry_popup_elapsed(now, s_popup.anim_start_ms);
        int16_t old_y = s_popup.panel_y;

        s_popup.panel_y = ui_poetry_popup_lerp_i16(s_popup.hidden_panel_y, s_popup.target_panel_y, elapsed, UI_POETRY_POPUP_ANIM_MS);
        if (elapsed >= UI_POETRY_POPUP_ANIM_MS)
        {
            s_popup.phase = UI_POETRY_POPUP_PHASE_VISIBLE;
            s_popup.panel_y = s_popup.target_panel_y;
        }
        if (s_popup.panel_y != old_y)
        {
            ui_poetry_popup_invalidate_panel_sweep(old_y, s_popup.panel_y);
        }
        return;
    }

    if (s_popup.phase == UI_POETRY_POPUP_PHASE_VISIBLE)
    {
        if (ui_poetry_popup_elapsed(now, s_popup.shown_at_ms) >= (uint32_t)s_popup.duration_s * 1000U)
        {
            ui_poetry_popup_start_leave();
            ui_poetry_popup_invalidate_panel(s_popup.panel_y);
        }
        return;
    }

    if (s_popup.phase == UI_POETRY_POPUP_PHASE_LEAVING)
    {
        uint32_t elapsed = ui_poetry_popup_elapsed(now, s_popup.anim_start_ms);
        int16_t old_y = s_popup.panel_y;

        s_popup.panel_y = ui_poetry_popup_lerp_i16(s_popup.target_panel_y, s_popup.hidden_panel_y, elapsed, UI_POETRY_POPUP_ANIM_MS);
        if (elapsed >= UI_POETRY_POPUP_ANIM_MS)
        {
            ui_poetry_popup_hide();
            return;
        }
        if (s_popup.panel_y != old_y)
        {
            ui_poetry_popup_invalidate_panel_sweep(old_y, s_popup.panel_y);
        }
    }
}

static bool ui_poetry_popup_time_reached(uint32_t now, uint32_t target)
{
    return ((int32_t)(now - target) >= 0);
}

static bool ui_poetry_popup_prepare_text(void)
{
    PoetryApp_Poem_t poem;
    PoetryApp_Collection_t coll;
    uint16_t pos = 0U;
    uint8_t copied_lines = 0U;
    int ret;

    if (!ui_heiti_font_is_ready(UI_POETRY_POPUP_FONT_SIZE))
    {
        (void)snprintf(s_popup.text, sizeof(s_popup.text), "Poetry font missing\n%s", ui_heiti_font_get_path(UI_POETRY_POPUP_FONT_SIZE));
        ui_poetry_popup_collect_lines();
        return true;
    }

    coll = (PoetryApp_Collection_t)(rand() % POETRY_COLL_COUNT);

    ret = PoetryApp_OpenCollection(coll);
    if (ret != POETRY_APP_OK)
    {
        log_printf("[UI_POETRY] open coll=%u ret=%d", (unsigned)coll, ret);
        (void)snprintf(s_popup.text, sizeof(s_popup.text), "Poetry data missing\ncollection %u", (unsigned)coll);
        ui_poetry_popup_collect_lines();
        return true;
    }

    ret = PoetryApp_GetRandomPoem(coll, s_popup.poem_buf, sizeof(s_popup.poem_buf), &poem);
    if (ret != POETRY_APP_OK)
    {
        log_printf("[UI_POETRY] get poem coll=%u ret=%d", (unsigned)coll, ret);
        (void)snprintf(s_popup.text, sizeof(s_popup.text), "Poetry read failed\ncollection %u", (unsigned)coll);
        ui_poetry_popup_collect_lines();
        return true;
    }

    for (uint8_t i = 0U;
         (i < poem.line_count) && (copied_lines < UI_POETRY_POPUP_MAX_VISIBLE_LINES) && (pos < sizeof(s_popup.text) - 2U);
         i++)
    {
        const char *line = poem.lines[i];

        if ((line == NULL) || (line[0] == '\0'))
        {
            continue;
        }

        ui_poetry_popup_append_line(line, &pos);
        copied_lines++;
    }

    s_popup.text[pos] = '\0';
    if (pos == 0U)
    {
        (void)snprintf(s_popup.text, sizeof(s_popup.text), "Poetry popup\ncollection %u", (unsigned)coll);
    }
    ui_poetry_popup_collect_lines();

    return true;
}

static void ui_poetry_popup_append_line(const char *line, uint16_t *pos)
{
    if ((line == NULL) || (pos == NULL) || (*pos >= sizeof(s_popup.text) - 1U))
    {
        return;
    }

    if ((*pos > 0U) && (*pos < sizeof(s_popup.text) - 1U))
    {
        s_popup.text[(*pos)++] = '\n';
    }

    while ((*line != '\0') && (*pos < sizeof(s_popup.text) - 1U))
    {
        uint8_t ch = (uint8_t)*line;
        uint8_t bytes = 1U;

        if ((ch & 0x80U) == 0U)
        {
            bytes = 1U;
        }
        else if ((ch & 0xE0U) == 0xC0U)
        {
            bytes = 2U;
        }
        else if ((ch & 0xF0U) == 0xE0U)
        {
            bytes = 3U;
        }
        else if ((ch & 0xF8U) == 0xF0U)
        {
            bytes = 4U;
        }

        if ((uint16_t)(*pos + bytes) >= sizeof(s_popup.text))
        {
            break;
        }

        for (uint8_t i = 0U; i < bytes; i++)
        {
            if (line[i] == '\0')
            {
                line += i;
                return;
            }
            s_popup.text[(*pos)++] = line[i];
        }

        line += bytes;
    }
}

static void ui_poetry_popup_show(void)
{
    uint32_t now;
    egui_view_t *view = EGUI_VIEW_OF(&s_popup.base);

    if (!ui_poetry_popup_prepare_text())
    {
        return;
    }

    now = egui_timer_get_current_time();
    s_popup.phase = UI_POETRY_POPUP_PHASE_ENTERING;
    s_popup.shown_at_ms = now;
    s_popup.anim_start_ms = now;
    s_popup.panel_y = s_popup.hidden_panel_y;
    egui_view_remove_from_user_root(view);
    egui_core_add_user_root_view(view);
    egui_view_set_visible(view, 1);
    ui_poetry_popup_invalidate_panel(s_popup.panel_y);
    log_printf("[UI_POETRY] show");
    egui_core_force_refresh(egui_port_get_core());
}

static void ui_poetry_popup_start_leave(void)
{
    s_popup.phase = UI_POETRY_POPUP_PHASE_LEAVING;
    s_popup.anim_start_ms = egui_timer_get_current_time();
    s_popup.panel_y = s_popup.target_panel_y;
}

static void ui_poetry_popup_hide(void)
{
    egui_dim_t old_y = s_popup.panel_y;

    s_popup.phase = UI_POETRY_POPUP_PHASE_WAITING;
    s_popup.next_show_ms = egui_timer_get_current_time() + (uint32_t)s_popup.interval_s * 1000U;
    s_popup.shown_at_ms = 0U;
    s_popup.anim_start_ms = 0U;
    s_popup.panel_y = s_popup.hidden_panel_y;
    ui_poetry_popup_invalidate_panel(old_y);
    egui_view_set_visible(EGUI_VIEW_OF(&s_popup.base), 0);
    log_printf("[UI_POETRY] hide");
    egui_core_force_refresh(egui_port_get_core());
}

static uint32_t ui_poetry_popup_elapsed(uint32_t now, uint32_t since)
{
    return (uint32_t)(now - since);
}

static int16_t ui_poetry_popup_lerp_i16(int16_t from, int16_t to, uint32_t elapsed, uint32_t duration)
{
    int32_t delta;

    if (elapsed >= duration)
    {
        return to;
    }

    delta = (int32_t)to - (int32_t)from;
    return (int16_t)((int32_t)from + (delta * (int32_t)elapsed) / (int32_t)duration);
}

static void ui_poetry_popup_invalidate_panel(egui_dim_t panel_y)
{
    egui_region_t dirty_region;

    dirty_region.location.x = UI_POETRY_POPUP_PANEL_X;
    dirty_region.location.y = panel_y;
    dirty_region.size.width = UI_POETRY_POPUP_PANEL_W;
    dirty_region.size.height = UI_POETRY_POPUP_PANEL_H;
    egui_view_invalidate_region(EGUI_VIEW_OF(&s_popup.base), &dirty_region);
}

static void ui_poetry_popup_invalidate_panel_sweep(egui_dim_t old_y, egui_dim_t new_y)
{
    egui_region_t dirty_region;
    egui_dim_t top = (old_y < new_y) ? old_y : new_y;
    egui_dim_t bottom_old = (egui_dim_t)(old_y + UI_POETRY_POPUP_PANEL_H);
    egui_dim_t bottom_new = (egui_dim_t)(new_y + UI_POETRY_POPUP_PANEL_H);
    egui_dim_t bottom = (bottom_old > bottom_new) ? bottom_old : bottom_new;

    dirty_region.location.x = UI_POETRY_POPUP_PANEL_X;
    dirty_region.location.y = top;
    dirty_region.size.width = UI_POETRY_POPUP_PANEL_W;
    dirty_region.size.height = (egui_dim_t)(bottom - top);
    egui_view_invalidate_region(EGUI_VIEW_OF(&s_popup.base), &dirty_region);
}

static void ui_poetry_popup_collect_lines(void)
{
    const egui_font_t *font = ui_heiti_font_get(UI_POETRY_POPUP_FONT_SIZE);
    char *line = s_popup.text;
    uint8_t count = 0U;

    while ((*line != '\0') && (count < POETRY_APP_MAX_POEM_LINES) && (count < UI_POETRY_POPUP_MAX_VISIBLE_LINES))
    {
        s_popup.lines[count++] = line;

        while ((*line != '\0') && (*line != '\n'))
        {
            line++;
        }

        if (*line == '\n')
        {
            *line++ = '\0';
        }
    }

    s_popup.line_count = count;
    s_popup.text_total_h = (count == 0U) ? 0U : (uint16_t)((uint16_t)(count - 1U) * UI_POETRY_POPUP_LINE_STEP + UI_POETRY_POPUP_FONT_H);

    for (uint8_t i = 0U; i < count; i++)
    {
        egui_dim_t line_h = 0;

        s_popup.line_widths[i] = 0;
        (void)egui_font_get_str_size_with_core(font, egui_port_get_core(), s_popup.lines[i], 0, 0, &s_popup.line_widths[i], &line_h);
    }
}

static void ui_poetry_popup_draw_lines(egui_canvas_t *canvas, int16_t panel_y)
{
    const egui_font_t *font = ui_heiti_font_get(UI_POETRY_POPUP_FONT_SIZE);
    egui_dim_t text_x = UI_POETRY_POPUP_PANEL_X + UI_POETRY_POPUP_TEXT_PAD_X;
    egui_dim_t text_y = (egui_dim_t)(panel_y + UI_POETRY_POPUP_TEXT_PAD_Y);
    egui_dim_t text_w = UI_POETRY_POPUP_PANEL_W - 2 * UI_POETRY_POPUP_TEXT_PAD_X;
    egui_dim_t text_h = UI_POETRY_POPUP_PANEL_H - 2 * UI_POETRY_POPUP_TEXT_PAD_Y;
    egui_dim_t line_h = UI_POETRY_POPUP_FONT_H;
    egui_dim_t start_y;
    egui_region_t text_clip;
    egui_region_t prev_work;
    egui_region_t clipped_work;
    egui_region_t *work_region;

    if (s_popup.line_count == 0U)
    {
        return;
    }

    if (s_popup.text_total_h < text_h)
    {
        start_y = (egui_dim_t)(text_y + (text_h - s_popup.text_total_h) / 2U);
    }
    else
    {
        start_y = text_y;
    }

    text_clip.location.x = text_x;
    text_clip.location.y = text_y;
    text_clip.size.width = text_w;
    text_clip.size.height = text_h;
    work_region = egui_canvas_get_base_view_work_region(canvas);
    prev_work = *work_region;
    egui_region_intersect(&prev_work, &text_clip, &clipped_work);
    *work_region = clipped_work;

    for (uint8_t i = 0U; i < s_popup.line_count; i++)
    {
        egui_dim_t line_y = (egui_dim_t)(start_y + (egui_dim_t)i * UI_POETRY_POPUP_LINE_STEP);
        egui_dim_t line_w = s_popup.line_widths[i];
        egui_dim_t line_x;

        if (((egui_dim_t)(line_y + line_h) <= text_y) || (line_y >= (egui_dim_t)(text_y + text_h)))
        {
            continue;
        }

        if (line_w < text_w)
        {
            line_x = (egui_dim_t)(text_x + (text_w - line_w) / 2U);
        }
        else
        {
            line_x = text_x;
        }

        egui_canvas_draw_text(canvas, font, s_popup.lines[i], line_x, line_y, ui_color(0xE2E8F0), EGUI_ALPHA_100);
    }

    *work_region = prev_work;
}

static void ui_poetry_popup_on_draw(egui_view_t *self)
{
    egui_canvas_t *canvas = egui_view_get_canvas(self);

    if (s_popup.phase == UI_POETRY_POPUP_PHASE_WAITING)
    {
        return;
    }

    ui_draw_panel(canvas, UI_POETRY_POPUP_PANEL_X, s_popup.panel_y, UI_POETRY_POPUP_PANEL_W, UI_POETRY_POPUP_PANEL_H, 0x111827, 0x93C5FD);
    ui_poetry_popup_draw_lines(canvas, s_popup.panel_y);
}
