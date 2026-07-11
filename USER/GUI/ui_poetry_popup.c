#include "ui_poetry_popup.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/egui_timer.h"
#include "egui_port_stm32l471_fan.h"
#include "lptim.h"
#include "page_manager.h"
#include "poetry_app.h"
#include "ui_common.h"

#define UI_POETRY_POPUP_TIMER_MS      200U
#define UI_POETRY_POPUP_SCROLL_STEP_S  2U

typedef struct
{
    egui_view_t base;
    egui_timer_t timer;
    uint16_t interval_s;
    uint16_t duration_s;
    uint16_t idle_s;
    uint16_t shown_s;
    uint8_t scroll_s;
    uint8_t visible;
    uint8_t poem_buf[POETRY_APP_MAX_TEXT_SIZE];
    char text[160];
} ui_poetry_popup_state_t;

static ui_poetry_popup_state_t s_popup;
static egui_view_api_t s_popup_api;

static void ui_poetry_popup_timer_cb(egui_timer_t *timer);
static void ui_poetry_popup_on_second(void);
static bool ui_poetry_popup_prepare_text(void);
static void ui_poetry_popup_show(void);
static void ui_poetry_popup_hide(void);
static void ui_poetry_popup_on_draw(egui_view_t *self);

void ui_poetry_popup_init(void)
{
    egui_view_t *view = EGUI_VIEW_OF(&s_popup.base);

    memset(&s_popup, 0, sizeof(s_popup));
    s_popup.interval_s = UI_POETRY_POPUP_DEFAULT_INTERVAL_S;
    s_popup.duration_s = UI_POETRY_POPUP_DEFAULT_DURATION_S;
    s_popup.idle_s = s_popup.interval_s;

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

    if (!s_popup.visible && (s_popup.idle_s > interval_s))
    {
        s_popup.idle_s = interval_s;
    }
}

static void ui_poetry_popup_timer_cb(egui_timer_t *timer)
{
    uint32_t ticks = LPTIM1_ConsumeSecondTicks();

    (void)timer;

    while (ticks-- > 0U)
    {
        ui_poetry_popup_on_second();
    }
}

static void ui_poetry_popup_on_second(void)
{
    if (!s_popup.visible)
    {
        if (s_popup.idle_s > 1U)
        {
            s_popup.idle_s--;
            return;
        }

        ui_poetry_popup_show();
        return;
    }

    s_popup.shown_s++;
    s_popup.scroll_s++;

    if (s_popup.shown_s >= s_popup.duration_s)
    {
        ui_poetry_popup_hide();
    }
}

static bool ui_poetry_popup_prepare_text(void)
{
    PoetryApp_Poem_t poem;
    PoetryApp_Collection_t coll;
    uint8_t ascii_only = 1U;
    uint16_t pos = 0U;

    coll = (PoetryApp_Collection_t)(rand() % POETRY_COLL_COUNT);

    if (PoetryApp_OpenCollection(coll) != POETRY_APP_OK)
    {
        (void)snprintf(s_popup.text, sizeof(s_popup.text), "Poetry popup\ncollection %u", (unsigned)coll);
        return true;
    }

    if (PoetryApp_GetRandomPoem(coll, s_popup.poem_buf, sizeof(s_popup.poem_buf), &poem) != POETRY_APP_OK)
    {
        (void)snprintf(s_popup.text, sizeof(s_popup.text), "Poetry popup\ncollection %u", (unsigned)coll);
        return true;
    }

    for (uint8_t i = 0U; (i < poem.line_count) && (pos < sizeof(s_popup.text) - 2U); i++)
    {
        const char *line = poem.lines[i];

        if ((line == NULL) || (line[0] == '\0'))
        {
            continue;
        }

        while (*line != '\0')
        {
            if (((unsigned char)*line) >= 0x80U)
            {
                ascii_only = 0U;
            }
            line++;
        }
    }

    if (!ascii_only)
    {
        (void)snprintf(s_popup.text, sizeof(s_popup.text), "Poetry selected\ncollection %u\nlines %u", (unsigned)coll, (unsigned)poem.line_count);
        return true;
    }

    for (uint8_t i = 0U; (i < poem.line_count) && (pos < sizeof(s_popup.text) - 2U); i++)
    {
        const char *line = poem.lines[i];

        if ((line == NULL) || (line[0] == '\0'))
        {
            continue;
        }

        if ((pos > 0U) && (pos < sizeof(s_popup.text) - 1U))
        {
            s_popup.text[pos++] = '\n';
        }

        while ((*line != '\0') && (pos < sizeof(s_popup.text) - 1U))
        {
            s_popup.text[pos++] = *line++;
        }
    }

    s_popup.text[pos] = '\0';
    if (pos == 0U)
    {
        (void)snprintf(s_popup.text, sizeof(s_popup.text), "Poetry popup\ncollection %u", (unsigned)coll);
    }

    return true;
}

static void ui_poetry_popup_show(void)
{
    if (!ui_poetry_popup_prepare_text())
    {
        return;
    }

    s_popup.visible = 1U;
    s_popup.idle_s = 0U;
    s_popup.shown_s = 0U;
    s_popup.scroll_s = 0U;
    egui_view_set_visible(EGUI_VIEW_OF(&s_popup.base), 1);
    egui_view_invalidate_full(EGUI_VIEW_OF(&s_popup.base));
    egui_core_force_refresh(egui_port_get_core());
}

static void ui_poetry_popup_hide(void)
{
    s_popup.visible = 0U;
    s_popup.idle_s = s_popup.interval_s;
    s_popup.shown_s = 0U;
    s_popup.scroll_s = 0U;
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

    ui_draw_panel(canvas, 60, 38, 308, 66, 0x111827, 0x93C5FD);
    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_16_4), "Poetry popup", 74, 46, 180, 20,
                 EGUI_ALIGN_LEFT | EGUI_ALIGN_VCENTER, 0xF8FAFC);
    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_12_4), s_popup.text, 74, 66, 272, 28,
                 EGUI_ALIGN_LEFT | EGUI_ALIGN_VCENTER, 0xE2E8F0);
}
