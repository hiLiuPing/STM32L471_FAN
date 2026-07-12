#include "ui_HomePage.h"

#include "core/egui_timer.h"
#include "egui_port_stm32l471_fan.h"
#include "page_manager.h"
#include "qoi_scene_res.h"
#include "ui_common.h"

typedef struct
{
    egui_view_t base;
    egui_timer_t timer;
} ui_home_page_t;

static ui_home_page_t s_home_page;
static egui_view_api_t s_home_api;

egui_view_t *ui_HomePage = NULL;

static void ui_HomePage_on_draw(egui_view_t *self);
static void ui_HomePage_draw_scene(egui_canvas_t *canvas);
static void ui_HomePage_timer_cb(egui_timer_t *timer);

/* Screen bounds for culling */
#define SCREEN_W (int)UI_SCREEN_W

/* Draw image only if partially visible on screen.
 * All scene images are ≤232px wide, so x∈(-232, 428) guarantees visibility. */
static inline void draw_if_visible(const egui_image_qoi_t *img, egui_canvas_t *canvas,
                                    int x, int y)
{
    if (x < SCREEN_W && x > -232)
    {
        egui_image_draw_image(&img->base, canvas, x, y);
    }
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
    egui_view_start_periodic(view, &s_home_page.timer, view, ui_HomePage_timer_cb, 100U);
}

void ui_HomePage_screen_destroy(void)
{
}

bool ui_HomePage_key_handler(void *key_event)
{
    return ui_page_consume_nav_key_event(key_event);
}

static void ui_HomePage_timer_cb(egui_timer_t *timer)
{
    egui_view_t *view = (egui_view_t *)timer->user_data;

    if ((view != NULL) && egui_view_get_visible(view))
    {
        egui_view_invalidate_full(view);
    }
}

static void draw_cloud_group(egui_canvas_t *canvas, int base_x)
{
    draw_if_visible(&qoi_scene_cloud1, canvas, base_x + 20, 8);
    draw_if_visible(&qoi_scene_cloud2, canvas, base_x + 300, 50);
}

static void draw_grass_front_group(egui_canvas_t *canvas, int base_x)
{
    draw_if_visible(&qoi_scene_grassF0, canvas, base_x + 62, 120);
    draw_if_visible(&qoi_scene_grassF1, canvas, base_x + 126, 120);
    draw_if_visible(&qoi_scene_grassF2, canvas, base_x + 200, 120);
    draw_if_visible(&qoi_scene_grassF3, canvas, base_x + 254, 120);
    draw_if_visible(&qoi_scene_grassF4, canvas, base_x + 300, 120);
    draw_if_visible(&qoi_scene_grassF5, canvas, base_x + 390, 120);
}

static void ui_HomePage_draw_scene(egui_canvas_t *canvas)
{
#if EGUI_CONFIG_FUNCTION_IMAGE_CODEC_QOI

    static const egui_image_qoi_t *const bikes[] =
    {
        &qoi_scene_bike1,
        &qoi_scene_bike2,
        &qoi_scene_bike3,
        &qoi_scene_bike4,
    };

    uint32_t tick = egui_timer_get_current_time();

    uint8_t bike_index = (tick / 100) % 4;

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
    egui_image_draw_image(&bikes[bike_index]->base, canvas, 178, 55);

#else
    EGUI_UNUSED(canvas);
#endif
}

static void ui_HomePage_on_draw(egui_view_t *self)
{
    egui_canvas_t *canvas = egui_view_get_canvas(self);

    ui_draw_rect(canvas, 0, 0, UI_SCREEN_W, UI_SCREEN_H, 0x87CEEB);
    ui_HomePage_draw_scene(canvas);
}
