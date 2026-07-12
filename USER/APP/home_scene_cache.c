#include "home_scene_cache.h"

#include <stdint.h>
#include <string.h>

#include "core/egui_api.h"
#include "log.h"
#include "psram_app.h"
// #include "qoi_scene_res.h"
#include "home_scene_res.h"


#if EGUI_CONFIG_FUNCTION_IMAGE_CODEC_QOI

#define HOME_SCENE_CACHE_ALIGN_BYTES 4U
#define HOME_SCENE_QOI_OP_INDEX      0x00U
#define HOME_SCENE_QOI_OP_DIFF       0x40U
#define HOME_SCENE_QOI_OP_LUMA       0x80U
#define HOME_SCENE_QOI_OP_RUN        0xC0U
#define HOME_SCENE_QOI_OP_RGB        0xFEU
#define HOME_SCENE_QOI_OP_RGBA       0xFFU

#define HOME_SCENE_QOI_COLOR_HASH(r, g, b, a) (((r) * 3U + (g) * 5U + (b) * 7U + (a) * 11U) & 63U)
#define HOME_SCENE_QOI_RGBA_PACK(r, g, b, a)  ((uint32_t)(r) | ((uint32_t)(g) << 8) | ((uint32_t)(b) << 16) | ((uint32_t)(a) << 24))

typedef struct
{
    const egui_image_qoi_t *image;
    uint32_t pixel_offset;
    uint32_t alpha_offset;
    uint32_t total_bytes;
    uint16_t width;
    uint16_t height;
    uint8_t ready;
} HomeSceneCache_Entry_t;

typedef struct
{
    HomeSceneCache_Entry_t *entry;
} HomeSceneCache_WriteCtx_t;

typedef struct
{
    const uint8_t *data;
    const uint8_t *end;
    uint32_t index_rgba[64];
    uint16_t index_rgb565[64];
    uint8_t prev_r;
    uint8_t prev_g;
    uint8_t prev_b;
    uint8_t prev_a;
    uint16_t prev_rgb565;
    uint8_t run;
} HomeSceneCache_QoiState_t;

static HomeSceneCache_Entry_t s_entries[] = {
    {&qoi_scene_bike1, 0U, 0U, 0U, 0U, 0U, 0U},
    {&qoi_scene_bike2, 0U, 0U, 0U, 0U, 0U, 0U},
    {&qoi_scene_bike3, 0U, 0U, 0U, 0U, 0U, 0U},
    {&qoi_scene_bike4, 0U, 0U, 0U, 0U, 0U, 0U},
    {&qoi_scene_cloud1, 0U, 0U, 0U, 0U, 0U, 0U},
    {&qoi_scene_cloud2, 0U, 0U, 0U, 0U, 0U, 0U},
    {&qoi_scene_grass, 0U, 0U, 0U, 0U, 0U, 0U},
    {&qoi_scene_grass0, 0U, 0U, 0U, 0U, 0U, 0U},
    {&qoi_scene_grassF0, 0U, 0U, 0U, 0U, 0U, 0U},
    {&qoi_scene_grassF1, 0U, 0U, 0U, 0U, 0U, 0U},
    {&qoi_scene_grassF2, 0U, 0U, 0U, 0U, 0U, 0U},
    {&qoi_scene_grassF3, 0U, 0U, 0U, 0U, 0U, 0U},
    {&qoi_scene_grassF4, 0U, 0U, 0U, 0U, 0U, 0U},
    {&qoi_scene_grassF5, 0U, 0U, 0U, 0U, 0U, 0U},
};

static uint16_t s_pixel_row[EGUI_CONFIG_IMAGE_DECODE_ROW_BUF_WIDTH];
static uint8_t s_alpha_row[EGUI_CONFIG_IMAGE_DECODE_ROW_BUF_WIDTH];
static uint8_t s_ready = 0U;

static int HomeSceneCache_WriteRow(void *user, uint16_t row, const uint8_t *pixel_row, const uint8_t *alpha_row, uint16_t width);
static void HomeSceneCache_ClearReady(void);

static uint16_t HomeSceneCache_Rgb888ToRgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return ((uint16_t)(r >> 3) << 11) | ((uint16_t)(g >> 2) << 5) | (uint16_t)(b >> 3);
}

