#ifndef __UI_WEATHERPAGE_H__
#define __UI_WEATHERPAGE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "widget/egui_view.h"

extern egui_view_t *ui_WeatherPage;

void ui_WeatherPage_screen_init(void);
void ui_WeatherPage_screen_destroy(void);
void ui_WeatherPage_key_handler(void *key_event);

#ifdef __cplusplus
}
#endif

#endif /* __UI_WEATHERPAGE_H__ */
