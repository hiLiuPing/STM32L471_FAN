#include "ui_poetry_popup.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "heiti_lvgl_font.h"
#include "log.h"
#include "lptim.h"
#include "poetry_app.h"
#include "lvgl.h"

#define UI_POETRY_POPUP_TIMER_MS       200U
#define UI_POETRY_POPUP_SCROLL_STEP_S  2U
#define UI_POETRY_POPUP_BOX_W          336
#define UI_POETRY_POPUP_BOX_H          86
#define UI_POETRY_POPUP_VIEW_W         292
#define UI_POETRY_POPUP_VIEW_H         58
#define UI_POETRY_POPUP_LINE_H         20
#define UI_POETRY_POPUP_MAX_TEXT       768U

typedef struct
{
    lv_obj_t *box;
    lv_obj_t *viewport;
    lv_obj_t *label;
    lv_timer_t *tick_timer;
    const lv_font_t *font;
    uint16_t interval_s;
    uint16_t duration_s;
    uint16_t idle_s;
    uint16_t shown_s;
    uint8_t scroll_s;
    uint8_t scroll_index;
    uint8_t scroll_limit;
    bool visible;
    uint8_t poem_buf[POETRY_APP_MAX_TEXT_SIZE];
    char text[UI_POETRY_POPUP_MAX_TEXT];
} ui_poetry_popup_state_t;

static ui_poetry_popup_state_t s_popup;

static void ui_poetry_popup_timer_cb(lv_timer_t *timer);
static void ui_poetry_popup_on_second(void);
static bool ui_poetry_popup_prepare_text(void);
static void ui_poetry_popup_show(void);
static void ui_poetry_popup_hide(void);
static void ui_poetry_popup_apply_scroll(void);

