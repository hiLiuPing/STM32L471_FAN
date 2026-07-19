#include "ui_WeatherPage.h"

#include <stdio.h>

#include "core/egui_timer.h"
#include "egui_port_stm32l471_fan.h"
#include "icons.h"
#include "page_manager.h"
#include "ui_common.h"
#include "ui_heiti_font.h"
#include "weather_app.h"

#define UI_WEATHER_DAY_COUNT       7U
#define UI_WEATHER_DATE_Y          1
#define UI_WEATHER_DATE_H          11
#define UI_WEATHER_WEEKDAY_Y       18
#define UI_WEATHER_WEEKDAY_H       13
#define UI_WEATHER_ICON_Y          32
#define UI_WEATHER_ICON_SIZE       27
#define UI_WEATHER_HIGH_TRACK_TOP  74
#define UI_WEATHER_HIGH_TRACK_BOT  86
#define UI_WEATHER_LOW_TRACK_TOP   106
#define UI_WEATHER_LOW_TRACK_BOT   120
#define UI_WEATHER_TEMP_TEXT_H     14

#define UI_WEATHER_BG_RGB          0x0B1020U
#define UI_WEATHER_DATE_RGB        0xCBD5E1U
#define UI_WEATHER_WEEKDAY_RGB     0x94A3B8U
#define UI_WEATHER_TODAY_RGB       0xF97316U
#define UI_WEATHER_HIGH_RGB        0xFACC15U
#define UI_WEATHER_LOW_RGB         0x3B82F6U
#define UI_WEATHER_STATE_RGB       0x94A3B8U
#define UI_WEATHER_STALE_ALPHA     160U

typedef struct
{
    egui_view_t base;
    egui_timer_t timer;
} ui_weather_page_t;

static ui_weather_page_t s_weather_page;
static egui_view_api_t s_weather_api;

egui_view_t *ui_WeatherPage = NULL;

static void ui_WeatherPage_on_draw(egui_view_t *self);
static void ui_WeatherPage_timer_cb(egui_timer_t *timer);

static egui_dim_t ui_weather_column_center(uint8_t index)
{
    return (egui_dim_t)(((uint32_t)(index * 2U + 1U) * UI_SCREEN_W) /
                        (UI_WEATHER_DAY_COUNT * 2U));
}

static egui_dim_t ui_weather_column_left(uint8_t index)
{
    return (egui_dim_t)(((uint32_t)index * UI_SCREEN_W) / UI_WEATHER_DAY_COUNT);
}

static egui_dim_t ui_weather_column_width(uint8_t index)
{
    egui_dim_t left = ui_weather_column_left(index);
    egui_dim_t right = (egui_dim_t)(((uint32_t)(index + 1U) * UI_SCREEN_W) /
                                    UI_WEATHER_DAY_COUNT);

    return (egui_dim_t)(right - left);
}

static uint8_t ui_weather_is_leap_year(int year)
{
    return (uint8_t)(((year % 4 == 0) && (year % 100 != 0)) ||
                     (year % 400 == 0));
}

static uint8_t ui_weather_parse_date(const char *text, int *year, int *month, int *day)
{
    static const uint8_t days_in_month[] = {
        31U, 28U, 31U, 30U, 31U, 30U,
        31U, 31U, 30U, 31U, 30U, 31U,
    };
    int parsed_year;
    int parsed_month;
    int parsed_day;
    int max_day;
    char tail;

    if ((text == NULL) || (year == NULL) || (month == NULL) || (day == NULL) ||
        (sscanf(text, "%d-%d-%d%c", &parsed_year, &parsed_month, &parsed_day, &tail) != 3) ||
        (parsed_year < 1) || (parsed_month < 1) || (parsed_month > 12))
    {
        return 0U;
    }

    max_day = days_in_month[parsed_month - 1];
    if ((parsed_month == 2) && (ui_weather_is_leap_year(parsed_year) != 0U))
    {
        max_day++;
    }
    if ((parsed_day < 1) || (parsed_day > max_day))
    {
        return 0U;
    }

    *year = parsed_year;
    *month = parsed_month;
    *day = parsed_day;
    return 1U;
}

static uint8_t ui_weather_weekday(int year, int month, int day)
{
    static const uint8_t month_offset[] = {
        0U, 3U, 2U, 5U, 0U, 3U,
        5U, 1U, 4U, 6U, 2U, 4U,
    };

    if (month < 3)
    {
        year--;
    }

    return (uint8_t)((year + year / 4 - year / 100 + year / 400 +
                      month_offset[month - 1] + day) % 7);
}

static egui_dim_t ui_weather_map_temperature(int value,
                                             int min_value,
                                             int max_value,
                                             egui_dim_t track_top,
                                             egui_dim_t track_bottom)
{
    if (max_value == min_value)
    {
        return (egui_dim_t)(track_top + (track_bottom - track_top) / 2);
    }

    return (egui_dim_t)(track_top +
                        ((int32_t)(max_value - value) *
                         (track_bottom - track_top)) /
                            (max_value - min_value));
}

