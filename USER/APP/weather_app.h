#ifndef __WEATHER_APP_H
#define __WEATHER_APP_H

/* Set to 1 to enable the Home time/weather scene demo; 0 for normal operation. */
#define WEATHER_DEMO_ENABLE 1

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "FreeRTOS.h"
#include "main.h"
#include "queue.h"
#include "task.h"
#include "event_groups.h"
#include "uart_dma.h"

#define FRAME_HEAD 0xAAU
#define FRAME_TAIL 0x55U

typedef enum
{
    CMD_GET_TIME = 0x01,
    CMD_GET_NOW_DETAIL = 0x02,
    CMD_GET_AIR_DETAIL = 0x03,
    CMD_GET_FUTURE_7DAY = 0x04,
    CMD_RESTART = 0x05,
    CMD_OTA_START = 0x11,
    CMD_FS_LIST = 0x20,
    CMD_FS_SELECT = 0x22,
    CMD_FS_SAVE = 0x30
} Weather_cmd_t;

typedef struct
{
    Weather_cmd_t type;
    char filename[64];
} Weather_msg_t;

typedef enum
{
    STATE_IDLE = 0,
    STATE_CMD,
    STATE_LEN_H,
    STATE_LEN_L,
    STATE_DATA,
    STATE_CRC,
    STATE_TAIL
} ProtocolState_t;

typedef enum
{
    WEATHER_SCENE_UNKNOWN = 0,
    WEATHER_SCENE_CLEAR,
    WEATHER_SCENE_CLOUDY,
    WEATHER_SCENE_LIGHT_RAIN,
    WEATHER_SCENE_MODERATE_RAIN,
    WEATHER_SCENE_HEAVY_RAIN,
    WEATHER_SCENE_SNOW
} WeatherScene_t;

typedef struct
{
    char date[16];
    char weather[16];
    int temp_high;
    int temp_low;
    int icon_id;
} WeatherDay_t;

typedef struct
{
    char text[32];
    int icon;
    int temp;
    int feelsLike;
    char windDir[32];
    int windScale;
    int vis;
    int humidity;
    int aqi;
    int pm10;
    int pm25;
    int no2;
    int so2;
    float co;
    int o3;
} WeatherData_t;

typedef struct
{
    int aqi;
    int pm10;
    int pm25;
    int no2;
    int so2;
    float co;
    int o3;
} AirQuality_t;

typedef struct
{
    uint8_t first_sync_done;
    uint8_t power_on;
    uint8_t syncing;
    uint8_t abort_requested;
    uint8_t time_synced;
} WeatherModule_t;

typedef struct
{
    WeatherData_t now;
    AirQuality_t air;
    WeatherDay_t future[7];
    TickType_t last_sync_tick;
    uint8_t valid;
    uint8_t stale;
} WeatherSnapshot_t;

#define WEATHER_SYNC_BIT_TIME   (1U << 0)
#define WEATHER_SYNC_BIT_NOW    (1U << 1)
#define WEATHER_SYNC_BIT_AIR    (1U << 2)
#define WEATHER_SYNC_BIT_FUTURE (1U << 3)
#define WEATHER_SYNC_BITS_ALL   (WEATHER_SYNC_BIT_NOW | WEATHER_SYNC_BIT_AIR | \
                                 WEATHER_SYNC_BIT_FUTURE)

extern uart_dma_t uart2_admin;
extern uint8_t u2_dma_buf[UART_Transmit_DMA_RX_SIZE];
extern uint8_t u2_rb_buf[UART_Transmit_LWRB_SIZE];

void process_protocol_data(uint8_t cmd, char *data);
uint8_t stm32_calc_crc8(uint8_t *ptr, uint16_t len);

void WeatherApp_Init(void);
void WeatherApp_GetSnapshot(WeatherSnapshot_t *out);
uint8_t WeatherApp_IsFirstSyncDone(void);
void WeatherApp_SetFirstSyncDone(uint8_t done);
uint8_t WeatherApp_IsSyncing(void);
void WeatherApp_SetSyncing(uint8_t syncing);
uint8_t WeatherApp_IsTimeSynced(void);
uint8_t WeatherApp_IsAbortRequested(void);
EventBits_t WeatherApp_WaitSyncBits(TickType_t timeout);
void WeatherApp_CommitSync(void);
void WeatherApp_MarkSyncFailed(void);
void Weather_PowerOn(void);
void Weather_PowerOff(void);
void Weather_RequestAbortForSleep(void);
uint8_t Weather_CanEnterStop(void);
void Weather_SendCommand(Weather_cmd_t cmd);
#if defined(WEATHER_DEMO_ENABLE) && WEATHER_DEMO_ENABLE
void Weather_FillDemoData(void);
#endif
void Weather_BeginSyncCycle(void);
uint8_t Weather_HasCompletedSync(void);
WeatherScene_t Weather_GetScene(void);
uint16_t Weather_GetDisplayIcon(void);
void Weather_DebugPrintAll(void);

#ifdef __cplusplus
}
#endif

#endif
