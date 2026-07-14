#include "ui_HomePage.h"

#include "core/egui_timer.h"
#include "data_app.h"
#include "egui_port_stm32l471_fan.h"
#include "home_scene_res.h"
#include "icons.h"
#include "page_manager.h"
#include "ui_common.h"
#include "ui_heiti_font.h"

typedef struct
{
    egui_view_t base;
    egui_timer_t timer;
} ui_home_page_t;

typedef struct
{
    int cloud_base;
    int grass_base;
    int bike_x;
    uint8_t bike_index;
    uint8_t is_valid;
} ui_home_scene_state_t;

static ui_home_page_t s_home_page;
static egui_view_api_t s_home_api;

egui_view_t *ui_HomePage = NULL;
static bool s_home_animation_enabled = true;
static uint32_t s_home_scene_tick = 0U;
static uint32_t s_home_status_tick = 0U;
static uint32_t s_home_status_version = 0xFFFFFFFFU;
static ui_home_scene_state_t s_home_scene_state;
static const egui_font_t *s_home_heiti_16 = NULL;

static void ui_HomePage_on_draw(egui_view_t *self);
static void ui_HomePage_draw_scene(egui_canvas_t *canvas);
static void ui_HomePage_draw_status(egui_canvas_t *canvas);
static void ui_HomePage_timer_cb(egui_timer_t *timer);

/* Screen bounds for culling */
#define SCREEN_W (int)UI_SCREEN_W

#define HOME_STATUS_TOP_X 0
#define HOME_STATUS_TOP_Y 0
#define HOME_STATUS_TOP_W UI_SCREEN_W
#define HOME_STATUS_TOP_H 34
#define HOME_STATUS_ENV_X 288
#define HOME_STATUS_ENV_Y 110
#define HOME_STATUS_ENV_W 136
#define HOME_STATUS_ENV_H 32
#define HOME_STATUS_PM25_X 8
#define HOME_STATUS_PM25_Y 116
#define HOME_STATUS_PM25_W 130
#define HOME_STATUS_PM25_H 24

#define HOME_SCENE_WRAP_W 428
#define HOME_SCENE_CLOUD_STEP_MS 100U
#define HOME_SCENE_GRASS_STEP_MS 50U
#define HOME_SCENE_BIKE_STEP_MS 100U
#define HOME_BIKE_CYCLE_MS 3600000U
#define HOME_SCENE_RESUME_GAP_MS 250U

#define HOME_CLOUD1_X 20
#define HOME_CLOUD1_Y 8
#define HOME_CLOUD1_W 229
#define HOME_CLOUD1_H 112
#define HOME_CLOUD2_X 300
#define HOME_CLOUD2_Y 50
#define HOME_CLOUD2_W 156
#define HOME_CLOUD2_H 76

#define HOME_GRASS_F0_X 62
#define HOME_GRASS_F0_Y 120
#define HOME_GRASS_F0_W 28
#define HOME_GRASS_F0_H 20
#define HOME_GRASS_F1_X 126
#define HOME_GRASS_F1_Y 120
#define HOME_GRASS_F1_W 46
#define HOME_GRASS_F1_H 22
#define HOME_GRASS_F2_X 200
#define HOME_GRASS_F2_Y 120
#define HOME_GRASS_F2_W 39
#define HOME_GRASS_F2_H 36
#define HOME_GRASS_F3_X 254
#define HOME_GRASS_F3_Y 120
#define HOME_GRASS_F3_W 42
#define HOME_GRASS_F3_H 21
#define HOME_GRASS_F4_X 300
#define HOME_GRASS_F4_Y 120
#define HOME_GRASS_F4_W 38
#define HOME_GRASS_F4_H 27
#define HOME_GRASS_F5_X 390
#define HOME_GRASS_F5_Y 120
#define HOME_GRASS_F5_W 54
#define HOME_GRASS_F5_H 27

#define HOME_BIKE_START_X (int)((((uint32_t)UI_SCREEN_W * 20U) + 50U) / 100U)
#define HOME_BIKE_END_X (int)((((uint32_t)UI_SCREEN_W * 80U) + 50U) / 100U)
#define HOME_BIKE_Y 55
#define HOME_BIKE_W 62
#define HOME_BIKE_H 64

/* Draw image only if partially visible on screen.
 * All scene images are ≤232px wide, so x∈(-232, 428) guarantees visibility. */
