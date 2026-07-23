#include "home_theme2_cloud_cache.h"

#include <stddef.h>
#include <string.h>

#include "canvas/egui_canvas.h"
#include "home_theme2_cloud_res.h"
#include "image/egui_image_std.h"
#include "log.h"
#include "psram_app.h"

#define HOME_THEME2_CLOUD_CACHE_MAX_WIDTH       229U
#define HOME_THEME2_CLOUD_CACHE_MAX_ROW_BYTES   (HOME_THEME2_CLOUD_CACHE_MAX_WIDTH * 3U)
#define HOME_THEME2_CLOUD_CACHE_ALIGN           4U
#define HOME_THEME2_CLOUD_CACHE_FNV_OFFSET      2166136261UL
#define HOME_THEME2_CLOUD_CACHE_FNV_PRIME       16777619UL
#define HOME_THEME2_CLOUD_BLEND_SLOT_COUNT      2U
#define HOME_THEME2_CLOUD_SERVICE_ROWS_PER_TICK 4U
#define HOME_THEME2_CLOUD_INVALID_SLOT          0xFFU

typedef struct
{
    uint16_t payload_offset;
    uint8_t first_segment;
    uint8_t segment_count;
} home_theme2_cloud_row_meta_t;

typedef struct
{
    uint16_t pixel_offset;
    uint8_t x_start;
    uint8_t length;
} home_theme2_cloud_segment_meta_t;

typedef struct
{
    const home_theme2_cloud_row_meta_t *rows;
    const home_theme2_cloud_segment_meta_t *segments;
    uint32_t payload_size;
    uint16_t width;
    uint16_t height;
} home_theme2_cloud_sparse_resource_t;

#include "home_theme2_cloud_sparse_rows.inc"

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
    uint32_t base_addr;
    uint32_t payload_hash;
} home_theme2_cloud_keyframe_slot_t;

typedef struct
{
    uint32_t shape_addr[HOME_THEME2_CLOUD_SHAPE_COUNT];
    uint8_t from_state;
    uint8_t to_state;
    uint8_t blend;
    uint8_t valid;
} home_theme2_cloud_blend_slot_t;

typedef union
{
    uint32_t align;
    uint8_t bytes[HOME_THEME2_CLOUD_CACHE_MAX_ROW_BYTES];
} home_theme2_cloud_row_buffer_t;

static const egui_image_qoi_t *const s_cloud_sources[HOME_THEME2_CLOUD_SHAPE_COUNT][HOME_THEME2_CLOUD_STATE_COUNT] = {
    {&qoi_scene_cloud_a_1, &qoi_scene_cloud_a_2, &qoi_scene_cloud_a_3},
    {&qoi_scene_cloud_b_1, &qoi_scene_cloud_b_2, &qoi_scene_cloud_b_3},
    {&qoi_scene_cloud_c_1, &qoi_scene_cloud_c_2, &qoi_scene_cloud_c_3},
};