static void ui_weather_draw_curve(egui_canvas_t *canvas,
                                  const egui_dim_t *point_x,
                                  const egui_dim_t *point_y,
                                  uint32_t color_rgb)
{
    egui_color_t color = ui_color(color_rgb);

    for (uint8_t i = 0U; i < (UI_WEATHER_DAY_COUNT - 1U); i++)
    {
        egui_dim_t control_dx = (egui_dim_t)((point_x[i + 1U] - point_x[i]) / 3);

        egui_canvas_draw_bezier_cubic(canvas,
                                      point_x[i],
                                      point_y[i],
                                      (egui_dim_t)(point_x[i] + control_dx),
                                      point_y[i],
                                      (egui_dim_t)(point_x[i + 1U] - control_dx),
                                      point_y[i + 1U],
                                      point_x[i + 1U],
                                      point_y[i + 1U],
                                      2,
                                      color,
                                      EGUI_ALPHA_100);
    }

    for (uint8_t i = 0U; i < UI_WEATHER_DAY_COUNT; i++)
    {
        egui_canvas_draw_circle_fill(canvas,
                                     point_x[i],
                                     point_y[i],
                                     2,
                                     color,
                                     EGUI_ALPHA_100);
    }
}

void ui_WeatherPage_screen_init(void)
{
    egui_view_t *view = EGUI_VIEW_OF(&s_weather_page.base);

    ui_WeatherPage = view;
    egui_view_init(view, egui_port_get_core());
    egui_view_copy_api(view, &s_weather_api);
    s_weather_api.on_draw = ui_WeatherPage_on_draw;
    view->api = &s_weather_api;
    egui_view_set_position(view, 0, 0);
    egui_view_set_size(view, UI_SCREEN_W, UI_SCREEN_H);
    egui_view_set_visible(view, 1);
    egui_view_start_periodic(view, &s_weather_page.timer, view, ui_WeatherPage_timer_cb, 2000U);
}

void ui_WeatherPage_screen_destroy(void)
{
}

bool ui_WeatherPage_key_handler(void *key_event)
{
    return ui_page_consume_nav_key_event(key_event);
}

static void ui_WeatherPage_timer_cb(egui_timer_t *timer)
{
    egui_view_t *view = (egui_view_t *)timer->user_data;

    if ((view != NULL) && egui_view_get_visible(view))
    {
        egui_view_invalidate_full(view);
    }
}

