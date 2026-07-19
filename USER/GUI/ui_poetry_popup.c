#include "ui_poetry_popup.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "core/egui_timer.h"
#include "egui_port_stm32l471_fan.h"
#include "log.h"
#include "page_manager.h"
#include "poetry_app.h"
#include "screens/ui_HomePage.h"
#include "ui_common.h"
#include "ui_heiti_font.h"
#include "ui_shutdown_popup.h"
#include "ui_system_popup.h"
#include "widget/egui_view.h"

#ifndef UI_POETRY_POPUP_PAUSE_HOME_ANIM
#define UI_POETRY_POPUP_PAUSE_HOME_ANIM 1
#endif

#define UI_POETRY_POPUP_TIMER_MS       50U
#define UI_POETRY_POPUP_COLLECTION     POETRY_COLL_SONG_3000
#define UI_POETRY_POPUP_PANEL_X        54
#define UI_POETRY_POPUP_PANEL_Y        0
#define UI_POETRY_POPUP_PANEL_W        320
#define UI_POETRY_POPUP_PANEL_H        UI_SCREEN_H
#define UI_POETRY_POPUP_TEXT_PAD_X     12
#define UI_POETRY_POPUP_TEXT_PAD_Y     6
#define UI_POETRY_POPUP_FONT_SIZE      18U
#define UI_POETRY_POPUP_LINE_STEP      22
#define UI_POETRY_POPUP_FONT_H         UI_POETRY_POPUP_FONT_SIZE
#define UI_POETRY_POPUP_FONT_TOP_GUARD 4
#define UI_POETRY_POPUP_DISPLAY_W      (UI_POETRY_POPUP_PANEL_W - 2 * UI_POETRY_POPUP_TEXT_PAD_X)
#define UI_POETRY_POPUP_DISPLAY_H      (UI_POETRY_POPUP_PANEL_H - 2 * UI_POETRY_POPUP_TEXT_PAD_Y)
#define UI_POETRY_POPUP_TITLE_LINE_H   UI_POETRY_POPUP_LINE_STEP
#define UI_POETRY_POPUP_TITLE_BODY_GAP 2
#define UI_POETRY_POPUP_BODY_W         UI_POETRY_POPUP_DISPLAY_W
#define UI_POETRY_POPUP_BODY_H \
    (UI_POETRY_POPUP_DISPLAY_H - UI_POETRY_POPUP_TITLE_LINE_H - UI_POETRY_POPUP_TITLE_BODY_GAP)
#define UI_POETRY_POPUP_BODY_INDENT_SPACES 2U
#define UI_POETRY_POPUP_MAX_LAYOUT_LINES   96U
#define UI_POETRY_POPUP_TITLE_TEXT_SIZE    128U
#define UI_POETRY_POPUP_PREPARE_RETRIES    24U

typedef enum
{
    UI_POETRY_POPUP_PHASE_WAITING = 0,
    UI_POETRY_POPUP_PHASE_VISIBLE
} ui_poetry_popup_phase_t;

typedef struct
{
    egui_view_t base;
    egui_timer_t timer;
    uint16_t interval_s;
    uint16_t duration_s;
    uint8_t enabled;
    uint32_t next_show_ms;
    uint32_t shown_at_ms;
    ui_poetry_popup_phase_t phase;
    int16_t panel_y;
    int16_t target_panel_y;
    uint16_t body_total_h;
    uint8_t line_count;
    uint8_t text_ready;
    egui_dim_t title_width;
    egui_dim_t body_indent_w;
    uint8_t home_anim_was_enabled;
    uint8_t poem_buf[POETRY_APP_MAX_TEXT_SIZE];
    const char *lines[UI_POETRY_POPUP_MAX_LAYOUT_LINES];
    char title[UI_POETRY_POPUP_TITLE_TEXT_SIZE];
    char text[POETRY_APP_MAX_TEXT_SIZE];
} ui_poetry_popup_state_t;

static ui_poetry_popup_state_t s_popup;
static egui_view_api_t s_popup_api;

