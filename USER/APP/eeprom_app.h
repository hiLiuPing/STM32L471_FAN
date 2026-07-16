#ifndef __EEPROM_APP_H
#define __EEPROM_APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "ee24.h"
#include "i2c_bus.h"

typedef struct
{
    EE24_HandleTypeDef handle;
    I2C_Bus_t *bus;
    uint8_t initialized;
} eeprom_ctx_t;

extern eeprom_ctx_t g_ee_ctx;

#define EE_BLOCK_SIZE       256U
#define OFF_NET_CONFIG      0U
#define OFF_CALIB_DATA      (EE_BLOCK_SIZE * 1U)
#define OFF_WEATHER_CONFIG  (EE_BLOCK_SIZE * 2U)
#define OFF_APP_SETTINGS    (EE_BLOCK_SIZE * 3U)
#define OFF_FAN_SETTINGS    (EE_BLOCK_SIZE * 4U)

#pragma pack(push, 1)
typedef struct
{
    uint32_t magic;
    char ssid[32];
    char pwd[64];
    char weather_url[128];
    uint16_t crc;
} NetConfig_t;

typedef struct
{
    uint32_t magic;
    float sensor_offset;
    int32_t calibration_val;
    uint16_t crc;
} CalibData_t;
#pragma pack(pop)

bool AppConfig_Init(eeprom_ctx_t *ctx, I2C_Bus_t *bus, uint8_t dev_addr);
bool AppConfig_Load(uint32_t offset, void *data, uint16_t size);
bool AppConfig_Save(uint32_t offset, void *data, uint16_t size);
uint16_t AppConfig_CRC16(uint8_t *ptr, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif
