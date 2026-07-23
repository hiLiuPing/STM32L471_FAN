#include "home_theme2_cloud_cache.h"

#include <stddef.h>
#include <string.h>

#include "home_theme2_cloud_res.h"
#include "log.h"
#include "psram_app.h"

#define HOME_THEME2_CLOUD_CACHE_MAX_WIDTH 229U
#define HOME_THEME2_CLOUD_CACHE_ALIGN      4U
#define HOME_THEME2_CLOUD_VERIFY_CHUNK     128U
#define HOME_THEME2_CLOUD_CACHE_FNV_OFFSET 2166136261UL
#define HOME_THEME2_CLOUD_CACHE_FNV_PRIME  16777619UL

typedef struct
{
    const egui_image_qoi_info_t *info;
    uint32_t pos;
    uint32_t pixels_decoded;
    uint32_t index_rgba[64];
    uint8_t prev_r;
    uint8_t prev_g;
    uint8_t prev_b;
    uint8_t prev_a;
    uint8_t run;
} home_theme2_qoi_decoder_t;

typedef struct
{
    uint32_t pixel_offset;
    uint32_t alpha_offset;
    uint32_t pixel_size;
    uint32_t alpha_size;
    uint32_t pixel_hash;
    uint32_t alpha_hash;
} home_theme2_cloud_cache_slot_t;

static const egui_image_qoi_t *const s_cloud_sources[HOME_THEME2_CLOUD_SHAPE_COUNT][HOME_THEME2_CLOUD_STATE_COUNT] = {
    {&qoi_scene_cloud_a_1, &qoi_scene_cloud_a_2, &qoi_scene_cloud_a_3},
    {&qoi_scene_cloud_b_1, &qoi_scene_cloud_b_2, &qoi_scene_cloud_b_3},
    {&qoi_scene_cloud_c_1, &qoi_scene_cloud_c_2, &qoi_scene_cloud_c_3},
};

static egui_image_std_info_t s_cloud_infos[HOME_THEME2_CLOUD_SHAPE_COUNT][HOME_THEME2_CLOUD_STATE_COUNT];
static egui_image_std_t s_cloud_images[HOME_THEME2_CLOUD_SHAPE_COUNT][HOME_THEME2_CLOUD_STATE_COUNT];
static home_theme2_cloud_cache_slot_t s_cloud_slots[HOME_THEME2_CLOUD_SHAPE_COUNT][HOME_THEME2_CLOUD_STATE_COUNT];
static uint16_t s_pixel_row[HOME_THEME2_CLOUD_CACHE_MAX_WIDTH];
static uint8_t s_alpha_row[HOME_THEME2_CLOUD_CACHE_MAX_WIDTH];
static uint8_t s_verify_buf[HOME_THEME2_CLOUD_VERIFY_CHUNK];
static bool s_cache_ready = false;
static bool s_external_read_failed = false;

static uint32_t home_theme2_cloud_cache_align(uint32_t value)
{
    return (value + (HOME_THEME2_CLOUD_CACHE_ALIGN - 1U)) & ~(HOME_THEME2_CLOUD_CACHE_ALIGN - 1U);
}

static uint32_t home_theme2_cloud_cache_hash(const uint8_t *data, uint32_t size, uint32_t hash)
{
    uint32_t i;

    for (i = 0U; i < size; i++)
    {
        hash ^= data[i];
        hash *= HOME_THEME2_CLOUD_CACHE_FNV_PRIME;
    }
    return hash;
}

static bool home_theme2_cloud_cache_hash_psram(uint32_t addr,
                                               uint32_t size,
                                               uint32_t *result)
{
    uint32_t hash = HOME_THEME2_CLOUD_CACHE_FNV_OFFSET;
    uint32_t offset = 0U;

    if ((result == NULL) || (size == 0U))
    {
        return false;
    }

    while (offset < size)
    {
        uint32_t chunk = size - offset;

        if (chunk > sizeof(s_verify_buf))
        {
            chunk = sizeof(s_verify_buf);
        }
        if (qspi_psram_read(&g_psram, addr + offset, s_verify_buf, chunk) != 0)
        {
            return false;
        }
        hash = home_theme2_cloud_cache_hash(s_verify_buf, chunk, hash);
        offset += chunk;
    }

    *result = hash;
    return true;
}

