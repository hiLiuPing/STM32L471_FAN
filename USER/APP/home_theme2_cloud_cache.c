#include "home_theme2_cloud_cache.h"

#include <stddef.h>
#include <string.h>

#include "home_theme2_cloud_res.h"
#include "log.h"
#include "psram_app.h"

#define HOME_THEME2_CLOUD_CACHE_MAX_WIDTH 229U
#define HOME_THEME2_CLOUD_CACHE_ALIGN      4U
#define HOME_THEME2_CLOUD_BLEND_SLOT_MAX   41U
#define HOME_THEME2_CLOUD_BLEND_SLOT_NONE  0xFFU
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

typedef struct
{
    uint32_t pixel_offset;
    uint32_t alpha_offset;
    uint32_t pixel_size;
    uint32_t alpha_size;
} home_theme2_cloud_buffer_layout_t;

typedef struct
{
    uint32_t last_used;
    uint8_t from_state;
    uint8_t to_state;
    uint8_t blend;
    uint8_t valid;
} home_theme2_cloud_blend_slot_t;

static const egui_image_qoi_t *const s_cloud_sources[HOME_THEME2_CLOUD_SHAPE_COUNT][HOME_THEME2_CLOUD_STATE_COUNT] = {
    {&qoi_scene_cloud_a_1, &qoi_scene_cloud_a_2, &qoi_scene_cloud_a_3},
    {&qoi_scene_cloud_b_1, &qoi_scene_cloud_b_2, &qoi_scene_cloud_b_3},
    {&qoi_scene_cloud_c_1, &qoi_scene_cloud_c_2, &qoi_scene_cloud_c_3},
};

static egui_image_std_info_t s_cloud_infos[HOME_THEME2_CLOUD_SHAPE_COUNT][HOME_THEME2_CLOUD_STATE_COUNT];
static egui_image_std_t s_cloud_images[HOME_THEME2_CLOUD_SHAPE_COUNT][HOME_THEME2_CLOUD_STATE_COUNT];
static home_theme2_cloud_cache_slot_t s_cloud_slots[HOME_THEME2_CLOUD_SHAPE_COUNT][HOME_THEME2_CLOUD_STATE_COUNT];
static home_theme2_cloud_buffer_layout_t s_blend_shape_layouts[HOME_THEME2_CLOUD_SHAPE_COUNT];
static home_theme2_cloud_blend_slot_t s_blend_slots[HOME_THEME2_CLOUD_BLEND_SLOT_MAX];
static egui_image_std_info_t s_blend_infos[HOME_THEME2_CLOUD_SHAPE_COUNT];
static egui_image_std_t s_blend_images[HOME_THEME2_CLOUD_SHAPE_COUNT];
static uint16_t s_pixel_row[HOME_THEME2_CLOUD_CACHE_MAX_WIDTH];
static uint8_t s_alpha_row[HOME_THEME2_CLOUD_CACHE_MAX_WIDTH];
static uint16_t s_to_pixel_row[HOME_THEME2_CLOUD_CACHE_MAX_WIDTH];
static uint8_t s_to_alpha_row[HOME_THEME2_CLOUD_CACHE_MAX_WIDTH];
static uint32_t s_source_used_bytes = 0U;
static uint32_t s_blend_base_offset = 0U;
static uint32_t s_blend_slot_size = 0U;
static uint32_t s_blend_use_counter = 0U;
static uint8_t s_blend_slot_count = 0U;
static uint8_t s_active_blend_slot = HOME_THEME2_CLOUD_BLEND_SLOT_NONE;
static bool s_cache_ready = false;

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

