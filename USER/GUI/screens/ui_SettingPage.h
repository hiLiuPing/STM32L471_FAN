#ifndef __UI_SETTINGPAGE_H__
#define __UI_SETTINGPAGE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "widget/egui_view.h"

extern egui_view_t *ui_SettingPage;

void ui_SettingPage_screen_init(void);
void ui_SettingPage_screen_destroy(void);
void ui_SettingPage_key_handler(void *key_event);

#ifdef __cplusplus
}
#endif

#endif /* __UI_SETTINGPAGE_H__ */