static home_theme2_cloud_keyframe_slot_t s_keyframe_slots[HOME_THEME2_CLOUD_SHAPE_COUNT][HOME_THEME2_CLOUD_STATE_COUNT];
static home_theme2_cloud_blend_slot_t s_blend_slots[HOME_THEME2_CLOUD_BLEND_SLOT_COUNT];
static uint16_t s_decode_pixel_row[HOME_THEME2_CLOUD_CACHE_MAX_WIDTH];
static uint8_t s_decode_alpha_row[HOME_THEME2_CLOUD_CACHE_MAX_WIDTH];
static home_theme2_cloud_row_buffer_t s_row_buffer_a;
static home_theme2_cloud_row_buffer_t s_row_buffer_b;
static home_theme2_cloud_row_buffer_t s_row_buffer_c;
static HomeTheme2CloudCacheStats_t s_cache_stats;
static uint32_t s_cache_used_bytes;
static uint16_t s_pending_row;
static uint8_t s_pending_shape;
static uint8_t s_pending_slot = HOME_THEME2_CLOUD_INVALID_SLOT;
static volatile uint8_t s_active_slot = HOME_THEME2_CLOUD_INVALID_SLOT;
static bool s_cache_ready;
static bool s_external_read_failed;

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
    uint8_t blend_slot;

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
            const home_theme2_cloud_sparse_resource_t *resource = &s_cloud_sparse_resources[shape][state];

            if ((source == NULL) || (source->data_buf == NULL) ||
                (source->data_type != EGUI_IMAGE_DATA_TYPE_RGB565) ||
                (source->alpha_type != EGUI_IMAGE_ALPHA_TYPE_8) ||
                (source->res_type != EGUI_RESOURCE_TYPE_INTERNAL) ||
                (source->channels != 4U) ||
                (source->width != resource->width) || (source->height != resource->height) ||
                (resource->width > HOME_THEME2_CLOUD_CACHE_MAX_WIDTH))
            {
                return false;
            }

            offset = home_theme2_cloud_cache_align(offset);
            s_keyframe_slots[shape][state].base_addr = PSRAM_UI_CACHE_BASE + offset;
            offset += resource->payload_size;
        }
    }

    for (blend_slot = 0U; blend_slot < HOME_THEME2_CLOUD_BLEND_SLOT_COUNT; blend_slot++)
    {
        for (shape = 0U; shape < HOME_THEME2_CLOUD_SHAPE_COUNT; shape++)
        {
            offset = home_theme2_cloud_cache_align(offset);
            s_blend_slots[blend_slot].shape_addr[shape] = PSRAM_UI_CACHE_BASE + offset;
            offset += s_cloud_blend_slot_payload_sizes[shape];
        }
    }

    offset = home_theme2_cloud_cache_align(offset);
    if (offset > PSRAM_UI_CACHE_SIZE)
    {
        return false;
    }

    *used_bytes = offset;
    return true;
}

static bool home_theme2_cloud_cache_decode_image(uint8_t shape, uint8_t state)
{
    const egui_image_qoi_info_t *source =
        (const egui_image_qoi_info_t *)s_cloud_sources[shape][state]->base.res;
    const home_theme2_cloud_sparse_resource_t *resource = &s_cloud_sparse_resources[shape][state];
    home_theme2_cloud_keyframe_slot_t *slot = &s_keyframe_slots[shape][state];
    home_theme2_qoi_decoder_t decoder;
    uint32_t hash = HOME_THEME2_CLOUD_CACHE_FNV_OFFSET;
    uint16_t row;

    home_theme2_qoi_decoder_init(&decoder, source);
    for (row = 0U; row < source->height; row++)
    {
        const home_theme2_cloud_row_meta_t *meta = &resource->rows[row];
        uint16_t column;
        uint32_t row_bytes = 0U;
        uint16_t row_pixel_count = 0U;
        uint8_t segment_index;

        for (column = 0U; column < source->width; column++)
        {
            if (!home_theme2_qoi_decode_pixel(&decoder,
                                              &s_decode_pixel_row[column],
                                              &s_decode_alpha_row[column]))
            {
                return false;
            }
        }

        for (column = 0U; column < source->width; column++)
        {
            bool covered = false;

            for (segment_index = 0U; segment_index < meta->segment_count; segment_index++)
            {
                const home_theme2_cloud_segment_meta_t *segment =
                    &resource->segments[meta->first_segment + segment_index];

                if ((column >= segment->x_start) &&
                    (column < (uint16_t)(segment->x_start + segment->length)))
                {
                    covered = true;
                    break;
                }
            }
            if (covered != (s_decode_alpha_row[column] != 0U))
            {
                return false;
            }
        }

        for (segment_index = 0U; segment_index < meta->segment_count; segment_index++)
        {
            row_pixel_count += resource->segments[meta->first_segment + segment_index].length;
        }
        row_bytes = (uint32_t)row_pixel_count * 3U;
        for (segment_index = 0U; segment_index < meta->segment_count; segment_index++)
        {
            const home_theme2_cloud_segment_meta_t *segment =
                &resource->segments[meta->first_segment + segment_index];
            uint32_t pixel_bytes = (uint32_t)segment->length * sizeof(uint16_t);

            memcpy(s_row_buffer_a.bytes + (uint32_t)segment->pixel_offset * sizeof(uint16_t),
                   &s_decode_pixel_row[segment->x_start], pixel_bytes);
            memcpy(s_row_buffer_a.bytes + (uint32_t)row_pixel_count * sizeof(uint16_t) +
                       segment->pixel_offset,
                   &s_decode_alpha_row[segment->x_start], segment->length);
        }

        if (row_bytes != 0U)
        {
            hash = home_theme2_cloud_cache_hash(s_row_buffer_a.bytes, row_bytes, hash);
            if (qspi_psram_write(&g_psram,
                                 slot->base_addr + meta->payload_offset,
                                 s_row_buffer_a.bytes,
                                 row_bytes) != 0)
            {
                return false;
            }
        }
    }

    if ((decoder.pixels_decoded != ((uint32_t)source->width * source->height)) ||
        (decoder.run != 0U) || (decoder.pos != source->data_size))
    {
        return false;
    }

    slot->payload_hash = hash;
    return true;
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

        if (chunk > sizeof(s_row_buffer_a.bytes))
        {
            chunk = sizeof(s_row_buffer_a.bytes);
        }
        if (qspi_psram_read(&g_psram, addr + offset, s_row_buffer_a.bytes, chunk) != 0)
        {
            return false;
        }
        hash = home_theme2_cloud_cache_hash(s_row_buffer_a.bytes, chunk, hash);
        offset += chunk;
    }

    *result = hash;
    return true;
}