static inline void draw_if_visible(const egui_image_std_t *img, egui_canvas_t *canvas,
                                    int x, int y)
{
    if (x < SCREEN_W && x > -232)
    {
        egui_image_draw_image(&img->base, canvas, x, y);
    }
}

static void ui_HomePage_invalidate_rect(egui_view_t *view,
                                        egui_dim_t x,
                                        egui_dim_t y,
                                        egui_dim_t w,
                                        egui_dim_t h)
{
    egui_region_t region;

    region.location.x = x;
    region.location.y = y;
    region.size.width = w;
    region.size.height = h;
    egui_view_invalidate_region(view, &region);
}

static void ui_HomePage_invalidate_clipped_rect(egui_view_t *view,
                                                int x,
                                                int y,
                                                int w,
                                                int h)
{
    int x2 = x + w;
    int y2 = y + h;

    if ((view == NULL) || (w <= 0) || (h <= 0) || (x2 <= 0) || (y2 <= 0) || (x >= (int)UI_SCREEN_W) || (y >= (int)UI_SCREEN_H))
    {
        return;
    }

    if (x < 0)
    {
        x = 0;
    }
    if (y < 0)
    {
        y = 0;
    }
    if (x2 > (int)UI_SCREEN_W)
    {
        x2 = (int)UI_SCREEN_W;
    }
    if (y2 > (int)UI_SCREEN_H)
    {
        y2 = (int)UI_SCREEN_H;
    }

    if ((x2 > x) && (y2 > y))
    {
        ui_HomePage_invalidate_rect(view, (egui_dim_t)x, (egui_dim_t)y, (egui_dim_t)(x2 - x), (egui_dim_t)(y2 - y));
    }
}

static int ui_HomePage_scene_cloud_base(uint32_t tick)
{
    return -(int)((tick / HOME_SCENE_CLOUD_STEP_MS) % HOME_SCENE_WRAP_W);
}

static int ui_HomePage_scene_grass_base(uint32_t tick)
{
    return -(int)((tick / HOME_SCENE_GRASS_STEP_MS) % HOME_SCENE_WRAP_W);
}

static uint8_t ui_HomePage_scene_bike_index(uint32_t tick)
{
    return (uint8_t)((tick / HOME_SCENE_BIKE_STEP_MS) % 4U);
}

static int ui_HomePage_scene_bike_x(uint32_t tick)
{
    uint32_t cycle_tick = tick % HOME_BIKE_CYCLE_MS;
    int range = HOME_BIKE_END_X - HOME_BIKE_START_X;

    if (range <= 0)
    {
        return HOME_BIKE_START_X;
    }

    return HOME_BIKE_START_X + (int)(((cycle_tick * (uint32_t)range) + (HOME_BIKE_CYCLE_MS / 2U)) / HOME_BIKE_CYCLE_MS);
}

static void ui_HomePage_get_scene_state(uint32_t tick, ui_home_scene_state_t *state)
{
    if (state == NULL)
    {
        return;
    }

    state->cloud_base = ui_HomePage_scene_cloud_base(tick);
    state->grass_base = ui_HomePage_scene_grass_base(tick);
    state->bike_x = ui_HomePage_scene_bike_x(tick);
    state->bike_index = ui_HomePage_scene_bike_index(tick);
    state->is_valid = 1U;
}

static void ui_HomePage_invalidate_cloud_base(egui_view_t *view, int base_x)
{
    ui_HomePage_invalidate_clipped_rect(view, base_x + HOME_CLOUD1_X, HOME_CLOUD1_Y, HOME_CLOUD1_W, HOME_CLOUD1_H);
    ui_HomePage_invalidate_clipped_rect(view, base_x + HOME_CLOUD2_X, HOME_CLOUD2_Y, HOME_CLOUD2_W, HOME_CLOUD2_H);
}

