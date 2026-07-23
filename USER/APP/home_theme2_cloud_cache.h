#ifndef __HOME_THEME2_CLOUD_CACHE_H__
#define __HOME_THEME2_CLOUD_CACHE_H__

#include <stdbool.h>
#include <stdint.h>

#include "core/egui_typedef.h"

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

typedef struct
{
    uint32_t draw_call_count;
    uint32_t draw_row_count;
    uint32_t draw_read_bytes;
    uint32_t blend_request_count;
    uint32_t blend_complete_count;
    uint32_t blend_row_count;
    uint32_t blend_read_bytes;
    uint32_t blend_write_bytes;
    uint32_t cache_failure_count;
    uint32_t psram_read_call_count;
    uint32_t psram_read_transaction_count;
    uint32_t psram_read_byte_count;
    uint32_t psram_read_max_time_ms;
    uint32_t psram_read_failure_count;
} HomeTheme2CloudCacheStats_t;

bool HomeTheme2CloudCache_Init(void);
bool HomeTheme2CloudCache_IsReady(void);
bool HomeTheme2CloudCache_Recover(void);
void HomeTheme2CloudCache_MarkReadFault(void);

void HomeTheme2CloudCache_RequestBlend(uint8_t from_state,
                                       uint8_t to_state,
                                       uint8_t blend);
bool HomeTheme2CloudCache_Service(void);
bool HomeTheme2CloudCache_Draw(egui_canvas_t *canvas,
                               uint8_t shape,
                               int16_t x,
                               int16_t y,
                               uint8_t from_state,
                               uint8_t to_state,
                               uint8_t blend);
void HomeTheme2CloudCache_GetStats(HomeTheme2CloudCacheStats_t *stats);
void HomeTheme2CloudCache_ResetStats(void);

#ifdef __cplusplus
}
#endif

#endif /* __HOME_THEME2_CLOUD_CACHE_H__ */
