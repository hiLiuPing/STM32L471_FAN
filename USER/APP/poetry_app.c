#include "poetry_app.h"

#include <stdbool.h>
#include <string.h>

#include "lfs_port.h"
#include "log.h"
#include "rtc.h"

/* Module-local Weyl sequence; the avalanche mixer turns each state into a PRNG output. */
#define POETRY_APP_RANDOM_STEP (0x9E3779B9UL)

typedef struct
{
    uint32_t state;
    uint32_t last_index[POETRY_COLL_COUNT];
    bool initialized;
} PoetryApp_RandomState_t;

static PoetryApp_RandomState_t s_random;

static uint32_t PoetryApp_Mix32(uint32_t value)
{
    value ^= value >> 16;
    value *= 0x7FEB352DUL;
    value ^= value >> 15;
    value *= 0x846CA68BUL;
    value ^= value >> 16;
    return value;
}

static void PoetryApp_RandomInit(void)
{
    RTC_TimeTypeDef time = {0};
    RTC_DateTypeDef date = {0};
    uint32_t seed;

    if (s_random.initialized)
    {
        return;
    }

    seed = PoetryApp_Mix32(HAL_GetTick() ^ 0xA511E9B3UL);
    seed = PoetryApp_Mix32(seed ^ HAL_GetUIDw0());
    seed = PoetryApp_Mix32(seed ^ HAL_GetUIDw1());
    seed = PoetryApp_Mix32(seed ^ HAL_GetUIDw2());

    if ((HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN) == HAL_OK) &&
        (HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN) == HAL_OK))
    {
        uint32_t calendar = ((uint32_t)date.Year << 24) |
                            ((uint32_t)date.Month << 16) |
                            ((uint32_t)date.Date << 8) |
                            (uint32_t)date.WeekDay;
        uint32_t clock = ((uint32_t)time.Hours << 24) |
                         ((uint32_t)time.Minutes << 16) |
                         ((uint32_t)time.Seconds << 8);

        seed = PoetryApp_Mix32(seed ^ calendar);
        seed = PoetryApp_Mix32(seed ^ clock ^ time.SubSeconds);
    }

    if (seed == 0U)
    {
        seed = 0x6D2B79F5UL;
    }

    s_random.state = seed;
    for (uint32_t i = 0U; i < (uint32_t)POETRY_COLL_COUNT; i++)
    {
        s_random.last_index[i] = UINT32_MAX;
    }
    s_random.initialized = true;
}

static uint32_t PoetryApp_RandomNext(void)
{
    PoetryApp_RandomInit();
    s_random.state += POETRY_APP_RANDOM_STEP;
    return PoetryApp_Mix32(s_random.state);
}

static uint32_t PoetryApp_RandomBounded(uint32_t bound)
{
    uint32_t threshold;
    uint32_t value;

    if (bound <= 1U)
    {
        return 0U;
    }

    /* Reject the short tail so every result below bound has the same probability. */
    threshold = (uint32_t)(0U - bound) % bound;
    do
    {
        value = PoetryApp_RandomNext();
    } while (value < threshold);

    return value % bound;
}

static uint32_t PoetryApp_SelectIndex(PoetryApp_Collection_t coll, uint32_t poem_count)
{
    uint32_t last_index;
    uint32_t index;

    PoetryApp_RandomInit();

    if (poem_count <= 1U)
    {
        return 0U;
    }

    last_index = s_random.last_index[coll];
    if (last_index >= poem_count)
    {
        return PoetryApp_RandomBounded(poem_count);
    }

    /* Draw uniformly from N-1 candidates, then map around the previous index. */
    index = PoetryApp_RandomBounded(poem_count - 1U);
    if (index >= last_index)
    {
        index++;
    }

    return index;
}