static void ui_HomePage_union_clipped_bounds(int x,
                                             int y,
                                             int w,
                                             int h,
                                             int *min_x,
                                             int *min_y,
                                             int *max_x,
                                             int *max_y)
{
    int x2 = x + w;
    int y2 = y + h;

    if ((w <= 0) || (h <= 0) || (x2 <= 0) || (y2 <= 0) || (x >= (int)UI_SCREEN_W) || (y >= (int)UI_SCREEN_H))
    {
        return;
    }

    if (x < 0)
    {
        x = 0;
    }
    if (y < 0)
    {
        y = 0;
    }
    if (x2 > (int)UI_SCREEN_W)
    {
        x2 = (int)UI_SCREEN_W;
    }
    if (y2 > (int)UI_SCREEN_H)
    {
        y2 = (int)UI_SCREEN_H;
    }

    if ((x2 <= x) || (y2 <= y))
    {
        return;
    }

    if (x < *min_x)
    {
        *min_x = x;
    }
    if (y < *min_y)
    {
        *min_y = y;
    }
    if (x2 > *max_x)
    {
        *max_x = x2;
    }
    if (y2 > *max_y)
    {
        *max_y = y2;
    }
}

static void ui_HomePage_invalidate_grass_front_base(egui_view_t *view, int base_x)
{
    int min_x = (int)UI_SCREEN_W;
    int min_y = (int)UI_SCREEN_H;
    int max_x = 0;
    int max_y = 0;

    ui_HomePage_union_clipped_bounds(base_x + HOME_GRASS_F0_X, HOME_GRASS_F0_Y, HOME_GRASS_F0_W, HOME_GRASS_F0_H, &min_x, &min_y, &max_x, &max_y);
    ui_HomePage_union_clipped_bounds(base_x + HOME_GRASS_F1_X, HOME_GRASS_F1_Y, HOME_GRASS_F1_W, HOME_GRASS_F1_H, &min_x, &min_y, &max_x, &max_y);
    ui_HomePage_union_clipped_bounds(base_x + HOME_GRASS_F2_X, HOME_GRASS_F2_Y, HOME_GRASS_F2_W, HOME_GRASS_F2_H, &min_x, &min_y, &max_x, &max_y);
    ui_HomePage_union_clipped_bounds(base_x + HOME_GRASS_F3_X, HOME_GRASS_F3_Y, HOME_GRASS_F3_W, HOME_GRASS_F3_H, &min_x, &min_y, &max_x, &max_y);
    ui_HomePage_union_clipped_bounds(base_x + HOME_GRASS_F4_X, HOME_GRASS_F4_Y, HOME_GRASS_F4_W, HOME_GRASS_F4_H, &min_x, &min_y, &max_x, &max_y);
    ui_HomePage_union_clipped_bounds(base_x + HOME_GRASS_F5_X, HOME_GRASS_F5_Y, HOME_GRASS_F5_W, HOME_GRASS_F5_H, &min_x, &min_y, &max_x, &max_y);

    if ((max_x > min_x) && (max_y > min_y))
    {
        ui_HomePage_invalidate_rect(view, (egui_dim_t)min_x, (egui_dim_t)min_y, (egui_dim_t)(max_x - min_x), (egui_dim_t)(max_y - min_y));
    }
}

static void ui_HomePage_invalidate_precise_scene(egui_view_t *view, const ui_home_scene_state_t *prev, const ui_home_scene_state_t *next)
{
    if ((view == NULL) || (prev == NULL) || (next == NULL))
    {
        return;
    }

    if (prev->cloud_base != next->cloud_base)
    {
        ui_HomePage_invalidate_cloud_base(view, prev->cloud_base);
        ui_HomePage_invalidate_cloud_base(view, prev->cloud_base + HOME_SCENE_WRAP_W);
        ui_HomePage_invalidate_cloud_base(view, next->cloud_base);
        ui_HomePage_invalidate_cloud_base(view, next->cloud_base + HOME_SCENE_WRAP_W);
    }

    if (prev->grass_base != next->grass_base)
    {
        ui_HomePage_invalidate_grass_front_base(view, prev->grass_base);
        ui_HomePage_invalidate_grass_front_base(view, prev->grass_base + HOME_SCENE_WRAP_W);
        ui_HomePage_invalidate_grass_front_base(view, next->grass_base);
        ui_HomePage_invalidate_grass_front_base(view, next->grass_base + HOME_SCENE_WRAP_W);
    }

    if ((prev->bike_index != next->bike_index) || (prev->bike_x != next->bike_x))
    {
        ui_HomePage_invalidate_clipped_rect(view, prev->bike_x, HOME_BIKE_Y, HOME_BIKE_W, HOME_BIKE_H);
        ui_HomePage_invalidate_clipped_rect(view, next->bike_x, HOME_BIKE_Y, HOME_BIKE_W, HOME_BIKE_H);
    }
}

