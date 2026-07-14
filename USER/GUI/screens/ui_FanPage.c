#include "ui_FanPage.h"

#include <stdio.h>

#include "core/egui_timer.h"
#include "egui_port_stm32l471_fan.h"
#include "fan_anim_res.h"
#include "fan_app.h"
#include "key.h"
#include "page_manager.h"
#include "ui_common.h"
#include "widget/egui_view_animated_image.h"
#include "widget/egui_view_gauge.h"
#include "widget/egui_view_group.h"
#include "widget/egui_view_label.h"
#include "widget/egui_view_list.h"
#include "widget/egui_view_segmented_control.h"
#include "widget/egui_view_switch.h"

#define FAN_SETTING_COUNT              5U
#define FAN_SETTING_POWER              0U
#define FAN_SETTING_MODE               1U
#define FAN_SETTING_SPEED              2U
#define FAN_SETTING_INTENSITY          3U
#define FAN_SETTING_AUTO_OFF           4U
#define FAN_AUTO_OFF_MAX_MIN           120U
#define FAN_AUTO_OFF_STEP_MIN          5U
#define FAN_PERCENT_STEP               5U
#define FAN_STATUS_REFRESH_MS          100U

typedef struct
{
    egui_view_group_t root;
    egui_timer_t timer;
    egui_view_animated_image_t fan_image;
    egui_view_gauge_t speed_gauge;
    egui_view_label_t speed_label;
    egui_view_label_t mode_label;
    egui_view_label_t hint_label;
    egui_view_list_t settings_list;
    egui_view_switch_t power_switch;
    egui_view_segmented_control_t mode_segments;
    char speed_text[16];
    char mode_text[24];
    char hint_text[32];
    char row_text[FAN_SETTING_COUNT][32];
    fan_mode_t anim_mode;
    uint8_t setting_active;
    uint8_t editing;
    uint8_t focus_index;
} ui_fan_page_t;

static ui_fan_page_t s_fan_page;
static egui_view_api_t s_fan_api;

egui_view_t *ui_FanPage = NULL;

static const char *s_mode_segment_texts[] = {
    "M",
    "N",
    "S",
    "R",
    "Sl",
    "T",
    "E",
};

static void ui_FanPage_on_draw(egui_view_t *self);
static void ui_FanPage_timer_cb(egui_timer_t *timer);
static void ui_FanPage_sync_from_state(void);

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

static uint8_t ui_FanPage_mode_to_segment(fan_mode_t mode)
{
    if ((mode == FAN_MODE_OFF) || (mode >= FAN_MODE_COUNT))
    {
        return 0U;
    }

    return (uint8_t)(mode - 1U);
}