static void HomeSceneCache_QoiStoreIndex(HomeSceneCache_QoiState_t *state)
{
    uint8_t hash = (uint8_t)HOME_SCENE_QOI_COLOR_HASH(state->prev_r, state->prev_g, state->prev_b, state->prev_a);

    state->index_rgb565[hash] = state->prev_rgb565;
    state->index_rgba[hash] = HOME_SCENE_QOI_RGBA_PACK(state->prev_r, state->prev_g, state->prev_b, state->prev_a);
}

static int HomeSceneCache_QoiRead(HomeSceneCache_QoiState_t *state, uint8_t *value)
{
    if ((state == NULL) || (value == NULL) || (state->data >= state->end))
    {
        return 0;
    }

    *value = *state->data++;
    return 1;
}

static int HomeSceneCache_QoiDecodeNext(HomeSceneCache_QoiState_t *state, uint8_t channels, uint16_t *pixel, uint8_t *alpha)
{
    uint8_t b1;

    if ((state == NULL) || (pixel == NULL) || (alpha == NULL))
    {
        return 0;
    }

    if (state->run > 0U)
    {
        state->run--;
        *pixel = state->prev_rgb565;
        *alpha = state->prev_a;
        return 1;
    }

    if (!HomeSceneCache_QoiRead(state, &b1))
    {
        return 0;
    }

    if (b1 == HOME_SCENE_QOI_OP_RGB)
    {
        if (!HomeSceneCache_QoiRead(state, &state->prev_r) ||
            !HomeSceneCache_QoiRead(state, &state->prev_g) ||
            !HomeSceneCache_QoiRead(state, &state->prev_b))
        {
            return 0;
        }
    }
    else if (b1 == HOME_SCENE_QOI_OP_RGBA)
    {
        if (!HomeSceneCache_QoiRead(state, &state->prev_r) ||
            !HomeSceneCache_QoiRead(state, &state->prev_g) ||
            !HomeSceneCache_QoiRead(state, &state->prev_b) ||
            !HomeSceneCache_QoiRead(state, &state->prev_a))
        {
            return 0;
        }
    }
    else if (b1 < HOME_SCENE_QOI_OP_DIFF)
    {
        uint8_t index = b1 & 0x3FU;
        uint32_t rgba = state->index_rgba[index];

        state->prev_r = (uint8_t)rgba;
        state->prev_g = (uint8_t)(rgba >> 8);
        state->prev_b = (uint8_t)(rgba >> 16);
        state->prev_a = (uint8_t)(rgba >> 24);
        state->prev_rgb565 = state->index_rgb565[index];
        *pixel = state->prev_rgb565;
        *alpha = state->prev_a;
        return 1;
    }
    else if (b1 < HOME_SCENE_QOI_OP_LUMA)
    {
        state->prev_r = (uint8_t)((int16_t)state->prev_r + (int16_t)((b1 >> 4) & 0x03U) - 2);
        state->prev_g = (uint8_t)((int16_t)state->prev_g + (int16_t)((b1 >> 2) & 0x03U) - 2);
        state->prev_b = (uint8_t)((int16_t)state->prev_b + (int16_t)(b1 & 0x03U) - 2);
    }
    else if (b1 < HOME_SCENE_QOI_OP_RUN)
    {
        uint8_t b2;
        int8_t vg;

        if (!HomeSceneCache_QoiRead(state, &b2))
        {
            return 0;
        }

        vg = (int8_t)((b1 & 0x3FU) - 32);
        state->prev_r = (uint8_t)((int16_t)state->prev_r + vg - 8 + (int16_t)((b2 >> 4) & 0x0FU));
        state->prev_g = (uint8_t)((int16_t)state->prev_g + vg);
        state->prev_b = (uint8_t)((int16_t)state->prev_b + vg - 8 + (int16_t)(b2 & 0x0FU));
    }
    else
    {
        state->run = (uint8_t)(b1 & 0x3FU);
        *pixel = state->prev_rgb565;
        *alpha = state->prev_a;
        return 1;
    }

    if (channels == 3U)
    {
        state->prev_a = EGUI_ALPHA_100;
    }
    state->prev_rgb565 = HomeSceneCache_Rgb888ToRgb565(state->prev_r, state->prev_g, state->prev_b);
    HomeSceneCache_QoiStoreIndex(state);
    *pixel = state->prev_rgb565;
    *alpha = state->prev_a;
    return 1;
}