static void ui_poetry_popup_timer_cb(egui_timer_t *timer);
static bool ui_poetry_popup_time_reached(uint32_t now, uint32_t target);
static bool ui_poetry_popup_prepare_text(void);
static const char *ui_poetry_popup_find_title(const PoetryApp_Poem_t *poem, uint8_t *title_idx);
static void ui_poetry_popup_copy_utf8_text(char *dst, uint16_t dst_size, const char *src);
static uint8_t ui_poetry_popup_utf8_bytes(const char *text);
static egui_dim_t ui_poetry_popup_estimate_glyph_width(uint8_t bytes);
static egui_dim_t ui_poetry_popup_estimate_text_width(const char *text);
static egui_dim_t ui_poetry_popup_measure_indent(void);
static void ui_poetry_popup_layout_body(const PoetryApp_Poem_t *poem, uint8_t body_start_idx);
static bool ui_poetry_popup_is_home_active(void);
static void ui_poetry_popup_show(void);
static void ui_poetry_popup_hide(void);
static uint32_t ui_poetry_popup_elapsed(uint32_t now, uint32_t since);
static void ui_poetry_popup_invalidate_panel(egui_dim_t panel_y);
static void ui_poetry_popup_collect_lines(void);
static void ui_poetry_popup_draw_text(egui_canvas_t *canvas, int16_t panel_y);
static void ui_poetry_popup_on_draw(egui_view_t *self);

