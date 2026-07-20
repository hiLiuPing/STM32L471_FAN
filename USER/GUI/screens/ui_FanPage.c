#include "ui_FanPage.h"

#include <stdio.h>
#include <string.h>

#include "core/egui_timer.h"
#include "egui_port_stm32l471_fan.h"
#include "fan_anim_res.h"
#include "fan_app.h"
#include "key.h"
#include "page_manager.h"
#include "sensors_app.h"
#include "ui_common.h"
#include "widget/egui_view_animated_image.h"
#include "widget/egui_view_gauge.h"
#include "widget/egui_view_group.h"
#include "widget/egui_view_label.h"

#define FAN_SETTING_COUNT             5U
#define FAN_SETTING_POWER             0U
#define FAN_SETTING_MODE              1U
#define FAN_SETTING_SPEED             2U
#define FAN_SETTING_INTENSITY         3U
#define FAN_SETTING_AUTO_OFF          4U
#define FAN_AUTO_OFF_STEP_MIN         30U
#define FAN_PERCENT_STEP              5U
#define FAN_PERCENT_FAST_STEP         10U
#define FAN_AUTO_OFF_FAST_STEP        60U
#define FAN_STATUS_REFRESH_MS         50U
#define FAN_CONFIRM_MS                360U
#define FAN_ANIM_VIEW_SIZE            82U
#define FAN_GAUGE_SIZE                112U
#define FAN_SETTING_ROW_X             132
#define FAN_SETTING_ROW_W             288
#define FAN_SETTING_ROW_Y             10
#define FAN_SETTING_ROW_H             23
#define FAN_SETTING_ROW_GAP           3
#define FAN_SETTING_ENTER_STEP_MS     65U
#define FAN_SETTING_ENTER_LENGTH_MS   260U
#define FAN_PARTICLE_COUNT             8U

typedef struct
{
    egui_view_group_t root;
    egui_timer_t timer;
    egui_view_animated_image_t fan_image;
    egui_view_gauge_t speed_gauge;
    egui_view_label_t speed_label;
    egui_view_label_t mode_label;
    egui_view_label_t rpm_label;
    fan_state_t state;
    fan_state_t edit_state;
    fan_state_t edit_backup;
    uint32_t enter_tick;
    uint32_t frame_tick;
    uint32_t confirm_until;
    uint16_t hue;
    fan_mode_t anim_mode;
    uint8_t setting_active;
    uint8_t editing;
    uint8_t focus_index;
    uint8_t animation_active;
    uint8_t power_stale;
    char power_text[16];
    char speed_text[16];
    char rpm_text[16];
    char mode_text[24];
} ui_fan_page_t;

static ui_fan_page_t s_fan_page;
static egui_view_api_t s_fan_api;

egui_view_t *ui_FanPage = NULL;

static const int16_t s_particle_base_x[FAN_PARTICLE_COUNT] = {12, 31, 55, 79, 102, 19, 66, 111};
static const int16_t s_particle_base_y[FAN_PARTICLE_COUNT] = {18, 37, 24, 49, 31, 72, 84, 66};
static const uint8_t s_particle_size[FAN_PARTICLE_COUNT] = {1, 2, 1, 2, 1, 1, 2, 1};
static const char *const s_fan_setting_labels[FAN_SETTING_COUNT] = {
    "Power",
    "Mode",
    "Speed",
    "Boost",
    "Auto off",
};

static void ui_FanPage_on_draw(egui_view_t *self);
static void ui_FanPage_timer_cb(egui_timer_t *timer);

static void ui_FanPage_activate_animation(void)
{
    uint32_t now = egui_timer_get_current_time();

    s_fan_page.enter_tick = now;
    s_fan_page.frame_tick = now;
    s_fan_page.animation_active = 1U;
}

static uint32_t ui_fan_hsv_to_rgb(uint16_t hue, uint8_t saturation, uint8_t value)
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

static uint8_t ui_fan_clamp_percent(uint8_t value, int8_t direction, uint8_t step)
{
    int16_t next = (int16_t)value + ((int16_t)direction * (int16_t)step);

    if (next < 0)
    {
        return 0U;
    }
    if (next > 100)
    {
        return 100U;
    }
    return (uint8_t)next;
}