static uint32_t home_theme2_cloud_cache_hash_mmap(const volatile uint8_t *data, uint32_t size)
{
    uint32_t hash = HOME_THEME2_CLOUD_CACHE_FNV_OFFSET;
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
    uint32_t blend_offset = 0U;
    uint32_t available_slots;
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

    s_source_used_bytes = offset;
    s_blend_base_offset = home_theme2_cloud_cache_align(offset);

    for (shape = 0U; shape < HOME_THEME2_CLOUD_SHAPE_COUNT; shape++)
    {
        const egui_image_qoi_info_t *source =
            (const egui_image_qoi_info_t *)s_cloud_sources[shape][0U]->base.res;
        home_theme2_cloud_buffer_layout_t *layout = &s_blend_shape_layouts[shape];
        uint32_t pixel_count = (uint32_t)source->width * source->height;

        for (state = 1U; state < HOME_THEME2_CLOUD_STATE_COUNT; state++)
        {
            const egui_image_qoi_info_t *other =
                (const egui_image_qoi_info_t *)s_cloud_sources[shape][state]->base.res;

            if ((other->width != source->width) || (other->height != source->height))
            {
                return false;
            }
        }

        layout->pixel_offset = home_theme2_cloud_cache_align(blend_offset);
        layout->pixel_size = pixel_count * sizeof(uint16_t);
        layout->alpha_offset = home_theme2_cloud_cache_align(layout->pixel_offset + layout->pixel_size);
        layout->alpha_size = pixel_count;
        blend_offset = home_theme2_cloud_cache_align(layout->alpha_offset + layout->alpha_size);
    }

    s_blend_slot_size = home_theme2_cloud_cache_align(blend_offset);
    if ((s_blend_slot_size == 0U) || (s_blend_base_offset > PSRAM_UI_CACHE_SIZE))
    {
        return false;
    }

    available_slots = (PSRAM_UI_CACHE_SIZE - s_blend_base_offset) / s_blend_slot_size;
    if (available_slots > HOME_THEME2_CLOUD_BLEND_SLOT_MAX)
    {
        available_slots = HOME_THEME2_CLOUD_BLEND_SLOT_MAX;
    }
    if (available_slots == 0U)
    {
        return false;
    }

    s_blend_slot_count = (uint8_t)available_slots;
    offset = s_blend_base_offset + available_slots * s_blend_slot_size;

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
            const volatile uint8_t *pixel_addr =
                PSRAM_MMAP_BASE + PSRAM_UI_CACHE_BASE + slot->pixel_offset;
            const volatile uint8_t *alpha_addr =
                PSRAM_MMAP_BASE + PSRAM_UI_CACHE_BASE + slot->alpha_offset;
            egui_image_std_info_t *info = &s_cloud_infos[shape][state];

            if ((home_theme2_cloud_cache_hash_mmap(pixel_addr, slot->pixel_size) != slot->pixel_hash) ||
                (home_theme2_cloud_cache_hash_mmap(alpha_addr, slot->alpha_size) != slot->alpha_hash))
            {
                return false;
            }

            info->data_buf = (const void *)pixel_addr;
            info->alpha_buf = (const void *)alpha_addr;
            info->data_type = EGUI_IMAGE_DATA_TYPE_RGB565;
            info->alpha_type = EGUI_IMAGE_ALPHA_TYPE_8;
            info->res_type = EGUI_RESOURCE_TYPE_INTERNAL;
            info->width = source->width;
            info->height = source->height;
            egui_image_std_init(&s_cloud_images[shape][state].base, info);
        }
    }

    return true;
}

static uint8_t home_theme2_cloud_cache_alpha_mix(uint8_t alpha_0, uint8_t alpha_1)
{
    if (alpha_0 == 255U)
    {
        return alpha_1;
    }
    if (alpha_1 == 255U)
    {
        return alpha_0;
    }

    return (uint8_t)((((uint16_t)alpha_0 * alpha_1) + 128U) >> 8);
}

static uint8_t home_theme2_cloud_cache_render_alpha5(uint8_t alpha)
{
    if (alpha < 4U)
    {
        return 0U;
    }
    if (alpha > 251U)
    {
        return 32U;
    }
    return alpha >> 3;
}