static bool home_theme2_cloud_cache_verify_keyframes(void)
{
    uint8_t shape;
    uint8_t state;

    for (shape = 0U; shape < HOME_THEME2_CLOUD_SHAPE_COUNT; shape++)
    {
        for (state = 0U; state < HOME_THEME2_CLOUD_STATE_COUNT; state++)
        {
            uint32_t hash;

            if (!home_theme2_cloud_cache_hash_psram(
                    s_keyframe_slots[shape][state].base_addr,
                    s_cloud_sparse_resources[shape][state].payload_size,
                    &hash) ||
                (hash != s_keyframe_slots[shape][state].payload_hash))
            {
                return false;
            }
        }
    }
    return true;
}

static void home_theme2_cloud_cache_reset_blend(void)
{
    memset(s_blend_slots, 0, sizeof(s_blend_slots));
    s_pending_shape = 0U;
    s_pending_row = 0U;
    s_pending_slot = HOME_THEME2_CLOUD_INVALID_SLOT;
    s_active_slot = HOME_THEME2_CLOUD_INVALID_SLOT;
}

static uint32_t home_theme2_cloud_cache_row_size(
    const home_theme2_cloud_sparse_resource_t *resource,
    uint16_t row)
{
    const home_theme2_cloud_row_meta_t *meta = &resource->rows[row];
    uint32_t size = 0U;
    uint8_t i;

    for (i = 0U; i < meta->segment_count; i++)
    {
        size += (uint32_t)resource->segments[meta->first_segment + i].length * 3U;
    }
    return size;
}

static bool home_theme2_cloud_cache_load_row(
    const home_theme2_cloud_sparse_resource_t *resource,
    uint32_t base_addr,
    uint16_t row,
    home_theme2_cloud_row_buffer_t *buffer,
    bool blend_service)
{
    const home_theme2_cloud_row_meta_t *meta = &resource->rows[row];
    uint32_t row_bytes = home_theme2_cloud_cache_row_size(resource, row);

    if (row_bytes == 0U)
    {
        return true;
    }
    if (qspi_psram_read(&g_psram,
                        base_addr + meta->payload_offset,
                        buffer->bytes,
                        row_bytes) != 0)
    {
        HomeTheme2CloudCache_MarkReadFault();
        return false;
    }

    if (blend_service)
    {
        s_cache_stats.blend_read_bytes += row_bytes;
    }
    else
    {
        s_cache_stats.draw_read_bytes += row_bytes;
    }
    return true;
}

