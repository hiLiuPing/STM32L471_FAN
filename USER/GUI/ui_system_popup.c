#include "ui_system_popup.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/egui_timer.h"
#include "egui_port_stm32l471_fan.h"
#include "resource/egui_icon_material_symbols.h"
#include "ui_common.h"
#include "ui_poetry_popup.h"
#include "widget/egui_view.h"

#define UI_SYSTEM_POPUP_TIMER_MS       50U
#define UI_SYSTEM_POPUP_ENTER_MS       150U
#define UI_SYSTEM_POPUP_EXIT_MS        150U
#define UI_SYSTEM_POPUP_VISIBLE_MS     3000U
#define UI_SYSTEM_POPUP_PANEL_X        42
#define UI_SYSTEM_POPUP_PANEL_Y        35
#define UI_SYSTEM_POPUP_PANEL_HIDDEN_Y (-72)
#define UI_SYSTEM_POPUP_PANEL_W        344
#define UI_SYSTEM_POPUP_PANEL_H        72

typedef enum
{
    UI_SYSTEM_POPUP_IDLE = 0,
    UI_SYSTEM_POPUP_ENTERING,
    UI_SYSTEM_POPUP_HOLDING,
    UI_SYSTEM_POPUP_EXITING
} ui_system_popup_phase_t;

typedef struct
{
    const char *title;
    const char *subtitle;
    const char *icon;
    uint32_t accent;
} ui_system_popup_content_t;

typedef struct
{
    egui_view_t base;
    egui_timer_t timer;
    SystemNotifyMessage_t message;
    ui_system_popup_phase_t phase;
    uint32_t phase_started_ms;
    uint32_t visible_until_ms;
    int16_t panel_y;
    int16_t exit_started_y;
} ui_system_popup_state_t;

static ui_system_popup_state_t s_popup;
static egui_view_api_t s_popup_api;

static void ui_system_popup_timer_cb(egui_timer_t *timer);
static void ui_system_popup_on_draw(egui_view_t *self);
static void ui_system_popup_hide(void);
static void ui_system_popup_get_content(char *detail,
                                        uint16_t detail_size,
                                        ui_system_popup_content_t *content);
static bool ui_system_popup_type_is_fan_control(SystemNotifyType_t type);

void ui_system_popup_init(void)
{
    egui_view_t *view = EGUI_VIEW_OF(&s_popup.base);

    memset(&s_popup, 0, sizeof(s_popup));
    s_popup.panel_y = UI_SYSTEM_POPUP_PANEL_HIDDEN_Y;

    egui_view_init(view, egui_port_get_core());
    egui_view_copy_api(view, &s_popup_api);
    s_popup_api.on_draw = ui_system_popup_on_draw;
    view->api = &s_popup_api;
    egui_view_set_position(view, 0, 0);
    egui_view_set_size(view, UI_SCREEN_W, UI_SCREEN_H);
    egui_view_set_visible(view, 0);
    egui_core_add_user_root_view(view);
    egui_view_start_periodic(view, &s_popup.timer, &s_popup,
                             ui_system_popup_timer_cb,
                             UI_SYSTEM_POPUP_TIMER_MS);
}

void ui_system_popup_show(const SystemNotifyMessage_t *message)
{
    egui_view_t *view = EGUI_VIEW_OF(&s_popup.base);

    if ((message == NULL) || ((uint32_t)message->type >= (uint32_t)SYSTEM_NOTIFY_COUNT) ||
        (s_popup.phase != UI_SYSTEM_POPUP_IDLE))
    {
        return;
    }

    ui_poetry_popup_dismiss();
    s_popup.message = *message;
    s_popup.phase = UI_SYSTEM_POPUP_ENTERING;
    s_popup.phase_started_ms = egui_timer_get_current_time();
    s_popup.visible_until_ms = s_popup.phase_started_ms + UI_SYSTEM_POPUP_VISIBLE_MS;
    s_popup.panel_y = UI_SYSTEM_POPUP_PANEL_HIDDEN_Y;

    egui_view_remove_from_user_root(view);
    egui_core_add_user_root_view(view);
    egui_view_set_visible(view, 1);
    egui_view_invalidate_full(view);
    egui_core_force_refresh(egui_port_get_core());
}

