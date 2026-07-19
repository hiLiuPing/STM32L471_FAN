#ifndef __UI_POETRYPAGE_H__
#define __UI_POETRYPAGE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#include "widget/egui_view.h"

extern egui_view_t *ui_PoetryPage;

void ui_PoetryPage_screen_init(void);
void ui_PoetryPage_screen_destroy(void);
void ui_PoetryPage_on_enter(void);
bool ui_PoetryPage_key_handler(void *key_event);

#ifdef __cplusplus
}
#endif

#endif /* __UI_POETRYPAGE_H__ */
