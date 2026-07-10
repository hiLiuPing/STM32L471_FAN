#ifndef __HEITI_LVGL_FONT_H__
#define __HEITI_LVGL_FONT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include "heiti_font_drv.h"

/**
 * Register a heiti bitmap font as an LVGL custom font.
 *
 * The returned lv_font_t pointer remains valid as long as the
 * underlying HeitiFont_Context_t is open.
 *
 * @param ctx  an already-opened heiti font context
 * @return     pointer to a static lv_font_t, or NULL on error
 */
const lv_font_t *HeitiLvgl_Register(HeitiFont_Context_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* __HEITI_LVGL_FONT_H__ */