static uint8_t home_theme2_cloud_cache_mix_component(uint8_t from_component,
                                                       uint8_t to_component,
                                                       uint16_t from_weight,
                                                       uint16_t to_weight,
                                                       uint16_t output_weight)
{
    uint32_t value;

    if (output_weight == 0U)
    {
        return 0U;
    }

    value = (uint32_t)from_component * from_weight +
            (uint32_t)to_component * to_weight;
    return (uint8_t)((value + ((uint32_t)output_weight / 2U)) / output_weight);
}

static void home_theme2_cloud_cache_mix_pixel(uint16_t from_pixel,
                                               uint8_t from_alpha,
                                               uint16_t to_pixel,
                                               uint8_t to_alpha,
                                               uint8_t blend,
                                               uint16_t *output_pixel,
                                               uint8_t *output_alpha)
{
    uint8_t from_alpha5 = home_theme2_cloud_cache_render_alpha5(from_alpha);
    uint8_t to_alpha5 = home_theme2_cloud_cache_render_alpha5(
        home_theme2_cloud_cache_alpha_mix(blend, to_alpha));
    uint16_t background_weight;
    uint16_t from_weight;
    uint16_t to_weight;
    uint16_t output_weight;
    uint8_t output_alpha5;
    uint8_t out_alpha;
    uint8_t red;
    uint8_t green;
    uint8_t blue;

    background_weight = (uint16_t)(32U - from_alpha5) * (32U - to_alpha5);
    output_alpha5 = (uint8_t)(32U - ((background_weight + 16U) >> 5));
    from_weight = (uint16_t)from_alpha5 * (32U - to_alpha5);
    to_weight = (uint16_t)to_alpha5 * 32U;
    output_weight = (uint16_t)output_alpha5 * 32U;
    out_alpha = (output_alpha5 == 32U) ? 255U : (uint8_t)(output_alpha5 << 3);

    red = home_theme2_cloud_cache_mix_component(
        (uint8_t)((from_pixel >> 11) & 0x1FU),
        (uint8_t)((to_pixel >> 11) & 0x1FU),
        from_weight,
        to_weight,
        output_weight);
    green = home_theme2_cloud_cache_mix_component(
        (uint8_t)((from_pixel >> 5) & 0x3FU),
        (uint8_t)((to_pixel >> 5) & 0x3FU),
        from_weight,
        to_weight,
        output_weight);
    blue = home_theme2_cloud_cache_mix_component(
        (uint8_t)(from_pixel & 0x1FU),
        (uint8_t)(to_pixel & 0x1FU),
        from_weight,
        to_weight,
        output_weight);

    if (red > 31U)
    {
        red = 31U;
    }
    if (green > 63U)
    {
        green = 63U;
    }
    if (blue > 31U)
    {
        blue = 31U;
    }

    *output_pixel = (uint16_t)(((uint16_t)red << 11) |
                               ((uint16_t)green << 5) |
                               blue);
    *output_alpha = out_alpha;
}

static uint32_t home_theme2_cloud_cache_blend_slot_base(uint8_t slot_index)
{
    return s_blend_base_offset + (uint32_t)slot_index * s_blend_slot_size;
}

static bool home_theme2_cloud_cache_blend_key_matches(
    const home_theme2_cloud_blend_slot_t *slot,
    uint8_t from_state,
    uint8_t to_state,
    uint8_t blend)
{
    return (slot != NULL) && (slot->valid != 0U) &&
           (slot->from_state == from_state) &&
           (slot->to_state == to_state) &&
           (slot->blend == blend);
}