bool ui_system_popup_show_or_update(const SystemNotifyMessage_t *message)
{
    uint32_t now;

    if ((message == NULL) ||
        ((uint32_t)message->type >= (uint32_t)SYSTEM_NOTIFY_COUNT) ||
        !ui_system_popup_type_is_fan_control(message->type))
    {
        return false;
    }

    if (s_popup.phase == UI_SYSTEM_POPUP_IDLE)
    {
        ui_system_popup_show(message);
        return ui_system_popup_is_visible();
    }

    if (!ui_system_popup_type_is_fan_control(s_popup.message.type))
    {
        return false;
    }

    now = egui_timer_get_current_time();
    s_popup.message = *message;
    s_popup.visible_until_ms = now + UI_SYSTEM_POPUP_VISIBLE_MS;
    if (s_popup.phase == UI_SYSTEM_POPUP_EXITING)
    {
        s_popup.phase = UI_SYSTEM_POPUP_HOLDING;
        s_popup.phase_started_ms = now;
        s_popup.panel_y = UI_SYSTEM_POPUP_PANEL_Y;
    }
    egui_view_invalidate_full(EGUI_VIEW_OF(&s_popup.base));
    egui_core_force_refresh(egui_port_get_core());
    return true;
}

bool ui_system_popup_is_visible(void)
{
    return (s_popup.phase != UI_SYSTEM_POPUP_IDLE);
}

bool ui_system_popup_is_fan_control(void)
{
    return ui_system_popup_is_visible() &&
           ui_system_popup_type_is_fan_control(s_popup.message.type);
}

void ui_system_popup_dismiss(void)
{
    if ((s_popup.phase == UI_SYSTEM_POPUP_IDLE) ||
        (s_popup.phase == UI_SYSTEM_POPUP_EXITING))
    {
        return;
    }

    s_popup.phase = UI_SYSTEM_POPUP_EXITING;
    s_popup.phase_started_ms = egui_timer_get_current_time();
    s_popup.exit_started_y = s_popup.panel_y;
    egui_view_invalidate_full(EGUI_VIEW_OF(&s_popup.base));
}

void ui_system_popup_dismiss_immediate(void)
{
    if (s_popup.phase != UI_SYSTEM_POPUP_IDLE)
    {
        ui_system_popup_hide();
    }
}

static void ui_system_popup_timer_cb(egui_timer_t *timer)
{
    uint32_t now;
    uint32_t elapsed;
    int32_t travel;

    (void)timer;

    if (s_popup.phase == UI_SYSTEM_POPUP_IDLE)
    {
        return;
    }

    now = egui_timer_get_current_time();
    elapsed = (uint32_t)(now - s_popup.phase_started_ms);
    travel = UI_SYSTEM_POPUP_PANEL_Y - UI_SYSTEM_POPUP_PANEL_HIDDEN_Y;

    if (s_popup.phase == UI_SYSTEM_POPUP_ENTERING)
    {
        if (elapsed >= UI_SYSTEM_POPUP_ENTER_MS)
        {
            s_popup.phase = UI_SYSTEM_POPUP_HOLDING;
            s_popup.phase_started_ms = now;
            s_popup.panel_y = UI_SYSTEM_POPUP_PANEL_Y;
        }
        else
        {
            s_popup.panel_y = (int16_t)(UI_SYSTEM_POPUP_PANEL_HIDDEN_Y +
                                        (travel * (int32_t)elapsed) /
                                            (int32_t)UI_SYSTEM_POPUP_ENTER_MS);
        }
        egui_view_invalidate_full(EGUI_VIEW_OF(&s_popup.base));
        return;
    }

    if (s_popup.phase == UI_SYSTEM_POPUP_HOLDING)
    {
        if ((int32_t)(now - s_popup.visible_until_ms) >= 0)
        {
            s_popup.phase = UI_SYSTEM_POPUP_EXITING;
            s_popup.phase_started_ms = now;
            s_popup.panel_y = UI_SYSTEM_POPUP_PANEL_Y;
            s_popup.exit_started_y = UI_SYSTEM_POPUP_PANEL_Y;
        }
        return;
    }

    if (elapsed >= UI_SYSTEM_POPUP_EXIT_MS)
    {
        ui_system_popup_hide();
        return;
    }

    travel = s_popup.exit_started_y - UI_SYSTEM_POPUP_PANEL_HIDDEN_Y;
    s_popup.panel_y = (int16_t)(s_popup.exit_started_y -
                                (travel * (int32_t)elapsed) /
                                    (int32_t)UI_SYSTEM_POPUP_EXIT_MS);
    egui_view_invalidate_full(EGUI_VIEW_OF(&s_popup.base));
}

