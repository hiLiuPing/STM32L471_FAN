#include "ui.h"

#include <stdlib.h>

#include "core/egui_timer.h"
#include "egui_port_stm32l471_fan.h"
#include "settings_app.h"
#include "screens/ui_FanPage.h"
#include "screens/ui_HomePage.h"
#include "screens/ui_SettingPage.h"
#include "screens/ui_StartPage.h"
#include "ui_poetry_popup.h"

static ui_page_t s_pages[] = {
    {
        .init = ui_StartPage_screen_init,
        .deinit = ui_StartPage_screen_destroy,
        .page_view = &ui_StartPage,
        .key_consume = ui_StartPage_key_handler,
        .service = NULL,
        .name = "START",
        .nav_enabled = false,
        .initialized = 0U,
    },
    {
        .init = ui_HomePage_screen_init,
        .deinit = ui_HomePage_screen_destroy,
        .page_view = &ui_HomePage,
        .key_consume = ui_HomePage_key_handler,
        .service = NULL,
        .name = "HOME",
        .nav_enabled = true,
        .initialized = 0U,
    },
    {
        .init = ui_FanPage_screen_init,
        .deinit = ui_FanPage_screen_destroy,
        .page_view = &ui_FanPage,
        .key_consume = ui_FanPage_key_handler,
        .service = NULL,
        .name = "FAN",
        .nav_enabled = true,
        .initialized = 0U,
    },
    {
        .init = ui_SettingPage_screen_init,
        .deinit = ui_SettingPage_screen_destroy,
        .page_view = &ui_SettingPage,
        .key_consume = ui_SettingPage_key_handler,
        .service = NULL,
        .name = "SETTING",
        .nav_enabled = true,
        .initialized = 0U,
    },
};

void ui_init(void)
{
    egui_core_t *core = egui_port_get_core();

    srand(egui_timer_get_current_time());
    ui_page_manager_init(core);

    for (uint8_t i = 0U; i < (uint8_t)(sizeof(s_pages) / sizeof(s_pages[0])); i++)
    {
        ui_page_manager_register(&s_pages[i]);
    }

    ui_page_manager_load_init();
    ui_poetry_popup_init();
    SettingsApp_Apply();
}
