#ifndef __HOME_THEME2_CLOUD_CACHE_H__
#define __HOME_THEME2_CLOUD_CACHE_H__

#include <stdbool.h>
#include <stdint.h>

#include "image/egui_image_std.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HOME_THEME2_CLOUD_SHAPE_COUNT 3U
#define HOME_THEME2_CLOUD_STATE_COUNT 3U

typedef enum
{
    HOME_THEME2_CLOUD_STATE_DAY = 0,
    HOME_THEME2_CLOUD_STATE_DAWN,
    HOME_THEME2_CLOUD_STATE_SUNSET,
} HomeTheme2CloudState_t;

bool HomeTheme2CloudCache_Init(void);
bool HomeTheme2CloudCache_IsReady(void);
const egui_image_std_t *HomeTheme2CloudCache_Get(uint8_t shape, uint8_t state);
bool HomeTheme2CloudCache_Recover(void);

#ifdef __cplusplus
}
#endif

#endif /* __HOME_THEME2_CLOUD_CACHE_H__ */