static void ui_HomePage_invalidate_status_regions(egui_view_t *view)
{
    ui_HomePage_invalidate_rect(view, HOME_STATUS_TOP_X, HOME_STATUS_TOP_Y, HOME_STATUS_TOP_W, HOME_STATUS_TOP_H);
    ui_HomePage_invalidate_rect(view, HOME_STATUS_PM25_X, HOME_STATUS_PM25_Y, HOME_STATUS_PM25_W, HOME_STATUS_PM25_H);
    ui_HomePage_invalidate_rect(view, HOME_STATUS_ENV_X, HOME_STATUS_ENV_Y, HOME_STATUS_ENV_W, HOME_STATUS_ENV_H);
}

void ui_HomePage_screen_init(void)
{
    egui_view_t *view = EGUI_VIEW_OF(&s_home_page.base);

    ui_HomePage = view;
    egui_view_init(view, egui_port_get_core());
    egui_view_copy_api(view, &s_home_api);
    s_home_api.on_draw = ui_HomePage_on_draw;
    view->api = &s_home_api;
    egui_view_set_position(view, 0, 0);
    egui_view_set_size(view, UI_SCREEN_W, UI_SCREEN_H);
    egui_view_set_visible(view, 1);
    s_home_animation_enabled = true;
    s_home_scene_tick = egui_timer_get_current_time();
    s_home_status_tick = s_home_scene_tick;
    s_home_status_version = 0xFFFFFFFFU;
    s_home_scene_state.is_valid = 0U;
    s_home_heiti_16 = NULL;
    egui_view_start_periodic(view, &s_home_page.timer, view, ui_HomePage_timer_cb, 50U);
}

void ui_HomePage_screen_destroy(void)
{
}

bool ui_HomePage_key_handler(void *key_event)
{
    return ui_page_consume_nav_key_event(key_event);
}

void ui_HomePage_set_animation_enabled(bool enable)
{
    bool next = enable ? true : false;

    if (s_home_animation_enabled == next)
    {
        return;
    }

    s_home_animation_enabled = next;
    if (s_home_animation_enabled)
    {
        s_home_scene_tick = egui_timer_get_current_time();
        s_home_scene_state.is_valid = 0U;
        if ((ui_HomePage != NULL) && egui_view_get_visible(ui_HomePage))
        {
            egui_view_invalidate_full(ui_HomePage);
        }
    }
}

bool ui_HomePage_get_animation_enabled(void)
{
    return s_home_animation_enabled;
}

static void ui_HomePage_timer_cb(egui_timer_t *timer)
{
    egui_view_t *view = (egui_view_t *)timer->user_data;
    DataApp_HomeStatus_t status;
    uint32_t now;
    uint8_t refresh_status = 0U;

    if ((view == NULL) || !egui_view_get_visible(view))
    {
        s_home_scene_state.is_valid = 0U;
        return;
    }

    now = egui_timer_get_current_time();
    DataApp_HomeStatus_Get(&status);
    if (status.version != s_home_status_version)
    {
        s_home_status_version = status.version;
        refresh_status = 1U;
    }
    if ((now - s_home_status_tick) >= 1000U)
    {
        s_home_status_tick = now;
        refresh_status = 1U;
    }

    if (s_home_animation_enabled)
    {
        ui_home_scene_state_t next_scene_state;

        ui_HomePage_get_scene_state(now, &next_scene_state);
        if ((s_home_scene_state.is_valid != 0U) && ((now - s_home_scene_tick) > HOME_SCENE_RESUME_GAP_MS))
        {
            s_home_scene_state.is_valid = 0U;
        }
        if (s_home_scene_state.is_valid == 0U)
        {
            egui_view_invalidate_full(view);
        }
        else
        {
            ui_HomePage_invalidate_precise_scene(view, &s_home_scene_state, &next_scene_state);
        }
        s_home_scene_state = next_scene_state;
        s_home_scene_tick = now;

        if (refresh_status != 0U)
        {
            ui_HomePage_invalidate_status_regions(view);
        }
    }
    else if (refresh_status != 0U)
    {
        ui_HomePage_invalidate_status_regions(view);
    }
}

static void draw_cloud_group(egui_canvas_t *canvas, int base_x)
{
    draw_if_visible(&qoi_scene_cloud1, canvas, base_x + HOME_CLOUD1_X, HOME_CLOUD1_Y);
    draw_if_visible(&qoi_scene_cloud2, canvas, base_x + HOME_CLOUD2_X, HOME_CLOUD2_Y);
}

