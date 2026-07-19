#ifndef __PROTOCOL_CRC8_H__
#define __PROTOCOL_CRC8_H__

#include <stdint.h>

static inline uint8_t Protocol_Crc8(const uint8_t *data, uint16_t len)
{
    uint8_t crc = 0U;

    while (len-- != 0U)
    {
        crc ^= *data++;
        for (uint8_t i = 0U; i < 8U; i++)
        {
            crc = (crc & 0x80U) ? (uint8_t)((crc << 1) ^ 0x31U) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

#endif /* __PROTOCOL_CRC8_H__ */