static void ui_WeatherPage_on_draw(egui_view_t *self)
{
    egui_canvas_t *canvas = egui_view_get_canvas(self);
    WeatherSnapshot_t weather;
    egui_dim_t point_x[UI_WEATHER_DAY_COUNT];
    egui_dim_t high_y[UI_WEATHER_DAY_COUNT];
    egui_dim_t low_y[UI_WEATHER_DAY_COUNT];
    egui_alpha_t original_alpha;
    int high_min;
    int high_max;
    int low_min;
    int low_max;

    WeatherApp_GetSnapshot(&weather);
    ui_draw_rect(canvas, 0, 0, UI_SCREEN_W, UI_SCREEN_H, UI_WEATHER_BG_RGB);

    if (weather.valid == 0U)
    {
        const char *state_text = (WeatherApp_IsSyncing() != 0U) ?
                                     "天气同步中" :
                                     "暂无天气数据";

        ui_draw_text(canvas,
                     ui_heiti_font_get_16(),
                     state_text,
                     0,
                     0,
                     UI_SCREEN_W,
                     UI_SCREEN_H,
                     EGUI_ALIGN_CENTER,
                     UI_WEATHER_STATE_RGB);
        return;
    }

    original_alpha = egui_canvas_get_alpha(canvas);
    if (weather.stale != 0U)
    {
        egui_canvas_mix_alpha(canvas, UI_WEATHER_STALE_ALPHA);
    }

    high_min = weather.future[0].temp_high;
    high_max = weather.future[0].temp_high;
    low_min = weather.future[0].temp_low;
    low_max = weather.future[0].temp_low;

    for (uint8_t i = 1U; i < UI_WEATHER_DAY_COUNT; i++)
    {
        if (weather.future[i].temp_high < high_min)
        {
            high_min = weather.future[i].temp_high;
        }
        if (weather.future[i].temp_high > high_max)
        {
            high_max = weather.future[i].temp_high;
        }
        if (weather.future[i].temp_low < low_min)
        {
            low_min = weather.future[i].temp_low;
        }
        if (weather.future[i].temp_low > low_max)
        {
            low_max = weather.future[i].temp_low;
        }
    }

    for (uint8_t i = 0U; i < UI_WEATHER_DAY_COUNT; i++)
    {
        const WeatherDay_t *day = &weather.future[i];
        const egui_image_std_t *icon = ui_weather_icon_get((uint16_t)day->icon_id);
        egui_dim_t column_x = ui_weather_column_left(i);
        egui_dim_t column_w = ui_weather_column_width(i);
        egui_dim_t center_x = ui_weather_column_center(i);
        char date_text[8] = "--";
        const char *weekday_text = "--";
        int year;
        int month;
        int date;

        point_x[i] = center_x;
        high_y[i] = ui_weather_map_temperature(day->temp_high,
                                               high_min,
                                               high_max,
                                               UI_WEATHER_HIGH_TRACK_TOP,
                                               UI_WEATHER_HIGH_TRACK_BOT);
        low_y[i] = ui_weather_map_temperature(day->temp_low,
                                              low_min,
                                              low_max,
                                              UI_WEATHER_LOW_TRACK_TOP,
                                              UI_WEATHER_LOW_TRACK_BOT);

        if (ui_weather_parse_date(day->date, &year, &month, &date) != 0U)
        {
            static const char *const weekday_names[] = {
                "周日", "周一", "周二", "周三", "周四", "周五", "周六",
            };

            (void)snprintf(date_text, sizeof(date_text), "%02d-%02d", month, date);
            weekday_text = (i == 0U) ?
                               "今天" :
                               weekday_names[ui_weather_weekday(year, month, date)];
        }
        else if (i == 0U)
        {
            weekday_text = "今天";
        }

        ui_draw_text(canvas,
                     EGUI_FONT_OF(&egui_res_font_montserrat_10_4),
                     date_text,
                     column_x,
                     UI_WEATHER_DATE_Y,
                     column_w,
                     UI_WEATHER_DATE_H,
                     EGUI_ALIGN_CENTER,
                     UI_WEATHER_DATE_RGB);
        ui_draw_text(canvas,
                     ui_heiti_font_get_12(),
                     weekday_text,
                     column_x,
                     UI_WEATHER_WEEKDAY_Y,
                     column_w,
                     UI_WEATHER_WEEKDAY_H,
                     EGUI_ALIGN_CENTER,
                     (i == 0U) ? UI_WEATHER_TODAY_RGB : UI_WEATHER_WEEKDAY_RGB);

        if (icon != NULL)
        {
            egui_image_draw_image_resize(&icon->base,
                                         canvas,
                                         (egui_dim_t)(center_x - UI_WEATHER_ICON_SIZE / 2),
                                         UI_WEATHER_ICON_Y,
                                         UI_WEATHER_ICON_SIZE,
                                         UI_WEATHER_ICON_SIZE);
        }
        else
        {
            ui_draw_text(canvas,
                         EGUI_FONT_OF(&egui_res_font_montserrat_12_4),
                         "--",
                         column_x,
                         UI_WEATHER_ICON_Y,
                         column_w,
                         UI_WEATHER_ICON_SIZE,
                         EGUI_ALIGN_CENTER,
                         UI_WEATHER_WEEKDAY_RGB);
        }
    }

    ui_weather_draw_curve(canvas, point_x, high_y, UI_WEATHER_HIGH_RGB);
    ui_weather_draw_curve(canvas, point_x, low_y, UI_WEATHER_LOW_RGB);

    for (uint8_t i = 0U; i < UI_WEATHER_DAY_COUNT; i++)
    {
        egui_dim_t column_x = ui_weather_column_left(i);
        egui_dim_t column_w = ui_weather_column_width(i);
        char temp_text[16];

        (void)snprintf(temp_text,
                       sizeof(temp_text),
                       "%s%dC",
                       (weather.stale != 0U) ? "~" : "",
                       weather.future[i].temp_high);
        ui_draw_text(canvas,
                     EGUI_FONT_OF(&egui_res_font_montserrat_12_4),
                     temp_text,
                     column_x,
                     (egui_dim_t)(high_y[i] - UI_WEATHER_TEMP_TEXT_H),
                     column_w,
                     UI_WEATHER_TEMP_TEXT_H,
                     EGUI_ALIGN_CENTER,
                     UI_WEATHER_HIGH_RGB);

        (void)snprintf(temp_text,
                       sizeof(temp_text),
                       "%s%dC",
                       (weather.stale != 0U) ? "~" : "",
                       weather.future[i].temp_low);
        ui_draw_text(canvas,
                     EGUI_FONT_OF(&egui_res_font_montserrat_12_4),
                     temp_text,
                     column_x,
                     (egui_dim_t)(low_y[i] + 2),
                     column_w,
                     UI_WEATHER_TEMP_TEXT_H,
                     EGUI_ALIGN_CENTER,
                     UI_WEATHER_LOW_RGB);
    }

    egui_canvas_set_alpha(canvas, original_alpha);
}
