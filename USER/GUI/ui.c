#include "ui.h"

#include "screens/ui_FanPage.h"
#include "screens/ui_HomePage.h"
#include "screens/ui_SettingPage.h"
#include "screens/ui_StartPage.h"
#include "screens/ui_WeatherPage.h"
#include <stdlib.h>
#include "ui_poetry_popup.h"

static ui_page_t s_pages[] = {
    {
        .init = ui_StartPage_screen_init,
        .deinit = ui_StartPage_screen_destroy,
        .page_obj = &ui_StartPage,
        .key_event_handler = ui_StartPage_key_handler,
        .name = "START",
        .nav_enabled = false,
    },
    {
        .init = ui_HomePage_screen_init,
        .deinit = ui_HomePage_screen_destroy,
        .page_obj = &ui_HomePage,
        .key_event_handler = ui_HomePage_key_handler,
        .name = "HOME",
        .nav_enabled = true,
    },
    {
        .init = ui_FanPage_screen_init,
        .deinit = ui_FanPage_screen_destroy,
        .page_obj = &ui_FanPage,
        .key_event_handler = ui_FanPage_key_handler,
        .name = "FAN",
        .nav_enabled = true,
    },
    {
        .init = ui_WeatherPage_screen_init,
        .deinit = ui_WeatherPage_screen_destroy,
        .page_obj = &ui_WeatherPage,
        .key_event_handler = ui_WeatherPage_key_handler,
        .name = "WEATHER",
        .nav_enabled = true,
    },
    {
        .init = ui_SettingPage_screen_init,
        .deinit = ui_SettingPage_screen_destroy,
        .page_obj = &ui_SettingPage,
        .key_event_handler = ui_SettingPage_key_handler,
        .name = "SETTING",
        .nav_enabled = true,
    },
};

void ui_init(void)
{
    lv_disp_t *disp = lv_disp_get_default();
    lv_theme_t *theme = lv_theme_default_init(disp,
                                              lv_palette_main(LV_PALETTE_BLUE),
                                              lv_palette_main(LV_PALETTE_CYAN),
                                              false,
                                              LV_FONT_DEFAULT);

    if (theme != NULL)
    {
        lv_disp_set_theme(disp, theme);
    }

    srand(lv_tick_get());
    ui_page_manager_init();

    for (uint8_t i = 0U; i < (uint8_t)(sizeof(s_pages) / sizeof(s_pages[0])); i++)
    {
        ui_page_manager_register(&s_pages[i]);
    }

    ui_page_manager_load_init();
    ui_poetry_popup_init();
}