static void home_theme2_cloud_cache_bind_blend_slot(uint8_t slot_index)
{
    uint32_t slot_base = home_theme2_cloud_cache_blend_slot_base(slot_index);
    uint8_t shape;

    for (shape = 0U; shape < HOME_THEME2_CLOUD_SHAPE_COUNT; shape++)
    {
        const egui_image_qoi_info_t *source =
            (const egui_image_qoi_info_t *)s_cloud_sources[shape][0U]->base.res;
        const home_theme2_cloud_buffer_layout_t *layout = &s_blend_shape_layouts[shape];
        egui_image_std_info_t *info = &s_blend_infos[shape];

        info->data_buf = (const void *)(PSRAM_MMAP_BASE + PSRAM_UI_CACHE_BASE +
                                        slot_base + layout->pixel_offset);
        info->alpha_buf = (const void *)(PSRAM_MMAP_BASE + PSRAM_UI_CACHE_BASE +
                                         slot_base + layout->alpha_offset);
        info->data_type = EGUI_IMAGE_DATA_TYPE_RGB565;
        info->alpha_type = EGUI_IMAGE_ALPHA_TYPE_8;
        info->res_type = EGUI_RESOURCE_TYPE_INTERNAL;
        info->width = source->width;
        info->height = source->height;
        egui_image_std_init(&s_blend_images[shape].base, info);
    }

    s_active_blend_slot = slot_index;
}

static bool home_theme2_cloud_cache_generate_blend_slot(uint8_t slot_index,
                                                         uint8_t from_state,
                                                         uint8_t to_state,
                                                         uint8_t blend)
{
    uint32_t blend_slot_base = home_theme2_cloud_cache_blend_slot_base(slot_index);
    uint8_t shape;

    for (shape = 0U; shape < HOME_THEME2_CLOUD_SHAPE_COUNT; shape++)
    {
        const egui_image_qoi_info_t *source =
            (const egui_image_qoi_info_t *)s_cloud_sources[shape][from_state]->base.res;
        const home_theme2_cloud_cache_slot_t *from_slot = &s_cloud_slots[shape][from_state];
        const home_theme2_cloud_cache_slot_t *to_slot = &s_cloud_slots[shape][to_state];
        const home_theme2_cloud_buffer_layout_t *output_layout = &s_blend_shape_layouts[shape];
        uint32_t pixel_row_size = (uint32_t)source->width * sizeof(uint16_t);
        uint16_t row;

        for (row = 0U; row < source->height; row++)
        {
            uint32_t pixel_row_offset = (uint32_t)row * pixel_row_size;
            uint32_t alpha_row_offset = (uint32_t)row * source->width;
            uint16_t column;

            if ((qspi_psram_read(&g_psram,
                                 PSRAM_UI_CACHE_BASE + from_slot->pixel_offset + pixel_row_offset,
                                 (uint8_t *)s_pixel_row,
                                 pixel_row_size) != 0) ||
                (qspi_psram_read(&g_psram,
                                 PSRAM_UI_CACHE_BASE + from_slot->alpha_offset + alpha_row_offset,
                                 s_alpha_row,
                                 source->width) != 0) ||
                (qspi_psram_read(&g_psram,
                                 PSRAM_UI_CACHE_BASE + to_slot->pixel_offset + pixel_row_offset,
                                 (uint8_t *)s_to_pixel_row,
                                 pixel_row_size) != 0) ||
                (qspi_psram_read(&g_psram,
                                 PSRAM_UI_CACHE_BASE + to_slot->alpha_offset + alpha_row_offset,
                                 s_to_alpha_row,
                                 source->width) != 0))
            {
                return false;
            }

            for (column = 0U; column < source->width; column++)
            {
                home_theme2_cloud_cache_mix_pixel(s_pixel_row[column],
                                                   s_alpha_row[column],
                                                   s_to_pixel_row[column],
                                                   s_to_alpha_row[column],
                                                   blend,
                                                   &s_pixel_row[column],
                                                   &s_alpha_row[column]);
            }

            if ((qspi_psram_write(&g_psram,
                                  PSRAM_UI_CACHE_BASE + blend_slot_base +
                                      output_layout->pixel_offset + pixel_row_offset,
                                  (const uint8_t *)s_pixel_row,
                                  pixel_row_size) != 0) ||
                (qspi_psram_write(&g_psram,
                                  PSRAM_UI_CACHE_BASE + blend_slot_base +
                                      output_layout->alpha_offset + alpha_row_offset,
                                  s_alpha_row,
                                  source->width) != 0))
            {
                return false;
            }
        }
    }

    return true;
}