static void draw_grass_front_group(egui_canvas_t *canvas, int base_x)
{
    draw_if_visible(&qoi_scene_grassF0, canvas, base_x + HOME_GRASS_F0_X, HOME_GRASS_F0_Y);
    draw_if_visible(&qoi_scene_grassF1, canvas, base_x + HOME_GRASS_F1_X, HOME_GRASS_F1_Y);
    draw_if_visible(&qoi_scene_grassF2, canvas, base_x + HOME_GRASS_F2_X, HOME_GRASS_F2_Y);
    draw_if_visible(&qoi_scene_grassF3, canvas, base_x + HOME_GRASS_F3_X, HOME_GRASS_F3_Y);
    draw_if_visible(&qoi_scene_grassF4, canvas, base_x + HOME_GRASS_F4_X, HOME_GRASS_F4_Y);
    draw_if_visible(&qoi_scene_grassF5, canvas, base_x + HOME_GRASS_F5_X, HOME_GRASS_F5_Y);
}

static void ui_HomePage_draw_scene(egui_canvas_t *canvas)
{
#if EGUI_CONFIG_FUNCTION_IMAGE_FORMAT_RGB565

    static const egui_image_std_t *const bikes[] =
    {
        &qoi_scene_bike1,
        &qoi_scene_bike2,
        &qoi_scene_bike3,
        &qoi_scene_bike4,
    };

    uint32_t tick = s_home_scene_tick;

    uint8_t bike_index = ui_HomePage_scene_bike_index(tick);
    int bike_x = ui_HomePage_scene_bike_x(tick);

    /* 云：慢速滚动，裁掉屏幕外的副本 */
    int cloud_offset = (tick / 100) % 428;
    int cloud_base1 = -cloud_offset;
    int cloud_base2 = cloud_base1 + 428;

    draw_cloud_group(canvas, cloud_base1);
    if (cloud_base2 < SCREEN_W)
    {
        draw_cloud_group(canvas, cloud_base2);
    }

    /* 地面：只画屏幕内可见的瓦片 */
    for (int x = 0; x < SCREEN_W; x += 40)
    {
        egui_image_draw_image(&qoi_scene_grass.base, canvas, x, 110);
    }

    draw_if_visible(&qoi_scene_grass0, canvas, 0, 117);
    draw_if_visible(&qoi_scene_grass0, canvas, 195, 117);

    /* 前景草：比云快，裁掉屏幕外的副本 */
    int grass_offset = (tick / 50) % 428;
    int grass_base1 = -grass_offset;
    int grass_base2 = grass_base1 + 428;

    draw_grass_front_group(canvas, grass_base1);
    if (grass_base2 < SCREEN_W)
    {
        draw_grass_front_group(canvas, grass_base2);
    }

    /* 自行车 */
    egui_image_draw_image(&bikes[bike_index]->base, canvas, bike_x, HOME_BIKE_Y);

#else
    EGUI_UNUSED(canvas);
#endif
}

static void ui_HomePage_draw_raw_text(egui_canvas_t *canvas,
                                      const egui_font_t *font,
                                      const char *text,
                                      egui_dim_t x,
                                      egui_dim_t y,
                                      uint32_t rgb)
{
    if ((font != NULL) && (text != NULL))
    {
        egui_canvas_draw_text(canvas, font, text, x, y, ui_color(rgb), EGUI_ALPHA_100);
    }
}

static uint8_t ui_HomePage_canvas_intersects(egui_canvas_t *canvas,
                                             egui_dim_t x,
                                             egui_dim_t y,
                                             egui_dim_t w,
                                             egui_dim_t h)
{
    egui_region_t target;
    egui_region_t clipped;
    egui_region_t *work_region;

    if (canvas == NULL)
    {
        return 0U;
    }

    target.location.x = x;
    target.location.y = y;
    target.size.width = w;
    target.size.height = h;
    work_region = egui_canvas_get_base_view_work_region(canvas);
    egui_region_intersect(work_region, &target, &clipped);

    return (uint8_t)((clipped.size.width > 0) && (clipped.size.height > 0));
}

