#ifndef __DATA_APP_H
#define __DATA_APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "FreeRTOS.h"
#include "sensors_app.h"

#define RAD2DEG         57.29578f
#define ANGLE_TH        25.0f
#define ANGLE_HYST      20.0f
#define HOLD_COUNT      5U

#define FALL_THRESHOLD  0.2f
#define FALL_TRIG_CNT   5U

#define DATA_APP_QUOTE_DEFAULT_PERIOD_MS 50000U
#define DATA_APP_QUOTE_TEXT_LEN          96U
#define DATA_APP_HOME_TEXT_LEN           24U

typedef struct
{
    uint16_t year;
    uint8_t month;
    uint8_t date;
    uint8_t hour;
    uint8_t min;
    uint8_t sec;
} app_time_t;

typedef struct
{
    char time_text[8];
    char date_text[DATA_APP_HOME_TEXT_LEN];
    char week_text[DATA_APP_HOME_TEXT_LEN];
    char temp_range_text[DATA_APP_HOME_TEXT_LEN];
    char pm25_text[DATA_APP_HOME_TEXT_LEN];
    char env_text[DATA_APP_HOME_TEXT_LEN];
    uint16_t weather_icon_id;
    uint32_t version;
} DataApp_HomeStatus_t;

typedef enum
{
    MSG_TILT_NONE = 0,
    MSG_TILT_UP,
    MSG_TILT_DOWN,
    MSG_TILT_LEFT,
    MSG_TILT_RIGHT,
    MSG_BLUETOOTH_CONNECT,
    MSG_BLUETOOTH_DISCONNECT,
    MSG_MUSIC_PLAY,
    MSG_MUSIC_STOP,
    MSG_PWER_IN,
    MSG_PWER_OUT,
    MSG_PWER_CHAGNE,
    MSG_PWER_FULL,
    MSG_FALL_DOWN,
    MSG_TILT_SHAKE_VERTICAL,
    MSG_TILT_SHAKE_HORIZONTAL,
} TiltKey_t;

typedef struct
{
    char text[DATA_APP_QUOTE_TEXT_LEN];
    uint32_t duration_ms;
} DataApp_QuotePopupRequest_t;

typedef struct
{
    char text[DATA_APP_QUOTE_TEXT_LEN];
    uint8_t valid;
} DataApp_QuotePopupFrame_t;

extern TiltKey_t current_raw_direction;

void DataApp_Init(void);
void Time_Init(void);
void Time_Get(app_time_t *out);
uint8_t Time_GetColon(void);
void Time_BlinkUpdate(void);
void RTC_ReadToBuffer(void);
void Buffer_Swap(void);
void Time_Format(char *out);
void DataApp_HomeStatus_Update(void);
void DataApp_HomeStatus_Get(DataApp_HomeStatus_t *out);

void DataApp_QuoteInvalidate(void);
void DataApp_QuoteServiceUpdate(TickType_t now);
uint8_t DataApp_QuoteShowNext(DataApp_QuotePopupRequest_t *out);
uint8_t DataApp_QuotePopup_CopyFrame(DataApp_QuotePopupFrame_t *out);
uint8_t DataApp_QuotePopup_PeekPending(DataApp_QuotePopupRequest_t *out);
void DataApp_QuotePopup_CommitPending(TickType_t now);

TiltKey_t TiltKey_Update(motion_module_t *motion);
TiltKey_t FallDetect_Check(motion_module_t *motion);

#ifdef __cplusplus
}
#endif

#endif