static uint16_t ui_fan_clamp_auto_off(uint16_t value, int8_t direction, uint16_t step)
{
    int32_t next;

    if (direction > 0)
    {
        if (value == FAN_AUTO_OFF_MINUTES_DISABLED)
        {
            return FAN_AUTO_OFF_MINUTES_MIN;
        }
        next = (int32_t)value + (int32_t)step;
        return (next > (int32_t)FAN_AUTO_OFF_MINUTES_MAX) ? FAN_AUTO_OFF_MINUTES_MAX : (uint16_t)next;
    }

    if (direction < 0)
    {
        if (value <= FAN_AUTO_OFF_MINUTES_MIN)
        {
            return FAN_AUTO_OFF_MINUTES_DISABLED;
        }
        next = (int32_t)value - (int32_t)step;
        return (next < (int32_t)FAN_AUTO_OFF_MINUTES_MIN) ? FAN_AUTO_OFF_MINUTES_DISABLED : (uint16_t)next;
    }

    return value;
}

static fan_mode_t ui_FanPage_visible_mode(const fan_state_t *state)
{
    if (state == NULL)
    {
        return FAN_MODE_OFF;
    }
    if (state->mode != FAN_MODE_OFF)
    {
        return state->mode;
    }
    if ((state->last_mode > FAN_MODE_OFF) && (state->last_mode < FAN_MODE_COUNT))
    {
        return state->last_mode;
    }
    return FAN_MODE_MANUAL;
}

static fan_mode_t ui_FanPage_next_mode(fan_mode_t mode, int8_t direction)
{
    int8_t index;

    if ((mode <= FAN_MODE_OFF) || (mode >= FAN_MODE_COUNT))
    {
        mode = FAN_MODE_MANUAL;
    }
    index = (int8_t)(mode - FAN_MODE_MANUAL);
    index = (int8_t)(index + direction);
    if (index < 0)
    {
        index = (int8_t)(FAN_MODE_COUNT - FAN_MODE_MANUAL - 1U);
    }
    else if (index >= (int8_t)(FAN_MODE_COUNT - FAN_MODE_MANUAL))
    {
        index = 0;
    }
    return (fan_mode_t)(index + FAN_MODE_MANUAL);
}

static void ui_FanPage_init_label(egui_view_label_t *label,
                                  egui_core_t *core,
                                  egui_dim_t x,
                                  egui_dim_t y,
                                  egui_dim_t w,
                                  egui_dim_t h,
                                  const egui_font_t *font,
                                  uint32_t color,
                                  uint8_t align,
                                  const char *text)
{
    egui_view_t *view = EGUI_VIEW_OF(label);

    egui_view_label_init(view, core);
    egui_view_set_position(view, x, y);
    egui_view_set_size(view, w, h);
    egui_view_label_set_font(view, font);
    egui_view_label_set_font_color(view, ui_color(color), EGUI_ALPHA_100);
    egui_view_label_set_align_type(view, align);
    egui_view_label_set_text(view, text);
}

static void ui_FanPage_update_power_text(void)
{
    sensors_snapshot_t sensors;
    float power_mw;
    uint32_t power_x10 = 0U;

    SensorsApp_GetSnapshot(&sensors);
    s_fan_page.power_stale = (uint8_t)((sensors.ina226.health.valid == 0U) ||
                                       (sensors.ina226.health.stale != 0U));
    if (sensors.ina226.health.valid == 0U)
    {
        (void)snprintf(s_fan_page.power_text, sizeof(s_fan_page.power_text), "--");
        return;
    }
    power_mw = sensors.ina226.value.power;
    if (power_mw > 0.0f)
    {
        power_x10 = (power_mw >= 999900.0f) ? 9999U : (uint32_t)((power_mw + 50.0f) / 100.0f);
    }
    (void)snprintf(s_fan_page.power_text, sizeof(s_fan_page.power_text), "%s%lu.%luW",
                   (sensors.ina226.health.stale != 0U) ? "*" : "",
                   (unsigned long)(power_x10 / 10U), (unsigned long)(power_x10 % 10U));
}