static void home_theme2_cloud_cache_get_row_pixel(
    const home_theme2_cloud_sparse_resource_t *resource,
    uint16_t row,
    const home_theme2_cloud_row_buffer_t *buffer,
    uint16_t row_pixel_count,
    uint16_t x,
    uint16_t *pixel,
    uint8_t *alpha)
{
    const home_theme2_cloud_row_meta_t *meta = &resource->rows[row];
    uint8_t i;

    for (i = 0U; i < meta->segment_count; i++)
    {
        const home_theme2_cloud_segment_meta_t *segment =
            &resource->segments[meta->first_segment + i];

        if ((x >= segment->x_start) &&
            (x < (uint16_t)(segment->x_start + segment->length)))
        {
            uint16_t index = x - segment->x_start;
            const uint16_t *pixels = (const uint16_t *)buffer->bytes;
            const uint8_t *alphas = buffer->bytes +
                                    (uint32_t)row_pixel_count * sizeof(uint16_t);

            *pixel = pixels[segment->pixel_offset + index];
            *alpha = alphas[segment->pixel_offset + index];
            return;
        }
    }

    *pixel = 0U;
    *alpha = 0U;
}

static uint16_t home_theme2_cloud_cache_compose_pixel(uint16_t from_pixel,
                                                       uint8_t from_alpha,
                                                       uint16_t to_pixel,
                                                       uint8_t to_alpha,
                                                       uint8_t blend,
                                                       uint8_t *out_alpha)
{
    uint32_t to_effective_alpha = egui_color_alpha_mix(to_alpha, blend);
    uint32_t weight_to = to_effective_alpha * 255U;
    uint32_t weight_from = (uint32_t)from_alpha * (255U - to_effective_alpha);
    uint32_t denominator = weight_to + weight_from;
    uint32_t r;
    uint32_t g;
    uint32_t b;

    if (denominator == 0U)
    {
        *out_alpha = 0U;
        return 0U;
    }

    r = ((((uint32_t)(to_pixel >> 11) & 0x1FU) * weight_to) +
         (((uint32_t)(from_pixel >> 11) & 0x1FU) * weight_from) +
         denominator / 2U) / denominator;
    g = ((((uint32_t)(to_pixel >> 5) & 0x3FU) * weight_to) +
         (((uint32_t)(from_pixel >> 5) & 0x3FU) * weight_from) +
         denominator / 2U) / denominator;
    b = ((((uint32_t)to_pixel & 0x1FU) * weight_to) +
         (((uint32_t)from_pixel & 0x1FU) * weight_from) +
         denominator / 2U) / denominator;
    *out_alpha = (uint8_t)((denominator + 127U) / 255U);
    return (uint16_t)((r << 11) | (g << 5) | b);
}

