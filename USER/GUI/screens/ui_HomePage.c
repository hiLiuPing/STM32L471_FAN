#include "screens/ui_HomePage.h"

#include "ui.h"

lv_obj_t *ui_HomePage = NULL;

void ui_HomePage_screen_init(void)
{
    if (ui_HomePage != NULL)
    {
        return;
    }

    ui_HomePage = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui_HomePage, lv_color_hex(0xF4F7FB), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_HomePage, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
}

void ui_HomePage_screen_destroy(void)
{
    ui_HomePage = NULL;
}

void ui_HomePage_key_handler(void *key_event)
{
    ui_page_handle_default_key_event(key_event);
}
