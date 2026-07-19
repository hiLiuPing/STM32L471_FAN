#include "ui_SettingPage.h"

#include <stdio.h>
#include <string.h>

#include "core/egui_timer.h"
#include "egui_port_stm32l471_fan.h"
#include "key.h"
#include "page_manager.h"
#include "settings_app.h"
#include "ui_common.h"
#include "widget/egui_view.h"

#define UI_SETTING_ITEM_COUNT       8U
#define UI_SETTING_FRAME_MS         50U
#define UI_SETTING_ROW_X            132
#define UI_SETTING_ROW_W            288
#define UI_SETTING_ROW_Y            4
#define UI_SETTING_ROW_H            20
#define UI_SETTING_ROW_GAP          2
#define UI_SETTING_ROW_STEP         (UI_SETTING_ROW_H + UI_SETTING_ROW_GAP)
#define UI_SETTING_SCROLL_DURATION_MS 200U
#define UI_SETTING_VALUE_STEP       1U
#define UI_SETTING_FAST_STEP        5U
#define UI_SETTING_DURATION_STEP    10U
#define UI_SETTING_DURATION_FAST_STEP 30U
#define UI_SETTING_AUTO_OFF_STEP    30U
#define UI_SETTING_AUTO_OFF_FAST_STEP 60U
#define UI_SETTING_ENTER_STEP_MS    65U
#define UI_SETTING_ENTER_LENGTH_MS  260U
#define UI_SETTING_CONFIRM_MS       360U
#define UI_SETTING_PARTICLE_COUNT   8U

typedef enum
{
    UI_SETTING_ITEM_HOME_THEME = 0,
    UI_SETTING_ITEM_POETRY_ENABLE,
    UI_SETTING_ITEM_POETRY_INTERVAL,
    UI_SETTING_ITEM_POETRY_DURATION,
    UI_SETTING_ITEM_WEATHER_INTERVAL,
    UI_SETTING_ITEM_RGB_PWR_ENABLE,
    UI_SETTING_ITEM_IDLE_TIMEOUT,
    UI_SETTING_ITEM_SYSTEM_AUTO_OFF
} ui_setting_item_t;

typedef struct
{
    egui_view_t base;
    egui_timer_t timer;
    AppSettings_t settings;
    AppSettings_t edit_backup;
    uint32_t enter_tick;
    uint32_t frame_tick;
    uint32_t confirm_until;
    uint32_t scroll_start_tick;
    int16_t scroll_y;
    int16_t scroll_start_y;
    int16_t scroll_target_y;
    uint16_t hue;
    uint8_t selected_index;
    uint8_t editing;
    uint8_t setting_active;
    uint8_t animation_active;
    uint8_t scroll_animating;
} ui_setting_page_t;

static const char *const s_setting_labels[UI_SETTING_ITEM_COUNT] = {
    "Home theme",
    "Poetry popup",
    "Poetry gap",
    "Poetry stay",
    "Weather sync",
    "RGB rainbow",
    "Screen sleep",
    "System power off",
};

static const int16_t s_particle_base_x[UI_SETTING_PARTICLE_COUNT] = {12, 31, 55, 79, 102, 19, 66, 111};
static const int16_t s_particle_base_y[UI_SETTING_PARTICLE_COUNT] = {18, 37, 24, 49, 31, 72, 84, 66};
static const uint8_t s_particle_size[UI_SETTING_PARTICLE_COUNT] = {1, 2, 1, 2, 1, 1, 2, 1};

static ui_setting_page_t s_setting_page;
static egui_view_api_t s_setting_api;

egui_view_t *ui_SettingPage = NULL;

static void ui_SettingPage_on_draw(egui_view_t *self);
static void ui_SettingPage_timer_cb(egui_timer_t *timer);
static void ui_SettingPage_move_selection(int8_t delta);
static void ui_SettingPage_adjust_selected(int8_t delta, uint8_t fast);
static void ui_SettingPage_scroll_reset(void);
static void ui_SettingPage_scroll_update(uint32_t now);
static void ui_SettingPage_scroll_to(int16_t target_y, uint32_t now);
static void ui_SettingPage_update_scroll_for_selection(uint8_t selected_index);
static bool ui_SettingPage_is_toggle_item(uint8_t index);
static bool ui_SettingPage_is_cycle_item(uint8_t index);
static bool ui_SettingPage_commit_edit(void);

