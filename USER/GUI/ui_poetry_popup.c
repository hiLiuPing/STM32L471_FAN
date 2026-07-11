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

#define UI_POETRY_POPUP_TIMER_MS  200U
#define UI_POETRY_POPUP_MAX_LINES  4U

typedef struct
{
    egui_view_t base;
    egui_timer_t timer;
    uint16_t interval_s;
    uint16_t duration_s;
    uint32_t next_show_ms;
    uint32_t shown_at_ms;
    uint8_t visible;
    uint8_t poem_buf[POETRY_APP_MAX_TEXT_SIZE];
    char text[160];
} ui_poetry_popup_state_t;

static ui_poetry_popup_state_t s_popup;
static egui_view_api_t s_popup_api;

static void ui_poetry_popup_timer_cb(egui_timer_t *timer);
static bool ui_poetry_popup_time_reached(uint32_t now, uint32_t target);
static bool ui_poetry_popup_prepare_text(void);
static void ui_poetry_popup_append_line(const char *line, uint16_t *pos);
static void ui_poetry_popup_show(void);
static void ui_poetry_popup_hide(void);
static void ui_poetry_popup_on_draw(egui_view_t *self);

void ui_poetry_popup_init(void)
{
    egui_view_t *view = EGUI_VIEW_OF(&s_popup.base);

    memset(&s_popup, 0, sizeof(s_popup));
    s_popup.interval_s = UI_POETRY_POPUP_DEFAULT_INTERVAL_S;
    s_popup.duration_s = UI_POETRY_POPUP_DEFAULT_DURATION_S;
    s_popup.next_show_ms = egui_timer_get_current_time() + (uint32_t)s_popup.interval_s * 1000U;

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

    if (!s_popup.visible)
    {
        s_popup.next_show_ms = egui_timer_get_current_time() + (uint32_t)s_popup.interval_s * 1000U;
    }
}

static void ui_poetry_popup_timer_cb(egui_timer_t *timer)
{
    uint32_t now = egui_timer_get_current_time();

    (void)timer;

    if (!s_popup.visible)
    {
        if (ui_poetry_popup_time_reached(now, s_popup.next_show_ms))
        {
            ui_poetry_popup_show();
        }
        return;
    }

    if ((uint32_t)(now - s_popup.shown_at_ms) >= (uint32_t)s_popup.duration_s * 1000U)
    {
        ui_poetry_popup_hide();
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
    uint8_t copied_lines = 0U;
    uint16_t pos = 0U;
    int ret;

    if (!ui_heiti_font_16_is_ready())
    {
        (void)snprintf(s_popup.text, sizeof(s_popup.text), "Poetry font missing\n%s", UI_HEITI_FONT_16_PATH);
        return true;
    }

    coll = (PoetryApp_Collection_t)(rand() % POETRY_COLL_COUNT);

    ret = PoetryApp_OpenCollection(coll);
    if (ret != POETRY_APP_OK)
    {
        log_printf("[UI_POETRY] open coll=%u ret=%d", (unsigned)coll, ret);
        (void)snprintf(s_popup.text, sizeof(s_popup.text), "Poetry data missing\ncollection %u", (unsigned)coll);
        return true;
    }

    ret = PoetryApp_GetRandomPoem(coll, s_popup.poem_buf, sizeof(s_popup.poem_buf), &poem);
    if (ret != POETRY_APP_OK)
    {
        log_printf("[UI_POETRY] get poem coll=%u ret=%d", (unsigned)coll, ret);
        (void)snprintf(s_popup.text, sizeof(s_popup.text), "Poetry read failed\ncollection %u", (unsigned)coll);
        return true;
    }

    for (uint8_t i = 0U;
         (i < poem.line_count) && (copied_lines < UI_POETRY_POPUP_MAX_LINES) && (pos < sizeof(s_popup.text) - 2U);
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
    if (!ui_poetry_popup_prepare_text())
    {
        return;
    }

    s_popup.visible = 1U;
    s_popup.shown_at_ms = egui_timer_get_current_time();
    egui_view_set_visible(EGUI_VIEW_OF(&s_popup.base), 1);
    egui_view_invalidate_full(EGUI_VIEW_OF(&s_popup.base));
    egui_core_force_refresh(egui_port_get_core());
}

static void ui_poetry_popup_hide(void)
{
    s_popup.visible = 0U;
    s_popup.next_show_ms = egui_timer_get_current_time() + (uint32_t)s_popup.interval_s * 1000U;
    s_popup.shown_at_ms = 0U;
    egui_view_set_visible(EGUI_VIEW_OF(&s_popup.base), 0);
    egui_view_invalidate_full(EGUI_VIEW_OF(&s_popup.base));
    egui_core_force_refresh(egui_port_get_core());
}

static void ui_poetry_popup_on_draw(egui_view_t *self)
{
    egui_canvas_t *canvas = egui_view_get_canvas(self);

    if (!s_popup.visible)
    {
        return;
    }

    ui_draw_panel(canvas, 32, 20, 364, 102, 0x111827, 0x93C5FD);
    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_16_4), "Poetry", 46, 28, 180, 18,
                 EGUI_ALIGN_LEFT | EGUI_ALIGN_VCENTER, 0xF8FAFC);
    ui_draw_text(canvas, ui_heiti_font_get_16(), s_popup.text, 46, 50, 330, 64,
                 EGUI_ALIGN_LEFT | EGUI_ALIGN_TOP, 0xE2E8F0);
}