static fan_mode_t ui_FanPage_segment_to_mode(uint8_t index)
{
    if (index >= (uint8_t)(FAN_MODE_COUNT - 1U))
    {
        return FAN_MODE_MANUAL;
    }

    return (fan_mode_t)(index + 1U);
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

static void ui_FanPage_set_setting_active(uint8_t active)
{
    s_fan_page.setting_active = active ? 1U : 0U;
    s_fan_page.editing = 0U;

    if (s_fan_page.setting_active != 0U)
    {
        egui_view_list_set_selected_index(EGUI_VIEW_OF(&s_fan_page.settings_list), s_fan_page.focus_index);
    }
    else
    {
        s_fan_page.settings_list.selected_index = EGUI_VIEW_LIST_SELECTED_NONE;
        egui_view_invalidate(EGUI_VIEW_OF(&s_fan_page.settings_list));
    }

    egui_view_invalidate_full(ui_FanPage);
}

static void ui_FanPage_update_rows(const fan_state_t *state)
{
    if (state == NULL)
    {
        return;
    }

    (void)snprintf(s_fan_page.row_text[FAN_SETTING_POWER], sizeof(s_fan_page.row_text[FAN_SETTING_POWER]), "Power");
    (void)snprintf(s_fan_page.row_text[FAN_SETTING_MODE], sizeof(s_fan_page.row_text[FAN_SETTING_MODE]), "Mode");
    (void)snprintf(s_fan_page.row_text[FAN_SETTING_SPEED], sizeof(s_fan_page.row_text[FAN_SETTING_SPEED]),
                   "Speed: %u%%", state->base_speed_percent);
    (void)snprintf(s_fan_page.row_text[FAN_SETTING_INTENSITY], sizeof(s_fan_page.row_text[FAN_SETTING_INTENSITY]),
                   "Intensity: %u%%", state->intensity_percent);
    (void)snprintf(s_fan_page.row_text[FAN_SETTING_AUTO_OFF], sizeof(s_fan_page.row_text[FAN_SETTING_AUTO_OFF]),
                   "Auto: %umin", state->auto_off_min);
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
    fan_mode_t anim_mode;

    FanApp_GetState(&state);
    display_mode = ui_FanPage_visible_mode(&state);
    anim_mode = (state.power_on != 0U) ? state.mode : FAN_MODE_OFF;

    ui_FanPage_sync_anim(anim_mode);
    ui_FanPage_update_rows(&state);

    egui_view_gauge_set_value(EGUI_VIEW_OF(&s_fan_page.speed_gauge), state.current_speed_percent);
    egui_view_switch_set_checked(EGUI_VIEW_OF(&s_fan_page.power_switch), state.power_on);
    egui_view_segmented_control_set_current_index(EGUI_VIEW_OF(&s_fan_page.mode_segments), ui_FanPage_mode_to_segment(display_mode));

    (void)snprintf(s_fan_page.speed_text, sizeof(s_fan_page.speed_text), "%u%%", state.current_speed_percent);
    (void)snprintf(s_fan_page.mode_text, sizeof(s_fan_page.mode_text), "%s", FanApp_GetModeName(display_mode));
    if (s_fan_page.setting_active == 0U)
    {
        (void)snprintf(s_fan_page.hint_text, sizeof(s_fan_page.hint_text), "B SET  N/R PAGE");
    }
    else if (s_fan_page.editing != 0U)
    {
        (void)snprintf(s_fan_page.hint_text, sizeof(s_fan_page.hint_text), "N/R ADJ  L OK  B BACK");
    }
    else
    {
        (void)snprintf(s_fan_page.hint_text, sizeof(s_fan_page.hint_text), "N/R SEL  L OK  B EXIT");
    }

    egui_view_label_set_text(EGUI_VIEW_OF(&s_fan_page.speed_label), s_fan_page.speed_text);
    egui_view_label_set_text(EGUI_VIEW_OF(&s_fan_page.mode_label), s_fan_page.mode_text);
    egui_view_label_set_text(EGUI_VIEW_OF(&s_fan_page.hint_label), s_fan_page.hint_text);
    egui_view_invalidate(EGUI_VIEW_OF(&s_fan_page.settings_list));
}

void ui_FanPage_screen_init(void)
{
    egui_core_t *core = egui_port_get_core();
    egui_view_t *view = EGUI_VIEW_OF(&s_fan_page.root);
    fan_state_t state;

    ui_FanPage = view;
    egui_view_group_init(view, core);
    egui_view_copy_api(view, &s_fan_api);
    s_fan_api.on_draw = ui_FanPage_on_draw;
    view->api = &s_fan_api;
    egui_view_set_position(view, 0, 0);
    egui_view_set_size(view, UI_SCREEN_W, UI_SCREEN_H);
    egui_view_set_visible(view, 1);

    s_fan_page.setting_active = 0U;
    s_fan_page.editing = 0U;
    s_fan_page.focus_index = FAN_SETTING_POWER;
    s_fan_page.anim_mode = FAN_MODE_COUNT;

    FanApp_GetState(&state);
    ui_FanPage_update_rows(&state);

    egui_view_animated_image_init(EGUI_VIEW_OF(&s_fan_page.fan_image), core);
    egui_view_set_position(EGUI_VIEW_OF(&s_fan_page.fan_image), 48, 38);
    egui_view_set_size(EGUI_VIEW_OF(&s_fan_page.fan_image), FAN_ANIM_ICON_SIZE, FAN_ANIM_ICON_SIZE);

    egui_view_gauge_init(EGUI_VIEW_OF(&s_fan_page.speed_gauge), core);
    egui_view_set_position(EGUI_VIEW_OF(&s_fan_page.speed_gauge), 143, 12);
    egui_view_set_size(EGUI_VIEW_OF(&s_fan_page.speed_gauge), 112, 112);
    egui_view_gauge_set_start_angle(EGUI_VIEW_OF(&s_fan_page.speed_gauge), 180);
    egui_view_gauge_set_sweep_angle(EGUI_VIEW_OF(&s_fan_page.speed_gauge), 180);
    egui_view_gauge_set_stroke_width(EGUI_VIEW_OF(&s_fan_page.speed_gauge), 7);
    egui_view_gauge_set_needle_width(EGUI_VIEW_OF(&s_fan_page.speed_gauge), 2);
    egui_view_gauge_set_bk_color(EGUI_VIEW_OF(&s_fan_page.speed_gauge), ui_color(0xB9E6FF));
    egui_view_gauge_set_progress_color(EGUI_VIEW_OF(&s_fan_page.speed_gauge), ui_color(0x0EA5E9));
    egui_view_gauge_set_needle_color(EGUI_VIEW_OF(&s_fan_page.speed_gauge), ui_color(0x0F172A));
    egui_view_gauge_set_text_color(EGUI_VIEW_OF(&s_fan_page.speed_gauge), ui_color(0x0F172A));
    egui_view_gauge_set_font(EGUI_VIEW_OF(&s_fan_page.speed_gauge), EGUI_FONT_OF(&egui_res_font_montserrat_20_4));

    ui_FanPage_init_label(&s_fan_page.speed_label, core, 156, 90, 86, 24, EGUI_FONT_OF(&egui_res_font_montserrat_24_4),
                          0x0F172A, EGUI_ALIGN_CENTER, s_fan_page.speed_text);
    ui_FanPage_init_label(&s_fan_page.mode_label, core, 137, 116, 124, 18, EGUI_FONT_OF(&egui_res_font_montserrat_14_4),
                          0x155E75, EGUI_ALIGN_CENTER, s_fan_page.mode_text);
    ui_FanPage_init_label(&s_fan_page.hint_label, core, 8, 118, 128, 18, EGUI_FONT_OF(&egui_res_font_montserrat_10_4),
                          0x0F766E, EGUI_ALIGN_CENTER, s_fan_page.hint_text);

    egui_view_list_init(EGUI_VIEW_OF(&s_fan_page.settings_list), core);
    egui_view_set_position(EGUI_VIEW_OF(&s_fan_page.settings_list), 284, 8);
    egui_view_set_size(EGUI_VIEW_OF(&s_fan_page.settings_list), 138, 128);
    egui_view_list_set_item_height(EGUI_VIEW_OF(&s_fan_page.settings_list), 24);
    for (uint8_t i = 0U; i < FAN_SETTING_COUNT; i++)
    {
        (void)egui_view_list_add_item(EGUI_VIEW_OF(&s_fan_page.settings_list), s_fan_page.row_text[i]);
    }

    egui_view_switch_init(EGUI_VIEW_OF(&s_fan_page.power_switch), core);
    egui_view_set_position(EGUI_VIEW_OF(&s_fan_page.power_switch), 372, 13);
    egui_view_set_size(EGUI_VIEW_OF(&s_fan_page.power_switch), 42, 18);

    egui_view_segmented_control_init(EGUI_VIEW_OF(&s_fan_page.mode_segments), core);
    egui_view_set_position(EGUI_VIEW_OF(&s_fan_page.mode_segments), 294, 38);
    egui_view_set_size(EGUI_VIEW_OF(&s_fan_page.mode_segments), 118, 20);
    egui_view_segmented_control_set_segments(EGUI_VIEW_OF(&s_fan_page.mode_segments), s_mode_segment_texts,
                                             (uint8_t)(sizeof(s_mode_segment_texts) / sizeof(s_mode_segment_texts[0])));
    egui_view_segmented_control_set_font(EGUI_VIEW_OF(&s_fan_page.mode_segments), EGUI_FONT_OF(&egui_res_font_montserrat_10_4));
    egui_view_segmented_control_set_corner_radius(EGUI_VIEW_OF(&s_fan_page.mode_segments), 5U);
    egui_view_segmented_control_set_segment_gap(EGUI_VIEW_OF(&s_fan_page.mode_segments), 1U);
    egui_view_segmented_control_set_horizontal_padding(EGUI_VIEW_OF(&s_fan_page.mode_segments), 2U);

    egui_view_group_add_child(view, EGUI_VIEW_OF(&s_fan_page.fan_image));
    egui_view_group_add_child(view, EGUI_VIEW_OF(&s_fan_page.speed_gauge));
    egui_view_group_add_child(view, EGUI_VIEW_OF(&s_fan_page.speed_label));
    egui_view_group_add_child(view, EGUI_VIEW_OF(&s_fan_page.mode_label));
    egui_view_group_add_child(view, EGUI_VIEW_OF(&s_fan_page.hint_label));
    egui_view_group_add_child(view, EGUI_VIEW_OF(&s_fan_page.settings_list));
    egui_view_group_add_child(view, EGUI_VIEW_OF(&s_fan_page.power_switch));
    egui_view_group_add_child(view, EGUI_VIEW_OF(&s_fan_page.mode_segments));

    ui_FanPage_sync_from_state();
    egui_view_start_periodic(view, &s_fan_page.timer, view, ui_FanPage_timer_cb, FAN_STATUS_REFRESH_MS);
}

void ui_FanPage_screen_destroy(void)
{
}

static uint8_t ui_FanPage_clamp_percent_delta(uint8_t value, int8_t direction)
{
    int16_t next = (int16_t)value + ((int16_t)direction * (int16_t)FAN_PERCENT_STEP);

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

static uint16_t ui_FanPage_clamp_auto_off_delta(uint16_t value, int8_t direction)
{
    int16_t next = (int16_t)value + ((int16_t)direction * (int16_t)FAN_AUTO_OFF_STEP_MIN);

    if (next < 0)
    {
        return 0U;
    }
    if (next > (int16_t)FAN_AUTO_OFF_MAX_MIN)
    {
        return FAN_AUTO_OFF_MAX_MIN;
    }

    return (uint16_t)next;
}

static fan_mode_t ui_FanPage_next_mode(fan_mode_t mode, int8_t direction)
{
    int8_t index;

    if ((mode <= FAN_MODE_OFF) || (mode >= FAN_MODE_COUNT))
    {
        mode = FAN_MODE_MANUAL;
    }

    index = (int8_t)ui_FanPage_mode_to_segment(mode);
    index = (int8_t)(index + direction);
    if (index < 0)
    {
        index = (int8_t)(FAN_MODE_COUNT - 2U);
    }
    else if (index >= (int8_t)(FAN_MODE_COUNT - 1U))
    {
        index = 0;
    }

    return ui_FanPage_segment_to_mode((uint8_t)index);
}

static void ui_FanPage_adjust_current(int8_t direction)
{
    fan_state_t state;

    FanApp_GetState(&state);
    switch (s_fan_page.focus_index)
    {
    case FAN_SETTING_MODE:
        (void)FanApp_SetMode(ui_FanPage_next_mode(ui_FanPage_visible_mode(&state), direction), 0U);
        break;
    case FAN_SETTING_SPEED:
        (void)FanApp_SetSpeed(ui_FanPage_clamp_percent_delta(state.base_speed_percent, direction), 0U);
        break;
    case FAN_SETTING_INTENSITY:
        (void)FanApp_SetIntensity(ui_FanPage_clamp_percent_delta(state.intensity_percent, direction), 0U);
        break;
    case FAN_SETTING_AUTO_OFF:
        (void)FanApp_SetAutoOffMinutes(ui_FanPage_clamp_auto_off_delta(state.auto_off_min, direction), 0U);
        break;
    default:
        break;
    }

    ui_FanPage_sync_from_state();
}

static void ui_FanPage_confirm_current(void)
{
    fan_state_t state;

    FanApp_GetState(&state);
    switch (s_fan_page.focus_index)
    {
    case FAN_SETTING_POWER:
        (void)FanApp_SetPower(state.power_on == 0U, 0U);
        break;
    case FAN_SETTING_MODE:
        (void)FanApp_SetMode(ui_FanPage_next_mode(ui_FanPage_visible_mode(&state), 1), 0U);
        break;
    case FAN_SETTING_SPEED:
    case FAN_SETTING_INTENSITY:
    case FAN_SETTING_AUTO_OFF:
        s_fan_page.editing = (s_fan_page.editing == 0U) ? 1U : 0U;
        break;
    default:
        break;
    }

    ui_FanPage_sync_from_state();
}

bool ui_FanPage_key_handler(void *key_event)
{
     return ui_page_consume_nav_key_event(key_event);
    // const key_event_t *event = (const key_event_t *)key_event;

    // if (event == NULL)
    // {
    //     return false;
    // }

    // if ((event->type != KEY_EVT_CLICK) && (event->type != KEY_EVT_REPEAT))
    // {
    //     return false;
    // }

    // if (s_fan_page.setting_active == 0U)
    // {
    //     if ((event->id == KEY_ID_B) && (event->type == KEY_EVT_CLICK))
    //     {
    //         ui_FanPage_set_setting_active(1U);
    //         ui_FanPage_sync_from_state();
    //         return true;
    //     }

    //     return ui_page_consume_nav_key_event(key_event);
    // }

    // if ((event->id == KEY_ID_B) && (event->type == KEY_EVT_CLICK))
    // {
    //     if (s_fan_page.editing != 0U)
    //     {
    //         s_fan_page.editing = 0U;
    //         ui_FanPage_sync_from_state();
    //     }
    //     else
    //     {
    //         ui_FanPage_set_setting_active(0U);
    //     }
    //     return true;
    // }

    // if ((event->id == KEY_ID_N) || (event->id == KEY_ID_R))
    // {
    //     int8_t direction = (event->id == KEY_ID_R) ? 1 : -1;

    //     if (s_fan_page.editing != 0U)
    //     {
    //         ui_FanPage_adjust_current(direction);
    //     }
    //     else
    //     {
    //         if (direction > 0)
    //         {
    //             s_fan_page.focus_index = (uint8_t)((s_fan_page.focus_index + 1U) % FAN_SETTING_COUNT);
    //         }
    //         else
    //         {
    //             s_fan_page.focus_index = (s_fan_page.focus_index == 0U) ? (FAN_SETTING_COUNT - 1U) : (uint8_t)(s_fan_page.focus_index - 1U);
    //         }
    //         egui_view_list_set_selected_index(EGUI_VIEW_OF(&s_fan_page.settings_list), s_fan_page.focus_index);
    //         ui_FanPage_sync_from_state();
    //     }
    //     return true;
    // }

    // if ((event->id == KEY_ID_L) && (event->type == KEY_EVT_CLICK))
    // {
    //     ui_FanPage_confirm_current();
    //     return true;
    // }

    // return true;
}

static void ui_FanPage_timer_cb(egui_timer_t *timer)
{
    egui_view_t *view = (egui_view_t *)timer->user_data;

    if ((view == NULL) || !egui_view_get_visible(view))
    {
        return;
    }

    egui_view_animated_image_update(EGUI_VIEW_OF(&s_fan_page.fan_image), FAN_STATUS_REFRESH_MS);
    ui_FanPage_sync_from_state();
    egui_view_invalidate_full(view);
}

static void ui_FanPage_on_draw(egui_view_t *self)
{
    egui_canvas_t *canvas = egui_view_get_canvas(self);

    ui_draw_rect(canvas, 0, 0, UI_SCREEN_W, UI_SCREEN_H, 0x111827);

    ui_draw_round_panel(canvas, 8, 12, 124, 104, 8, 0xE0F2FE, 0x38BDF8);
    ui_draw_round_panel(canvas, 138, 8, 128, 128, 8, 0xE0F7FA, 0x22D3EE);
    ui_draw_round_panel(canvas, 280, 4, 146, 134, 8, 0xECFEFF, 0x06B6D4);

    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_14_4), "FAN", 20, 18, 54, 18,
                 EGUI_ALIGN_LEFT | EGUI_ALIGN_VCENTER, 0x0F766E);
    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_10_4), s_fan_page.setting_active ? "SETTING" : "LIVE", 74, 19, 48, 16,
                 EGUI_ALIGN_RIGHT | EGUI_ALIGN_VCENTER, s_fan_page.setting_active ? 0xDC2626 : 0x0284C7);
}
