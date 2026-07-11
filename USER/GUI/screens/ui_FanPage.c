#include "screens/ui_FanPage.h"

#include <stdbool.h>
#include <stdio.h>

#include "fan_app.h"
#include "ui.h"

lv_obj_t *ui_FanPage = NULL;

static lv_obj_t *s_title_label = NULL;
static lv_obj_t *s_state_label = NULL;
static lv_obj_t *s_detail_label = NULL;
static lv_obj_t *s_mode_dropdown = NULL;
static lv_obj_t *s_power_switch = NULL;
static lv_obj_t *s_speed_slider = NULL;
static lv_obj_t *s_speed_value = NULL;
static lv_obj_t *s_intensity_slider = NULL;
static lv_obj_t *s_intensity_value = NULL;
static lv_obj_t *s_temp_slider = NULL;
static lv_obj_t *s_temp_value = NULL;
static lv_obj_t *s_hum_slider = NULL;
static lv_obj_t *s_hum_value = NULL;
static lv_obj_t *s_natural_slider = NULL;
static lv_obj_t *s_natural_value = NULL;
static lv_obj_t *s_random_slider = NULL;
static lv_obj_t *s_random_value = NULL;
static lv_obj_t *s_auto_off_slider = NULL;
static lv_obj_t *s_auto_off_value = NULL;
static lv_timer_t *s_refresh_timer = NULL;
static bool s_syncing = false;

static void ui_FanPage_set_label_fmt(lv_obj_t *label, const char *fmt, int value)
{
    char buf[32];

    if (label == NULL)
    {
        return;
    }

    (void)snprintf(buf, sizeof(buf), fmt, value);
    lv_label_set_text(label, buf);
}

static void ui_FanPage_set_temp_label(lv_obj_t *label, uint16_t temp_x10)
{
    char buf[32];

    if (label == NULL)
    {
        return;
    }

    (void)snprintf(buf, sizeof(buf), "%u.%uC",
                   (unsigned int)(temp_x10 / 10U),
                   (unsigned int)(temp_x10 % 10U));
    lv_label_set_text(label, buf);
}