static uint32_t ui_setting_hsv_to_rgb(uint16_t hue, uint8_t saturation, uint8_t value)
{
    uint16_t remainder;
    uint8_t region;
    uint8_t p;
    uint8_t q;
    uint8_t t;
    uint8_t r;
    uint8_t g;
    uint8_t b;

    hue %= 360U;
    region = (uint8_t)(hue / 60U);
    remainder = (uint16_t)(((hue % 60U) * 255U) / 60U);
    p = (uint8_t)(((uint16_t)value * (255U - saturation)) >> 8);
    q = (uint8_t)(((uint16_t)value * (255U - (((uint16_t)saturation * remainder) >> 8))) >> 8);
    t = (uint8_t)(((uint16_t)value * (255U - (((uint16_t)saturation * (255U - remainder)) >> 8))) >> 8);

    switch (region)
    {
    case 0U: r = value; g = t; b = p; break;
    case 1U: r = q; g = value; b = p; break;
    case 2U: r = p; g = value; b = t; break;
    case 3U: r = p; g = q; b = value; break;
    case 4U: r = t; g = p; b = value; break;
    default: r = value; g = p; b = q; break;
    }

    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

static uint16_t ui_setting_adjust_u16(uint16_t value, int8_t delta, uint16_t step, uint16_t min_value, uint16_t max_value)
{
    int32_t next = (int32_t)value + ((int32_t)delta * (int32_t)step);

    if (next < (int32_t)min_value)
    {
        next = min_value;
    }
    if (next > (int32_t)max_value)
    {
        next = max_value;
    }
    return (uint16_t)next;
}

static uint16_t ui_setting_adjust_optional_u16(uint16_t value,
                                               int8_t delta,
                                               uint16_t step,
                                               uint16_t disabled_value,
                                               uint16_t min_value,
                                               uint16_t max_value)
{
    int32_t next;

    if (delta > 0)
    {
        if (value == disabled_value)
        {
            return min_value;
        }
        next = (int32_t)value + (int32_t)step;
        return (next > (int32_t)max_value) ? max_value : (uint16_t)next;
    }

    if (delta < 0)
    {
        if (value <= min_value)
        {
            return disabled_value;
        }
        next = (int32_t)value - (int32_t)step;
        return (next < (int32_t)min_value) ? disabled_value : (uint16_t)next;
    }

    return value;
}

static void ui_SettingPage_activate_animation(void)
{
    uint32_t now = egui_timer_get_current_time();

    s_setting_page.enter_tick = now;
    s_setting_page.frame_tick = now;
    s_setting_page.animation_active = 1U;
}

static void ui_SettingPage_scroll_reset(void)
{
    s_setting_page.scroll_y = 0;
    s_setting_page.scroll_start_y = 0;
    s_setting_page.scroll_target_y = 0;
    s_setting_page.scroll_start_tick = 0U;
    s_setting_page.scroll_animating = 0U;
}

static void ui_SettingPage_scroll_update(uint32_t now)
{
    uint32_t elapsed;
    uint32_t progress;
    uint32_t eased;
    int32_t delta;

    if (s_setting_page.scroll_animating == 0U)
    {
        return;
    }

    elapsed = now - s_setting_page.scroll_start_tick;
    if (elapsed >= UI_SETTING_SCROLL_DURATION_MS)
    {
        s_setting_page.scroll_y = s_setting_page.scroll_target_y;
        s_setting_page.scroll_animating = 0U;
        return;
    }

    progress = (elapsed * 256U) / UI_SETTING_SCROLL_DURATION_MS;
    eased = (progress * progress * (768U - (2U * progress)) + 32768U) / 65536U;
    delta = (int32_t)s_setting_page.scroll_target_y - (int32_t)s_setting_page.scroll_start_y;
    s_setting_page.scroll_y = (int16_t)((int32_t)s_setting_page.scroll_start_y +
                                        ((delta * (int32_t)eased) / 256));
}

static void ui_SettingPage_scroll_to(int16_t target_y, uint32_t now)
{
    ui_SettingPage_scroll_update(now);
    if (target_y == s_setting_page.scroll_y)
    {
        s_setting_page.scroll_start_y = target_y;
        s_setting_page.scroll_target_y = target_y;
        s_setting_page.scroll_animating = 0U;
        return;
    }
    if ((s_setting_page.scroll_animating != 0U) &&
        (target_y == s_setting_page.scroll_target_y))
    {
        return;
    }

    s_setting_page.scroll_start_y = s_setting_page.scroll_y;
    s_setting_page.scroll_target_y = target_y;
    s_setting_page.scroll_start_tick = now;
    s_setting_page.scroll_animating = 1U;
}

static void ui_SettingPage_update_scroll_for_selection(uint8_t selected_index)
{
    int16_t row_bottom = (int16_t)(UI_SETTING_ROW_Y +
                                   ((int16_t)selected_index * UI_SETTING_ROW_STEP) +
                                   UI_SETTING_ROW_H);
    int16_t target_y = 0;

    if (row_bottom > (int16_t)UI_SCREEN_H)
    {
        target_y = (int16_t)((int16_t)UI_SCREEN_H - row_bottom);
    }
    ui_SettingPage_scroll_to(target_y, egui_timer_get_current_time());
}

void ui_SettingPage_screen_init(void)
{
    egui_view_t *view = &s_setting_page.base;

    memset(&s_setting_page, 0, sizeof(s_setting_page));
    ui_SettingPage = view;

    egui_view_init(view, egui_port_get_core());
    egui_view_copy_api(view, &s_setting_api);
    s_setting_api.on_draw = ui_SettingPage_on_draw;
    view->api = &s_setting_api;
    egui_view_set_position(view, 0, 0);
    egui_view_set_size(view, UI_SCREEN_W, UI_SCREEN_H);
    egui_view_set_visible(view, 1);
}

void ui_SettingPage_screen_enter(void)
{
    egui_view_t *view = ui_SettingPage;

    if (view == NULL)
    {
        return;
    }
    if (egui_view_check_timer_start(view, &s_setting_page.timer))
    {
        egui_view_stop_periodic(view, &s_setting_page.timer);
    }
    SettingsApp_Get(&s_setting_page.settings);
    s_setting_page.setting_active = 0U;
    s_setting_page.editing = 0U;
    s_setting_page.selected_index = 0U;
    s_setting_page.confirm_until = 0U;
    ui_SettingPage_scroll_reset();
    ui_SettingPage_activate_animation();
    egui_view_start_periodic(view, &s_setting_page.timer, view, ui_SettingPage_timer_cb, UI_SETTING_FRAME_MS);
    egui_view_invalidate_full(view);
}

void ui_SettingPage_screen_destroy(void)
{
    if ((ui_SettingPage != NULL) &&
        egui_view_check_timer_start(ui_SettingPage, &s_setting_page.timer))
    {
        egui_view_stop_periodic(ui_SettingPage, &s_setting_page.timer);
    }
    s_setting_page.setting_active = 0U;
    s_setting_page.editing = 0U;
    s_setting_page.selected_index = 0U;
    s_setting_page.confirm_until = 0U;
    s_setting_page.animation_active = 0U;
    ui_SettingPage_scroll_reset();
}

bool ui_SettingPage_key_handler(void *key_event)
{
    const key_event_t *event = (const key_event_t *)key_event;

    if (event == NULL)
    {
        return false;
    }
    if ((event->type != KEY_EVT_CLICK) && (event->type != KEY_EVT_REPEAT) && (event->type != KEY_EVT_LONG))
    {
        return false;
    }

    if (s_setting_page.animation_active == 0U)
    {
        SettingsApp_Get(&s_setting_page.settings);
        ui_SettingPage_activate_animation();
    }

    if (s_setting_page.setting_active == 0U)
    {
        if ((event->id == KEY_ID_OK) && (event->type == KEY_EVT_CLICK))
        {
            SettingsApp_Get(&s_setting_page.settings);
            s_setting_page.setting_active = 1U;
            s_setting_page.editing = 0U;
            s_setting_page.selected_index = 0U;
            ui_SettingPage_scroll_reset();
            egui_view_invalidate_full(ui_SettingPage);
            return true;
        }
        return ui_page_consume_nav_key_event(key_event);
    }

    if (s_setting_page.editing != 0U)
    {
        if (event->id == KEY_ID_UP)
        {
            ui_SettingPage_adjust_selected(-1, (uint8_t)(event->type != KEY_EVT_CLICK));
            return true;
        }
        if (event->id == KEY_ID_DOWN)
        {
            ui_SettingPage_adjust_selected(1, (uint8_t)(event->type != KEY_EVT_CLICK));
            return true;
        }
        if ((event->id == KEY_ID_OK) && (event->type == KEY_EVT_CLICK))
        {
            return ui_SettingPage_commit_edit();
        }
        if ((event->id == KEY_ID_PWR) && (event->type == KEY_EVT_CLICK))
        {
            s_setting_page.settings = s_setting_page.edit_backup;
            s_setting_page.editing = 0U;
            egui_view_invalidate_full(ui_SettingPage);
            return true;
        }
        return true;
    }

    if (event->id == KEY_ID_UP)
    {
        ui_SettingPage_move_selection(-1);
        return true;
    }
    if (event->id == KEY_ID_DOWN)
    {
        ui_SettingPage_move_selection(1);
        return true;
    }
    if ((event->id == KEY_ID_OK) && (event->type == KEY_EVT_CLICK))
    {
        if (ui_SettingPage_is_cycle_item(s_setting_page.selected_index))
        {
            ui_SettingPage_adjust_selected(1, 0U);
            return ui_SettingPage_commit_edit();
        }
        if (ui_SettingPage_is_toggle_item(s_setting_page.selected_index))
        {
            ui_SettingPage_adjust_selected(1, 0U);
            return ui_SettingPage_commit_edit();
        }

        s_setting_page.edit_backup = s_setting_page.settings;
        s_setting_page.editing = 1U;
        egui_view_invalidate_full(ui_SettingPage);
        return true;
    }
    if ((event->id == KEY_ID_PWR) && (event->type == KEY_EVT_CLICK))
    {
        s_setting_page.setting_active = 0U;
        s_setting_page.editing = 0U;
        s_setting_page.selected_index = 0U;
        ui_SettingPage_scroll_to(0, egui_timer_get_current_time());
        egui_view_invalidate_full(ui_SettingPage);
        return true;
    }

    return true;
}

static void ui_SettingPage_timer_cb(egui_timer_t *timer)
{
    egui_view_t *view = (egui_view_t *)timer->user_data;
    uint32_t now;

    if ((view == NULL) || !egui_view_get_visible(view))
    {
        return;
    }

    if (s_setting_page.animation_active == 0U)
    {
        SettingsApp_Get(&s_setting_page.settings);
        ui_SettingPage_activate_animation();
    }

    now = egui_timer_get_current_time();
    s_setting_page.frame_tick = now;
    ui_SettingPage_scroll_update(now);
    s_setting_page.hue = (uint16_t)((s_setting_page.hue + 4U) % 360U);
    egui_view_invalidate_full(view);
}

static void ui_setting_draw_switch(egui_canvas_t *canvas, int16_t x, int16_t y, uint8_t enabled, uint32_t accent)
{
    uint32_t track = (enabled != 0U) ? accent : 0x334155;
    int16_t knob_x = (enabled != 0U) ? (x + 25) : (x + 7);

    egui_canvas_draw_round_rectangle_fill(canvas, x, y, 38, 16, 8, ui_color(track), EGUI_ALPHA_100);
    egui_canvas_draw_circle_fill_basic(canvas, knob_x, y + 8, 5, ui_color(0xF8FAFC), EGUI_ALPHA_100);
}

static void ui_setting_draw_background(egui_canvas_t *canvas)
{
    const char *key_hint;
    uint32_t accent = ui_setting_hsv_to_rgb(s_setting_page.hue, 210U, 255U);
    uint16_t phase = (uint16_t)((s_setting_page.frame_tick / 20U) % UI_SCREEN_W);

    if (s_setting_page.setting_active == 0U)
    {
        key_hint = "OK SETTINGS";
    }
    else if (ui_SettingPage_is_cycle_item(s_setting_page.selected_index))
    {
        key_hint = "PWR BACK  OK NEXT";
    }
    else if (ui_SettingPage_is_toggle_item(s_setting_page.selected_index))
    {
        key_hint = "PWR BACK  OK TOGGLE";
    }
    else
    {
        key_hint = "PWR BACK  OK EDIT";
    }

    ui_draw_rect(canvas, 0, 0, UI_SCREEN_W, UI_SCREEN_H, 0x07111F);
    ui_draw_rect(canvas, 0, 0, 128, UI_SCREEN_H, 0x0B1F33);

    for (uint8_t i = 0U; i < 6U; i++)
    {
        int16_t x = (int16_t)(((phase + (uint16_t)i * 86U) % (UI_SCREEN_W + 70U)) - 35);
        uint32_t color = ui_setting_hsv_to_rgb((uint16_t)(s_setting_page.hue + (uint16_t)i * 54U), 220U, 230U);
        egui_canvas_draw_line(canvas, x, 0, x + 36, UI_SCREEN_H, 2, ui_color(color), EGUI_ALPHA_20);
    }

    for (uint8_t i = 0U; i < UI_SETTING_PARTICLE_COUNT; i++)
    {
        int16_t drift = (int16_t)(((s_setting_page.frame_tick / (35U + (uint32_t)i * 3U)) + (uint32_t)i * 9U) % 28U);
        int16_t y = (int16_t)((s_particle_base_y[i] + drift) % 104);
        uint32_t color = ui_setting_hsv_to_rgb((uint16_t)(s_setting_page.hue + (uint16_t)i * 38U), 190U, 255U);
        egui_canvas_draw_circle_fill_basic(canvas, s_particle_base_x[i], y + 7, s_particle_size[i], ui_color(color), EGUI_ALPHA_80);
    }

    egui_canvas_draw_circle_basic(canvas, 64, 52, 31, 2, ui_color(accent), EGUI_ALPHA_80);
    egui_canvas_draw_circle_basic(canvas, 64, 52, 23, 1, ui_color(0xE2E8F0), EGUI_ALPHA_40);
    for (uint8_t i = 0U; i < 3U; i++)
    {
        uint16_t hue = (uint16_t)(s_setting_page.hue + (uint16_t)i * 120U);
        int16_t offset = (int16_t)(((s_setting_page.frame_tick / 50U) + (uint32_t)i * 10U) % 30U);
        egui_canvas_draw_circle_fill_basic(canvas, 48 + offset, 47 + (int16_t)i * 6, 5, ui_color(ui_setting_hsv_to_rgb(hue, 255U, 255U)), EGUI_ALPHA_90);
    }

    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_18_4), "RGB", 38, 88, 52, 22,
                 EGUI_ALIGN_CENTER, 0xF8FAFC);
    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_10_4),
                 (s_setting_page.settings.rgb_pwr_enabled != 0U) ? "RAINBOW ON" : "RAINBOW OFF",
                 18, 111, 92, 15, EGUI_ALIGN_CENTER, accent);
    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_10_4),
                 key_hint,
                 7, 128, 114, 12, EGUI_ALIGN_CENTER, 0x94A3B8);
}