static void ui_FanPage_sync_anim(fan_mode_t mode)
{
    const egui_image_t **frames;
    uint8_t frame_count;
    uint16_t interval_ms;
    egui_view_t *image_view = EGUI_VIEW_OF(&s_fan_page.fan_image);

    if (mode == s_fan_page.anim_mode)
    {
        return;
    }
    frames = FanAnimRes_GetFrames(mode, &frame_count, &interval_ms);
    egui_view_animated_image_set_frames(image_view, frames, frame_count);
    egui_view_animated_image_set_interval(image_view, interval_ms);
    if ((mode == FAN_MODE_OFF) || (frame_count <= 1U))
    {
        egui_view_animated_image_stop(image_view);
    }
    else
    {
        egui_view_animated_image_play(image_view);
    }
    s_fan_page.anim_mode = mode;
}

static void ui_FanPage_sync_from_state(void)
{
    fan_state_t state;
    fan_mode_t display_mode;

    FanApp_GetState(&state);
    s_fan_page.state = state;
    if (s_fan_page.editing == 0U)
    {
        s_fan_page.edit_state = state;
    }
    else
    {
        s_fan_page.edit_state.current_speed_percent = state.current_speed_percent;
        s_fan_page.edit_state.target_speed_percent = state.target_speed_percent;
        s_fan_page.edit_state.current_rpm = state.current_rpm;
        s_fan_page.edit_state.current_temp_x10 = state.current_temp_x10;
        s_fan_page.edit_state.current_hum_percent = state.current_hum_percent;
        s_fan_page.edit_state.pwm_compare = state.pwm_compare;
    }

    display_mode = ui_FanPage_visible_mode(&state);
    ui_FanPage_sync_anim((state.power_on != 0U) ? state.mode : FAN_MODE_OFF);
    ui_FanPage_update_power_text();

    (void)snprintf(s_fan_page.speed_text, sizeof(s_fan_page.speed_text), "%u%%", state.current_speed_percent);
    (void)snprintf(s_fan_page.rpm_text, sizeof(s_fan_page.rpm_text), "%u RPM", (unsigned)state.current_rpm);
    (void)snprintf(s_fan_page.mode_text, sizeof(s_fan_page.mode_text), "%s", FanApp_GetModeName(display_mode));
    egui_view_gauge_set_value(EGUI_VIEW_OF(&s_fan_page.speed_gauge), state.current_speed_percent);
    egui_view_label_set_text(EGUI_VIEW_OF(&s_fan_page.speed_label), s_fan_page.speed_text);
    egui_view_label_set_text(EGUI_VIEW_OF(&s_fan_page.rpm_label), s_fan_page.rpm_text);
    egui_view_label_set_text(EGUI_VIEW_OF(&s_fan_page.mode_label), s_fan_page.mode_text);
}

