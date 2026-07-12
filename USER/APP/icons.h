#ifndef __WEATHER_ICONS_H__
#define __WEATHER_ICONS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "image/egui_image_std.h"

#define WEATHER_ICON_SIZE        32U
#define WEATHER_ICON_RGB565_LEN 1024U
#define WEATHER_ICON_ALPHA8_LEN 1024U

#if EGUI_CONFIG_FUNCTION_IMAGE_FORMAT_RGB565

extern const uint16_t U16_IMG_100_RGB565[WEATHER_ICON_RGB565_LEN];
extern const uint8_t U8_IMG_100_ALPHA[WEATHER_ICON_ALPHA8_LEN];
extern const egui_image_std_t weather_icon_100;

extern const uint16_t U16_IMG_102_RGB565[WEATHER_ICON_RGB565_LEN];
extern const uint8_t U8_IMG_102_ALPHA[WEATHER_ICON_ALPHA8_LEN];
extern const egui_image_std_t weather_icon_102;

extern const uint16_t U16_IMG_104_RGB565[WEATHER_ICON_RGB565_LEN];
extern const uint8_t U8_IMG_104_ALPHA[WEATHER_ICON_ALPHA8_LEN];
extern const egui_image_std_t weather_icon_104;

extern const uint16_t U16_IMG_150_RGB565[WEATHER_ICON_RGB565_LEN];
extern const uint8_t U8_IMG_150_ALPHA[WEATHER_ICON_ALPHA8_LEN];
extern const egui_image_std_t weather_icon_150;

extern const uint16_t U16_IMG_302_RGB565[WEATHER_ICON_RGB565_LEN];
extern const uint8_t U8_IMG_302_ALPHA[WEATHER_ICON_ALPHA8_LEN];
extern const egui_image_std_t weather_icon_302;

extern const uint16_t U16_IMG_305_RGB565[WEATHER_ICON_RGB565_LEN];
extern const uint8_t U8_IMG_305_ALPHA[WEATHER_ICON_ALPHA8_LEN];
extern const egui_image_std_t weather_icon_305;

extern const uint16_t U16_IMG_306_RGB565[WEATHER_ICON_RGB565_LEN];
extern const uint8_t U8_IMG_306_ALPHA[WEATHER_ICON_ALPHA8_LEN];
extern const egui_image_std_t weather_icon_306;

extern const uint16_t U16_IMG_307_RGB565[WEATHER_ICON_RGB565_LEN];
extern const uint8_t U8_IMG_307_ALPHA[WEATHER_ICON_ALPHA8_LEN];
extern const egui_image_std_t weather_icon_307;

extern const uint16_t U16_IMG_499_RGB565[WEATHER_ICON_RGB565_LEN];
extern const uint8_t U8_IMG_499_ALPHA[WEATHER_ICON_ALPHA8_LEN];
extern const egui_image_std_t weather_icon_499;

extern const uint16_t U16_IMG_509_RGB565[WEATHER_ICON_RGB565_LEN];
extern const uint8_t U8_IMG_509_ALPHA[WEATHER_ICON_ALPHA8_LEN];
extern const egui_image_std_t weather_icon_509;

const egui_image_std_t *ui_weather_icon_get(uint16_t icon_id);

#endif /* EGUI_CONFIG_FUNCTION_IMAGE_FORMAT_RGB565 */

#ifdef __cplusplus
}
#endif

#endif /* __WEATHER_ICONS_H__ */
