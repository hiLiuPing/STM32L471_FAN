#include "ui_WeatherPage.h"

#include <stdio.h>

#include "core/egui_timer.h"
#include "egui_port_stm32l471_fan.h"
#include "page_manager.h"
#include "ui_common.h"
#include "weather_app.h"

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

static const char *ui_weather_text_or_dash(const char *text)
{
    return ((text != NULL) && (text[0] != '\0')) ? text : "--";
}

static void ui_WeatherPage_on_draw(egui_view_t *self)
{
    char line[48];
    egui_canvas_t *canvas = egui_view_get_canvas(self);

    ui_draw_header(canvas, "Weather", "N/R switch pages", 0xF97316);

    ui_draw_panel(canvas, 18, 48, 122, 72, 0x2B1A10, 0xF97316);
    (void)snprintf(line, sizeof(line), "%dC", g_now_weather.temp);
    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_36_4), line, 30, 54, 92, 38, EGUI_ALIGN_LEFT | EGUI_ALIGN_VCENTER, 0xFFEDD5);
    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_12_4), ui_weather_text_or_dash(g_now_weather.text), 30, 92, 92, 16,
                 EGUI_ALIGN_LEFT | EGUI_ALIGN_VCENTER, 0xFDBA74);

    ui_draw_panel(canvas, 154, 48, 118, 72, 0x111827, 0x334155);
    (void)snprintf(line, sizeof(line), "AQI %d", g_air_detail.aqi);
    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_18_4), line, 166, 55, 92, 22, EGUI_ALIGN_LEFT | EGUI_ALIGN_VCENTER, 0xF8FAFC);
    (void)snprintf(line, sizeof(line), "PM2.5 %d", g_air_detail.pm25);
    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_12_4), line, 166, 80, 92, 14, EGUI_ALIGN_LEFT | EGUI_ALIGN_VCENTER, 0xCBD5E1);
    (void)snprintf(line, sizeof(line), "Hum %d%%", g_now_weather.humidity);
    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_12_4), line, 166, 96, 92, 14, EGUI_ALIGN_LEFT | EGUI_ALIGN_VCENTER, 0xCBD5E1);

    ui_draw_panel(canvas, 290, 48, 118, 72, 0x10202B, 0x38BDF8);
    for (uint8_t i = 0U; i < 3U; i++)
    {
        const WeatherDay_t *day = &g_future_weather[i];
        egui_dim_t y = (egui_dim_t)(54 + i * 20);

        (void)snprintf(line, sizeof(line), "%s %d/%d", ui_weather_text_or_dash(day->date), day->temp_low, day->temp_high);
        ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_12_4), line, 300, y, 96, 16, EGUI_ALIGN_LEFT | EGUI_ALIGN_VCENTER, 0xE0F2FE);
    }
}
