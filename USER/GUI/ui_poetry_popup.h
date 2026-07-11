#ifndef __UI_POETRY_POPUP_H__
#define __UI_POETRY_POPUP_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#ifndef UI_POETRY_POPUP_DEFAULT_INTERVAL_S
#define UI_POETRY_POPUP_DEFAULT_INTERVAL_S 5U
#endif

#ifndef UI_POETRY_POPUP_DEFAULT_DURATION_S
#define UI_POETRY_POPUP_DEFAULT_DURATION_S 14U
#endif

void ui_poetry_popup_init(void);
void ui_poetry_popup_set_timing(uint16_t interval_s, uint16_t duration_s);

#ifdef __cplusplus
}
#endif

#endif /* __UI_POETRY_POPUP_H__ */