static uint8_t home_theme2_cloud_cache_find_blend_slot(uint8_t from_state,
                                                        uint8_t to_state,
                                                        uint8_t blend)
{
    uint8_t slot_index;

    for (slot_index = 0U; slot_index < s_blend_slot_count; slot_index++)
    {
        if (home_theme2_cloud_cache_blend_key_matches(&s_blend_slots[slot_index],
                                                       from_state,
                                                       to_state,
                                                       blend))
        {
            return slot_index;
        }
    }

    return HOME_THEME2_CLOUD_BLEND_SLOT_NONE;
}

static uint8_t home_theme2_cloud_cache_select_blend_slot(void)
{
    uint32_t oldest_use = 0xFFFFFFFFUL;
    uint8_t oldest_slot = HOME_THEME2_CLOUD_BLEND_SLOT_NONE;
    uint8_t slot_index;

    for (slot_index = 0U; slot_index < s_blend_slot_count; slot_index++)
    {
        if (s_blend_slots[slot_index].valid == 0U)
        {
            return slot_index;
        }
    }

    for (slot_index = 0U; slot_index < s_blend_slot_count; slot_index++)
    {
        if ((slot_index != s_active_blend_slot) &&
            (s_blend_slots[slot_index].last_used < oldest_use))
        {
            oldest_use = s_blend_slots[slot_index].last_used;
            oldest_slot = slot_index;
        }
    }

    return oldest_slot;
}

bool HomeTheme2CloudCache_Init(void)
{
    uint32_t used_bytes;
    uint8_t shape;
    uint8_t state;

    s_cache_ready = false;
    s_source_used_bytes = 0U;
    s_blend_base_offset = 0U;
    s_blend_slot_size = 0U;
    s_blend_use_counter = 0U;
    s_blend_slot_count = 0U;
    s_active_blend_slot = HOME_THEME2_CLOUD_BLEND_SLOT_NONE;
    memset(s_cloud_infos, 0, sizeof(s_cloud_infos));
    memset(s_cloud_images, 0, sizeof(s_cloud_images));
    memset(s_cloud_slots, 0, sizeof(s_cloud_slots));
    memset(s_blend_shape_layouts, 0, sizeof(s_blend_shape_layouts));
    memset(s_blend_slots, 0, sizeof(s_blend_slots));
    memset(s_blend_infos, 0, sizeof(s_blend_infos));
    memset(s_blend_images, 0, sizeof(s_blend_images));

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

    if (qspi_psram_enable_memory_mapped(&g_psram) != 0)
    {
        log_printf("[HomeCloud] PSRAM mmap failed, QOI fallback");
        return false;
    }

    if (!home_theme2_cloud_cache_bind_and_verify())
    {
        (void)qspi_psram_exit_memory_mapped(&g_psram);
        log_printf("[HomeCloud] PSRAM verify failed, QOI fallback");
        return false;
    }

    s_cache_ready = true;
    log_printf("[HomeCloud] PSRAM cache ready source=%lu blend_slot=%lu slots=%u total=%lu free=%lu",
               (unsigned long)s_source_used_bytes,
               (unsigned long)s_blend_slot_size,
               (unsigned)s_blend_slot_count,
               (unsigned long)used_bytes,
               (unsigned long)(PSRAM_UI_CACHE_SIZE - used_bytes));
    return true;
}

bool HomeTheme2CloudCache_IsReady(void)
{
    return s_cache_ready;
}

const egui_image_std_t *HomeTheme2CloudCache_Get(uint8_t shape, uint8_t state)
{
    if (!s_cache_ready ||
        (shape >= HOME_THEME2_CLOUD_SHAPE_COUNT) ||
        (state >= HOME_THEME2_CLOUD_STATE_COUNT))
    {
        return NULL;
    }

    return &s_cloud_images[shape][state];
}