static void ui_HomePage_draw_top_status(egui_canvas_t *canvas, const DataApp_HomeStatus_t *status)
{
    const egui_font_t *small_font = EGUI_FONT_OF(&egui_res_font_montserrat_16_4);
    const egui_font_t *heiti_font;
#if EGUI_CONFIG_FUNCTION_IMAGE_FORMAT_RGB565
    const egui_image_std_t *weather_icon;
#endif

    if ((status == NULL) ||
        !ui_HomePage_canvas_intersects(canvas, HOME_STATUS_TOP_X, HOME_STATUS_TOP_Y, HOME_STATUS_TOP_W, HOME_STATUS_TOP_H))
    {
        return;
    }

    if (s_home_heiti_16 == NULL)
    {
        s_home_heiti_16 = ui_heiti_font_get_16();
    }
    heiti_font = (s_home_heiti_16 != NULL) ? s_home_heiti_16 : small_font;

    ui_draw_text(canvas, EGUI_FONT_OF(&egui_res_font_montserrat_30_4), status->time_text, 10, 1, 86, 32, EGUI_ALIGN_LEFT | EGUI_ALIGN_VCENTER, 0x4A90E2);
    ui_HomePage_draw_raw_text(canvas, heiti_font, status->date_text, 104, 9, 0x4A90E2);
    ui_HomePage_draw_raw_text(canvas, heiti_font, status->week_text, 194, 9, 0x4A90E2);
    ui_HomePage_draw_raw_text(canvas, heiti_font, status->temp_range_text, 270, 9, 0x4A90E2);

#if EGUI_CONFIG_FUNCTION_IMAGE_FORMAT_RGB565
    weather_icon = ui_weather_icon_get(status->weather_icon_id);
    if (weather_icon != NULL)
    {
        egui_image_draw_image(&weather_icon->base, canvas, 390, 2);
    }
#endif
}

static void ui_HomePage_draw_pm25_status(egui_canvas_t *canvas, const DataApp_HomeStatus_t *status)
{
    const egui_font_t *small_font = EGUI_FONT_OF(&egui_res_font_montserrat_16_4);
    const egui_font_t *heiti_font;

    if ((status == NULL) ||
        !ui_HomePage_canvas_intersects(canvas, HOME_STATUS_PM25_X, HOME_STATUS_PM25_Y, HOME_STATUS_PM25_W, HOME_STATUS_PM25_H))
    {
        return;
    }

    if (s_home_heiti_16 == NULL)
    {
        s_home_heiti_16 = ui_heiti_font_get_16();
    }
    heiti_font = (s_home_heiti_16 != NULL) ? s_home_heiti_16 : small_font;

    ui_HomePage_draw_raw_text(canvas, heiti_font, status->pm25_text, HOME_STATUS_PM25_X, HOME_STATUS_PM25_Y, 0x4A90E2);
}

static void ui_HomePage_draw_env_status(egui_canvas_t *canvas, const DataApp_HomeStatus_t *status)
{
    const egui_font_t *small_font = EGUI_FONT_OF(&egui_res_font_montserrat_16_4);
    const egui_font_t *heiti_font;

    if ((status == NULL) ||
        !ui_HomePage_canvas_intersects(canvas, HOME_STATUS_ENV_X, HOME_STATUS_ENV_Y, HOME_STATUS_ENV_W, HOME_STATUS_ENV_H))
    {
        return;
    }

    if (s_home_heiti_16 == NULL)
    {
        s_home_heiti_16 = ui_heiti_font_get_16();
    }
    heiti_font = (s_home_heiti_16 != NULL) ? s_home_heiti_16 : small_font;

    ui_HomePage_draw_raw_text(canvas, heiti_font, status->env_text, 316, 116, 0x4A90E2);
}

static void ui_HomePage_draw_status(egui_canvas_t *canvas)
{
    DataApp_HomeStatus_t status;

    DataApp_HomeStatus_Get(&status);
    ui_HomePage_draw_top_status(canvas, &status);
    ui_HomePage_draw_pm25_status(canvas, &status);
    ui_HomePage_draw_env_status(canvas, &status);
}

static void ui_HomePage_on_draw(egui_view_t *self)
{
    egui_canvas_t *canvas = egui_view_get_canvas(self);

    ui_draw_rect(canvas, 0, 0, UI_SCREEN_W, UI_SCREEN_H, 0x87CEEB);
    ui_HomePage_draw_scene(canvas);
    ui_HomePage_draw_status(canvas);
}
