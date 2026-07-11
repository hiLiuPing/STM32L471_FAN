#ifndef __UI_STARTPAGE_H__
#define __UI_STARTPAGE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "widget/egui_view.h"

extern egui_view_t *ui_StartPage;

void ui_StartPage_screen_init(void);
void ui_StartPage_screen_destroy(void);
void ui_StartPage_key_handler(void *key_event);

#ifdef __cplusplus
}
#endif

#endif /* __UI_STARTPAGE_H__ */
