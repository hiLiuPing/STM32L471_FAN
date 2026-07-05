#include "screens/ui_HomePage.h"

#include <stdio.h>

#include "data_app.h"
#include "sensors_app.h"
#include "ui.h"

#define HOME_PAGE_ROW_COUNT 13U
#define HOME_PAGE_COL_COUNT 2U
#define HOME_PAGE_ROW_Y0    40
#define HOME_PAGE_ROW_STEP  44
#define HOME_PAGE_COL_X0    8
#define HOME_PAGE_COL_STEP  114
#define HOME_PAGE_CELL_W    106

lv_obj_t *ui_HomePage = NULL;

static lv_obj_t *s_title_label = NULL;
static lv_obj_t *s_rows[HOME_PAGE_ROW_COUNT] = {0};
static lv_timer_t *s_refresh_timer = NULL;

static const char *bool_text(bool value)
{
    return value ? "YES" : "NO";
}

static const char *charger_state_text(bq24295_state_t state)
{
    switch (state)
    {
    case BQ_CHG_PRECHARGE: return "PRE";
    case BQ_CHG_FAST_CHARGE: return "FAST";
    case BQ_CHG_DONE: return "DONE";
    case BQ_CHG_SUSPEND: return "SUSP";
    case BQ_CHG_FAULT: return "FAULT";
    case BQ_CHG_IDLE:
    default: return "IDLE";
    }
}

static const char *tilt_text(TiltKey_t tilt)
{
    switch (tilt)
    {
    case MSG_TILT_UP: return "UP";
    case MSG_TILT_DOWN: return "DOWN";
    case MSG_TILT_LEFT: return "LEFT";
    case MSG_TILT_RIGHT: return "RIGHT";
    case MSG_FALL_DOWN: return "FALL";
    case MSG_TILT_SHAKE_VERTICAL: return "SHAKE_V";
    case MSG_TILT_SHAKE_HORIZONTAL: return "SHAKE_H";
    default: return "NONE";
    }
}

static int32_t float_to_scaled(float value, float scale)
{
    if (value >= 0.0f)
    {
        return (int32_t)((value * scale) + 0.5f);
    }

    return (int32_t)((value * scale) - 0.5f);
}

static void format_dec1(char *buf, size_t buf_len, float value, const char *unit)
{
    int32_t scaled = float_to_scaled(value, 10.0f);
    int32_t whole = scaled / 10;
    int32_t frac = scaled % 10;

    if (frac < 0)
    {
        frac = -frac;
    }

    (void)snprintf(buf, buf_len, "%ld.%ld%s", (long)whole, (long)frac, unit);
}

static void format_dec2(char *buf, size_t buf_len, float value, const char *unit)
{
    int32_t scaled = float_to_scaled(value, 100.0f);
    int32_t whole = scaled / 100;
    int32_t frac = scaled % 100;

    if (frac < 0)
    {
        frac = -frac;
    }

    (void)snprintf(buf, buf_len, "%ld.%02ld%s", (long)whole, (long)frac, unit);
}

