#ifndef __UI_HEITI_FONT_H__
#define __UI_HEITI_FONT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#include "font/egui_font.h"

#define UI_HEITI_FONT_16_PATH "heiti_4_16.bin"

const egui_font_t *ui_heiti_font_get_16(void);
bool ui_heiti_font_16_is_ready(void);

#ifdef __cplusplus
}
#endif

#endif /* __UI_HEITI_FONT_H__ */