static uint16_t PoetryApp_ReadLe16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t PoetryApp_ReadLe32(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

uint32_t PoetryApp_CalcCrc32(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFUL;

    if ((data == 0) && (len != 0U))
    {
        return 0U;
    }

    while (len-- != 0U)
    {
        crc ^= *data++;
        for (uint32_t i = 0U; i < 8U; i++)
        {
            uint32_t mask = 0U - (crc & 1U);
            crc = (crc >> 1) ^ (0xEDB88320UL & mask);
        }
    }

    return ~crc;
}

int PoetryApp_ParseHeader(const uint8_t raw[POETRY_APP_HEADER_SIZE], PoetryApp_Header_t *header)
{
    uint32_t expected_data_offset;

    if ((raw == 0) || (header == 0))
    {
        return POETRY_APP_ERR_PARAM;
    }

    header->magic = PoetryApp_ReadLe32(&raw[0]);
    header->version = PoetryApp_ReadLe16(&raw[4]);
    header->header_size = PoetryApp_ReadLe16(&raw[6]);
    header->poem_count = PoetryApp_ReadLe32(&raw[8]);
    header->entry_size = PoetryApp_ReadLe32(&raw[12]);
    header->data_offset = PoetryApp_ReadLe32(&raw[16]);
    header->data_size = PoetryApp_ReadLe32(&raw[20]);
    header->crc32 = PoetryApp_ReadLe32(&raw[24]);
    header->flags = PoetryApp_ReadLe32(&raw[28]);

    if ((header->magic != POETRY_APP_MAGIC) ||
        (header->version != POETRY_APP_VERSION) ||
        (header->header_size != POETRY_APP_HEADER_SIZE) ||
        (header->entry_size != POETRY_APP_ENTRY_SIZE) ||
        ((header->flags & POETRY_APP_FLAG_UTF8) == 0U))
    {
        return POETRY_APP_ERR_FORMAT;
    }

    if ((header->poem_count == 0U) ||
        (header->poem_count > ((UINT32_MAX - POETRY_APP_HEADER_SIZE) / POETRY_APP_ENTRY_SIZE)))
    {
        return POETRY_APP_ERR_FORMAT;
    }

    expected_data_offset = POETRY_APP_HEADER_SIZE + header->poem_count * POETRY_APP_ENTRY_SIZE;
    if (header->data_offset != expected_data_offset)
    {
        return POETRY_APP_ERR_FORMAT;
    }

    return POETRY_APP_OK;
}

int PoetryApp_ParseEntry(const uint8_t raw[POETRY_APP_ENTRY_SIZE], const PoetryApp_Header_t *header, PoetryApp_Entry_t *entry)
{
    if ((raw == 0) || (header == 0) || (entry == 0))
    {
        return POETRY_APP_ERR_PARAM;
    }

    entry->offset = PoetryApp_ReadLe32(&raw[0]);
    entry->length = PoetryApp_ReadLe32(&raw[4]);

    if ((entry->offset > header->data_size) ||
        (entry->length > header->data_size - entry->offset))
    {
        return POETRY_APP_ERR_FORMAT;
    }

    return POETRY_APP_OK;
}

int PoetryApp_CheckIndex(const PoetryApp_Header_t *header, uint32_t index)
{
    if (header == 0)
    {
        return POETRY_APP_ERR_PARAM;
    }

    return (index < header->poem_count) ? POETRY_APP_OK : POETRY_APP_ERR_RANGE;
}

int PoetryApp_CheckTextBuffer(const PoetryApp_Entry_t *entry, uint32_t buf_size)
{
    if (entry == 0)
    {
        return POETRY_APP_ERR_PARAM;
    }

    return (buf_size > entry->length) ? POETRY_APP_OK : POETRY_APP_ERR_BUF_SMALL;
}

int PoetryApp_CheckDataCrc(const PoetryApp_Header_t *header, const uint8_t *data)
{
    uint32_t crc;

    if ((header == 0) || ((data == 0) && (header->data_size != 0U)))
    {
        return POETRY_APP_ERR_PARAM;
    }

    crc = PoetryApp_CalcCrc32(data, header->data_size);
    return (crc == header->crc32) ? POETRY_APP_OK : POETRY_APP_ERR_CRC;
}

/* ========== Collection-level implementation ========== */

typedef struct
{
    bool            is_open;
    lfs_file_t      file;
    PoetryApp_Header_t header;
    const char     *path;

    /* LittleFS file config with static buffer (LFS_NO_MALLOC) */
    struct lfs_file_config file_cfg;
    uint8_t               file_buf[1024];   /* LFS_CACHE_SIZE */
} PoetryApp_CollectionState_t;

static PoetryApp_CollectionState_t s_coll[POETRY_COLL_COUNT];
static lfs_t *s_lfs = NULL;

static const char *coll_path(PoetryApp_Collection_t coll)
{
    switch (coll)
    {
    case POETRY_COLL_SONG_300:  return POETRY_APP_PATH_SONG_300;
    case POETRY_COLL_TANG_300:  return POETRY_APP_PATH_TANG_300;
    case POETRY_COLL_SONG_3000: return POETRY_APP_PATH_SONG_3000;
    default:                    return NULL;
    }
}

int PoetryApp_OpenCollection(PoetryApp_Collection_t coll)
{
    uint8_t raw_hdr[POETRY_APP_HEADER_SIZE];
    const char *path;
    int ret;

    if ((unsigned)coll >= POETRY_COLL_COUNT)
    {
        return POETRY_APP_ERR_PARAM;
    }

    if (s_lfs == NULL)
    {
        s_lfs = lfs_port_get();
        if (s_lfs == NULL)
        {
            return POETRY_APP_ERR_PARAM;
        }
    }

    path = coll_path(coll);
    if (path == NULL)
    {
        return POETRY_APP_ERR_PARAM;
    }

    PoetryApp_CollectionState_t *st = &s_coll[coll];

    if (st->is_open)
    {
        return POETRY_APP_OK;  /* already open */
    }

    memset(st, 0, sizeof(*st));

    st->file_cfg.buffer       = st->file_buf;
    st->file_cfg.attrs        = NULL;
    st->file_cfg.attr_count   = 0;

    ret = lfs_file_opencfg(s_lfs, &st->file, path, LFS_O_RDONLY, &st->file_cfg);
    if (ret != 0)
    {
        log_printf("[POETRY] open FAIL %s (ret=%d)", path, ret);
        return POETRY_APP_ERR_FORMAT;
    }

    /* read 32-byte header */
    if (lfs_file_read(s_lfs, &st->file, raw_hdr, sizeof(raw_hdr)) != sizeof(raw_hdr))
    {
        lfs_file_close(s_lfs, &st->file);
        return POETRY_APP_ERR_IO;
    }

    ret = PoetryApp_ParseHeader(raw_hdr, &st->header);
    if (ret != POETRY_APP_OK)
    {
        lfs_file_close(s_lfs, &st->file);
        log_printf("[POETRY] bad header %s", path);
        return ret;
    }

    st->path = path;
    st->is_open = true;

    log_printf("[POETRY] open %s: %u poems",
               path, (unsigned)st->header.poem_count);

    return POETRY_APP_OK;
}

void PoetryApp_CloseCollection(PoetryApp_Collection_t coll)
{
    if ((unsigned)coll < POETRY_COLL_COUNT && s_coll[coll].is_open)
    {
        lfs_file_close(s_lfs, &s_coll[coll].file);
        s_coll[coll].is_open = false;
    }
}

int PoetryApp_GetRandomPoem(PoetryApp_Collection_t coll,
                            uint8_t *text_buf, uint32_t buf_size,
                            PoetryApp_Poem_t *out)
{
    PoetryApp_CollectionState_t *st;
    PoetryApp_Entry_t entry;
    uint32_t idx;
    uint8_t raw_entry[POETRY_APP_ENTRY_SIZE];
    int ret;

    if ((unsigned)coll >= POETRY_COLL_COUNT || text_buf == NULL || out == NULL)
    {
        return POETRY_APP_ERR_PARAM;
    }

    st = &s_coll[coll];
    if (!st->is_open)
    {
        return POETRY_APP_ERR_PARAM;
    }

    /* pick a random poem */
    idx = PoetryApp_SelectIndex(coll, st->header.poem_count);

    /* seek to entry */
    uint32_t entry_off = st->header.header_size + idx * st->header.entry_size;
    if (lfs_file_seek(s_lfs, &st->file, (lfs_off_t)entry_off, LFS_SEEK_SET) < 0)
    {
        return POETRY_APP_ERR_IO;
    }

    if (lfs_file_read(s_lfs, &st->file, raw_entry, sizeof(raw_entry)) != sizeof(raw_entry))
    {
        return POETRY_APP_ERR_IO;
    }

    ret = PoetryApp_ParseEntry(raw_entry, &st->header, &entry);
    if (ret != POETRY_APP_OK)
    {
        return ret;
    }

    ret = PoetryApp_CheckTextBuffer(&entry, buf_size);
    if (ret != POETRY_APP_OK)
    {
        return ret;
    }

    /* seek to data payload */
    uint32_t data_off = st->header.data_offset + entry.offset;
    if (lfs_file_seek(s_lfs, &st->file, (lfs_off_t)data_off, LFS_SEEK_SET) < 0)
    {
        return POETRY_APP_ERR_IO;
    }

    if (lfs_file_read(s_lfs, &st->file, text_buf, entry.length) != (lfs_ssize_t)entry.length)
    {
        return POETRY_APP_ERR_IO;
    }
    text_buf[entry.length] = '\0';

    /* split into lines by \n */
    uint8_t line_cnt = 0;
    char *line_start = (char *)text_buf;

    for (uint32_t i = 0; i < entry.length && line_cnt < POETRY_APP_MAX_POEM_LINES; i++)
    {
        if (text_buf[i] == '\n')
        {
            text_buf[i] = '\0';
            out->lines[line_cnt++] = line_start;
            line_start = (char *)&text_buf[i + 1];
        }
    }

    /* last line (no trailing \n) */
    if (line_start[0] != '\0' && line_cnt < POETRY_APP_MAX_POEM_LINES)
    {
        out->lines[line_cnt++] = line_start;
    }

    out->line_count = line_cnt;

    s_random.last_index[coll] = idx;
    log_printf("[POETRY] pick coll=%u idx=%u count=%u",
               (unsigned)coll,
               (unsigned)idx,
               (unsigned)st->header.poem_count);

    return POETRY_APP_OK;
}