static void ui_setting_format_value(uint8_t index, char *buf, uint16_t size)
{
    switch ((ui_setting_item_t)index)
    {
    case UI_SETTING_ITEM_HOME_THEME:
        (void)snprintf(buf, size, "%s",
                       (s_setting_page.settings.home_theme == SETTINGS_APP_HOME_THEME_2) ?
                           "Dynamic" : "Classic");
        break;
    case UI_SETTING_ITEM_POETRY_INTERVAL:
        (void)snprintf(buf, size, "%u min", (unsigned)s_setting_page.settings.poetry_popup_interval_min);
        break;
    case UI_SETTING_ITEM_POETRY_DURATION:
        (void)snprintf(buf, size, "%u s", (unsigned)s_setting_page.settings.poetry_popup_duration_s);
        break;
    case UI_SETTING_ITEM_WEATHER_INTERVAL:
        if (s_setting_page.settings.weather_time_sync_interval_min ==
            SETTINGS_APP_WEATHER_SYNC_INTERVAL_MIN_DISABLED)
        {
            (void)snprintf(buf, size, "OFF");
        }
        else
        {
            (void)snprintf(buf, size, "%u min", (unsigned)s_setting_page.settings.weather_time_sync_interval_min);
        }
        break;
    case UI_SETTING_ITEM_IDLE_TIMEOUT:
        if (s_setting_page.settings.screen_idle_timeout_min ==
            SETTINGS_APP_SCREEN_IDLE_TIMEOUT_MIN_DISABLED)
        {
            (void)snprintf(buf, size, "OFF");
        }
        else
        {
            (void)snprintf(buf, size, "%u min", (unsigned)s_setting_page.settings.screen_idle_timeout_min);
        }
        break;
    case UI_SETTING_ITEM_SYSTEM_AUTO_OFF:
        if (s_setting_page.settings.system_auto_off_min ==
            SETTINGS_APP_SYSTEM_AUTO_OFF_MIN_DISABLED)
        {
            (void)snprintf(buf, size, "OFF");
        }
        else
        {
            (void)snprintf(buf, size, "%u min", (unsigned)s_setting_page.settings.system_auto_off_min);
        }
        break;
    default:
        buf[0] = '\0';
        break;
    }
}