static uint16_t home_theme2_cloud_cache_rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)((((uint16_t)r & 0xF8U) << 8) |
                      (((uint16_t)g & 0xFCU) << 3) |
                      ((uint16_t)b >> 3));
}

static bool home_theme2_qoi_read_u8(home_theme2_qoi_decoder_t *decoder, uint8_t *value)
{
    if ((decoder == NULL) || (decoder->info == NULL) || (value == NULL) ||
        (decoder->pos >= decoder->info->data_size))
    {
        return false;
    }

    *value = decoder->info->data_buf[decoder->pos++];
    return true;
}

static void home_theme2_qoi_decoder_init(home_theme2_qoi_decoder_t *decoder,
                                         const egui_image_qoi_info_t *info)
{
    memset(decoder, 0, sizeof(*decoder));
    decoder->info = info;
    decoder->prev_a = 255U;
}

static bool home_theme2_qoi_decode_pixel(home_theme2_qoi_decoder_t *decoder,
                                         uint16_t *pixel,
                                         uint8_t *alpha)
{
    uint8_t b1;
    uint8_t b2;
    uint8_t hash;
    uint32_t rgba;

    if ((decoder == NULL) || (decoder->info == NULL) || (pixel == NULL) || (alpha == NULL) ||
        (decoder->pixels_decoded >= ((uint32_t)decoder->info->width * decoder->info->height)))
    {
        return false;
    }

    if (decoder->run > 0U)
    {
        decoder->run--;
    }
    else
    {
        if (!home_theme2_qoi_read_u8(decoder, &b1))
        {
            return false;
        }

        if (b1 == 0xFEU)
        {
            if (!home_theme2_qoi_read_u8(decoder, &decoder->prev_r) ||
                !home_theme2_qoi_read_u8(decoder, &decoder->prev_g) ||
                !home_theme2_qoi_read_u8(decoder, &decoder->prev_b))
            {
                return false;
            }
        }
        else if (b1 == 0xFFU)
        {
            if (!home_theme2_qoi_read_u8(decoder, &decoder->prev_r) ||
                !home_theme2_qoi_read_u8(decoder, &decoder->prev_g) ||
                !home_theme2_qoi_read_u8(decoder, &decoder->prev_b) ||
                !home_theme2_qoi_read_u8(decoder, &decoder->prev_a))
            {
                return false;
            }
        }
        else
        {
            switch (b1 & 0xC0U)
            {
            case 0x00U:
                rgba = decoder->index_rgba[b1 & 0x3FU];
                decoder->prev_r = (uint8_t)rgba;
                decoder->prev_g = (uint8_t)(rgba >> 8);
                decoder->prev_b = (uint8_t)(rgba >> 16);
                decoder->prev_a = (uint8_t)(rgba >> 24);
                break;

            case 0x40U:
                decoder->prev_r = (uint8_t)(decoder->prev_r + ((b1 >> 4) & 0x03U) - 2U);
                decoder->prev_g = (uint8_t)(decoder->prev_g + ((b1 >> 2) & 0x03U) - 2U);
                decoder->prev_b = (uint8_t)(decoder->prev_b + (b1 & 0x03U) - 2U);
                break;

            case 0x80U:
                if (!home_theme2_qoi_read_u8(decoder, &b2))
                {
                    return false;
                }
                {
                    int16_t vg = (int16_t)(b1 & 0x3FU) - 32;
                    decoder->prev_r = (uint8_t)(decoder->prev_r + vg - 8 + ((b2 >> 4) & 0x0FU));
                    decoder->prev_g = (uint8_t)(decoder->prev_g + vg);
                    decoder->prev_b = (uint8_t)(decoder->prev_b + vg - 8 + (b2 & 0x0FU));
                }
                break;

            case 0xC0U:
                decoder->run = b1 & 0x3FU;
                break;

            default:
                return false;
            }
        }
    }

    hash = (uint8_t)((decoder->prev_r * 3U + decoder->prev_g * 5U +
                      decoder->prev_b * 7U + decoder->prev_a * 11U) & 63U);
    decoder->index_rgba[hash] = ((uint32_t)decoder->prev_r) |
                                ((uint32_t)decoder->prev_g << 8) |
                                ((uint32_t)decoder->prev_b << 16) |
                                ((uint32_t)decoder->prev_a << 24);

    *pixel = home_theme2_cloud_cache_rgb565(decoder->prev_r,
                                             decoder->prev_g,
                                             decoder->prev_b);
    *alpha = decoder->prev_a;
    decoder->pixels_decoded++;
    return true;
}