static bool home_theme2_cloud_cache_service_row(home_theme2_cloud_blend_slot_t *slot,
                                                uint8_t shape,
                                                uint16_t row)
{
    const home_theme2_cloud_sparse_resource_t *from_resource =
        &s_cloud_sparse_resources[shape][slot->from_state];
    const home_theme2_cloud_sparse_resource_t *to_resource =
        &s_cloud_sparse_resources[shape][slot->to_state];
    const home_theme2_cloud_sparse_resource_t *out_resource =
        &s_cloud_blend_resources[shape][slot->from_state][slot->to_state];
    const home_theme2_cloud_row_meta_t *out_meta = &out_resource->rows[row];
    uint16_t from_row_pixel_count =
        (uint16_t)(home_theme2_cloud_cache_row_size(from_resource, row) / 3U);
    uint16_t to_row_pixel_count =
        (uint16_t)(home_theme2_cloud_cache_row_size(to_resource, row) / 3U);
    uint16_t out_row_pixel_count =
        (uint16_t)(home_theme2_cloud_cache_row_size(out_resource, row) / 3U);
    uint8_t segment_index;

    if (!home_theme2_cloud_cache_load_row(
            from_resource,
            s_keyframe_slots[shape][slot->from_state].base_addr,
            row,
            &s_row_buffer_a,
            true) ||
        !home_theme2_cloud_cache_load_row(
            to_resource,
            s_keyframe_slots[shape][slot->to_state].base_addr,
            row,
            &s_row_buffer_b,
            true))
    {
        return false;
    }

    for (segment_index = 0U; segment_index < out_meta->segment_count; segment_index++)
    {
        const home_theme2_cloud_segment_meta_t *segment =
            &out_resource->segments[out_meta->first_segment + segment_index];
        uint16_t *out_pixels = (uint16_t *)s_row_buffer_c.bytes;
        uint8_t *out_alphas = s_row_buffer_c.bytes +
                              (uint32_t)out_row_pixel_count * sizeof(uint16_t);
        uint16_t index;

        for (index = 0U; index < segment->length; index++)
        {
            uint16_t x = (uint16_t)(segment->x_start + index);
            uint16_t from_pixel;
            uint16_t to_pixel;
            uint8_t from_alpha;
            uint8_t to_alpha;

            home_theme2_cloud_cache_get_row_pixel(
                from_resource, row, &s_row_buffer_a, from_row_pixel_count,
                x, &from_pixel, &from_alpha);
            home_theme2_cloud_cache_get_row_pixel(
                to_resource, row, &s_row_buffer_b, to_row_pixel_count,
                x, &to_pixel, &to_alpha);
            out_pixels[segment->pixel_offset + index] =
                home_theme2_cloud_cache_compose_pixel(
                    from_pixel, from_alpha, to_pixel, to_alpha, slot->blend,
                    &out_alphas[segment->pixel_offset + index]);
        }
    }

    if (out_meta->segment_count != 0U)
    {
        uint32_t row_bytes = home_theme2_cloud_cache_row_size(out_resource, row);

        if (qspi_psram_write(&g_psram,
                             slot->shape_addr[shape] + out_meta->payload_offset,
                             s_row_buffer_c.bytes,
                             row_bytes) != 0)
        {
            HomeTheme2CloudCache_MarkReadFault();
            return false;
        }
        s_cache_stats.blend_write_bytes += row_bytes;
    }
    s_cache_stats.blend_row_count++;
    return true;
}

