#ifndef __HOME_SCENE_CACHE_H__
#define __HOME_SCENE_CACHE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "canvas/egui_canvas.h"
#include "image/egui_image_qoi.h"

int HomeSceneCache_Init(void);
bool HomeSceneCache_IsReady(void);
bool HomeSceneCache_DrawQoi(const egui_image_qoi_t *image, egui_canvas_t *canvas, int16_t x, int16_t y);

#ifdef __cplusplus
}
#endif

#endif /* __HOME_SCENE_CACHE_H__ */
