#ifndef __UI_HOMEPAGE_H__
#define __UI_HOMEPAGE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#include "widget/egui_view.h"

extern egui_view_t *ui_HomePage;

void ui_HomePage_screen_init(void);
void ui_HomePage_screen_enter(void);
void ui_HomePage_screen_destroy(void);
bool ui_HomePage_key_handler(void *key_event);
void ui_HomePage_set_animation_enabled(bool enable);
bool ui_HomePage_get_animation_enabled(void);

#ifdef __cplusplus
}
#endif

#endif /* __UI_HOMEPAGE_H__ */
