#ifndef __FAN_ANIM_RES_H__
#define __FAN_ANIM_RES_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "fan_app.h"
#include "image/egui_image.h"

#define FAN_ANIM_ICON_SIZE 40U

const egui_image_t **FanAnimRes_GetFrames(fan_mode_t mode, uint8_t *count, uint16_t *interval_ms);

#ifdef __cplusplus
}
#endif

#endif /* __FAN_ANIM_RES_H__ */
