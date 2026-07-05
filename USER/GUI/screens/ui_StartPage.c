#include "screens/ui_StartPage.h"

#include "ui.h"

#define START_PAGE_DURATION_MS 2000U
#define START_PAGE_TICK_MS     100U

lv_obj_t *ui_StartPage = NULL;

static lv_obj_t *s_title_label = NULL;
static lv_obj_t *s_status_label = NULL;
static lv_obj_t *s_progress_bar = NULL;
static lv_obj_t *s_progress_label = NULL;
static lv_timer_t *s_start_timer = NULL;
static uint32_t s_elapsed_ms = 0U;

static void ui_StartPage_timer_cb(lv_timer_t *timer)
{
    uint32_t progress;

    (void)timer;

    s_elapsed_ms += START_PAGE_TICK_MS;
    if (s_elapsed_ms > START_PAGE_DURATION_MS)
    {
        s_elapsed_ms = START_PAGE_DURATION_MS;
    }

    progress = (s_elapsed_ms * 100U) / START_PAGE_DURATION_MS;

    if (s_progress_bar != NULL)
    {
        lv_bar_set_value(s_progress_bar, (int32_t)progress, LV_ANIM_ON);
    }

    if (s_progress_label != NULL)
    {
        if (progress < 50U)
        {
            lv_label_set_text(s_progress_label, "Init sensors...");
        }
        else if (progress < 100U)
        {
            lv_label_set_text(s_progress_label, "Sync local data...");
        }
        else
        {
            lv_label_set_text(s_progress_label, "Ready");
        }
    }

    if (s_elapsed_ms >= START_PAGE_DURATION_MS)
    {
        ui_page_manager_goto("HOME", 0U);
    }
}

void ui_StartPage_screen_init(void)
{
    if (ui_StartPage != NULL)
    {
        return;
    }

    ui_StartPage = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_StartPage, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_StartPage, lv_color_hex(0x101820), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_StartPage, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui_StartPage, 16, LV_PART_MAIN | LV_STATE_DEFAULT);

    s_title_label = lv_label_create(ui_StartPage);
    lv_label_set_text(s_title_label, "STM32L471 FAN");
    lv_obj_set_style_text_color(s_title_label, lv_color_hex(0xFEE715), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(s_title_label, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(s_title_label, LV_ALIGN_CENTER, 0, -44);

    s_status_label = lv_label_create(ui_StartPage);
    lv_label_set_text(s_status_label, "Powering up");
    lv_obj_set_style_text_color(s_status_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(s_status_label, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(s_status_label, LV_ALIGN_CENTER, 0, -8);

    s_progress_bar = lv_bar_create(ui_StartPage);
    lv_obj_set_size(s_progress_bar, 180, 12);
    lv_obj_align(s_progress_bar, LV_ALIGN_CENTER, 0, 28);
    lv_bar_set_range(s_progress_bar, 0, 100);
    lv_bar_set_value(s_progress_bar, 0, LV_ANIM_OFF);

    s_progress_label = lv_label_create(ui_StartPage);
    lv_label_set_text(s_progress_label, "Init sensors...");
    lv_obj_set_style_text_color(s_progress_label, lv_color_hex(0xA7F3D0), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(s_progress_label, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(s_progress_label, LV_ALIGN_CENTER, 0, 58);

    s_elapsed_ms = 0U;
    s_start_timer = lv_timer_create(ui_StartPage_timer_cb, START_PAGE_TICK_MS, NULL);
}

void ui_StartPage_screen_destroy(void)
{
    if (s_start_timer != NULL)
    {
        lv_timer_del(s_start_timer);
        s_start_timer = NULL;
    }

    s_title_label = NULL;
    s_status_label = NULL;
    s_progress_bar = NULL;
    s_progress_label = NULL;
    s_elapsed_ms = 0U;
    ui_StartPage = NULL;
}

void ui_StartPage_key_handler(void *key_event)
{
    (void)key_event;
}