static lv_obj_t *create_row(uint8_t row)
{
    lv_obj_t *label = lv_label_create(ui_HomePage);
    uint8_t col = (uint8_t)(row % HOME_PAGE_COL_COUNT);
    uint8_t grid_row = (uint8_t)(row / HOME_PAGE_COL_COUNT);

    lv_obj_set_width(label, HOME_PAGE_CELL_W);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(label, lv_color_hex(0x111827), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(label, LV_OPA_60, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(label, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(label,
                 LV_ALIGN_TOP_LEFT,
                 HOME_PAGE_COL_X0 + ((int16_t)col * HOME_PAGE_COL_STEP),
                 HOME_PAGE_ROW_Y0 + ((int16_t)grid_row * HOME_PAGE_ROW_STEP));

    return label;
}

static void set_row(uint8_t row, const char *text)
{
    if ((row < HOME_PAGE_ROW_COUNT) && (s_rows[row] != NULL))
    {
        lv_label_set_text(s_rows[row], text);
    }
}

static void ui_HomePage_refresh(void)
{
    char line[96];
    char value1[24];
    char value2[24];
    app_time_t time;
    uint8_t motion_idx = g_sensors_motion.buf_idx;

    Time_Get(&time);
    (void)snprintf(line, sizeof(line), "Time\n%02u:%02u:%02u",
                   time.hour, time.min, time.sec);
    set_row(0U, line);

    format_dec1(value1, sizeof(value1), g_sensors_environment.temp, "C");
    format_dec1(value2, sizeof(value2), g_sensors_environment.hum, "%");
    (void)snprintf(line, sizeof(line), "SHT40\nT:%s H:%s", value1, value2);
    set_row(1U, line);

    format_dec1(value1, sizeof(value1), g_sensors_battery.soc, "%");
    format_dec2(value2, sizeof(value2), g_sensors_battery.voltage, "V");
    (void)snprintf(line, sizeof(line), "MAX17048\nSOC:%s %s", value1, value2);
    set_row(2U, line);

    (void)snprintf(line, sizeof(line), "BQ24295\n%s Chg:%s",
                   charger_state_text(g_sensors_charger.state),
                   bool_text(g_sensors_charger.charging));
    set_row(3U, line);

    (void)snprintf(line, sizeof(line), "BQ Power\nPG:%s IN:%s",
                   bool_text(g_sensors_charger.power_good),
                   bool_text(g_sensors_charger.input_present));
    set_row(4U, line);

    (void)snprintf(line, sizeof(line), "BQ Fault\nT:%u Ti:%u W:%u I:%u",
                   (unsigned int)g_sensors_charger.fault_thermal,
                   (unsigned int)g_sensors_charger.fault_timer,
                   (unsigned int)g_sensors_charger.fault_watchdog,
                   (unsigned int)g_sensors_charger.fault_input);
    set_row(5U, line);

    (void)snprintf(line, sizeof(line), "BQ Reg\nS:0x%02X F:0x%02X",
                   (unsigned int)g_sensors_charger.reg_status,
                   (unsigned int)g_sensors_charger.reg_fault);
    set_row(6U, line);

    format_dec2(value1, sizeof(value1), g_sensors_ina226.voltage / 1000.0f, "V");
    format_dec1(value2, sizeof(value2), g_sensors_ina226.current, "mA");
    (void)snprintf(line, sizeof(line), "INA226\nV:%s I:%s", value1, value2);
    set_row(7U, line);

    format_dec1(value1, sizeof(value1), g_sensors_ina226.power, "mW");
    (void)snprintf(line, sizeof(line), "INA Power\n%s", value1);
    set_row(8U, line);

    (void)snprintf(line, sizeof(line), "LIS3DH X\n%ld",
                   (long)g_sensors_motion.x[motion_idx]);
    set_row(9U, line);

    (void)snprintf(line, sizeof(line), "LIS3DH Y\n%ld",
                   (long)g_sensors_motion.y[motion_idx]);
    set_row(10U, line);

    (void)snprintf(line, sizeof(line), "LIS3DH Z\n%ld",
                   (long)g_sensors_motion.z[motion_idx]);
    set_row(11U, line);

    (void)snprintf(line, sizeof(line), "Motion\nTilt:%s", tilt_text(current_raw_direction));
    set_row(12U, line);

}

static void ui_HomePage_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    ui_HomePage_refresh();
}

void ui_HomePage_screen_init(void)
{
    if (ui_HomePage != NULL)
    {
        return;
    }

    ui_HomePage = lv_obj_create(NULL);
    lv_obj_set_scroll_dir(ui_HomePage, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(ui_HomePage, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_bg_color(ui_HomePage, lv_color_hex(0xF4F7FB), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_HomePage, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_HomePage, 18, LV_PART_MAIN | LV_STATE_DEFAULT);

    s_title_label = lv_label_create(ui_HomePage);
    lv_label_set_text(s_title_label, "HOME SENSOR DASHBOARD");
    lv_obj_set_style_text_color(s_title_label, lv_color_hex(0x1F2937), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(s_title_label, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(s_title_label, LV_ALIGN_TOP_LEFT, 10, 14);

    for (uint8_t i = 0U; i < HOME_PAGE_ROW_COUNT; i++)
    {
        s_rows[i] = create_row(i);
    }

    ui_HomePage_refresh();
    s_refresh_timer = lv_timer_create(ui_HomePage_timer_cb, 1000U, NULL);
}

void ui_HomePage_screen_destroy(void)
{
    if (s_refresh_timer != NULL)
    {
        lv_timer_del(s_refresh_timer);
        s_refresh_timer = NULL;
    }

    s_title_label = NULL;
    for (uint8_t i = 0U; i < HOME_PAGE_ROW_COUNT; i++)
    {
        s_rows[i] = NULL;
    }
    ui_HomePage = NULL;
}

void ui_HomePage_key_handler(void *key_event)
{
    ui_page_handle_default_key_event(key_event);
}
