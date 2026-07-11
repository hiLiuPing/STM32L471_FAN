#ifndef __UI_HEITI_FONT_H__
#define __UI_HEITI_FONT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "font/egui_font.h"

#define UI_HEITI_FONT_12_PATH "heiti_4_12.bin"
#define UI_HEITI_FONT_16_PATH "heiti_4_16.bin"
#define UI_HEITI_FONT_18_PATH "heiti_4_18.bin"
#define UI_HEITI_FONT_20_PATH "heiti_4_20.bin"

const egui_font_t *ui_heiti_font_get(uint8_t size);
bool ui_heiti_font_is_ready(uint8_t size);
const char *ui_heiti_font_get_path(uint8_t size);

const egui_font_t *ui_heiti_font_get_12(void);
const egui_font_t *ui_heiti_font_get_16(void);
const egui_font_t *ui_heiti_font_get_18(void);
const egui_font_t *ui_heiti_font_get_20(void);
bool ui_heiti_font_12_is_ready(void);
bool ui_heiti_font_16_is_ready(void);
bool ui_heiti_font_18_is_ready(void);
bool ui_heiti_font_20_is_ready(void);

#ifdef __cplusplus
}
#endif

#endif /* __UI_HEITI_FONT_H__ */