static bool home_theme2_cloud_cache_prepare_layout(uint32_t *used_bytes)
{
    uint32_t offset = 0U;
    uint8_t shape;
    uint8_t state;

    if ((used_bytes == NULL) ||
        (g_psram.total_size < PSRAM_UI_CACHE_BASE) ||
        (PSRAM_UI_CACHE_SIZE > (g_psram.total_size - PSRAM_UI_CACHE_BASE)))
    {
        return false;
    }

    for (shape = 0U; shape < HOME_THEME2_CLOUD_SHAPE_COUNT; shape++)
    {
        for (state = 0U; state < HOME_THEME2_CLOUD_STATE_COUNT; state++)
        {
            const egui_image_qoi_info_t *source =
                (const egui_image_qoi_info_t *)s_cloud_sources[shape][state]->base.res;
            home_theme2_cloud_cache_slot_t *slot = &s_cloud_slots[shape][state];
            uint32_t pixel_count;

            if ((source == NULL) || (source->data_buf == NULL) ||
                (source->data_type != EGUI_IMAGE_DATA_TYPE_RGB565) ||
                (source->alpha_type != EGUI_IMAGE_ALPHA_TYPE_8) ||
                (source->res_type != EGUI_RESOURCE_TYPE_INTERNAL) ||
                (source->width == 0U) || (source->width > HOME_THEME2_CLOUD_CACHE_MAX_WIDTH) ||
                (source->height == 0U) || (source->channels != 4U))
            {
                return false;
            }

            pixel_count = (uint32_t)source->width * source->height;
            slot->pixel_offset = home_theme2_cloud_cache_align(offset);
            slot->pixel_size = pixel_count * sizeof(uint16_t);
            slot->alpha_offset = home_theme2_cloud_cache_align(slot->pixel_offset + slot->pixel_size);
            slot->alpha_size = pixel_count;
            offset = home_theme2_cloud_cache_align(slot->alpha_offset + slot->alpha_size);

            if (offset > PSRAM_UI_CACHE_SIZE)
            {
                return false;
            }
        }
    }

    *used_bytes = offset;
    return true;
}

