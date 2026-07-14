#ifndef __UI_POETRY_POPUP_H__
#define __UI_POETRY_POPUP_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#ifndef UI_POETRY_POPUP_DEFAULT_INTERVAL_S
#define UI_POETRY_POPUP_DEFAULT_INTERVAL_S 50U
#endif

#ifndef UI_POETRY_POPUP_DEFAULT_DURATION_S
#define UI_POETRY_POPUP_DEFAULT_DURATION_S 15U
#endif

void ui_poetry_popup_init(void);
void ui_poetry_popup_set_timing(uint16_t interval_s, uint16_t duration_s);
void ui_poetry_popup_set_enabled(bool enabled);

#ifdef __cplusplus
}
#endif

#endif /* __UI_POETRY_POPUP_H__ */
