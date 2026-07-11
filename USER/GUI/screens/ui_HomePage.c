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
    egui_view_start_periodic(view, &s_home_page.timer, view, ui_HomePage_timer_cb, 250U);
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

static void ui_HomePage_draw_scene(egui_canvas_t *canvas)
{
#if EGUI_CONFIG_FUNCTION_IMAGE_CODEC_QOI
    const egui_image_qoi_t *bikes[] = {
        &qoi_scene_bike1,
        &qoi_scene_bike2,
        &qoi_scene_bike3,
        &qoi_scene_bike4,
    };
    uint32_t tick = egui_timer_get_current_time();
    uint8_t bike_index = (uint8_t)((tick / 250U) % (sizeof(bikes) / sizeof(bikes[0])));

    egui_image_draw_image(&qoi_scene_cloud3.base, canvas, -28, 14);
    egui_image_draw_image(&qoi_scene_cloud4.base, canvas, 78, 4);
    egui_image_draw_image(&qoi_scene_cloud5.base, canvas, 194, 18);
    egui_image_draw_image(&qoi_scene_cloud1.base, canvas, 292, 8);
    egui_image_draw_image(&qoi_scene_cloud2.base, canvas, 10, 0);

    egui_image_draw_image(&qoi_scene_grass0.base, canvas, 0, 117);
    egui_image_draw_image(&qoi_scene_grass0.base, canvas, 196, 117);
    egui_image_draw_image(&qoi_scene_grass.base, canvas, 30, 103);
    egui_image_draw_image(&qoi_scene_grassF0.base, canvas, 82, 105);
    egui_image_draw_image(&qoi_scene_grassF1.base, canvas, 126, 103);
    egui_image_draw_image(&qoi_scene_grassF2.base, canvas, 246, 98);
    egui_image_draw_image(&qoi_scene_grassF3.base, canvas, 304, 105);
    egui_image_draw_image(&qoi_scene_grassF4.base, canvas, 354, 101);
    egui_image_draw_image(&qoi_scene_grassF5.base, canvas, 390, 102);

    egui_image_draw_image(&bikes[bike_index]->base, canvas, 178, 70);
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