bool HomeTheme2CloudCache_PrepareBlend(uint8_t from_state,
                                      uint8_t to_state,
                                      uint8_t blend)
{
    home_theme2_cloud_blend_slot_t *slot;
    uint32_t start_tick;
    uint8_t slot_index;
    bool generated;

    if (!s_cache_ready ||
        (from_state >= HOME_THEME2_CLOUD_STATE_COUNT) ||
        (to_state >= HOME_THEME2_CLOUD_STATE_COUNT) ||
        (from_state == to_state) || (blend == 0U) ||
        (s_blend_slot_count == 0U))
    {
        return false;
    }

    slot_index = home_theme2_cloud_cache_find_blend_slot(from_state,
                                                          to_state,
                                                          blend);
    if (slot_index != HOME_THEME2_CLOUD_BLEND_SLOT_NONE)
    {
        slot = &s_blend_slots[slot_index];
        slot->last_used = ++s_blend_use_counter;
        home_theme2_cloud_cache_bind_blend_slot(slot_index);
        return true;
    }

    slot_index = home_theme2_cloud_cache_select_blend_slot();
    if (slot_index == HOME_THEME2_CLOUD_BLEND_SLOT_NONE)
    {
        log_printf("[HomeCloud] blend slot unavailable from=%u to=%u alpha=%u",
                   (unsigned)from_state,
                   (unsigned)to_state,
                   (unsigned)blend);
        return false;
    }

    start_tick = HAL_GetTick();
    if ((g_psram.mmap_active != 0U) &&
        (qspi_psram_exit_memory_mapped(&g_psram) != 0))
    {
        log_printf("[HomeCloud] blend mmap exit failed from=%u to=%u alpha=%u",
                   (unsigned)from_state,
                   (unsigned)to_state,
                   (unsigned)blend);
        return false;
    }

    slot = &s_blend_slots[slot_index];
    slot->valid = 0U;
    generated = home_theme2_cloud_cache_generate_blend_slot(slot_index,
                                                             from_state,
                                                             to_state,
                                                             blend);

    if (qspi_psram_enable_memory_mapped(&g_psram) != 0)
    {
        s_cache_ready = false;
        s_active_blend_slot = HOME_THEME2_CLOUD_BLEND_SLOT_NONE;
        log_printf("[HomeCloud] blend mmap restore failed, QOI fallback");
        return false;
    }

    if (!generated)
    {
        log_printf("[HomeCloud] blend generate failed from=%u to=%u alpha=%u slot=%u ms=%lu",
                   (unsigned)from_state,
                   (unsigned)to_state,
                   (unsigned)blend,
                   (unsigned)slot_index,
                   (unsigned long)(HAL_GetTick() - start_tick));
        return false;
    }

    slot->from_state = from_state;
    slot->to_state = to_state;
    slot->blend = blend;
    slot->last_used = ++s_blend_use_counter;
    slot->valid = 1U;
    home_theme2_cloud_cache_bind_blend_slot(slot_index);

    log_printf("[HomeCloud] blend ready from=%u to=%u alpha=%u slot=%u ms=%lu",
               (unsigned)from_state,
               (unsigned)to_state,
               (unsigned)blend,
               (unsigned)slot_index,
               (unsigned long)(HAL_GetTick() - start_tick));
    return true;
}

const egui_image_std_t *HomeTheme2CloudCache_GetBlended(uint8_t shape,
                                                        uint8_t from_state,
                                                        uint8_t to_state,
                                                        uint8_t blend)
{
    if (!s_cache_ready ||
        (shape >= HOME_THEME2_CLOUD_SHAPE_COUNT) ||
        (s_active_blend_slot >= s_blend_slot_count) ||
        !home_theme2_cloud_cache_blend_key_matches(
            &s_blend_slots[s_active_blend_slot],
            from_state,
            to_state,
            blend))
    {
        return NULL;
    }

    return &s_blend_images[shape];
}
