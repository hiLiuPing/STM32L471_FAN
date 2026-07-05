#include "poetry_app.h"

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
        (header->data_offset != (POETRY_APP_HEADER_SIZE + header->poem_count * POETRY_APP_ENTRY_SIZE)) ||
        ((header->flags & POETRY_APP_FLAG_UTF8) == 0U))
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