static int HomeSceneCache_DecodeQoiToPsram(HomeSceneCache_Entry_t *entry)
{
    const egui_image_qoi_info_t *info;
    HomeSceneCache_QoiState_t state;
    HomeSceneCache_WriteCtx_t ctx;
    uint16_t row;
    uint16_t col;

    if ((entry == NULL) || (entry->image == NULL) || (entry->image->base.res == NULL))
    {
        return 0;
    }

    info = (const egui_image_qoi_info_t *)entry->image->base.res;
    if ((info->data_buf == NULL) || (info->data_type != EGUI_IMAGE_DATA_TYPE_RGB565) ||
        ((info->channels != 3U) && (info->channels != 4U)) ||
        (info->width == 0U) || (info->height == 0U) ||
        (info->width > EGUI_CONFIG_IMAGE_DECODE_ROW_BUF_WIDTH))
    {
        return 0;
    }

    memset(&state, 0, sizeof(state));
    state.data = info->data_buf;
    state.end = info->data_buf + info->data_size;
    state.prev_a = EGUI_ALPHA_100;
    ctx.entry = entry;

    for (row = 0U; row < info->height; row++)
    {
        for (col = 0U; col < info->width; col++)
        {
            if (!HomeSceneCache_QoiDecodeNext(&state, info->channels, &s_pixel_row[col], &s_alpha_row[col]))
            {
                return 0;
            }
        }

        if (!HomeSceneCache_WriteRow(&ctx, row, (const uint8_t *)s_pixel_row, s_alpha_row, info->width))
        {
            return 0;
        }
    }

    return 1;
}

static uint32_t HomeSceneCache_Align(uint32_t value)
{
    return (value + (HOME_SCENE_CACHE_ALIGN_BYTES - 1U)) & ~(HOME_SCENE_CACHE_ALIGN_BYTES - 1U);
}

static HomeSceneCache_Entry_t *HomeSceneCache_Find(const egui_image_qoi_t *image)
{
    uint32_t i;

    for (i = 0U; i < (uint32_t)EGUI_ARRAY_SIZE(s_entries); i++)
    {
        if (s_entries[i].image == image)
        {
            return &s_entries[i];
        }
    }

    return NULL;
}

static void HomeSceneCache_ClearReady(void)
{
    uint32_t i;

    s_ready = 0U;
    for (i = 0U; i < (uint32_t)EGUI_ARRAY_SIZE(s_entries); i++)
    {
        s_entries[i].ready = 0U;
    }
}

static int HomeSceneCache_WriteRow(void *user, uint16_t row, const uint8_t *pixel_row, const uint8_t *alpha_row, uint16_t width)
{
    HomeSceneCache_WriteCtx_t *ctx = (HomeSceneCache_WriteCtx_t *)user;
    HomeSceneCache_Entry_t *entry;
    uint32_t pixel_addr;
    uint32_t alpha_addr;

    if ((ctx == NULL) || (ctx->entry == NULL) || (pixel_row == NULL) || (alpha_row == NULL))
    {
        return 0;
    }

    entry = ctx->entry;
    if ((row >= entry->height) || (width != entry->width))
    {
        return 0;
    }

    pixel_addr = PSRAM_UI_CACHE_BASE + entry->pixel_offset + (uint32_t)row * (uint32_t)entry->width * sizeof(uint16_t);
    alpha_addr = PSRAM_UI_CACHE_BASE + entry->alpha_offset + (uint32_t)row * (uint32_t)entry->width;

    if (qspi_psram_write(&g_psram, pixel_addr, pixel_row, (uint32_t)entry->width * sizeof(uint16_t)) != 0)
    {
        return 0;
    }

    if (qspi_psram_write(&g_psram, alpha_addr, alpha_row, (uint32_t)entry->width) != 0)
    {
        return 0;
    }

    return 1;
}

