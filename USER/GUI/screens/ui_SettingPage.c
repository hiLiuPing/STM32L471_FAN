#include "ui_SettingPage.h"

#include <stdio.h>
#include <string.h>

#include "core/egui_timer.h"
#include "egui_port_stm32l471_fan.h"
#include "key.h"
#include "page_manager.h"
#include "settings_app.h"
#include "ui_common.h"
#include "widget/egui_view_group.h"
#include "widget/egui_view_list.h"

#define UI_SETTING_ITEM_COUNT 6U
#define UI_SETTING_ROW_H      24
#define UI_SETTING_LIST_Y     0
#define UI_SETTING_VALUE_STEP  1U
#define UI_SETTING_FAST_STEP   5U

typedef enum
{
    UI_SETTING_ITEM_POETRY_ENABLE = 0,
    UI_SETTING_ITEM_POETRY_INTERVAL,
    UI_SETTING_ITEM_WEATHER_INTERVAL,
    UI_SETTING_ITEM_BRIGHTNESS_DAY,
    UI_SETTING_ITEM_BRIGHTNESS_NIGHT,
    UI_SETTING_ITEM_IDLE_TIMEOUT
} ui_setting_item_t;

typedef struct
{
    egui_view_group_t base;
    egui_view_list_t list;
    egui_timer_t timer;
    AppSettings_t settings;
    AppSettings_t edit_backup;
    uint8_t selected_index;
    uint8_t editing;
    uint8_t setting_active;
    char item_text[UI_SETTING_ITEM_COUNT][56];
} ui_setting_page_t;

static ui_setting_page_t s_setting_page;
static egui_view_api_t s_setting_api;

egui_view_t *ui_SettingPage = NULL;

static void ui_SettingPage_on_draw(egui_view_t *self);
static void ui_SettingPage_timer_cb(egui_timer_t *timer);
static void ui_SettingPage_rebuild_items(void);
static void ui_SettingPage_format_item(uint8_t index, char *buf, uint16_t size);
static void ui_SettingPage_move_selection(int8_t delta);
static void ui_SettingPage_adjust_selected(int8_t delta, uint8_t fast);
static bool ui_SettingPage_commit_edit(void);

void ui_SettingPage_screen_init(void)
{
    egui_view_t *view = EGUI_VIEW_OF(&s_setting_page.base);
    egui_view_t *list_view = EGUI_VIEW_OF(&s_setting_page.list);

    memset(&s_setting_page, 0, sizeof(s_setting_page));
    ui_SettingPage = view;

    egui_view_group_init(view, egui_port_get_core());
    egui_view_copy_api(view, &s_setting_api);
    s_setting_api.on_draw = ui_SettingPage_on_draw;
    view->api = &s_setting_api;
    egui_view_set_position(view, 0, 0);
    egui_view_set_size(view, UI_SCREEN_W, UI_SCREEN_H);
    egui_view_set_visible(view, 1);

    egui_view_list_init(list_view, egui_port_get_core());
    egui_view_set_position(list_view, 0, UI_SETTING_LIST_Y);
    egui_view_list_set_item_height(list_view, UI_SETTING_ROW_H);
    egui_view_scroll_set_size(list_view, UI_SCREEN_W, UI_SCREEN_H);
    egui_view_group_add_child(view, list_view);

    SettingsApp_Get(&s_setting_page.settings);
    s_setting_page.selected_index = 0U;
    s_setting_page.editing = 0U;
    s_setting_page.setting_active = 0U;
    ui_SettingPage_rebuild_items();

    egui_view_start_periodic(view, &s_setting_page.timer, view, ui_SettingPage_timer_cb, 1000U);
}