static void ui_setting_draw_rows(egui_canvas_t *canvas)
{
    const egui_font_t *font = EGUI_FONT_OF(&egui_res_font_montserrat_12_4);
    uint32_t elapsed = s_setting_page.frame_tick - s_setting_page.enter_tick;
    uint32_t accent = ui_setting_hsv_to_rgb(s_setting_page.hue, 225U, 255U);
    uint8_t pulse = (uint8_t)((s_setting_page.frame_tick / 80U) % 20U);

    if (pulse > 10U)
    {
        pulse = (uint8_t)(20U - pulse);
    }

    for (uint8_t i = 0U; i < UI_SETTING_ITEM_COUNT; i++)
    {
        uint32_t delay = (uint32_t)i * UI_SETTING_ENTER_STEP_MS;
        uint32_t progress = (elapsed > delay) ? (elapsed - delay) : 0U;
        int16_t slide = 0;
        int16_t x;
        int16_t y = (int16_t)(UI_SETTING_ROW_Y +
                              (int16_t)i * UI_SETTING_ROW_STEP +
                              s_setting_page.scroll_y);
        uint8_t selected = (uint8_t)((s_setting_page.setting_active != 0U) && (s_setting_page.selected_index == i));
        uint32_t fill = selected ? 0x123451 : 0x0D2134;
        uint32_t border = selected ? accent : 0x1E3A50;
        char value[20];

        if (progress < UI_SETTING_ENTER_LENGTH_MS)
        {
            slide = (int16_t)(46U - ((progress * 46U) / UI_SETTING_ENTER_LENGTH_MS));
        }
        x = UI_SETTING_ROW_X + slide;

        egui_canvas_draw_round_rectangle_fill(canvas, x, y, UI_SETTING_ROW_W, UI_SETTING_ROW_H, 5, ui_color(fill), EGUI_ALPHA_90);
        egui_canvas_draw_round_rectangle(canvas, x, y, UI_SETTING_ROW_W, UI_SETTING_ROW_H, 5,
                                         selected ? 2 : 1, ui_color(border), EGUI_ALPHA_100);

        if (selected != 0U)
        {
            int16_t beam_x = x + 8 + (int16_t)((s_setting_page.frame_tick / 12U) % (UI_SETTING_ROW_W - 40));
            egui_canvas_draw_line(canvas, beam_x, y + 2, beam_x + 28, y + 2, 2,
                                  ui_color(ui_setting_hsv_to_rgb((uint16_t)(s_setting_page.hue + 90U), 210U, 255U)), EGUI_ALPHA_80);
            if (s_setting_page.editing != 0U)
            {
                egui_canvas_draw_round_rectangle(canvas, x + 3, y + 3, UI_SETTING_ROW_W - 6, UI_SETTING_ROW_H - 6, 3,
                                                 (pulse > 5U) ? 2 : 1, ui_color(0xF8FAFC), EGUI_ALPHA_40);
            }
        }

        ui_draw_text(canvas, font, s_setting_labels[i], x + 12, y + 2, 135, UI_SETTING_ROW_H - 4,
                     EGUI_ALIGN_LEFT | EGUI_ALIGN_VCENTER, selected ? 0xFFFFFF : 0xC8D8E8);

        if (i == UI_SETTING_ITEM_POETRY_ENABLE)
        {
            ui_setting_draw_switch(canvas, x + UI_SETTING_ROW_W - 50, y + 3,
                                   s_setting_page.settings.poetry_popup_enabled, accent);
        }
        else if (i == UI_SETTING_ITEM_RGB_PWR_ENABLE)
        {
            ui_setting_draw_switch(canvas, x + UI_SETTING_ROW_W - 50, y + 3,
                                   s_setting_page.settings.rgb_pwr_enabled, accent);
        }
        else
        {
            ui_setting_format_value(i, value, sizeof(value));
            ui_draw_text(canvas, font, value, x + 168, y + 2, 105, UI_SETTING_ROW_H - 4,
                         EGUI_ALIGN_RIGHT | EGUI_ALIGN_VCENTER, selected ? accent : 0x7DD3FC);
        }
    }

    if (Time32_Before(s_setting_page.frame_tick, s_setting_page.confirm_until))
    {
        egui_alpha_t alpha = (egui_alpha_t)(((s_setting_page.confirm_until - s_setting_page.frame_tick) * 178U) / UI_SETTING_CONFIRM_MS);
        egui_canvas_draw_rectangle_fill(canvas, UI_SETTING_ROW_X, 0, UI_SETTING_ROW_W + 8, UI_SCREEN_H,
                                        ui_color(accent), alpha);
    }
}