void ui_FanPage_screen_init(void)
{
    egui_core_t *core = egui_port_get_core();
    egui_view_t *view = EGUI_VIEW_OF(&s_fan_page.root);

    memset(&s_fan_page, 0, sizeof(s_fan_page));
    ui_FanPage = view;
    egui_view_group_init(view, core);
    egui_view_copy_api(view, &s_fan_api);
    s_fan_api.on_draw = ui_FanPage_on_draw;
    view->api = &s_fan_api;
    egui_view_set_position(view, 0, 0);
    egui_view_set_size(view, UI_SCREEN_W, UI_SCREEN_H);
    egui_view_set_visible(view, 1);

    s_fan_page.anim_mode = FAN_MODE_COUNT;
    s_fan_page.focus_index = FAN_SETTING_POWER;
    s_fan_page.hue = 190U;
    ui_FanPage_activate_animation();

    egui_view_animated_image_init(EGUI_VIEW_OF(&s_fan_page.fan_image), core);
    egui_view_set_position(EGUI_VIEW_OF(&s_fan_page.fan_image), 23, 10);
    egui_view_set_size(EGUI_VIEW_OF(&s_fan_page.fan_image), FAN_ANIM_VIEW_SIZE, FAN_ANIM_VIEW_SIZE);
    egui_view_animated_image_set_fit_to_view(EGUI_VIEW_OF(&s_fan_page.fan_image), 1U);

    egui_view_gauge_init(EGUI_VIEW_OF(&s_fan_page.speed_gauge), core);
    egui_view_set_position(EGUI_VIEW_OF(&s_fan_page.speed_gauge), 8, 4);
    egui_view_set_size(EGUI_VIEW_OF(&s_fan_page.speed_gauge), FAN_GAUGE_SIZE, FAN_GAUGE_SIZE);
    egui_view_gauge_set_start_angle(EGUI_VIEW_OF(&s_fan_page.speed_gauge), -90);
    egui_view_gauge_set_sweep_angle(EGUI_VIEW_OF(&s_fan_page.speed_gauge), 360);
    egui_view_gauge_set_stroke_width(EGUI_VIEW_OF(&s_fan_page.speed_gauge), 7);
    egui_view_gauge_set_needle_width(EGUI_VIEW_OF(&s_fan_page.speed_gauge), 1);
    egui_view_gauge_set_bk_color(EGUI_VIEW_OF(&s_fan_page.speed_gauge), ui_color(0x233B53));
    egui_view_gauge_set_progress_color(EGUI_VIEW_OF(&s_fan_page.speed_gauge), ui_color(0x22D3EE));
    egui_view_gauge_set_needle_color(EGUI_VIEW_OF(&s_fan_page.speed_gauge), ui_color(0x67E8F9));
    egui_view_gauge_set_text_color(EGUI_VIEW_OF(&s_fan_page.speed_gauge), ui_color(0xE0F7FA));
    egui_view_gauge_set_font(EGUI_VIEW_OF(&s_fan_page.speed_gauge), EGUI_FONT_OF(&egui_res_font_montserrat_10_4));

    ui_FanPage_init_label(&s_fan_page.speed_label, core, 20, 88, 88, 22,
                          EGUI_FONT_OF(&egui_res_font_montserrat_20_4), 0xF8FAFC,
                          EGUI_ALIGN_CENTER, s_fan_page.speed_text);
    ui_FanPage_init_label(&s_fan_page.mode_label, core, 8, 112, 58, 16,
                          EGUI_FONT_OF(&egui_res_font_montserrat_10_4), 0x7DD3FC,
                          EGUI_ALIGN_CENTER, s_fan_page.mode_text);
    ui_FanPage_init_label(&s_fan_page.rpm_label, core, 67, 112, 59, 16,
                          EGUI_FONT_OF(&egui_res_font_montserrat_10_4), 0xCBD5E1,
                          EGUI_ALIGN_CENTER, s_fan_page.rpm_text);

    egui_view_group_add_child(view, EGUI_VIEW_OF(&s_fan_page.speed_gauge));
    egui_view_group_add_child(view, EGUI_VIEW_OF(&s_fan_page.fan_image));
    egui_view_group_add_child(view, EGUI_VIEW_OF(&s_fan_page.speed_label));
    egui_view_group_add_child(view, EGUI_VIEW_OF(&s_fan_page.mode_label));
    egui_view_group_add_child(view, EGUI_VIEW_OF(&s_fan_page.rpm_label));

    ui_FanPage_sync_from_state();
    egui_view_start_periodic(view, &s_fan_page.timer, view, ui_FanPage_timer_cb, FAN_STATUS_REFRESH_MS);
}

void ui_FanPage_screen_destroy(void)
{
    s_fan_page.setting_active = 0U;
    s_fan_page.editing = 0U;
    s_fan_page.animation_active = 0U;
}

static void ui_FanPage_move_selection(int8_t delta)
{
    int16_t next = (int16_t)s_fan_page.focus_index + delta;

    if (next < 0)
    {
        next = (int16_t)(FAN_SETTING_COUNT - 1U);
    }
    else if (next >= (int16_t)FAN_SETTING_COUNT)
    {
        next = 0;
    }
    s_fan_page.focus_index = (uint8_t)next;
    egui_view_invalidate_full(ui_FanPage);
}

static void ui_FanPage_start_edit(void)
{
    s_fan_page.edit_state = s_fan_page.state;
    s_fan_page.edit_backup = s_fan_page.state;
    s_fan_page.editing = 1U;
    egui_view_invalidate_full(ui_FanPage);
}

