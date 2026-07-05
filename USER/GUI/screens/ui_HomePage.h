#ifndef __UI_HOMEPAGE_H__
#define __UI_HOMEPAGE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

extern lv_obj_t *ui_HomePage;

void ui_HomePage_screen_init(void);
void ui_HomePage_screen_destroy(void);
void ui_HomePage_key_handler(void *key_event);

#ifdef __cplusplus
}
#endif

#endif /* __UI_HOMEPAGE_H__ */