void ui_poetry_popup_init(void)
{
    memset(&s_popup, 0, sizeof(s_popup));
    s_popup.interval_s = UI_POETRY_POPUP_DEFAULT_INTERVAL_S;
    s_popup.duration_s = UI_POETRY_POPUP_DEFAULT_DURATION_S;
    s_popup.idle_s = s_popup.interval_s;
    s_popup.font = HeitiLvgl_OpenDefault("heiti_4_16.bin");
    if (s_popup.font == NULL)
    {
        log_printf("[POETRY_POPUP] font open FAIL");
    }

    s_popup.tick_timer = lv_timer_create(ui_poetry_popup_timer_cb,
                                         UI_POETRY_POPUP_TIMER_MS,
                                         NULL);
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

static void ui_poetry_popup_timer_cb(lv_timer_t *timer)
{
    uint32_t ticks;

    (void)timer;

    ticks = LPTIM1_ConsumeSecondTicks();
    while (ticks-- > 0U)
    {
        ui_poetry_popup_on_second();
    }
}

static void ui_poetry_popup_on_second(void)
{
    if (!s_popup.visible)
    {
        if (s_popup.interval_s > 1U)
        {
            if (s_popup.idle_s < (uint16_t)(s_popup.interval_s - 1U))
            {
                s_popup.idle_s++;
                return;
            }
        }
        else if (s_popup.idle_s == 0U)
        {
            s_popup.idle_s = 1U;
        }

        ui_poetry_popup_show();
        return;
    }

    s_popup.shown_s++;
    s_popup.scroll_s++;

    if (s_popup.scroll_s >= UI_POETRY_POPUP_SCROLL_STEP_S)
    {
        s_popup.scroll_s = 0U;
        if (s_popup.scroll_limit > 0U)
        {
            if (s_popup.scroll_index < s_popup.scroll_limit)
            {
                s_popup.scroll_index++;
            }
            else
            {
                s_popup.scroll_index = 0U;
            }
            ui_poetry_popup_apply_scroll();
        }
    }

    if (s_popup.shown_s >= s_popup.duration_s)
    {
        ui_poetry_popup_hide();
    }
}

static bool ui_poetry_popup_prepare_text(void)
{
    PoetryApp_Poem_t poem;
    PoetryApp_Collection_t coll;
    uint16_t pos = 0U;
    int ret;

    coll = (PoetryApp_Collection_t)(rand() % POETRY_COLL_COUNT);

    if (PoetryApp_OpenCollection(coll) != POETRY_APP_OK)
    {
        log_printf("[POETRY_POPUP] open poetry FAIL");
        return false;
    }

    ret = PoetryApp_GetRandomPoem(coll,
                                  s_popup.poem_buf,
                                  sizeof(s_popup.poem_buf),
                                  &poem);
    if (ret != POETRY_APP_OK)
    {
        log_printf("[POETRY_POPUP] random poetry FAIL ret=%d", ret);
        return false;
    }

    for (uint8_t i = 0U; (i < poem.line_count) && (pos < UI_POETRY_POPUP_MAX_TEXT - 2U); i++)
    {
        const char *line = poem.lines[i];

        if ((line == NULL) || (line[0] == '\0'))
        {
            continue;
        }

        if ((pos > 0U) && (pos < UI_POETRY_POPUP_MAX_TEXT - 1U))
        {
            s_popup.text[pos++] = '\n';
        }

        while ((*line != '\0') && (pos < UI_POETRY_POPUP_MAX_TEXT - 1U))
        {
            s_popup.text[pos++] = *line++;
        }
    }

    s_popup.text[pos] = '\0';

    if (poem.line_count > 3U)
    {
        s_popup.scroll_limit = (uint8_t)(poem.line_count - 3U);
    }
    else
    {
        s_popup.scroll_limit = 0U;
    }

    return pos > 0U;
}

static void ui_poetry_popup_show(void)
{
    if (s_popup.font == NULL)
    {
        s_popup.font = HeitiLvgl_OpenDefault("heiti_4_16.bin");
    }

    if (!ui_poetry_popup_prepare_text())
    {
        s_popup.idle_s = 0U;
        return;
    }

    if (s_popup.box == NULL)
    {
        s_popup.box = lv_obj_create(lv_layer_top());
        lv_obj_clear_flag(s_popup.box, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(s_popup.box, UI_POETRY_POPUP_BOX_W, UI_POETRY_POPUP_BOX_H);
        lv_obj_align(s_popup.box, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_radius(s_popup.box, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(s_popup.box, lv_color_hex(0x111827), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(s_popup.box, LV_OPA_90, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(s_popup.box, lv_color_hex(0x93C5FD), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(s_popup.box, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(s_popup.box, 12, LV_PART_MAIN | LV_STATE_DEFAULT);

        s_popup.viewport = lv_obj_create(s_popup.box);
        lv_obj_remove_style_all(s_popup.viewport);
        lv_obj_clear_flag(s_popup.viewport, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_clip_corner(s_popup.viewport, true, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_size(s_popup.viewport, UI_POETRY_POPUP_VIEW_W, UI_POETRY_POPUP_VIEW_H);
        lv_obj_align(s_popup.viewport, LV_ALIGN_TOP_MID, 0, 4);

        s_popup.label = lv_label_create(s_popup.viewport);
        lv_obj_set_width(s_popup.label, UI_POETRY_POPUP_VIEW_W);
        lv_label_set_long_mode(s_popup.label, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_color(s_popup.label, lv_color_hex(0xF9FAFB), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_align(s_popup.label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_line_space(s_popup.label, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
        if (s_popup.font != NULL)
        {
            lv_obj_set_style_text_font(s_popup.label, s_popup.font, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }

    lv_label_set_text_static(s_popup.label, s_popup.text);
    s_popup.visible = true;
    s_popup.idle_s = 0U;
    s_popup.shown_s = 0U;
    s_popup.scroll_s = 0U;
    s_popup.scroll_index = 0U;
    lv_obj_clear_flag(s_popup.box, LV_OBJ_FLAG_HIDDEN);
    ui_poetry_popup_apply_scroll();
}

static void ui_poetry_popup_hide(void)
{
    if (s_popup.box != NULL)
    {
        lv_obj_add_flag(s_popup.box, LV_OBJ_FLAG_HIDDEN);
    }

    s_popup.visible = false;
    s_popup.idle_s = 0U;
    s_popup.shown_s = 0U;
    s_popup.scroll_s = 0U;
    s_popup.scroll_index = 0U;
}

static void ui_poetry_popup_apply_scroll(void)
{
    if (s_popup.label == NULL)
    {
        return;
    }

    lv_obj_set_y(s_popup.label, -((lv_coord_t)s_popup.scroll_index * UI_POETRY_POPUP_LINE_H));
}
