#ifndef __UI_FANPAGE_H__
#define __UI_FANPAGE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

extern lv_obj_t *ui_FanPage;

void ui_FanPage_screen_init(void);
void ui_FanPage_screen_destroy(void);
void ui_FanPage_key_handler(void *key_event);

#ifdef __cplusplus
}
#endif

#endif /* __UI_FANPAGE_H__ */
