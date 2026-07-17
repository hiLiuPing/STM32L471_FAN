#ifndef __UI_SHUTDOWN_POPUP_H__
#define __UI_SHUTDOWN_POPUP_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#define UI_SHUTDOWN_POPUP_CONFIRM_MS 2300U

void ui_shutdown_popup_init(void);
void ui_shutdown_popup_start(void);
bool ui_shutdown_popup_is_active(void);

#ifdef __cplusplus
}
#endif

#endif /* __UI_SHUTDOWN_POPUP_H__ */