static void ui_FanPage_adjust_current(int8_t direction, uint8_t fast)
{
    uint8_t percent_step = (fast != 0U) ? FAN_PERCENT_FAST_STEP : FAN_PERCENT_STEP;
    uint16_t timer_step = (fast != 0U) ? FAN_AUTO_OFF_FAST_STEP : FAN_AUTO_OFF_STEP_MIN;

    switch (s_fan_page.focus_index)
    {
    case FAN_SETTING_SPEED:
        s_fan_page.edit_state.base_speed_percent =
            ui_fan_clamp_percent(s_fan_page.edit_state.base_speed_percent, direction, percent_step);
        break;
    case FAN_SETTING_INTENSITY:
        s_fan_page.edit_state.intensity_percent =
            ui_fan_clamp_percent(s_fan_page.edit_state.intensity_percent, direction, percent_step);
        break;
    case FAN_SETTING_AUTO_OFF:
        s_fan_page.edit_state.auto_off_min =
            ui_fan_clamp_auto_off(s_fan_page.edit_state.auto_off_min, direction, timer_step);
        break;
    default:
        break;
    }
    egui_view_invalidate_full(ui_FanPage);
}

static bool ui_FanPage_commit_edit(void)
{
    switch (s_fan_page.focus_index)
    {
    case FAN_SETTING_SPEED:
        if (s_fan_page.edit_state.base_speed_percent != s_fan_page.edit_backup.base_speed_percent)
        {
            (void)FanApp_SetSpeed(s_fan_page.edit_state.base_speed_percent, 0U);
        }
        break;
    case FAN_SETTING_INTENSITY:
        if (s_fan_page.edit_state.intensity_percent != s_fan_page.edit_backup.intensity_percent)
        {
            (void)FanApp_SetIntensity(s_fan_page.edit_state.intensity_percent, 0U);
        }
        break;
    case FAN_SETTING_AUTO_OFF:
        if (s_fan_page.edit_state.auto_off_min != s_fan_page.edit_backup.auto_off_min)
        {
            (void)FanApp_SetAutoOffMinutes(s_fan_page.edit_state.auto_off_min, 0U);
        }
        break;
    default:
        break;
    }
    s_fan_page.editing = 0U;
    s_fan_page.confirm_until = egui_timer_get_current_time() + FAN_CONFIRM_MS;
    ui_FanPage_sync_from_state();
    egui_view_invalidate_full(ui_FanPage);
    return true;
}

static void ui_FanPage_toggle_power(void)
{
    (void)FanApp_SetPower(s_fan_page.state.power_on == 0U, 0U);
    s_fan_page.confirm_until = egui_timer_get_current_time() + FAN_CONFIRM_MS;
    ui_FanPage_sync_from_state();
    egui_view_invalidate_full(ui_FanPage);
}

static bool ui_FanPage_cycle_mode(void)
{
    fan_mode_t next_mode = ui_FanPage_next_mode(ui_FanPage_visible_mode(&s_fan_page.state), 1);

    if (!FanApp_SetMode(next_mode, 0U))
    {
        return false;
    }

    s_fan_page.state.power_on = 1U;
    s_fan_page.state.mode = next_mode;
    s_fan_page.state.last_mode = next_mode;
    s_fan_page.edit_state = s_fan_page.state;
    (void)snprintf(s_fan_page.mode_text, sizeof(s_fan_page.mode_text), "%s",
                   FanApp_GetModeName(next_mode));
    egui_view_label_set_text(EGUI_VIEW_OF(&s_fan_page.mode_label), s_fan_page.mode_text);
    ui_FanPage_sync_anim(next_mode);
    s_fan_page.confirm_until = egui_timer_get_current_time() + FAN_CONFIRM_MS;
    egui_view_invalidate_full(ui_FanPage);
    return true;
}

