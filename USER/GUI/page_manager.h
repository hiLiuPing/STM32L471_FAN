#ifndef __GUI_PAGE_MANAGER_H__
#define __GUI_PAGE_MANAGER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "core/egui_core.h"

#define UI_PAGE_MANAGER_MAX_PAGES     5U
#define UI_PAGE_SWITCH_ANIM_TIME_MS 200U

typedef struct
{
    void (*init)(void);
    void (*deinit)(void);
    egui_view_t **page_view;
    void (*key_event_handler)(void *key_event);
    void (*service)(void);
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
ui_page_t *ui_page_manager_get_current(void);
void ui_page_manager_handle_key_event(void *key_event);
void ui_page_manager_service(void);
void ui_page_handle_default_key_event(void *key_event);

#ifdef __cplusplus
}
#endif

#endif /* __GUI_PAGE_MANAGER_H__ */