static void ui_system_popup_hide(void)
{
    egui_view_t *view = EGUI_VIEW_OF(&s_popup.base);

    s_popup.phase = UI_SYSTEM_POPUP_IDLE;
    s_popup.phase_started_ms = 0U;
    s_popup.visible_until_ms = 0U;
    s_popup.panel_y = UI_SYSTEM_POPUP_PANEL_HIDDEN_Y;
    s_popup.exit_started_y = UI_SYSTEM_POPUP_PANEL_HIDDEN_Y;
    memset(&s_popup.message, 0, sizeof(s_popup.message));
    egui_view_set_visible(view, 0);
    egui_view_invalidate_full(view);
    egui_core_force_refresh(egui_port_get_core());
}

static bool ui_system_popup_type_is_fan_control(SystemNotifyType_t type)
{
    return (type == SYSTEM_NOTIFY_FAN_ON) ||
           (type == SYSTEM_NOTIFY_FAN_OFF) ||
           (type == SYSTEM_NOTIFY_FAN_SPEED);
}

static void ui_system_popup_get_content(char *detail,
                                        uint16_t detail_size,
                                        ui_system_popup_content_t *content)
{
    content->title = "SYSTEM NOTICE";
    content->subtitle = "Status updated";
    content->icon = EGUI_ICON_MS_INFO;
    content->accent = 0x38BDF8;

    switch (s_popup.message.type)
    {
    case SYSTEM_NOTIFY_POWER_IN:
        content->title = "POWER IN";
        content->subtitle = "External power connected";
        content->icon = EGUI_ICON_MS_DOWNLOAD;
        content->accent = 0x22D3EE;
        break;

    case SYSTEM_NOTIFY_POWER_OUT:
        content->title = "POWER OUT";
        content->subtitle = "Running on battery";
        content->icon = EGUI_ICON_MS_UPLOAD;
        content->accent = 0xFBBF24;
        break;

    case SYSTEM_NOTIFY_CHARGE_COMPLETE:
        content->title = "CHARGE COMPLETE";
        content->subtitle = "Battery is fully charged";
        content->icon = EGUI_ICON_MS_BATTERY_FULL;
        content->accent = 0x34D399;
        break;

    case SYSTEM_NOTIFY_LOW_BATTERY:
        content->title = "LOW BATTERY";
        (void)snprintf(detail, detail_size, "%d%% remaining", (int)s_popup.message.value0);
        content->subtitle = detail;
        content->icon = EGUI_ICON_MS_WARNING;
        content->accent = 0xF59E0B;
        break;

    case SYSTEM_NOTIFY_WEATHER_SYNC_START:
        content->title = "WEATHER SYNC";
        content->subtitle = "Updating weather data";
        content->icon = EGUI_ICON_MS_SYNC;
        content->accent = 0x38BDF8;
        break;

    case SYSTEM_NOTIFY_WEATHER_SYNC_COMPLETE:
        content->title = "WEATHER UPDATED";
        content->subtitle = "Weather sync complete";
        content->icon = EGUI_ICON_MS_CHECK;
        content->accent = 0x2DD4BF;
        break;

    case SYSTEM_NOTIFY_WEATHER_SYNC_FAILED:
        content->title = "WEATHER SYNC FAILED";
        content->subtitle = "Check connection and try again";
        content->icon = EGUI_ICON_MS_WARNING;
        content->accent = 0xF87171;
        break;

    case SYSTEM_NOTIFY_ENVIRONMENT_ALERT:
        {
            int16_t temperature = s_popup.message.value0;
            int16_t absolute_temperature = (temperature < 0) ?
                                               (int16_t)-temperature :
                                               temperature;

        content->title = "ENVIRONMENT ALERT";
        (void)snprintf(detail, detail_size, "%s%d.%d C / %d%% RH",
                       (temperature < 0) ? "-" : "",
                       (int)(absolute_temperature / 10),
                       (int)(absolute_temperature % 10),
                       (int)s_popup.message.value1);
        content->subtitle = detail;
        content->icon = EGUI_ICON_MS_WARNING;
        content->accent = 0xFB7185;
        break;
        }

    case SYSTEM_NOTIFY_FAN_ON:
        content->title = "FAN ON";
        content->subtitle = "Fan started";
        content->icon = EGUI_ICON_MS_PLAY_ARROW;
        content->accent = 0x4ADE80;
        break;

    case SYSTEM_NOTIFY_FAN_OFF:
        content->title = "FAN OFF";
        content->subtitle = "Fan stopped";
        content->icon = EGUI_ICON_MS_PAUSE;
        content->accent = 0x94A3B8;
        break;

    case SYSTEM_NOTIFY_FAN_SPEED:
        content->title = "FAN SPEED";
        (void)snprintf(detail, detail_size, "%d%%", (int)s_popup.message.value0);
        content->subtitle = detail;
        content->icon = EGUI_ICON_MS_INFO;
        content->accent = 0x22D3EE;
        break;

    case SYSTEM_NOTIFY_COUNT:
    default:
        break;
    }
}