static bool home_theme2_cloud_cache_decode_image(uint8_t shape, uint8_t state)
{
    const egui_image_qoi_info_t *source =
        (const egui_image_qoi_info_t *)s_cloud_sources[shape][state]->base.res;
    home_theme2_cloud_cache_slot_t *slot = &s_cloud_slots[shape][state];
    home_theme2_qoi_decoder_t decoder;
    uint32_t pixel_hash = HOME_THEME2_CLOUD_CACHE_FNV_OFFSET;
    uint32_t alpha_hash = HOME_THEME2_CLOUD_CACHE_FNV_OFFSET;
    uint16_t row;

    home_theme2_qoi_decoder_init(&decoder, source);
    for (row = 0U; row < source->height; row++)
    {
        uint16_t column;
        uint32_t pixel_row_size = (uint32_t)source->width * sizeof(uint16_t);

        for (column = 0U; column < source->width; column++)
        {
            if (!home_theme2_qoi_decode_pixel(&decoder,
                                              &s_pixel_row[column],
                                              &s_alpha_row[column]))
            {
                return false;
            }
        }

        pixel_hash = home_theme2_cloud_cache_hash((const uint8_t *)s_pixel_row,
                                                   pixel_row_size,
                                                   pixel_hash);
        alpha_hash = home_theme2_cloud_cache_hash(s_alpha_row,
                                                   source->width,
                                                   alpha_hash);

        if ((qspi_psram_write(&g_psram,
                              PSRAM_UI_CACHE_BASE + slot->pixel_offset +
                                  (uint32_t)row * pixel_row_size,
                              (const uint8_t *)s_pixel_row,
                              pixel_row_size) != 0) ||
            (qspi_psram_write(&g_psram,
                              PSRAM_UI_CACHE_BASE + slot->alpha_offset +
                                  (uint32_t)row * source->width,
                              s_alpha_row,
                              source->width) != 0))
        {
            return false;
        }
    }

    if ((decoder.pixels_decoded != ((uint32_t)source->width * source->height)) ||
        (decoder.run != 0U) || (decoder.pos != source->data_size))
    {
        return false;
    }

    slot->pixel_hash = pixel_hash;
    slot->alpha_hash = alpha_hash;
    return true;
}

static bool home_theme2_cloud_cache_bind_and_verify(void)
{
    uint8_t shape;
    uint8_t state;

    for (shape = 0U; shape < HOME_THEME2_CLOUD_SHAPE_COUNT; shape++)
    {
        for (state = 0U; state < HOME_THEME2_CLOUD_STATE_COUNT; state++)
        {
            const egui_image_qoi_info_t *source =
                (const egui_image_qoi_info_t *)s_cloud_sources[shape][state]->base.res;
            home_theme2_cloud_cache_slot_t *slot = &s_cloud_slots[shape][state];
            uint32_t pixel_addr = PSRAM_UI_CACHE_BASE + slot->pixel_offset;
            uint32_t alpha_addr = PSRAM_UI_CACHE_BASE + slot->alpha_offset;
            uint32_t pixel_hash;
            uint32_t alpha_hash;
            egui_image_std_info_t *info = &s_cloud_infos[shape][state];

            if (!home_theme2_cloud_cache_hash_psram(pixel_addr, slot->pixel_size, &pixel_hash) ||
                !home_theme2_cloud_cache_hash_psram(alpha_addr, slot->alpha_size, &alpha_hash) ||
                (pixel_hash != slot->pixel_hash) || (alpha_hash != slot->alpha_hash))
            {
                return false;
            }

            info->data_buf = (const void *)PSRAM_EXTERNAL_RESOURCE_ID(pixel_addr);
            info->alpha_buf = (const void *)PSRAM_EXTERNAL_RESOURCE_ID(alpha_addr);
            info->data_type = EGUI_IMAGE_DATA_TYPE_RGB565;
            info->alpha_type = EGUI_IMAGE_ALPHA_TYPE_8;
            info->res_type = EGUI_RESOURCE_TYPE_EXTERNAL;
            info->width = source->width;
            info->height = source->height;
            egui_image_std_init(&s_cloud_images[shape][state].base, info);
        }
    }

    return true;
}

