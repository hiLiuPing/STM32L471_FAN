#ifndef __GUI_UI_H__
#define __GUI_UI_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "page_manager.h"

void ui_init(void);
void ui_page_handle_default_key_event(void *key_event);

#ifdef __cplusplus
}
#endif

#endif /* __GUI_UI_H__ */
