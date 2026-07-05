#include "screens/ui_FanPage.h"

#include "ui.h"

lv_obj_t *ui_FanPage = NULL;

static lv_obj_t *s_title_label = NULL;
static lv_obj_t *s_status_label = NULL;

void ui_FanPage_screen_init(void)
{
    if (ui_FanPage != NULL)
    {
        return;
    }

    ui_FanPage = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_FanPage, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_FanPage, lv_color_hex(0x0F766E), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_FanPage, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

    s_title_label = lv_label_create(ui_FanPage);
    lv_label_set_text(s_title_label, "FAN");
    lv_obj_set_style_text_color(s_title_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(s_title_label, &lv_font_montserrat_28, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(s_title_label, LV_ALIGN_CENTER, 0, -24);

    s_status_label = lv_label_create(ui_FanPage);
    lv_label_set_text(s_status_label, "Fan control");
    lv_obj_set_style_text_color(s_status_label, lv_color_hex(0xCCFBF1), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(s_status_label, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(s_status_label, LV_ALIGN_CENTER, 0, 18);
}

void ui_FanPage_screen_destroy(void)
{
    s_title_label = NULL;
    s_status_label = NULL;
    ui_FanPage = NULL;
}

void ui_FanPage_key_handler(void *key_event)
{
    ui_page_handle_default_key_event(key_event);
}