static bool home_theme2_cloud_cache_draw_resource(
    egui_canvas_t *canvas,
    const home_theme2_cloud_sparse_resource_t *resource,
    uint32_t base_addr,
    int16_t x,
    int16_t y,
    uint8_t layer_alpha)
{
    const egui_region_t *work = egui_canvas_get_base_view_work_region(canvas);
    int32_t top = y;
    int32_t bottom = (int32_t)y + resource->height;
    int32_t work_right = (int32_t)work->location.x + work->size.width;
    int32_t work_bottom = (int32_t)work->location.y + work->size.height;
    int32_t screen_y;

    if (top < work->location.y)
    {
        top = work->location.y;
    }
    if (bottom > work_bottom)
    {
        bottom = work_bottom;
    }
    if ((top >= bottom) || ((int32_t)x >= work_right) ||
        (((int32_t)x + resource->width) <= work->location.x))
    {
        return true;
    }

    for (screen_y = top; screen_y < bottom; screen_y++)
    {
        uint16_t row = (uint16_t)(screen_y - y);
        const home_theme2_cloud_row_meta_t *meta = &resource->rows[row];
        uint16_t row_pixel_count =
            (uint16_t)(home_theme2_cloud_cache_row_size(resource, row) / 3U);
        bool row_loaded = false;
        uint8_t segment_index;

        for (segment_index = 0U; segment_index < meta->segment_count; segment_index++)
        {
            const home_theme2_cloud_segment_meta_t *segment =
                &resource->segments[meta->first_segment + segment_index];
            int32_t left = (int32_t)x + segment->x_start;
            int32_t right = left + segment->length;
            uint16_t src_index;
            int32_t dst_x;
            int32_t dst_y;
            egui_color_int_t *dst;
            const uint16_t *pixels;
            const uint8_t *alphas;
            int32_t count;
            int32_t i;

            if ((right <= work->location.x) || (left >= work_right))
            {
                continue;
            }
            if (!row_loaded)
            {
                if (!home_theme2_cloud_cache_load_row(resource, base_addr, row,
                                                       &s_row_buffer_a, false))
                {
                    return false;
                }
                row_loaded = true;
            }

            if (left < work->location.x)
            {
                left = work->location.x;
            }
            if (right > work_right)
            {
                right = work_right;
            }
            src_index = (uint16_t)(left - ((int32_t)x + segment->x_start));
            dst_x = left - canvas->pfb_location_in_base_view.x;
            dst_y = screen_y - canvas->pfb_location_in_base_view.y;
            count = right - left;
            dst = &canvas->pfb[dst_y * canvas->pfb_region.size.width + dst_x];
            pixels = (const uint16_t *)s_row_buffer_a.bytes;
            alphas = s_row_buffer_a.bytes +
                     (uint32_t)row_pixel_count * sizeof(uint16_t);

            for (i = 0; i < count; i++)
            {
                uint16_t pixel = pixels[segment->pixel_offset + src_index + i];
                egui_alpha_t alpha = egui_color_alpha_mix(
                    alphas[segment->pixel_offset + src_index + i], layer_alpha);

#if EGUI_CONFIG_FUNCTION_SUPPORT_MASK
                if (canvas->mask != NULL)
                {
                    egui_color_t color;

                    color.full = EGUI_COLOR_RGB565_TRANS(pixel);
                    canvas->mask->api->mask_point(canvas->mask,
                                                  (egui_dim_t)(left + i),
                                                  (egui_dim_t)screen_y,
                                                  &color,
                                                  &alpha);
                    pixel = (uint16_t)color.full;
                }
#endif
                alpha = egui_color_alpha_mix(canvas->alpha, alpha);
                egui_image_std_blend_rgb565_src_pixel_fast(&dst[i], pixel, alpha);
            }
        }
        if (row_loaded)
        {
            s_cache_stats.draw_row_count++;
        }
    }
    return true;
}

bool HomeTheme2CloudCache_Init(void)
{
    uint8_t shape;
    uint8_t state;

    s_cache_ready = false;
    s_external_read_failed = false;
    memset(s_keyframe_slots, 0, sizeof(s_keyframe_slots));
    memset(&s_cache_stats, 0, sizeof(s_cache_stats));
    home_theme2_cloud_cache_reset_blend();

    if ((g_psram.initialized == 0U) ||
        !home_theme2_cloud_cache_prepare_layout(&s_cache_used_bytes))
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
                log_printf("[HomeCloud] sparse decode failed shape=%u state=%u, QOI fallback",
                           (unsigned)shape, (unsigned)state);
                return false;
            }
        }
    }

    if (!home_theme2_cloud_cache_verify_keyframes())
    {
        log_printf("[HomeCloud] sparse PSRAM verify failed, QOI fallback");
        return false;
    }

    s_cache_ready = true;
    qspi_psram_reset_stats(&g_psram);
    log_printf("[HomeCloud] sparse cache ready used=%lu free=%lu transfer=%u",
               (unsigned long)s_cache_used_bytes,
               (unsigned long)(PSRAM_UI_CACHE_SIZE - s_cache_used_bytes),
               (unsigned)PSRAM_COMMAND_MAX_TRANSFER_SIZE);
    return true;
}

bool HomeTheme2CloudCache_IsReady(void)
{
    return s_cache_ready && !s_external_read_failed &&
           (g_psram.initialized != 0U) && (g_psram.qpi_mode != 0U) &&
           (g_psram.mmap_active == 0U);
}

void HomeTheme2CloudCache_MarkReadFault(void)
{
    if (!s_external_read_failed)
    {
        log_printf("[HomeCloud] PSRAM command access failed, QOI fallback");
        s_cache_stats.cache_failure_count++;
    }
    s_external_read_failed = true;
    s_cache_ready = false;
    s_pending_slot = HOME_THEME2_CLOUD_INVALID_SLOT;
    s_active_slot = HOME_THEME2_CLOUD_INVALID_SLOT;
}