void ui_poetry_popup_init(void)
{
    egui_view_t *view = EGUI_VIEW_OF(&s_popup.base);

    memset(&s_popup, 0, sizeof(s_popup));
    s_popup.interval_s = UI_POETRY_POPUP_DEFAULT_INTERVAL_S;
    s_popup.duration_s = UI_POETRY_POPUP_DEFAULT_DURATION_S;
    s_popup.enabled = 1U;
    s_popup.next_show_ms = egui_timer_get_current_time() + (uint32_t)s_popup.interval_s * 1000U;
    s_popup.phase = UI_POETRY_POPUP_PHASE_WAITING;
    s_popup.target_panel_y = UI_POETRY_POPUP_PANEL_Y;
    s_popup.panel_y = s_popup.target_panel_y;

    egui_view_init(view, egui_port_get_core());
    egui_view_copy_api(view, &s_popup_api);
    s_popup_api.on_draw = ui_poetry_popup_on_draw;
    view->api = &s_popup_api;
    egui_view_set_position(view, 0, 0);
    egui_view_set_size(view, UI_SCREEN_W, UI_SCREEN_H);
    egui_view_set_visible(view, 0);
    egui_core_add_user_root_view(view);
    egui_view_start_periodic(view, &s_popup.timer, &s_popup, ui_poetry_popup_timer_cb, UI_POETRY_POPUP_TIMER_MS);
    (void)ui_poetry_popup_prepare_text();
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

void ui_poetry_popup_set_enabled(bool enabled)
{
    uint8_t next = enabled ? 1U : 0U;

    if (s_popup.enabled == next)
    {
        return;
    }

    s_popup.enabled = next;
    if (s_popup.enabled != 0U)
    {
        if (s_popup.phase == UI_POETRY_POPUP_PHASE_WAITING)
        {
            s_popup.next_show_ms = egui_timer_get_current_time() + (uint32_t)s_popup.interval_s * 1000U;
        }
        return;
    }

    if (s_popup.phase != UI_POETRY_POPUP_PHASE_WAITING)
    {
        ui_poetry_popup_hide();
    }
    else
    {
        egui_view_set_visible(EGUI_VIEW_OF(&s_popup.base), 0);
    }
}

bool ui_poetry_popup_is_visible(void)
{
    return (s_popup.phase == UI_POETRY_POPUP_PHASE_VISIBLE);
}

void ui_poetry_popup_dismiss(void)
{
    if (ui_poetry_popup_is_visible())
    {
        ui_poetry_popup_hide();
    }
}

bool ui_poetry_popup_refresh_cached(void)
{
    s_popup.text_ready = 0U;
    return ui_poetry_popup_prepare_text();
}

bool ui_poetry_popup_draw_cached(egui_canvas_t *canvas)
{
    if ((canvas == NULL) || (s_popup.text_ready == 0U))
    {
        return false;
    }

    ui_poetry_popup_draw_text(canvas, UI_POETRY_POPUP_PANEL_Y);
    return true;
}

static void ui_poetry_popup_timer_cb(egui_timer_t *timer)
{
    uint32_t now = egui_timer_get_current_time();

    (void)timer;

    if (ui_shutdown_popup_is_active() || ui_system_popup_is_visible())
    {
        if (s_popup.phase == UI_POETRY_POPUP_PHASE_VISIBLE)
        {
            ui_poetry_popup_hide();
        }
        else
        {
            s_popup.next_show_ms = now + (uint32_t)s_popup.interval_s * 1000U;
        }
        return;
    }

    if (s_popup.enabled == 0U)
    {
        return;
    }

    if (s_popup.phase == UI_POETRY_POPUP_PHASE_WAITING)
    {
        if (ui_poetry_popup_time_reached(now, s_popup.next_show_ms))
        {
            if (ui_poetry_popup_is_home_active())
            {
                ui_poetry_popup_show();
            }
            else
            {
                s_popup.next_show_ms = now + (uint32_t)s_popup.interval_s * 1000U;
            }
        }
        return;
    }

    if (!ui_poetry_popup_is_home_active())
    {
        ui_poetry_popup_hide();
        return;
    }

    if (s_popup.phase == UI_POETRY_POPUP_PHASE_VISIBLE)
    {
        if (ui_poetry_popup_elapsed(now, s_popup.shown_at_ms) >= (uint32_t)s_popup.duration_s * 1000U)
        {
            ui_poetry_popup_hide();
        }
        return;
    }
}

static bool ui_poetry_popup_time_reached(uint32_t now, uint32_t target)
{
    return ((int32_t)(now - target) >= 0);
}

static bool ui_poetry_popup_prepare_text(void)
{
    PoetryApp_Collection_t coll = UI_POETRY_POPUP_COLLECTION;
    int ret;

    if (s_popup.text_ready != 0U)
    {
        return true;
    }

    if (!ui_heiti_font_is_ready(UI_POETRY_POPUP_FONT_SIZE))
    {
        log_printf("[UI_POETRY] font missing %s", ui_heiti_font_get_path(UI_POETRY_POPUP_FONT_SIZE));
        return false;
    }

    ret = PoetryApp_OpenCollection(coll);
    if (ret != POETRY_APP_OK)
    {
        log_printf("[UI_POETRY] open coll=%u ret=%d", (unsigned)coll, ret);
        return false;
    }

    for (uint8_t retry = 0U; retry < UI_POETRY_POPUP_PREPARE_RETRIES; retry++)
    {
        PoetryApp_Poem_t poem;
        const char *title;
        uint8_t title_idx = 0U;

        s_popup.title[0] = '\0';
        s_popup.text[0] = '\0';
        s_popup.line_count = 0U;
        s_popup.title_width = 0;
        s_popup.body_indent_w = 0;
        s_popup.body_total_h = 0U;

        ret = PoetryApp_GetRandomPoem(coll, s_popup.poem_buf, sizeof(s_popup.poem_buf), &poem);
        if (ret != POETRY_APP_OK)
        {
            continue;
        }

        title = ui_poetry_popup_find_title(&poem, &title_idx);
        if (title != NULL)
        {
            ui_poetry_popup_copy_utf8_text(s_popup.title, sizeof(s_popup.title), title);
            ui_poetry_popup_layout_body(&poem, (uint8_t)(title_idx + 1U));
        }

        if ((s_popup.title[0] == '\0') || (s_popup.text[0] == '\0'))
        {
            continue;
        }

        ui_poetry_popup_collect_lines();
        if ((s_popup.line_count != 0U) &&
            (s_popup.title_width <= UI_POETRY_POPUP_DISPLAY_W) &&
            (s_popup.body_total_h <= UI_POETRY_POPUP_BODY_H))
        {
            s_popup.text_ready = 1U;
            return true;
        }
    }

    log_printf("[UI_POETRY] no fitting poem coll=%u", (unsigned)coll);
    return false;
}

static const char *ui_poetry_popup_find_title(const PoetryApp_Poem_t *poem, uint8_t *title_idx)
{
    if ((poem == NULL) || (title_idx == NULL))
    {
        return NULL;
    }

    for (uint8_t i = 0U; i < poem->line_count; i++)
    {
        const char *line = poem->lines[i];

        if (line == NULL)
        {
            continue;
        }

        while ((*line == ' ') || (*line == '\t'))
        {
            line++;
        }

        if (*line != '\0')
        {
            *title_idx = i;
            return line;
        }
    }

    return NULL;
}

static void ui_poetry_popup_copy_utf8_text(char *dst, uint16_t dst_size, const char *src)
{
    uint16_t pos = 0U;

    if ((dst == NULL) || (dst_size == 0U))
    {
        return;
    }

    if (src == NULL)
    {
        dst[0] = '\0';
        return;
    }

    while ((*src != '\0') && (pos < (uint16_t)(dst_size - 1U)))
    {
        uint8_t bytes = ui_poetry_popup_utf8_bytes(src);

        if ((uint16_t)(pos + bytes) >= dst_size)
        {
            break;
        }

        for (uint8_t i = 0U; i < bytes; i++)
        {
            dst[pos++] = src[i];
        }
        src += bytes;
    }

    dst[pos] = '\0';
}

static uint8_t ui_poetry_popup_utf8_bytes(const char *text)
{
    uint8_t ch;
    uint8_t bytes = 1U;

    if ((text == NULL) || (text[0] == '\0'))
    {
        return 0U;
    }

    ch = (uint8_t)text[0];
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

    for (uint8_t i = 1U; i < bytes; i++)
    {
        if (text[i] == '\0')
        {
            return i;
        }
    }

    return bytes;
}

static egui_dim_t ui_poetry_popup_estimate_glyph_width(uint8_t bytes)
{
    if (bytes == 0U)
    {
        return 0;
    }

    if (bytes == 1U)
    {
        return (egui_dim_t)(UI_POETRY_POPUP_FONT_SIZE / 2U);
    }

    return UI_POETRY_POPUP_FONT_SIZE;
}

static egui_dim_t ui_poetry_popup_estimate_text_width(const char *text)
{
    egui_dim_t width = 0;

    if (text == NULL)
    {
        return 0;
    }

    while (*text != '\0')
    {
        uint8_t bytes = ui_poetry_popup_utf8_bytes(text);

        if (bytes == 0U)
        {
            break;
        }
        width = (egui_dim_t)(width + ui_poetry_popup_estimate_glyph_width(bytes));
        text += bytes;
    }

    return width;
}

static egui_dim_t ui_poetry_popup_measure_indent(void)
{
    egui_dim_t indent = 0;

    for (uint8_t i = 0U; i < UI_POETRY_POPUP_BODY_INDENT_SPACES; i++)
    {
        indent = (egui_dim_t)(indent + ui_poetry_popup_estimate_glyph_width(1U));
    }

    return indent;
}

static void ui_poetry_popup_layout_body(const PoetryApp_Poem_t *poem, uint8_t body_start_idx)
{
    uint16_t pos = 0U;
    uint8_t line_idx = 0U;
    egui_dim_t line_width = 0;
    egui_dim_t line_limit;

    if (poem == NULL)
    {
        return;
    }

    s_popup.body_indent_w = ui_poetry_popup_measure_indent();
    line_limit = (UI_POETRY_POPUP_BODY_W > s_popup.body_indent_w) ? (egui_dim_t)(UI_POETRY_POPUP_BODY_W - s_popup.body_indent_w) : UI_POETRY_POPUP_BODY_W;

    for (uint8_t i = body_start_idx; (i < poem->line_count) && (pos < sizeof(s_popup.text) - 1U); i++)
    {
        const char *line = poem->lines[i];

        if (line == NULL)
        {
            continue;
        }

        while ((*line == ' ') || (*line == '\t'))
        {
            line++;
        }

        while ((*line != '\0') && (pos < sizeof(s_popup.text) - 1U))
        {
            uint8_t bytes;
            egui_dim_t glyph_w;

            if ((*line == '\r') || (*line == '\n'))
            {
                line++;
                continue;
            }

            bytes = ui_poetry_popup_utf8_bytes(line);
            if (bytes == 0U)
            {
                break;
            }

            glyph_w = ui_poetry_popup_estimate_glyph_width(bytes);
            if ((line_width > 0) && ((egui_dim_t)(line_width + glyph_w) > line_limit))
            {
                if (line_idx >= (uint8_t)(UI_POETRY_POPUP_MAX_LAYOUT_LINES - 1U))
                {
                    s_popup.text[pos] = '\0';
                    return;
                }

                s_popup.text[pos++] = '\n';
                line_idx++;
                line_width = 0;
                line_limit = UI_POETRY_POPUP_BODY_W;
            }

            if ((uint16_t)(pos + bytes) >= sizeof(s_popup.text))
            {
                break;
            }

            for (uint8_t j = 0U; j < bytes; j++)
            {
                s_popup.text[pos++] = line[j];
            }
            line_width = (egui_dim_t)(line_width + glyph_w);
            line += bytes;
        }
    }

    s_popup.text[pos] = '\0';
}

static void ui_poetry_popup_show(void)
{
    uint32_t now;
    egui_view_t *view = EGUI_VIEW_OF(&s_popup.base);

    if (!ui_poetry_popup_is_home_active())
    {
        return;
    }

    if (!ui_poetry_popup_prepare_text())
    {
        s_popup.phase = UI_POETRY_POPUP_PHASE_WAITING;
        s_popup.next_show_ms = egui_timer_get_current_time() + (uint32_t)s_popup.interval_s * 1000U;
        egui_view_set_visible(view, 0);
        return;
    }

    now = egui_timer_get_current_time();
#if UI_POETRY_POPUP_PAUSE_HOME_ANIM
    s_popup.home_anim_was_enabled = ui_HomePage_get_animation_enabled() ? 1U : 0U;
    ui_HomePage_set_animation_enabled(false);
#endif
    s_popup.phase = UI_POETRY_POPUP_PHASE_VISIBLE;
    s_popup.shown_at_ms = now;
    s_popup.panel_y = s_popup.target_panel_y;
    egui_view_remove_from_user_root(view);
    egui_core_add_user_root_view(view);
    egui_view_set_visible(view, 1);
    ui_poetry_popup_invalidate_panel(s_popup.panel_y);
    log_printf("[UI_POETRY] show");
    egui_core_force_refresh(egui_port_get_core());
}

static bool ui_poetry_popup_is_home_active(void)
{
    ui_page_t *current = ui_page_manager_get_current();

    return (current != NULL) &&
           (current->name != NULL) &&
           (strcmp(current->name, "HOME") == 0);
}

static void ui_poetry_popup_hide(void)
{
    egui_dim_t old_y = s_popup.panel_y;

    s_popup.phase = UI_POETRY_POPUP_PHASE_WAITING;
    s_popup.next_show_ms = egui_timer_get_current_time() + (uint32_t)s_popup.interval_s * 1000U;
    s_popup.shown_at_ms = 0U;
    s_popup.panel_y = s_popup.target_panel_y;
#if UI_POETRY_POPUP_PAUSE_HOME_ANIM
    if (s_popup.home_anim_was_enabled != 0U)
    {
        ui_HomePage_set_animation_enabled(true);
    }
    s_popup.home_anim_was_enabled = 0U;
#endif
    ui_poetry_popup_invalidate_panel(old_y);
    egui_view_set_visible(EGUI_VIEW_OF(&s_popup.base), 0);
    log_printf("[UI_POETRY] hide");
    egui_core_force_refresh(egui_port_get_core());
}

static uint32_t ui_poetry_popup_elapsed(uint32_t now, uint32_t since)
{
    return (uint32_t)(now - since);
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

static void ui_poetry_popup_collect_lines(void)
{
    char *line = s_popup.text;
    uint8_t count = 0U;

    s_popup.title_width = 0;
    if (s_popup.title[0] != '\0')
    {
        s_popup.title_width = ui_poetry_popup_estimate_text_width(s_popup.title);
    }

    while ((*line != '\0') && (count < UI_POETRY_POPUP_MAX_LAYOUT_LINES))
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
    s_popup.body_total_h = (count == 0U) ? 0U : (uint16_t)((uint16_t)(count - 1U) * UI_POETRY_POPUP_LINE_STEP + UI_POETRY_POPUP_FONT_H);
}

static void ui_poetry_popup_draw_text(egui_canvas_t *canvas, int16_t panel_y)
{
    const egui_font_t *font = ui_heiti_font_get(UI_POETRY_POPUP_FONT_SIZE);
    egui_dim_t display_x = UI_POETRY_POPUP_PANEL_X + UI_POETRY_POPUP_TEXT_PAD_X;
    egui_dim_t display_y = (egui_dim_t)(panel_y + UI_POETRY_POPUP_TEXT_PAD_Y);
    egui_dim_t title_y = display_y;
    egui_dim_t body_x = display_x;
    egui_dim_t body_y = (egui_dim_t)(display_y + UI_POETRY_POPUP_TITLE_LINE_H + UI_POETRY_POPUP_TITLE_BODY_GAP);
    egui_dim_t line_h = UI_POETRY_POPUP_FONT_H;
    egui_dim_t start_y;
    egui_region_t text_clip;
    egui_region_t prev_work;
    egui_region_t clipped_work;
    egui_region_t *work_region;

    work_region = egui_canvas_get_base_view_work_region(canvas);
    prev_work = *work_region;

    if (s_popup.title[0] != '\0')
    {
        egui_dim_t title_x = display_x;

        if (s_popup.title_width < UI_POETRY_POPUP_DISPLAY_W)
        {
            title_x = (egui_dim_t)(display_x + (UI_POETRY_POPUP_DISPLAY_W - s_popup.title_width) / 2U);
        }

        if (((egui_dim_t)(title_y + UI_POETRY_POPUP_FONT_H) > prev_work.location.y) &&
            ((egui_dim_t)(title_y - UI_POETRY_POPUP_FONT_TOP_GUARD) <
             (egui_dim_t)(prev_work.location.y + prev_work.size.height)))
        {
            egui_canvas_draw_text(canvas, font, s_popup.title, title_x, title_y, ui_color(0xF8FAFC), EGUI_ALPHA_100);
        }
    }

    if (s_popup.line_count == 0U)
    {
        return;
    }

    if (s_popup.body_total_h < UI_POETRY_POPUP_BODY_H)
    {
        start_y = (egui_dim_t)(body_y + (UI_POETRY_POPUP_BODY_H - s_popup.body_total_h) / 2U);
    }
    else
    {
        start_y = body_y;
    }

    text_clip.location.x = body_x;
    text_clip.location.y = (egui_dim_t)(body_y - UI_POETRY_POPUP_FONT_TOP_GUARD);
    text_clip.size.width = UI_POETRY_POPUP_BODY_W;
    text_clip.size.height = (egui_dim_t)(UI_POETRY_POPUP_BODY_H + UI_POETRY_POPUP_FONT_TOP_GUARD);
    egui_region_intersect(&prev_work, &text_clip, &clipped_work);
    *work_region = clipped_work;

    for (uint8_t i = 0U; i < s_popup.line_count; i++)
    {
        egui_dim_t line_y = (egui_dim_t)(start_y + (egui_dim_t)i * UI_POETRY_POPUP_LINE_STEP);
        egui_dim_t line_x = body_x;

        if (((egui_dim_t)(line_y + line_h) <= body_y) || (line_y >= (egui_dim_t)(body_y + UI_POETRY_POPUP_BODY_H)))
        {
            continue;
        }
        /*
         * Heiti glyph boxes can extend above their logical line origin.  Keep
         * that top bearing in the PFB visibility test so a line crossing a
         * 14 px tile boundary is also rendered into the preceding tile.
         */
        if (((egui_dim_t)(line_y + line_h) <= clipped_work.location.y) ||
            ((egui_dim_t)(line_y - UI_POETRY_POPUP_FONT_TOP_GUARD) >=
             (egui_dim_t)(clipped_work.location.y + clipped_work.size.height)))
        {
            continue;
        }

        if (i == 0U)
        {
            line_x = (egui_dim_t)(line_x + s_popup.body_indent_w);
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

    ui_draw_round_panel(canvas, UI_POETRY_POPUP_PANEL_X, s_popup.panel_y, UI_POETRY_POPUP_PANEL_W, UI_POETRY_POPUP_PANEL_H, 12, 0x4A90E2, 0xFFFFFF);
    ui_poetry_popup_draw_text(canvas, s_popup.panel_y);
}