bool ui_FanPage_key_handler(void *key_event)
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

    if (s_fan_page.setting_active == 0U)
    {
        if ((event->id == KEY_ID_OK) && (event->type == KEY_EVT_CLICK))
        {
            s_fan_page.setting_active = 1U;
            s_fan_page.editing = 0U;
            s_fan_page.focus_index = FAN_SETTING_POWER;
            ui_FanPage_sync_from_state();
            egui_view_invalidate_full(ui_FanPage);
            return true;
        }
        return ui_page_consume_nav_key_event(key_event);
    }

    if (s_fan_page.editing != 0U)
    {
        if (event->id == KEY_ID_UP)
        {
            ui_FanPage_adjust_current(-1, (uint8_t)(event->type != KEY_EVT_CLICK));
            return true;
        }
        if (event->id == KEY_ID_DOWN)
        {
            ui_FanPage_adjust_current(1, (uint8_t)(event->type != KEY_EVT_CLICK));
            return true;
        }
        if ((event->id == KEY_ID_OK) && (event->type == KEY_EVT_CLICK))
        {
            return ui_FanPage_commit_edit();
        }
        if ((event->id == KEY_ID_PWR) && (event->type == KEY_EVT_CLICK))
        {
            s_fan_page.edit_state = s_fan_page.edit_backup;
            s_fan_page.editing = 0U;
            ui_FanPage_sync_from_state();
            egui_view_invalidate_full(ui_FanPage);
            return true;
        }
        return true;
    }

    if (event->id == KEY_ID_UP)
    {
        ui_FanPage_move_selection(-1);
        return true;
    }
    if (event->id == KEY_ID_DOWN)
    {
        ui_FanPage_move_selection(1);
        return true;
    }
    if ((event->id == KEY_ID_OK) && (event->type == KEY_EVT_CLICK))
    {
        if (s_fan_page.focus_index == FAN_SETTING_POWER)
        {
            ui_FanPage_toggle_power();
        }
        else if (s_fan_page.focus_index == FAN_SETTING_MODE)
        {
            (void)ui_FanPage_cycle_mode();
        }
        else
        {
            ui_FanPage_start_edit();
        }
        return true;
    }
    if ((event->id == KEY_ID_PWR) && (event->type == KEY_EVT_CLICK))
    {
        s_fan_page.setting_active = 0U;
        s_fan_page.editing = 0U;
        ui_FanPage_sync_from_state();
        egui_view_invalidate_full(ui_FanPage);
        return true;
    }
    return true;
}

static void ui_FanPage_timer_cb(egui_timer_t *timer)
{
    egui_view_t *view = (egui_view_t *)timer->user_data;

    if ((view == NULL) || !egui_view_get_visible(view))
    {
        return;
    }

    if (s_fan_page.animation_active == 0U)
    {
        ui_FanPage_activate_animation();
    }

    s_fan_page.frame_tick = egui_timer_get_current_time();
    s_fan_page.hue = (uint16_t)((s_fan_page.hue + 4U) % 360U);
    egui_view_animated_image_update(EGUI_VIEW_OF(&s_fan_page.fan_image), FAN_STATUS_REFRESH_MS);
    ui_FanPage_sync_from_state();
    egui_view_invalidate_full(view);
}

static void ui_fan_draw_switch(egui_canvas_t *canvas, int16_t x, int16_t y, uint8_t enabled, uint32_t accent)
{
    uint32_t track = (enabled != 0U) ? accent : 0x334155;
    int16_t knob_x = (enabled != 0U) ? (x + 25) : (x + 7);

    egui_canvas_draw_round_rectangle_fill(canvas, x, y, 38, 16, 8, ui_color(track), EGUI_ALPHA_100);
    egui_canvas_draw_circle_fill_basic(canvas, knob_x, y + 8, 5, ui_color(0xF8FAFC), EGUI_ALPHA_100);
}

static void ui_FanPage_draw_background(egui_canvas_t *canvas)
{
    uint32_t accent = ui_fan_hsv_to_rgb(s_fan_page.hue, 215U, 255U);
    uint16_t phase = (uint16_t)((s_fan_page.frame_tick / 20U) % UI_SCREEN_W);

    ui_draw_rect(canvas, 0, 0, UI_SCREEN_W, UI_SCREEN_H, 0x07111F);
    ui_draw_rect(canvas, 0, 0, 128, UI_SCREEN_H, 0x0B1F33);

    for (uint8_t i = 0U; i < 6U; i++)
    {
        int16_t x = (int16_t)(((phase + (uint16_t)i * 86U) % (UI_SCREEN_W + 70U)) - 35);
        uint32_t color = ui_fan_hsv_to_rgb((uint16_t)(s_fan_page.hue + (uint16_t)i * 54U), 220U, 230U);
        egui_canvas_draw_line(canvas, x, 0, x + 36, UI_SCREEN_H, 2, ui_color(color), EGUI_ALPHA_20);
    }

    for (uint8_t i = 0U; i < FAN_PARTICLE_COUNT; i++)
    {
        int16_t drift = (int16_t)(((s_fan_page.frame_tick / (35U + (uint32_t)i * 3U)) + (uint32_t)i * 9U) % 28U);
        int16_t y = (int16_t)((s_particle_base_y[i] + drift) % 104);
        uint32_t color = ui_fan_hsv_to_rgb((uint16_t)(s_fan_page.hue + (uint16_t)i * 38U), 190U, 255U);
        egui_canvas_draw_circle_fill_basic(canvas, s_particle_base_x[i], y + 7, s_particle_size[i], ui_color(color), EGUI_ALPHA_80);
    }

    egui_canvas_draw_circle_basic(canvas, 64, 50, 57, 2, ui_color(accent), EGUI_ALPHA_60);
    egui_canvas_draw_circle_basic(canvas, 64, 50, 49, 1, ui_color(0xE2E8F0), EGUI_ALPHA_30);
    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_10_4), "FAN", 48, 8, 32, 15,
                 EGUI_ALIGN_CENTER, 0xF8FAFC);
    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_10_4), s_fan_page.power_text, 8, 8, 38, 15,
                 EGUI_ALIGN_LEFT | EGUI_ALIGN_VCENTER,
                 (s_fan_page.power_stale != 0U) ? 0x64748B : accent);
    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_10_4),
                 (s_fan_page.setting_active != 0U) ? "SET" : "LIVE", 82, 8, 39, 15,
                 EGUI_ALIGN_RIGHT | EGUI_ALIGN_VCENTER, accent);
}