bool HomeTheme2CloudCache_Recover(void)
{
    uint32_t start_tick = HAL_GetTick();
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
    home_theme2_cloud_cache_reset_blend();
    if (!home_theme2_cloud_cache_prepare_layout(&s_cache_used_bytes) ||
        !home_theme2_cloud_cache_verify_keyframes())
    {
        for (shape = 0U; shape < HOME_THEME2_CLOUD_SHAPE_COUNT; shape++)
        {
            for (state = 0U; state < HOME_THEME2_CLOUD_STATE_COUNT; state++)
            {
                if (!home_theme2_cloud_cache_decode_image(shape, state))
                {
                    log_printf("[HomeCloud] recover decode failed shape=%u state=%u, QOI fallback",
                               (unsigned)shape, (unsigned)state);
                    return false;
                }
            }
        }
        if (!home_theme2_cloud_cache_verify_keyframes())
        {
            log_printf("[HomeCloud] recover verify failed, QOI fallback");
            return false;
        }
    }

    s_cache_ready = true;
    qspi_psram_reset_stats(&g_psram);
    log_printf("[HomeCloud] recover ready used=%lu ms=%lu",
               (unsigned long)s_cache_used_bytes,
               (unsigned long)(HAL_GetTick() - start_tick));
    return true;
}

void HomeTheme2CloudCache_RequestBlend(uint8_t from_state,
                                       uint8_t to_state,
                                       uint8_t blend)
{
    uint8_t i;
    uint8_t target_slot;

    if (!HomeTheme2CloudCache_IsReady() ||
        (from_state >= HOME_THEME2_CLOUD_STATE_COUNT) ||
        (to_state >= HOME_THEME2_CLOUD_STATE_COUNT))
    {
        return;
    }
    if ((blend == 0U) || (from_state == to_state))
    {
        s_pending_slot = HOME_THEME2_CLOUD_INVALID_SLOT;
        return;
    }
    if (blend == 255U)
    {
        s_pending_slot = HOME_THEME2_CLOUD_INVALID_SLOT;
        return;
    }

    for (i = 0U; i < HOME_THEME2_CLOUD_BLEND_SLOT_COUNT; i++)
    {
        if ((s_blend_slots[i].valid != 0U) &&
            (s_blend_slots[i].from_state == from_state) &&
            (s_blend_slots[i].to_state == to_state) &&
            (s_blend_slots[i].blend == blend))
        {
            __DMB();
            s_active_slot = i;
            s_pending_slot = HOME_THEME2_CLOUD_INVALID_SLOT;
            return;
        }
    }

    if ((s_pending_slot < HOME_THEME2_CLOUD_BLEND_SLOT_COUNT) &&
        (s_blend_slots[s_pending_slot].from_state == from_state) &&
        (s_blend_slots[s_pending_slot].to_state == to_state) &&
        (s_blend_slots[s_pending_slot].blend == blend))
    {
        return;
    }

    target_slot = (s_active_slot < HOME_THEME2_CLOUD_BLEND_SLOT_COUNT) ?
                      (uint8_t)(s_active_slot ^ 1U) : 0U;
    s_blend_slots[target_slot].valid = 0U;
    s_blend_slots[target_slot].from_state = from_state;
    s_blend_slots[target_slot].to_state = to_state;
    s_blend_slots[target_slot].blend = blend;
    s_pending_shape = 0U;
    s_pending_row = 0U;
    s_pending_slot = target_slot;
    s_cache_stats.blend_request_count++;
}