bool HomeTheme2CloudCache_Init(void)
{
    uint32_t used_bytes;
    uint8_t shape;
    uint8_t state;

    s_cache_ready = false;
    s_external_read_failed = false;
    memset(s_cloud_infos, 0, sizeof(s_cloud_infos));
    memset(s_cloud_images, 0, sizeof(s_cloud_images));
    memset(s_cloud_slots, 0, sizeof(s_cloud_slots));

    if ((g_psram.initialized == 0U) ||
        !home_theme2_cloud_cache_prepare_layout(&used_bytes))
    {
        log_printf("[HomeCloud] PSRAM/layout unavailable, QOI fallback");
        return false;
    }

    if ((g_psram.mmap_active != 0U) && (qspi_psram_exit_memory_mapped(&g_psram) != 0))
    {
        log_printf("[HomeCloud] PSRAM mmap exit failed, QOI fallback");
        return false;
    }

    for (shape = 0U; shape < HOME_THEME2_CLOUD_SHAPE_COUNT; shape++)
    {
        for (state = 0U; state < HOME_THEME2_CLOUD_STATE_COUNT; state++)
        {
            if (!home_theme2_cloud_cache_decode_image(shape, state))
            {
                log_printf("[HomeCloud] decode failed shape=%u state=%u, QOI fallback",
                           (unsigned)shape,
                           (unsigned)state);
                return false;
            }
        }
    }

    if (!home_theme2_cloud_cache_bind_and_verify())
    {
        log_printf("[HomeCloud] PSRAM verify failed, QOI fallback");
        return false;
    }

    s_cache_ready = true;
    log_printf("[HomeCloud] PSRAM keyframe cache ready used=%lu free=%lu",
               (unsigned long)used_bytes,
               (unsigned long)(PSRAM_UI_CACHE_SIZE - used_bytes));
    return true;
}

bool HomeTheme2CloudCache_IsReady(void)
{
    return s_cache_ready && !s_external_read_failed &&
           (g_psram.initialized != 0U) && (g_psram.qpi_mode != 0U) &&
           (g_psram.mmap_active == 0U);
}

const egui_image_std_t *HomeTheme2CloudCache_Get(uint8_t shape, uint8_t state)
{
    if (!HomeTheme2CloudCache_IsReady() ||
        (shape >= HOME_THEME2_CLOUD_SHAPE_COUNT) ||
        (state >= HOME_THEME2_CLOUD_STATE_COUNT))
    {
        return NULL;
    }

    return &s_cloud_images[shape][state];
}

void HomeTheme2CloudCache_MarkReadFault(void)
{
    if (!s_external_read_failed)
    {
        log_printf("[HomeCloud] PSRAM external read failed, QOI fallback");
    }
    s_external_read_failed = true;
    s_cache_ready = false;
}

bool HomeTheme2CloudCache_Recover(void)
{
    uint32_t start_tick = HAL_GetTick();
    uint32_t used_bytes;
    uint8_t shape;
    uint8_t state;

    s_cache_ready = false;
    log_printf("[HomeCloud] recover begin");

    if (qspi_psram_recover(&g_psram) != 0)
    {
        log_printf("[HomeCloud] bus recover failed, QOI fallback");
        return false;
    }

    s_external_read_failed = false;
    if (home_theme2_cloud_cache_bind_and_verify())
    {
        s_cache_ready = true;
        log_printf("[HomeCloud] recover reused cache ms=%lu",
                   (unsigned long)(HAL_GetTick() - start_tick));
        return true;
    }

    memset(s_cloud_infos, 0, sizeof(s_cloud_infos));
    memset(s_cloud_images, 0, sizeof(s_cloud_images));
    memset(s_cloud_slots, 0, sizeof(s_cloud_slots));
    if (!home_theme2_cloud_cache_prepare_layout(&used_bytes))
    {
        log_printf("[HomeCloud] layout failed, QOI fallback");
        return false;
    }

    for (shape = 0U; shape < HOME_THEME2_CLOUD_SHAPE_COUNT; shape++)
    {
        for (state = 0U; state < HOME_THEME2_CLOUD_STATE_COUNT; state++)
        {
            if (!home_theme2_cloud_cache_decode_image(shape, state))
            {
                log_printf("[HomeCloud] recover decode failed shape=%u state=%u, QOI fallback",
                           (unsigned)shape,
                           (unsigned)state);
                return false;
            }
        }
    }

    if (!home_theme2_cloud_cache_bind_and_verify())
    {
        log_printf("[HomeCloud] recover verify failed, QOI fallback");
        return false;
    }

    s_cache_ready = true;
    log_printf("[HomeCloud] recover ready used=%lu ms=%lu",
               (unsigned long)used_bytes,
               (unsigned long)(HAL_GetTick() - start_tick));
    return true;
}
