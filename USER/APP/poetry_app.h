#ifndef __POETRY_APP_H
#define __POETRY_APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define POETRY_APP_MAGIC               (0x49474E53UL)
#define POETRY_APP_VERSION             (1U)
#define POETRY_APP_HEADER_SIZE         (32U)
#define POETRY_APP_ENTRY_SIZE          (8U)
#define POETRY_APP_FLAG_UTF8           (1UL << 0)
#define POETRY_APP_FLAG_LF_LINE_ENDING (1UL << 1)
#define POETRY_APP_MAX_TEXT_SIZE       (4096U)

typedef enum
{
    POETRY_APP_OK = 0,
    POETRY_APP_ERR_PARAM = -1,
    POETRY_APP_ERR_FORMAT = -2,
    POETRY_APP_ERR_RANGE = -3,
    POETRY_APP_ERR_BUF_SMALL = -4,
    POETRY_APP_ERR_CRC = -5,
    POETRY_APP_ERR_IO = -6
} PoetryApp_Result_t;

typedef struct
{
    uint32_t magic;
    uint16_t version;
    uint16_t header_size;
    uint32_t poem_count;
    uint32_t entry_size;
    uint32_t data_offset;
    uint32_t data_size;
    uint32_t crc32;
    uint32_t flags;
} PoetryApp_Header_t;

typedef struct
{
    uint32_t offset;
    uint32_t length;
} PoetryApp_Entry_t;

int PoetryApp_ParseHeader(const uint8_t raw[POETRY_APP_HEADER_SIZE], PoetryApp_Header_t *header);
int PoetryApp_ParseEntry(const uint8_t raw[POETRY_APP_ENTRY_SIZE], const PoetryApp_Header_t *header, PoetryApp_Entry_t *entry);
int PoetryApp_CheckIndex(const PoetryApp_Header_t *header, uint32_t index);
int PoetryApp_CheckTextBuffer(const PoetryApp_Entry_t *entry, uint32_t buf_size);
uint32_t PoetryApp_CalcCrc32(const uint8_t *data, uint32_t len);
int PoetryApp_CheckDataCrc(const PoetryApp_Header_t *header, const uint8_t *data);

/* ========== Collection-level API ========== */

typedef enum
{
    POETRY_COLL_SONG_300  = 0,
    POETRY_COLL_TANG_300  = 1,
    POETRY_COLL_SONG_3000 = 2,
    POETRY_COLL_COUNT     = 3
} PoetryApp_Collection_t;

#define POETRY_APP_PATH_SONG_300   "song_300.idx"
#define POETRY_APP_PATH_TANG_300   "tang_300.idx"
#define POETRY_APP_PATH_SONG_3000  "song_3000.idx"

#define POETRY_APP_MAX_POEM_LINES  96U

typedef struct
{
    const char *lines[POETRY_APP_MAX_POEM_LINES];
    uint8_t line_count;
} PoetryApp_Poem_t;

int PoetryApp_OpenCollection(PoetryApp_Collection_t coll);
void PoetryApp_CloseCollection(PoetryApp_Collection_t coll);
int PoetryApp_GetRandomPoem(PoetryApp_Collection_t coll,
                            uint8_t *text_buf, uint32_t buf_size,
                            PoetryApp_Poem_t *out);

#ifdef __cplusplus
}
#endif

#endif