static lv_obj_t *ui_FanPage_create_block(lv_obj_t *parent, const char *title, lv_obj_t **value_label)
{
    lv_obj_t *block;
    lv_obj_t *row;
    lv_obj_t *title_label;

    block = lv_obj_create(parent);
    lv_obj_remove_style_all(block);
    lv_obj_set_width(block, lv_pct(100));
    lv_obj_set_height(block, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(block, lv_color_hex(0x115E59), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(block, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(block, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(block, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_gap(block, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_flex_flow(block, LV_FLEX_FLOW_COLUMN);

    row = lv_obj_create(block);
    lv_obj_remove_style_all(row);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, 18);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    title_label = lv_label_create(row);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_color(title_label, lv_color_hex(0xF8FAFC), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

    if (value_label != NULL)
    {
        *value_label = lv_label_create(row);
        lv_label_set_text(*value_label, "--");
        lv_obj_set_style_text_color(*value_label, lv_color_hex(0x99F6E4), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(*value_label, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    return block;
}

static lv_obj_t *ui_FanPage_create_section(lv_obj_t *parent, const char *text)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_width(label, lv_pct(100));
    lv_obj_set_style_text_color(label, lv_color_hex(0xCCFBF1), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(label, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    return label;
}

static lv_obj_t *ui_FanPage_create_slider(lv_obj_t *parent, int32_t min, int32_t max, lv_event_cb_t cb)
{
    lv_obj_t *slider = lv_slider_create(parent);
    lv_obj_set_width(slider, lv_pct(100));
    lv_slider_set_range(slider, min, max);
    lv_obj_add_event_cb(slider, cb, LV_EVENT_VALUE_CHANGED, NULL);
    return slider;
}

static void ui_FanPage_build_mode_options(char *buf, uint16_t size)
{
    uint16_t used = 0U;

    if ((buf == NULL) || (size == 0U))
    {
        return;
    }

    buf[0] = '\0';
    for (uint8_t i = 0U; i < FanApp_GetModeCount(); i++)
    {
        int written = snprintf(&buf[used],
                               (size > used) ? (uint16_t)(size - used) : 0U,
                               "%s%s",
                               (i == 0U) ? "" : "\n",
                               FanApp_GetModeName(FanApp_ModeFromIndex(i)));

        if (written <= 0)
        {
            break;
        }

        if ((uint16_t)written >= (uint16_t)(size - used))
        {
            break;
        }

        used = (uint16_t)(used + (uint16_t)written);
    }
}

static void ui_FanPage_sync_from_state(void)
{
    fan_state_t state;
    char buf[64];
    int16_t temp_abs;
    const char *temp_sign;

    if (ui_FanPage == NULL)
    {
        return;
    }

    FanApp_GetState(&state);

    (void)snprintf(buf, sizeof(buf), "%s / %s / %u%%",
                   (state.power_on != 0U) ? "ON" : "OFF",
                   FanApp_GetModeName(state.mode),
                   (unsigned int)state.current_speed_percent);
    lv_label_set_text(s_state_label, buf);

    temp_sign = (state.current_temp_x10 < 0) ? "-" : "";
    temp_abs = (state.current_temp_x10 < 0) ? (int16_t)(-state.current_temp_x10) : state.current_temp_x10;
    (void)snprintf(buf, sizeof(buf), "%s%d.%dC  %u%%RH  left:%umin",
                   temp_sign,
                   (int)(temp_abs / 10),
                   (int)(temp_abs % 10),
                   (unsigned int)state.current_hum_percent,
                   (unsigned int)state.auto_off_remaining_min);
    lv_label_set_text(s_detail_label, buf);

    s_syncing = true;

    if ((s_power_switch != NULL) && !lv_obj_has_state(s_power_switch, LV_STATE_PRESSED))
    {
        if (state.power_on != 0U)
        {
            lv_obj_add_state(s_power_switch, LV_STATE_CHECKED);
        }
        else
        {
            lv_obj_clear_state(s_power_switch, LV_STATE_CHECKED);
        }
    }

    if ((s_mode_dropdown != NULL) && !lv_obj_has_state(s_mode_dropdown, LV_STATE_PRESSED))
    {
        lv_dropdown_set_selected(s_mode_dropdown, FanApp_ModeToIndex(state.mode));
    }

    if ((s_speed_slider != NULL) && !lv_obj_has_state(s_speed_slider, LV_STATE_PRESSED))
    {
        lv_slider_set_value(s_speed_slider, state.base_speed_percent, LV_ANIM_OFF);
    }
    ui_FanPage_set_label_fmt(s_speed_value, "%d%%", state.base_speed_percent);

    if ((s_intensity_slider != NULL) && !lv_obj_has_state(s_intensity_slider, LV_STATE_PRESSED))
    {
        lv_slider_set_value(s_intensity_slider, state.intensity_percent, LV_ANIM_OFF);
    }
    ui_FanPage_set_label_fmt(s_intensity_value, "%d%%", state.intensity_percent);

    if ((s_temp_slider != NULL) && !lv_obj_has_state(s_temp_slider, LV_STATE_PRESSED))
    {
        lv_slider_set_value(s_temp_slider, state.smart_temp_threshold_x10, LV_ANIM_OFF);
    }
    ui_FanPage_set_temp_label(s_temp_value, state.smart_temp_threshold_x10);

    if ((s_hum_slider != NULL) && !lv_obj_has_state(s_hum_slider, LV_STATE_PRESSED))
    {
        lv_slider_set_value(s_hum_slider, state.smart_hum_threshold_percent, LV_ANIM_OFF);
    }
    ui_FanPage_set_label_fmt(s_hum_value, "%d%%", state.smart_hum_threshold_percent);

    if ((s_natural_slider != NULL) && !lv_obj_has_state(s_natural_slider, LV_STATE_PRESSED))
    {
        lv_slider_set_value(s_natural_slider, state.natural_amplitude_percent, LV_ANIM_OFF);
    }
    ui_FanPage_set_label_fmt(s_natural_value, "+/-%d%%", state.natural_amplitude_percent);

    if ((s_random_slider != NULL) && !lv_obj_has_state(s_random_slider, LV_STATE_PRESSED))
    {
        lv_slider_set_value(s_random_slider, state.random_amplitude_percent, LV_ANIM_OFF);
    }
    ui_FanPage_set_label_fmt(s_random_value, "+/-%d%%", state.random_amplitude_percent);

    if ((s_auto_off_slider != NULL) && !lv_obj_has_state(s_auto_off_slider, LV_STATE_PRESSED))
    {
        lv_slider_set_value(s_auto_off_slider, state.auto_off_min, LV_ANIM_OFF);
    }
    ui_FanPage_set_label_fmt(s_auto_off_value, "%dmin", state.auto_off_min);

    s_syncing = false;
}

static void ui_FanPage_power_cb(lv_event_t *event)
{
    (void)event;

    if (s_syncing || (s_power_switch == NULL))
    {
        return;
    }

    (void)FanApp_SetPower(lv_obj_has_state(s_power_switch, LV_STATE_CHECKED), 0U);
}

static void ui_FanPage_mode_cb(lv_event_t *event)
{
    (void)event;

    if (s_syncing || (s_mode_dropdown == NULL))
    {
        return;
    }

    (void)FanApp_SetMode(FanApp_ModeFromIndex((uint8_t)lv_dropdown_get_selected(s_mode_dropdown)), 0U);
}

static void ui_FanPage_slider_cb(lv_event_t *event)
{
    lv_obj_t *target = lv_event_get_target(event);
    int32_t value;

    if (s_syncing || (target == NULL))
    {
        return;
    }

    value = lv_slider_get_value(target);

    if (target == s_speed_slider)
    {
        ui_FanPage_set_label_fmt(s_speed_value, "%d%%", (int)value);
        (void)FanApp_SetSpeed((uint8_t)value, 0U);
    }
    else if (target == s_intensity_slider)
    {
        ui_FanPage_set_label_fmt(s_intensity_value, "%d%%", (int)value);
        (void)FanApp_SetIntensity((uint8_t)value, 0U);
    }
    else if (target == s_temp_slider)
    {
        ui_FanPage_set_temp_label(s_temp_value, (uint16_t)value);
        (void)FanApp_SetSmartTempThreshold((uint16_t)value, 0U);
    }
    else if (target == s_hum_slider)
    {
        ui_FanPage_set_label_fmt(s_hum_value, "%d%%", (int)value);
        (void)FanApp_SetSmartHumidityThreshold((uint8_t)value, 0U);
    }
    else if (target == s_natural_slider)
    {
        ui_FanPage_set_label_fmt(s_natural_value, "+/-%d%%", (int)value);
        (void)FanApp_SetNaturalAmplitude((uint8_t)value, 0U);
    }
    else if (target == s_random_slider)
    {
        ui_FanPage_set_label_fmt(s_random_value, "+/-%d%%", (int)value);
        (void)FanApp_SetRandomAmplitude((uint8_t)value, 0U);
    }
    else if (target == s_auto_off_slider)
    {
        ui_FanPage_set_label_fmt(s_auto_off_value, "%dmin", (int)value);
        (void)FanApp_SetAutoOffMinutes((uint16_t)value, 0U);
    }
}

static void ui_FanPage_refresh_cb(lv_timer_t *timer)
{
    (void)timer;
    ui_FanPage_sync_from_state();
}

void ui_FanPage_screen_init(void)
{
    lv_obj_t *content;
    lv_obj_t *block;
    char mode_options[96];

    if (ui_FanPage != NULL)
    {
        return;
    }

    ui_FanPage = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui_FanPage, lv_color_hex(0x042F2E), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_FanPage, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_scroll_dir(ui_FanPage, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(ui_FanPage, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_pad_all(ui_FanPage, 8, LV_PART_MAIN | LV_STATE_DEFAULT);

    s_title_label = lv_label_create(ui_FanPage);
    lv_label_set_text(s_title_label, "FAN");
    lv_obj_set_style_text_color(s_title_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(s_title_label, &lv_font_montserrat_28, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(s_title_label, LV_ALIGN_TOP_LEFT, 0, 0);

    s_state_label = lv_label_create(ui_FanPage);
    lv_label_set_text(s_state_label, "OFF / Manual / 0%");
    lv_obj_set_style_text_color(s_state_label, lv_color_hex(0xCCFBF1), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(s_state_label, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(s_state_label, LV_ALIGN_TOP_LEFT, 86, 4);

    s_detail_label = lv_label_create(ui_FanPage);
    lv_label_set_text(s_detail_label, "--.-C  --%RH  left:0min");
    lv_obj_set_style_text_color(s_detail_label, lv_color_hex(0x5EEAD4), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(s_detail_label, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(s_detail_label, LV_ALIGN_TOP_LEFT, 86, 24);

    content = lv_obj_create(ui_FanPage);
    lv_obj_remove_style_all(content);
    lv_obj_set_width(content, lv_pct(100));
    lv_obj_set_height(content, LV_SIZE_CONTENT);
    lv_obj_set_y(content, 48);
    lv_obj_set_style_pad_gap(content, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);

    (void)ui_FanPage_create_section(content, "GENERAL");

    block = ui_FanPage_create_block(content, "Power", NULL);
    s_power_switch = lv_switch_create(block);
    lv_obj_align(s_power_switch, LV_ALIGN_RIGHT_MID, -2, 0);
    lv_obj_add_event_cb(s_power_switch, ui_FanPage_power_cb, LV_EVENT_VALUE_CHANGED, NULL);

    block = ui_FanPage_create_block(content, "Mode", NULL);
    s_mode_dropdown = lv_dropdown_create(block);
    ui_FanPage_build_mode_options(mode_options, sizeof(mode_options));
    lv_dropdown_set_options(s_mode_dropdown, mode_options);
    lv_obj_set_width(s_mode_dropdown, lv_pct(100));
    lv_obj_add_event_cb(s_mode_dropdown, ui_FanPage_mode_cb, LV_EVENT_VALUE_CHANGED, NULL);

    block = ui_FanPage_create_block(content, "Speed", &s_speed_value);
    s_speed_slider = ui_FanPage_create_slider(block, 0, 100, ui_FanPage_slider_cb);

    block = ui_FanPage_create_block(content, "Intensity", &s_intensity_value);
    s_intensity_slider = ui_FanPage_create_slider(block, 0, 100, ui_FanPage_slider_cb);

    (void)ui_FanPage_create_section(content, "SMART");

    block = ui_FanPage_create_block(content, "Temp Threshold", &s_temp_value);
    s_temp_slider = ui_FanPage_create_slider(block, 180, 360, ui_FanPage_slider_cb);

    block = ui_FanPage_create_block(content, "Humidity Threshold", &s_hum_value);
    s_hum_slider = ui_FanPage_create_slider(block, 30, 95, ui_FanPage_slider_cb);

    block = ui_FanPage_create_block(content, "Auto Off", &s_auto_off_value);
    s_auto_off_slider = ui_FanPage_create_slider(block, 0, 120, ui_FanPage_slider_cb);

    (void)ui_FanPage_create_section(content, "WIND PROFILE");

    block = ui_FanPage_create_block(content, "Natural Wave", &s_natural_value);
    s_natural_slider = ui_FanPage_create_slider(block, 0, 60, ui_FanPage_slider_cb);

    block = ui_FanPage_create_block(content, "Random Range", &s_random_value);
    s_random_slider = ui_FanPage_create_slider(block, 0, 60, ui_FanPage_slider_cb);

    ui_FanPage_sync_from_state();
    s_refresh_timer = lv_timer_create(ui_FanPage_refresh_cb, 300U, NULL);
}

void ui_FanPage_screen_destroy(void)
{
    if (s_refresh_timer != NULL)
    {
        lv_timer_del(s_refresh_timer);
        s_refresh_timer = NULL;
    }

    s_title_label = NULL;
    s_state_label = NULL;
    s_detail_label = NULL;
    s_mode_dropdown = NULL;
    s_power_switch = NULL;
    s_speed_slider = NULL;
    s_speed_value = NULL;
    s_intensity_slider = NULL;
    s_intensity_value = NULL;
    s_temp_slider = NULL;
    s_temp_value = NULL;
    s_hum_slider = NULL;
    s_hum_value = NULL;
    s_natural_slider = NULL;
    s_natural_value = NULL;
    s_random_slider = NULL;
    s_random_value = NULL;
    s_auto_off_slider = NULL;
    s_auto_off_value = NULL;
    s_syncing = false;
    ui_FanPage = NULL;
}

void ui_FanPage_key_handler(void *key_event)
{
    ui_page_handle_default_key_event(key_event);
}