static void ui_FanPage_format_value(uint8_t index, const fan_state_t *state, char *buf, uint16_t size)
{
    switch (index)
    {
    case FAN_SETTING_MODE:
        (void)snprintf(buf, size, "%s", FanApp_GetModeName(ui_FanPage_visible_mode(state)));
        break;
    case FAN_SETTING_SPEED:
        (void)snprintf(buf, size, "%u%%", (unsigned)state->base_speed_percent);
        break;
    case FAN_SETTING_INTENSITY:
        (void)snprintf(buf, size, "%u%%", (unsigned)state->intensity_percent);
        break;
    case FAN_SETTING_AUTO_OFF:
        if (state->auto_off_min == FAN_AUTO_OFF_MINUTES_DISABLED)
        {
            (void)snprintf(buf, size, "OFF");
        }
        else
        {
            (void)snprintf(buf, size, "%u min", (unsigned)state->auto_off_min);
        }
        break;
    default:
        buf[0] = '\0';
        break;
    }
}

static void ui_FanPage_format_countdown(uint32_t remaining_s, char *buf, uint16_t size)
{
    uint32_t hours = remaining_s / 3600U;
    uint32_t minutes = (remaining_s % 3600U) / 60U;
    uint32_t seconds = remaining_s % 60U;

    (void)snprintf(buf, size, "%02u:%02u:%02u",
                   (unsigned)hours, (unsigned)minutes, (unsigned)seconds);
}

