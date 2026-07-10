#include "screens/ui_HomePage.h"

#include <stdio.h>
#include "log.h"
#include "data_app.h"
#include "sensors_app.h"
#include "ui.h"

#include "poetry_app.h"
#include "heiti_lvgl_font.h"
#include <stdlib.h>

#define HOME_PAGE_ROW_COUNT 13U
#define HOME_PAGE_COL_COUNT 2U
#define HOME_PAGE_ROW_Y0    40
#define HOME_PAGE_ROW_STEP  44
#define HOME_PAGE_COL_X0    8
#define HOME_PAGE_COL_STEP  114
#define HOME_PAGE_CELL_W    106

/* Poem section */
#define HOME_PAGE_POEM_LINE_MAX   8U
#define HOME_PAGE_POEM_INTERVAL   10U
#define HOME_PAGE_POEM_Y0         4
#define HOME_PAGE_POEM_X_RIGHT    10U
#define HOME_PAGE_POEM_LINE_H     18U
#define HOME_PAGE_POEM_W          400U

lv_obj_t *ui_HomePage = NULL;

static lv_obj_t *s_title_label = NULL;
static lv_obj_t *s_rows[HOME_PAGE_ROW_COUNT] = {0};
static lv_timer_t *s_refresh_timer = NULL;

/* Poem state */
static HeitiFont_Context_t s_heiti_ctx;
static const lv_font_t *s_heiti_lvgl = NULL;
static lv_obj_t *s_poem_headline = NULL;
static lv_obj_t *s_poem_lines[HOME_PAGE_POEM_LINE_MAX];
static uint8_t s_poem_buf[POETRY_APP_MAX_TEXT_SIZE];
static uint32_t s_poem_tick = 0;

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

    /* --- Poem update (every ~60 s) --- */
    // s_poem_tick++;
    // if (s_poem_tick >= HOME_PAGE_POEM_INTERVAL && s_heiti_lvgl != NULL)
    // {
    //     s_poem_tick = 0;
    //     PoetryApp_Poem_t poem;
    //     int ret = PoetryApp_OpenCollection(POETRY_COLL_TANG_300);
    //     log_printf("[D] open=%d", ret);

    //     if (ret == POETRY_APP_OK)
    //     {
    //         ret = PoetryApp_GetRandomPoem(POETRY_COLL_TANG_300,
    //                                        s_poem_buf,
    //                                        sizeof(s_poem_buf),
    //                                        &poem);
    //         log_printf("[D] poem=%d lines=%u", ret, (unsigned)poem.line_count);
    //         if (ret == POETRY_APP_OK)
    //         {
    //             for (uint8_t i = 0U; i < HOME_PAGE_POEM_LINE_MAX; i++)
    //             {
    //                 if (i < poem.line_count)
    //                 {
    //                     lv_label_set_text(s_poem_lines[i], poem.lines[i]);
    //                     lv_obj_clear_flag(s_poem_lines[i], LV_OBJ_FLAG_HIDDEN);
    //                 }
    //                 else
    //                 {
    //                     lv_obj_add_flag(s_poem_lines[i], LV_OBJ_FLAG_HIDDEN);
    //                 }
    //             }
    //         }
    //         PoetryApp_CloseCollection(POETRY_COLL_TANG_300);
    //     }
    // }
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

    /* --- Poem section --- */
    if (HeitiFont_Open(&s_heiti_ctx, HEITI_FONT_16) == HEITI_FONT_OK)
    {
        s_heiti_lvgl = HeitiLvgl_Register(&s_heiti_ctx);
        log_printf("[D] font lvgl=%p", (void*)s_heiti_lvgl);
    }
    else
    {
        log_printf("[D] font open FAIL");
    }

    srand(lv_tick_get());

    s_poem_headline = lv_label_create(ui_HomePage);
    lv_label_set_text(s_poem_headline, "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80 POEM \xe2\x94\x80\xe2\x94\x80\xe2\x94\x80");
    lv_obj_set_style_text_color(s_poem_headline,
                                 lv_color_hex(0x9CA3AF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(s_poem_headline,
                                 &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(s_poem_headline, LV_ALIGN_TOP_LEFT, 10, HOME_PAGE_POEM_Y0);

    for (uint8_t i = 0U; i < HOME_PAGE_POEM_LINE_MAX; i++)
    {
        s_poem_lines[i] = lv_label_create(ui_HomePage);
        lv_label_set_text(s_poem_lines[i], "");
        lv_obj_set_width(s_poem_lines[i], HOME_PAGE_POEM_W);
        lv_label_set_long_mode(s_poem_lines[i], LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_color(s_poem_lines[i],
                                     lv_color_hex(0x374151), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (s_heiti_lvgl != NULL)
        {
            lv_obj_set_style_text_font(s_poem_lines[i],
                                        s_heiti_lvgl, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        lv_obj_set_style_text_align(s_poem_lines[i],
                                     LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_align(s_poem_lines[i], LV_ALIGN_TOP_RIGHT,
                     -(int16_t)HOME_PAGE_POEM_X_RIGHT,
                     HOME_PAGE_POEM_Y0 + 24 + i * HOME_PAGE_POEM_LINE_H);
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
    s_poem_headline = NULL;
    for (uint8_t i = 0U; i < HOME_PAGE_POEM_LINE_MAX; i++)
    {
        s_poem_lines[i] = NULL;
    }
    s_heiti_lvgl = NULL;
    HeitiFont_Close(&s_heiti_ctx);
    ui_HomePage = NULL;
}

void ui_HomePage_key_handler(void *key_event)
{
    ui_page_handle_default_key_event(key_event);
}