void ui_SettingPage_screen_destroy(void)
{
    if ((s_setting_page.editing != 0U) &&
        ((s_setting_page.selected_index == UI_SETTING_ITEM_BRIGHTNESS_DAY) ||
         (s_setting_page.selected_index == UI_SETTING_ITEM_BRIGHTNESS_NIGHT)))
    {
        SettingsApp_ApplyActiveBrightness();
    }

    s_setting_page.setting_active = 0U;
    s_setting_page.editing = 0U;
    s_setting_page.selected_index = 0U;
    ui_SettingPage_rebuild_items();
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

    if (s_setting_page.setting_active == 0U)
    {
        if ((event->id == KEY_ID_OK) && (event->type == KEY_EVT_CLICK))
        {
            SettingsApp_Get(&s_setting_page.settings);
            s_setting_page.setting_active = 1U;
            s_setting_page.editing = 0U;
            s_setting_page.selected_index = 0U;
            ui_SettingPage_rebuild_items();
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
            if ((s_setting_page.selected_index == UI_SETTING_ITEM_BRIGHTNESS_DAY) ||
                (s_setting_page.selected_index == UI_SETTING_ITEM_BRIGHTNESS_NIGHT))
            {
                SettingsApp_ApplyActiveBrightness();
            }
            s_setting_page.editing = 0U;
            ui_SettingPage_rebuild_items();
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
        s_setting_page.edit_backup = s_setting_page.settings;
        s_setting_page.editing = 1U;
        ui_SettingPage_rebuild_items();
        egui_view_invalidate_full(ui_SettingPage);
        return true;
    }
    if ((event->id == KEY_ID_PWR) && (event->type == KEY_EVT_CLICK))
    {
        s_setting_page.setting_active = 0U;
        s_setting_page.editing = 0U;
        s_setting_page.selected_index = 0U;
        ui_SettingPage_rebuild_items();
        egui_view_invalidate_full(ui_SettingPage);
        return true;
    }

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
    egui_canvas_t *canvas = egui_view_get_canvas(self);

    (void)self;
    ui_draw_rect(canvas, 0, 0, UI_SCREEN_W, UI_SCREEN_H, 0x87CEEB);
}

static void ui_SettingPage_rebuild_items(void)
{
    egui_view_t *list_view = EGUI_VIEW_OF(&s_setting_page.list);

    egui_view_list_clear(list_view);
    for (uint8_t i = 0U; i < UI_SETTING_ITEM_COUNT; i++)
    {
        ui_SettingPage_format_item(i, s_setting_page.item_text[i], sizeof(s_setting_page.item_text[i]));
        (void)egui_view_list_add_item(list_view, s_setting_page.item_text[i]);
    }

    if (s_setting_page.setting_active != 0U)
    {
        egui_view_list_set_selected_index(list_view, s_setting_page.selected_index);
    }
    else
    {
        egui_view_list_set_selected_index(list_view, EGUI_VIEW_LIST_SELECTED_NONE);
    }
}

static void ui_SettingPage_format_item(uint8_t index, char *buf, uint16_t size)
{
    const char *prefix;
    const AppSettings_t *settings = &s_setting_page.settings;

    if ((buf == NULL) || (size == 0U))
    {
        return;
    }

    if (s_setting_page.setting_active == 0U)
    {
        prefix = "  ";
    }
    else
    {
        prefix = (index == s_setting_page.selected_index) ? ((s_setting_page.editing != 0U) ? "* " : "> ") : "  ";
    }

    switch ((ui_setting_item_t)index)
    {
    case UI_SETTING_ITEM_POETRY_ENABLE:
        (void)snprintf(buf, size, "%sPoetry popup  %s", prefix, (settings->poetry_popup_enabled != 0U) ? "On" : "Off");
        break;
    case UI_SETTING_ITEM_POETRY_INTERVAL:
        (void)snprintf(buf, size, "%sPoetry gap    %u min", prefix, (unsigned)settings->poetry_popup_interval_min);
        break;
    case UI_SETTING_ITEM_WEATHER_INTERVAL:
        (void)snprintf(buf, size, "%sWeather sync  %u min", prefix, (unsigned)settings->weather_time_sync_interval_min);
        break;
    case UI_SETTING_ITEM_BRIGHTNESS_DAY:
        (void)snprintf(buf, size, "%sDay bright    %u%%", prefix, (unsigned)settings->brightness_day_percent);
        break;
    case UI_SETTING_ITEM_BRIGHTNESS_NIGHT:
        (void)snprintf(buf, size, "%sNight bright  %u%%", prefix, (unsigned)settings->brightness_night_percent);
        break;
    case UI_SETTING_ITEM_IDLE_TIMEOUT:
        (void)snprintf(buf, size, "%sScreen sleep  %u min", prefix, (unsigned)settings->screen_idle_timeout_min);
        break;
    default:
        buf[0] = '\0';
        break;
    }
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
    ui_SettingPage_rebuild_items();
    egui_view_invalidate_full(ui_SettingPage);
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

static uint8_t ui_setting_adjust_u8(uint8_t value, int8_t delta, uint8_t step, uint8_t min_value, uint8_t max_value)
{
    int16_t next = (int16_t)value + ((int16_t)delta * (int16_t)step);

    if (next < (int16_t)min_value)
    {
        next = min_value;
    }
    if (next > (int16_t)max_value)
    {
        next = max_value;
    }

    return (uint8_t)next;
}

static void ui_SettingPage_adjust_selected(int8_t delta, uint8_t fast)
{
    uint16_t step = (fast != 0U) ? UI_SETTING_FAST_STEP : UI_SETTING_VALUE_STEP;
    AppSettings_t *settings = &s_setting_page.settings;

    switch ((ui_setting_item_t)s_setting_page.selected_index)
    {
    case UI_SETTING_ITEM_POETRY_ENABLE:
        if (delta != 0)
        {
            settings->poetry_popup_enabled = (uint8_t)!settings->poetry_popup_enabled;
        }
        break;
    case UI_SETTING_ITEM_POETRY_INTERVAL:
        settings->poetry_popup_interval_min = ui_setting_adjust_u16(settings->poetry_popup_interval_min,
                                                                    delta,
                                                                    step,
                                                                    SETTINGS_APP_POETRY_INTERVAL_MIN_MIN,
                                                                    SETTINGS_APP_POETRY_INTERVAL_MIN_MAX);
        break;
    case UI_SETTING_ITEM_WEATHER_INTERVAL:
        settings->weather_time_sync_interval_min = ui_setting_adjust_u16(settings->weather_time_sync_interval_min,
                                                                         delta,
                                                                         step,
                                                                         SETTINGS_APP_WEATHER_SYNC_INTERVAL_MIN_MIN,
                                                                         SETTINGS_APP_WEATHER_SYNC_INTERVAL_MIN_MAX);
        break;
    case UI_SETTING_ITEM_BRIGHTNESS_DAY:
        settings->brightness_day_percent = ui_setting_adjust_u8(settings->brightness_day_percent,
                                                                delta,
                                                                UI_SETTING_VALUE_STEP,
                                                                SETTINGS_APP_BRIGHTNESS_MIN,
                                                                SETTINGS_APP_BRIGHTNESS_MAX);
        SettingsApp_PreviewBrightnessPercent(settings->brightness_day_percent);
        break;
    case UI_SETTING_ITEM_BRIGHTNESS_NIGHT:
        settings->brightness_night_percent = ui_setting_adjust_u8(settings->brightness_night_percent,
                                                                  delta,
                                                                  UI_SETTING_VALUE_STEP,
                                                                  SETTINGS_APP_BRIGHTNESS_MIN,
                                                                  SETTINGS_APP_BRIGHTNESS_MAX);
        SettingsApp_PreviewBrightnessPercent(settings->brightness_night_percent);
        break;
    case UI_SETTING_ITEM_IDLE_TIMEOUT:
        settings->screen_idle_timeout_min = ui_setting_adjust_u16(settings->screen_idle_timeout_min,
                                                                  delta,
                                                                  step,
                                                                  SETTINGS_APP_SCREEN_IDLE_TIMEOUT_MIN_MIN,
                                                                  SETTINGS_APP_SCREEN_IDLE_TIMEOUT_MIN_MAX);
        break;
    default:
        break;
    }

    ui_SettingPage_rebuild_items();
    egui_view_invalidate_full(ui_SettingPage);
}

static bool ui_SettingPage_commit_edit(void)
{
    (void)SettingsApp_Update(&s_setting_page.settings);
    SettingsApp_Apply();
    s_setting_page.editing = 0U;
    ui_SettingPage_rebuild_items();
    egui_view_invalidate_full(ui_SettingPage);

    return true;
}