static void ui_SettingPage_on_draw(egui_view_t *self)
{
    egui_canvas_t *canvas = egui_view_get_canvas(self);

    (void)self;
    ui_setting_draw_background(canvas);
    ui_setting_draw_rows(canvas);
}

static void ui_SettingPage_move_selection(int8_t delta)
{
    int16_t next = (int16_t)s_setting_page.selected_index + delta;

    if (next < 0)
    {
        next = (int16_t)(UI_SETTING_ITEM_COUNT - 1U);
    }
    else if (next >= (int16_t)UI_SETTING_ITEM_COUNT)
    {
        next = 0;
    }
    s_setting_page.selected_index = (uint8_t)next;
    ui_SettingPage_update_scroll_for_selection(s_setting_page.selected_index);
    egui_view_invalidate_full(ui_SettingPage);
}

static void ui_SettingPage_adjust_selected(int8_t delta, uint8_t fast)
{
    uint16_t step = (fast != 0U) ? UI_SETTING_FAST_STEP : UI_SETTING_VALUE_STEP;

    switch ((ui_setting_item_t)s_setting_page.selected_index)
    {
    case UI_SETTING_ITEM_HOME_THEME:
        if (delta != 0)
        {
            s_setting_page.settings.home_theme =
                (s_setting_page.settings.home_theme == SETTINGS_APP_HOME_THEME_2) ?
                    SETTINGS_APP_HOME_THEME_1 :
                    SETTINGS_APP_HOME_THEME_2;
        }
        break;
    case UI_SETTING_ITEM_POETRY_ENABLE:
        if (delta != 0)
        {
            s_setting_page.settings.poetry_popup_enabled = (uint8_t)!s_setting_page.settings.poetry_popup_enabled;
        }
        break;
    case UI_SETTING_ITEM_POETRY_INTERVAL:
        s_setting_page.settings.poetry_popup_interval_min =
            ui_setting_adjust_u16(s_setting_page.settings.poetry_popup_interval_min, delta, step,
                                  SETTINGS_APP_POETRY_INTERVAL_MIN_MIN, SETTINGS_APP_POETRY_INTERVAL_MIN_MAX);
        break;
    case UI_SETTING_ITEM_POETRY_DURATION:
        step = (fast != 0U) ? UI_SETTING_DURATION_FAST_STEP : UI_SETTING_DURATION_STEP;
        s_setting_page.settings.poetry_popup_duration_s =
            ui_setting_adjust_u16(s_setting_page.settings.poetry_popup_duration_s, delta, step,
                                  SETTINGS_APP_POETRY_DURATION_S_MIN, SETTINGS_APP_POETRY_DURATION_S_MAX);
        break;
    case UI_SETTING_ITEM_WEATHER_INTERVAL:
        s_setting_page.settings.weather_time_sync_interval_min =
            ui_setting_adjust_optional_u16(s_setting_page.settings.weather_time_sync_interval_min,
                                           delta, step,
                                           SETTINGS_APP_WEATHER_SYNC_INTERVAL_MIN_DISABLED,
                                           SETTINGS_APP_WEATHER_SYNC_INTERVAL_MIN_MIN,
                                           SETTINGS_APP_WEATHER_SYNC_INTERVAL_MIN_MAX);
        break;
    case UI_SETTING_ITEM_RGB_PWR_ENABLE:
        if (delta != 0)
        {
            s_setting_page.settings.rgb_pwr_enabled = (uint8_t)!s_setting_page.settings.rgb_pwr_enabled;
        }
        break;
    case UI_SETTING_ITEM_IDLE_TIMEOUT:
        s_setting_page.settings.screen_idle_timeout_min =
            ui_setting_adjust_optional_u16(s_setting_page.settings.screen_idle_timeout_min,
                                           delta, step,
                                           SETTINGS_APP_SCREEN_IDLE_TIMEOUT_MIN_DISABLED,
                                           SETTINGS_APP_SCREEN_IDLE_TIMEOUT_MIN_MIN,
                                           SETTINGS_APP_SCREEN_IDLE_TIMEOUT_MIN_MAX);
        break;
    case UI_SETTING_ITEM_SYSTEM_AUTO_OFF:
        step = (fast != 0U) ? UI_SETTING_AUTO_OFF_FAST_STEP : UI_SETTING_AUTO_OFF_STEP;
        s_setting_page.settings.system_auto_off_min =
            ui_setting_adjust_optional_u16(s_setting_page.settings.system_auto_off_min,
                                           delta, step,
                                           SETTINGS_APP_SYSTEM_AUTO_OFF_MIN_DISABLED,
                                           SETTINGS_APP_SYSTEM_AUTO_OFF_MIN_MIN,
                                           SETTINGS_APP_SYSTEM_AUTO_OFF_MIN_MAX);
        break;
    default:
        break;
    }
    egui_view_invalidate_full(ui_SettingPage);
}

static bool ui_SettingPage_is_toggle_item(uint8_t index)
{
    return ((index == (uint8_t)UI_SETTING_ITEM_POETRY_ENABLE) ||
            (index == (uint8_t)UI_SETTING_ITEM_RGB_PWR_ENABLE));
}

static bool ui_SettingPage_is_cycle_item(uint8_t index)
{
    return (index == (uint8_t)UI_SETTING_ITEM_HOME_THEME);
}

static bool ui_SettingPage_commit_edit(void)
{
    (void)SettingsApp_Update(&s_setting_page.settings);
    SettingsApp_Apply();
    s_setting_page.editing = 0U;
    s_setting_page.confirm_until = egui_timer_get_current_time() + UI_SETTING_CONFIRM_MS;
    egui_view_invalidate_full(ui_SettingPage);
    return true;
}