static int HomeSceneCache_PrepareEntry(HomeSceneCache_Entry_t *entry, uint32_t *used_bytes)
{
    const egui_image_qoi_info_t *info;
    uint32_t pixel_bytes;
    uint32_t alpha_bytes;
    uint32_t entry_start;

    if ((entry == NULL) || (entry->image == NULL) || (entry->image->base.res == NULL) || (used_bytes == NULL))
    {
        return 0;
    }

    info = (const egui_image_qoi_info_t *)entry->image->base.res;
    if ((info->data_type != EGUI_IMAGE_DATA_TYPE_RGB565) || (info->width == 0U) || (info->height == 0U) ||
        (info->width > EGUI_CONFIG_IMAGE_DECODE_ROW_BUF_WIDTH))
    {
        return 0;
    }

    pixel_bytes = (uint32_t)info->width * (uint32_t)info->height * sizeof(uint16_t);
    alpha_bytes = (uint32_t)info->width * (uint32_t)info->height;
    entry_start = HomeSceneCache_Align(*used_bytes);

    if ((entry_start + pixel_bytes + alpha_bytes) > PSRAM_UI_CACHE_SIZE)
    {
        return 0;
    }

    entry->pixel_offset = entry_start;
    entry->alpha_offset = entry_start + pixel_bytes;
    entry->total_bytes = pixel_bytes + alpha_bytes;
    entry->width = info->width;
    entry->height = info->height;
    entry->ready = 0U;

    if (!HomeSceneCache_DecodeQoiToPsram(entry))
    {
        return 0;
    }

    entry->ready = 1U;
    *used_bytes = entry_start + entry->total_bytes;
    return 1;
}

int HomeSceneCache_Init(void)
{
    uint32_t used_bytes = 0U;
    uint32_t i;

    if (s_ready != 0U)
    {
        return 0;
    }

    if (!g_psram.initialized)
    {
        log_printf("[HOME_CACHE] PSRAM not initialized, use QOI fallback");
        return -1;
    }

    if (g_psram.mmap_active)
    {
        (void)qspi_psram_exit_memory_mapped(&g_psram);
    }

    for (i = 0U; i < (uint32_t)EGUI_ARRAY_SIZE(s_entries); i++)
    {
        if (!HomeSceneCache_PrepareEntry(&s_entries[i], &used_bytes))
        {
            log_printf("[HOME_CACHE] build failed at %lu, use QOI fallback", (unsigned long)i);
            HomeSceneCache_ClearReady();
            return -1;
        }
    }

    s_ready = 1U;
    log_printf("[HOME_CACHE] ready, %lu bytes, command-read mode", (unsigned long)used_bytes);
    return 0;
}

bool HomeSceneCache_IsReady(void)
{
    return (s_ready != 0U) && g_psram.initialized;
}

static void HomeSceneCache_BlendRun(egui_color_int_t *dst, const uint16_t *src_pixels, const uint8_t *src_alpha, egui_dim_t count)
{
    egui_dim_t i = 0;

    while (i < count)
    {
        while ((i < count) && (src_alpha[i] == 0U))
        {
            i++;
        }

        if (i >= count)
        {
            break;
        }

        if (src_alpha[i] == EGUI_ALPHA_100)
        {
            egui_dim_t start = i;

            while ((i < count) && (src_alpha[i] == EGUI_ALPHA_100))
            {
                i++;
            }
            egui_api_memcpy(&dst[start], &src_pixels[start], (int)((uint32_t)(i - start) * sizeof(uint16_t)));
        }
        else
        {
            egui_color_t fore;

            fore.full = src_pixels[i];
            egui_rgb_mix_ptr((egui_color_t *)&dst[i], &fore, (egui_color_t *)&dst[i], src_alpha[i]);
            i++;
        }
    }
}