bool HomeTheme2CloudCache_Service(void)
{
    uint8_t rows_done = 0U;

    if (!HomeTheme2CloudCache_IsReady() ||
        (s_pending_slot >= HOME_THEME2_CLOUD_BLEND_SLOT_COUNT))
    {
        return false;
    }

    while ((rows_done < HOME_THEME2_CLOUD_SERVICE_ROWS_PER_TICK) &&
           (s_pending_shape < HOME_THEME2_CLOUD_SHAPE_COUNT))
    {
        home_theme2_cloud_blend_slot_t *slot = &s_blend_slots[s_pending_slot];

        if (!home_theme2_cloud_cache_service_row(slot, s_pending_shape, s_pending_row))
        {
            return false;
        }
        rows_done++;
        s_pending_row++;
        if (s_pending_row >=
            s_cloud_blend_resources[s_pending_shape]
                                   [slot->from_state]
                                   [slot->to_state].height)
        {
            s_pending_shape++;
            s_pending_row = 0U;
        }
    }

    if (s_pending_shape >= HOME_THEME2_CLOUD_SHAPE_COUNT)
    {
        uint8_t completed_slot = s_pending_slot;

        s_blend_slots[completed_slot].valid = 1U;
        __DMB();
        s_active_slot = completed_slot;
        s_pending_slot = HOME_THEME2_CLOUD_INVALID_SLOT;
        s_cache_stats.blend_complete_count++;
        return true;
    }
    return false;
}

bool HomeTheme2CloudCache_Draw(egui_canvas_t *canvas,
                               uint8_t shape,
                               int16_t x,
                               int16_t y,
                               uint8_t from_state,
                               uint8_t to_state,
                               uint8_t blend)
{
    uint8_t active_slot = s_active_slot;

    if ((canvas == NULL) || !HomeTheme2CloudCache_IsReady() ||
        (shape >= HOME_THEME2_CLOUD_SHAPE_COUNT) ||
        (from_state >= HOME_THEME2_CLOUD_STATE_COUNT) ||
        (to_state >= HOME_THEME2_CLOUD_STATE_COUNT))
    {
        return false;
    }

    s_cache_stats.draw_call_count++;
    if ((blend == 255U) && (from_state != to_state))
    {
        from_state = to_state;
        blend = 0U;
    }

    if ((blend != 0U) && (from_state != to_state) &&
        (active_slot < HOME_THEME2_CLOUD_BLEND_SLOT_COUNT) &&
        (s_blend_slots[active_slot].valid != 0U) &&
        (s_blend_slots[active_slot].from_state == from_state) &&
        (s_blend_slots[active_slot].to_state == to_state) &&
        (s_blend_slots[active_slot].blend == blend))
    {
        return home_theme2_cloud_cache_draw_resource(
            canvas,
            &s_cloud_blend_resources[shape]
                                    [s_blend_slots[active_slot].from_state]
                                    [s_blend_slots[active_slot].to_state],
            s_blend_slots[active_slot].shape_addr[shape], x, y, 255U);
    }

    if (!home_theme2_cloud_cache_draw_resource(
            canvas, &s_cloud_sparse_resources[shape][from_state],
            s_keyframe_slots[shape][from_state].base_addr, x, y, 255U))
    {
        return false;
    }
    if ((blend != 0U) && (from_state != to_state) &&
        !home_theme2_cloud_cache_draw_resource(
            canvas, &s_cloud_sparse_resources[shape][to_state],
            s_keyframe_slots[shape][to_state].base_addr, x, y, blend))
    {
        return false;
    }
    return true;
}

void HomeTheme2CloudCache_GetStats(HomeTheme2CloudCacheStats_t *stats)
{
    qspi_psram_stats_t psram_stats;

    if (stats == NULL)
    {
        return;
    }
    *stats = s_cache_stats;
    qspi_psram_get_stats(&g_psram, &psram_stats);
    stats->psram_read_call_count = psram_stats.read_call_count;
    stats->psram_read_transaction_count = psram_stats.read_transaction_count;
    stats->psram_read_byte_count = psram_stats.read_byte_count;
    stats->psram_read_max_time_ms = psram_stats.read_max_time_ms;
    stats->psram_read_failure_count = psram_stats.read_failure_count;
}

void HomeTheme2CloudCache_ResetStats(void)
{
    memset(&s_cache_stats, 0, sizeof(s_cache_stats));
    qspi_psram_reset_stats(&g_psram);
}