static void ui_FanPage_draw_rows(egui_canvas_t *canvas)
{
    const egui_font_t *font = EGUI_FONT_OF(&egui_res_font_montserrat_12_4);
    const fan_state_t *display_state = (s_fan_page.editing != 0U) ? &s_fan_page.edit_state : &s_fan_page.state;
    uint32_t elapsed = (s_fan_page.animation_active != 0U) ?
                           (s_fan_page.frame_tick - s_fan_page.enter_tick) : 0U;
    uint32_t accent = ui_fan_hsv_to_rgb(s_fan_page.hue, 225U, 255U);
    uint8_t pulse = (uint8_t)((s_fan_page.frame_tick / 80U) % 20U);

    if (pulse > 10U)
    {
        pulse = (uint8_t)(20U - pulse);
    }

    for (uint8_t i = 0U; i < FAN_SETTING_COUNT; i++)
    {
        uint32_t delay = (uint32_t)i * FAN_SETTING_ENTER_STEP_MS;
        uint32_t progress = (elapsed > delay) ? (elapsed - delay) : 0U;
        int16_t slide = 0;
        int16_t x;
        int16_t y = (int16_t)(FAN_SETTING_ROW_Y + (int16_t)i * (FAN_SETTING_ROW_H + FAN_SETTING_ROW_GAP));
        uint8_t selected = (uint8_t)((s_fan_page.setting_active != 0U) && (s_fan_page.focus_index == i));
        uint32_t fill = selected ? 0x123451 : 0x0D2134;
        uint32_t border = selected ? accent : 0x1E3A50;
        char value[24];

        if (progress < FAN_SETTING_ENTER_LENGTH_MS)
        {
            slide = (int16_t)(46U - ((progress * 46U) / FAN_SETTING_ENTER_LENGTH_MS));
        }
        x = FAN_SETTING_ROW_X + slide;

        egui_canvas_draw_round_rectangle_fill(canvas, x, y, FAN_SETTING_ROW_W, FAN_SETTING_ROW_H, 5,
                                              ui_color(fill), EGUI_ALPHA_90);
        egui_canvas_draw_round_rectangle(canvas, x, y, FAN_SETTING_ROW_W, FAN_SETTING_ROW_H, 5,
                                         selected ? 2 : 1, ui_color(border), EGUI_ALPHA_100);
        if (selected != 0U)
        {
            int16_t beam_x = x + 8 + (int16_t)((s_fan_page.frame_tick / 12U) % (FAN_SETTING_ROW_W - 40));
            egui_canvas_draw_line(canvas, beam_x, y + 2, beam_x + 28, y + 2, 2,
                                  ui_color(ui_fan_hsv_to_rgb((uint16_t)(s_fan_page.hue + 90U), 210U, 255U)), EGUI_ALPHA_80);
            if (s_fan_page.editing != 0U)
            {
                egui_canvas_draw_round_rectangle(canvas, x + 3, y + 3, FAN_SETTING_ROW_W - 6,
                                                 FAN_SETTING_ROW_H - 6, 3, (pulse > 5U) ? 2 : 1,
                                                 ui_color(0xF8FAFC), EGUI_ALPHA_40);
            }
        }

        ui_draw_text(canvas, font, s_fan_setting_labels[i], x + 12, y + 2, 135, FAN_SETTING_ROW_H - 4,
                     EGUI_ALIGN_LEFT | EGUI_ALIGN_VCENTER, selected ? 0xFFFFFF : 0xC8D8E8);
        if (i == FAN_SETTING_POWER)
        {
            ui_fan_draw_switch(canvas, x + FAN_SETTING_ROW_W - 50, y + 3, display_state->power_on, accent);
        }
        else
        {
            ui_FanPage_format_value(i, display_state, value, sizeof(value));
            ui_draw_text(canvas, font, value, x + 168, y + 2, 105, FAN_SETTING_ROW_H - 4,
                         EGUI_ALIGN_RIGHT | EGUI_ALIGN_VCENTER, selected ? accent : 0x7DD3FC);
        }
    }

    {
        const char *hint;
        char countdown[12];

        if (s_fan_page.setting_active == 0U)
        {
            if ((s_fan_page.state.power_on != 0U) &&
                (s_fan_page.state.auto_off_remaining_s > 0U))
            {
                ui_FanPage_format_countdown(s_fan_page.state.auto_off_remaining_s,
                                            countdown, sizeof(countdown));
                hint = countdown;
            }
            else
            {
                hint = "OK SETTINGS";
            }
        }
        else if (s_fan_page.editing != 0U)
        {
            hint = "PWR CANCEL  OK APPLY";
        }
        else
        {
            if (s_fan_page.focus_index == FAN_SETTING_POWER)
            {
                hint = "PWR BACK  OK TOGGLE";
            }
            else if (s_fan_page.focus_index == FAN_SETTING_MODE)
            {
                hint = "PWR BACK  OK NEXT";
            }
            else
            {
                hint = "PWR BACK  OK EDIT";
            }
        }
        ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_10_4), hint, 7, 128, 114, 12,
                     EGUI_ALIGN_CENTER, 0x94A3B8);
    }

    if (Time32_Before(s_fan_page.frame_tick, s_fan_page.confirm_until))
    {
        egui_alpha_t alpha = (egui_alpha_t)(((s_fan_page.confirm_until - s_fan_page.frame_tick) * 178U) / FAN_CONFIRM_MS);
        egui_canvas_draw_rectangle_fill(canvas, FAN_SETTING_ROW_X, 0, FAN_SETTING_ROW_W, UI_SCREEN_H,
                                        ui_color(accent), alpha);
    }
}

static void ui_FanPage_on_draw(egui_view_t *self)
{
    egui_canvas_t *canvas = egui_view_get_canvas(self);

    ui_FanPage_draw_background(canvas);
    ui_FanPage_draw_rows(canvas);
}