static void ui_system_popup_on_draw(egui_view_t *self)
{
    egui_canvas_t *canvas;
    ui_system_popup_content_t content;
    char detail[48];
    int16_t icon_x = UI_SYSTEM_POPUP_PANEL_X + 37;
    int16_t icon_y = s_popup.panel_y + (UI_SYSTEM_POPUP_PANEL_H / 2);

    if (s_popup.phase == UI_SYSTEM_POPUP_IDLE)
    {
        return;
    }

    canvas = egui_view_get_canvas(self);
    detail[0] = '\0';
    ui_system_popup_get_content(detail, sizeof(detail), &content);

    ui_draw_round_panel(canvas,
                        UI_SYSTEM_POPUP_PANEL_X,
                        s_popup.panel_y,
                        UI_SYSTEM_POPUP_PANEL_W,
                        UI_SYSTEM_POPUP_PANEL_H,
                        8, 0x07111F, content.accent);
    ui_draw_rect(canvas,
                 UI_SYSTEM_POPUP_PANEL_X,
                 (egui_dim_t)(s_popup.panel_y + 8),
                 3,
                 UI_SYSTEM_POPUP_PANEL_H - 16,
                 content.accent);
    egui_canvas_draw_circle_fill_basic(canvas, icon_x, icon_y, 21,
                                       ui_color(0x111827), EGUI_ALPHA_100);
    egui_canvas_draw_circle_basic(canvas, icon_x, icon_y, 21, 1,
                                  ui_color(content.accent), EGUI_ALPHA_80);
    ui_draw_text(canvas, EGUI_FONT_ICON_MS_24, content.icon,
                 icon_x - 12, icon_y - 12, 24, 24,
                 EGUI_ALIGN_CENTER, content.accent);
    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_16_4),
                 content.title,
                 UI_SYSTEM_POPUP_PANEL_X + 70,
                 s_popup.panel_y + 12,
                 250, 22,
                 EGUI_ALIGN_LEFT | EGUI_ALIGN_VCENTER,
                 0xF8FAFC);
    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_12_4),
                 content.subtitle,
                 UI_SYSTEM_POPUP_PANEL_X + 70,
                 s_popup.panel_y + 38,
                 250, 20,
                 EGUI_ALIGN_LEFT | EGUI_ALIGN_VCENTER,
                 0xCBD5E1);
}
