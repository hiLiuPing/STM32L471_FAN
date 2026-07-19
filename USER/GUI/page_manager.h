#ifndef __GUI_PAGE_MANAGER_H__
#define __GUI_PAGE_MANAGER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "core/egui_core.h"

#define UI_PAGE_MANAGER_MAX_PAGES     6U
#define UI_PAGE_SWITCH_ANIM_TIME_MS 200U

typedef struct
{
    void (*init)(void);
    void (*deinit)(void);
    void (*enter)(void);
    egui_view_t **page_view;
    bool (*key_consume)(void *key_event);
    void (*service)(void);
    bool (*nav_available)(void);
    const char *name;
    bool nav_enabled;
    uint8_t initialized;
} ui_page_t;

void ui_page_manager_init(egui_core_t *core);
void ui_page_manager_register(ui_page_t *page);
void ui_page_manager_load_init(void);
void ui_page_manager_next(void);
void ui_page_manager_prev(void);
void ui_page_manager_goto(const char *page_name, uint8_t index);
void ui_page_manager_wake_to_home(void);
ui_page_t *ui_page_manager_get_current(void);
bool ui_page_manager_is_startup_active(void);
void ui_page_manager_handle_key_event(void *key_event);
void ui_page_manager_service(void);
bool ui_page_consume_nav_key_event(void *key_event);

#ifdef __cplusplus
}
#endif

#endif /* __GUI_PAGE_MANAGER_H__ */
