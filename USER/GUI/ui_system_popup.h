#ifndef __UI_SYSTEM_POPUP_H__
#define __UI_SYSTEM_POPUP_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#include "system_notify.h"

void ui_system_popup_init(void);
void ui_system_popup_show(const SystemNotifyMessage_t *message);
bool ui_system_popup_show_or_update(const SystemNotifyMessage_t *message);
bool ui_system_popup_is_visible(void);
bool ui_system_popup_is_fan_control(void);
void ui_system_popup_dismiss(void);
void ui_system_popup_dismiss_immediate(void);

#ifdef __cplusplus
}
#endif

#endif /* __UI_SYSTEM_POPUP_H__ */