bool HomeSceneCache_DrawQoi(const egui_image_qoi_t *image, egui_canvas_t *canvas, int16_t x, int16_t y)
{
    HomeSceneCache_Entry_t *entry;
    egui_region_t image_region;
    egui_region_t draw_region;
    egui_region_t *work_region;
    egui_dim_t src_x;
    egui_dim_t src_y;
    egui_dim_t row;
    uint8_t blended_rows = 0U;

    if ((image == NULL) || (canvas == NULL) || !HomeSceneCache_IsReady())
    {
        return false;
    }

    entry = HomeSceneCache_Find(image);
    if ((entry == NULL) || (entry->ready == 0U))
    {
        return false;
    }

    image_region.location.x = (egui_dim_t)x;
    image_region.location.y = (egui_dim_t)y;
    image_region.size.width = (egui_dim_t)entry->width;
    image_region.size.height = (egui_dim_t)entry->height;
    work_region = egui_canvas_get_base_view_work_region(canvas);
    egui_region_intersect(&image_region, work_region, &draw_region);
    if (egui_region_is_empty(&draw_region))
    {
        return true;
    }

    if (g_psram.mmap_active)
    {
        if (qspi_psram_exit_memory_mapped(&g_psram) != 0)
        {
            return false;
        }
    }

    src_x = (egui_dim_t)(draw_region.location.x - x);
    src_y = (egui_dim_t)(draw_region.location.y - y);

    for (row = 0; row < draw_region.size.height; row++)
    {
        egui_dim_t screen_y = (egui_dim_t)(draw_region.location.y + row);
        egui_dim_t pfb_y = (egui_dim_t)(screen_y - canvas->pfb_location_in_base_view.y);
        egui_dim_t pfb_x = (egui_dim_t)(draw_region.location.x - canvas->pfb_location_in_base_view.x);
        egui_color_int_t *dst = &canvas->pfb[(uint32_t)pfb_y * (uint32_t)canvas->pfb_region.size.width + (uint32_t)pfb_x];
        uint32_t row_index = (uint32_t)(src_y + row);
        uint32_t pixel_addr = PSRAM_UI_CACHE_BASE + entry->pixel_offset +
                              (row_index * (uint32_t)entry->width + (uint32_t)src_x) * sizeof(uint16_t);
        uint32_t alpha_addr = PSRAM_UI_CACHE_BASE + entry->alpha_offset + row_index * (uint32_t)entry->width + (uint32_t)src_x;
        const uint16_t *src_pixels = s_pixel_row;
        const uint8_t *src_alpha = s_alpha_row;

        if (qspi_psram_read(&g_psram, pixel_addr, (uint8_t *)s_pixel_row, (uint32_t)draw_region.size.width * sizeof(uint16_t)) != 0)
        {
            HomeSceneCache_ClearReady();
            return (blended_rows != 0U);
        }

        if (qspi_psram_read(&g_psram, alpha_addr, s_alpha_row, (uint32_t)draw_region.size.width) != 0)
        {
            HomeSceneCache_ClearReady();
            return (blended_rows != 0U);
        }

#if EGUI_CONFIG_FUNCTION_SUPPORT_MASK
        if (canvas->mask != NULL)
        {
            egui_dim_t col;

            for (col = 0; col < draw_region.size.width; col++)
            {
                egui_color_t fore;

                if (src_alpha[col] == 0U)
                {
                    continue;
                }
                fore.full = src_pixels[col];
                egui_canvas_draw_point(canvas, (egui_dim_t)(draw_region.location.x + col), screen_y, fore, src_alpha[col]);
            }
        }
        else
#endif
        if (canvas->alpha == EGUI_ALPHA_100)
        {
            HomeSceneCache_BlendRun(dst, src_pixels, src_alpha, draw_region.size.width);
        }
        else
        {
            egui_dim_t col;

            for (col = 0; col < draw_region.size.width; col++)
            {
                egui_color_t fore;
                egui_alpha_t alpha;

                if (src_alpha[col] == 0U)
                {
                    continue;
                }
                fore.full = src_pixels[col];
                alpha = egui_color_alpha_mix(canvas->alpha, src_alpha[col]);
                egui_rgb_mix_ptr((egui_color_t *)&dst[col], &fore, (egui_color_t *)&dst[col], alpha);
            }
        }

        blended_rows = 1U;
    }

    return true;
}

#else

int HomeSceneCache_Init(void)
{
    return -1;
}

bool HomeSceneCache_IsReady(void)
{
    return false;
}

bool HomeSceneCache_DrawQoi(const egui_image_qoi_t *image, egui_canvas_t *canvas, int16_t x, int16_t y)
{
    EGUI_UNUSED(image);
    EGUI_UNUSED(canvas);
    EGUI_UNUSED(x);
    EGUI_UNUSED(y);
    return false;
}

#endif
