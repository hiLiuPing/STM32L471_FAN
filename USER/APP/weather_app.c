#include "weather_app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "protocol_crc8.h"
#include "rtc.h"
#include "usart.h"

uart_dma_t uart2_admin = {0};
uint8_t u2_dma_buf[UART_Transmit_DMA_RX_SIZE] = {0};
uint8_t u2_rb_buf[UART_Transmit_LWRB_SIZE] = {0};

static volatile WeatherModule_t s_weather_module = {0};
static WeatherSnapshot_t s_weather_published = {0};
static WeatherSnapshot_t s_weather_staging = {0};
static uint8_t s_weather_future_mask = 0U;
static uint8_t s_weather_future_list_started = 0U;
static StaticEventGroup_t s_weather_event_storage;
static EventGroupHandle_t s_weather_events = NULL;

#if WEATHER_DEMO_ENABLE
static uint8_t s_weather_demo_index = 0U;

typedef struct
{
    const char *name;
    const char *text;
    const char *forecast_text;
    int icon;
    int temp;
    int temp_high;
    int temp_low;
    int pm25;
    uint8_t hour;
    uint8_t minute;
} WeatherDemoData_t;

static const WeatherDemoData_t s_weather_demo_data[] = {
    /* Dynamic-theme keyframes plus the independent 07:00/19:00 day-state edges. */
    {"night",          "Clear", "Clear", 150, 24, 29, 21, 10,  0U,  0U},
    {"dawn-start",     "Clear", "Clear", 150, 24, 29, 21, 10,  6U,  0U},
    {"dawn-blend",     "Clear", "Clear", 150, 24, 29, 21, 10,  6U, 37U},
    {"day-edge-before","Clear", "Clear", 150, 24, 29, 21, 10,  6U, 59U},
    {"day-edge",       "Clear", "Clear", 100, 30, 34, 25, 12,  7U,  0U},
    {"dawn-peak",      "Clear", "Clear", 100, 30, 34, 25, 12,  7U, 15U},
    {"dawn-fade",      "Clear", "Clear", 100, 30, 34, 25, 12,  7U, 52U},
    {"dynamic-day",    "Clear", "Clear", 100, 30, 34, 25, 12,  8U, 30U},
    {"white-clouds",   "Clear", "Clear", 100, 30, 34, 25, 12,  9U,  0U},
    {"sunset-start",   "Clear", "Clear", 100, 30, 34, 25, 12, 17U,  0U},
    {"sunset-blend",   "Clear", "Clear", 100, 30, 34, 25, 12, 17U, 30U},
    {"sunset-peak",    "Clear", "Clear", 100, 30, 34, 25, 12, 18U,  0U},
    {"night-edge-before","Clear","Clear", 100, 30, 34, 25, 12, 18U, 59U},
    {"night-edge",     "Clear", "Clear", 150, 24, 29, 21, 10, 19U,  0U},
    {"night-stable",   "Clear", "Clear", 150, 24, 29, 21, 10, 19U, 30U},

    /* Every supported weather scene in both daytime and nighttime. */
    {"clear-day",      "Clear",     "Clear",     100, 30, 34, 25, 12, 10U, 0U},
    {"clear-night",    "Clear",     "Clear",     150, 24, 29, 21, 10, 22U, 0U},
    {"cloudy-day",     "Cloudy",    "Cloudy",    101, 27, 31, 23, 18, 10U, 0U},
    {"cloudy-night",   "Cloudy",    "Cloudy",    151, 22, 27, 19, 20, 22U, 0U},
    {"light-rain-day", "LightRain", "LightRain", 305, 25, 28, 22, 24, 10U, 0U},
    {"light-rain-night","LightRain", "LightRain", 309, 21, 25, 18, 26, 22U, 0U},
    {"mid-rain-day",   "MidRain",   "MidRain",   306, 24, 27, 20, 30, 10U, 0U},
    {"mid-rain-night", "MidRain",   "MidRain",   314, 20, 24, 17, 32, 22U, 0U},
    {"heavy-rain-day", "HeavyRain", "HeavyRain", 307, 22, 25, 19, 38, 10U, 0U},
    {"heavy-rain-night","HeavyRain", "HeavyRain", 310, 19, 23, 16, 42, 22U, 0U},
    {"snow-day",       "Snow",      "Snow",      400, -2,  1, -6, 15, 10U, 0U},
    {"snow-night",     "Snow",      "Snow",      401, -5, -1, -9, 16, 22U, 0U},
};
#endif

static WeatherScene_t Weather_ClassifyIcon(int icon)
{
    if ((icon == 100) || (icon == 150))
    {
        return WEATHER_SCENE_CLEAR;
    }
    if (((icon >= 101) && (icon <= 104)) ||
        ((icon >= 151) && (icon <= 153)))
    {
        return WEATHER_SCENE_CLOUDY;
    }
    if ((icon == 305) || (icon == 309))
    {
        return WEATHER_SCENE_LIGHT_RAIN;
    }
    if ((icon == 306) || (icon == 314))
    {
        return WEATHER_SCENE_MODERATE_RAIN;
    }
    if (((icon >= 300) && (icon <= 304)) ||
        ((icon >= 307) && (icon <= 313)) ||
        ((icon >= 315) && (icon <= 318)) ||
        ((icon >= 350) && (icon <= 351)) ||
        (icon == 399))
    {
        return WEATHER_SCENE_HEAVY_RAIN;
    }
    if (((icon >= 400) && (icon <= 410)) ||
        ((icon >= 456) && (icon <= 457)) ||
        (icon == 499))
    {
        return WEATHER_SCENE_SNOW;
    }

    return WEATHER_SCENE_UNKNOWN;
}

static int Weather_GetEffectiveIcon(const WeatherSnapshot_t *snapshot)
{
    int icon;
    int fallback_icon;

    if ((snapshot == NULL) || (snapshot->valid == 0U))
    {
        return 0;
    }
    icon = snapshot->now.icon;
    fallback_icon = snapshot->future[0].icon_id;

    /*
     * Scene classification and image availability are independent.  Keep any
     * positive current-condition code so ui_weather_icon_get() can apply its
     * shared-resource ranges (for example fog/dust codes 500-515).  Falling
     * back merely because a scene is unknown can replace valid current rain
     * with today's forecast icon, which is often clear weather.
     */
    if ((icon <= 0) && (fallback_icon > 0))
    {
        icon = fallback_icon;
    }

    return icon;
}

static void copy_token(char *dst, size_t dst_len, const char *src)
{
    if ((dst == NULL) || (dst_len == 0U) || (src == NULL))
    {
        return;
    }

    strncpy(dst, src, dst_len - 1U);
    dst[dst_len - 1U] = '\0';
}

void WeatherApp_Init(void)
{
    taskENTER_CRITICAL();
    memset((void *)&s_weather_module, 0, sizeof(s_weather_module));
    memset(&s_weather_published, 0, sizeof(s_weather_published));
    memset(&s_weather_staging, 0, sizeof(s_weather_staging));
    s_weather_future_mask = 0U;
    s_weather_future_list_started = 0U;
    taskEXIT_CRITICAL();
    s_weather_events = xEventGroupCreateStatic(&s_weather_event_storage);
}

void WeatherApp_GetSnapshot(WeatherSnapshot_t *out)
{
    if (out == NULL)
    {
        return;
    }
    taskENTER_CRITICAL();
    *out = s_weather_published;
    taskEXIT_CRITICAL();
}

uint8_t WeatherApp_IsFirstSyncDone(void)
{
    return s_weather_module.first_sync_done;
}

void WeatherApp_SetFirstSyncDone(uint8_t done)
{
    s_weather_module.first_sync_done = (done != 0U) ? 1U : 0U;
}

uint8_t WeatherApp_IsSyncing(void)
{
    return s_weather_module.syncing;
}

void WeatherApp_SetSyncing(uint8_t syncing)
{
    s_weather_module.syncing = (syncing != 0U) ? 1U : 0U;
}

uint8_t WeatherApp_IsTimeSynced(void)
{
    return s_weather_module.time_synced;
}

uint8_t WeatherApp_IsAbortRequested(void)
{
    return s_weather_module.abort_requested;
}

EventBits_t WeatherApp_WaitSyncBits(TickType_t timeout)
{
    if (s_weather_events == NULL)
    {
        return 0U;
    }
    return xEventGroupWaitBits(s_weather_events,
                               WEATHER_SYNC_BITS_ALL,
                               pdFALSE,
                               pdTRUE,
                               timeout);
}

void WeatherApp_CommitSync(void)
{
    TickType_t now;

    if (!Weather_HasCompletedSync())
    {
        return;
    }
    now = xTaskGetTickCount();
    taskENTER_CRITICAL();
    s_weather_staging.valid = 1U;
    s_weather_staging.stale = 0U;
    s_weather_staging.last_sync_tick = now;
    s_weather_published = s_weather_staging;
    taskEXIT_CRITICAL();
}

void WeatherApp_MarkSyncFailed(void)
{
    taskENTER_CRITICAL();
    if (s_weather_published.valid != 0U)
    {
        s_weather_published.stale = 1U;
    }
    taskEXIT_CRITICAL();
}

void Weather_BeginSyncCycle(void)
{
    taskENTER_CRITICAL();
    memset(&s_weather_staging, 0, sizeof(s_weather_staging));
    s_weather_future_mask = 0U;
    s_weather_future_list_started = 0U;
    taskEXIT_CRITICAL();
    if (s_weather_events != NULL)
    {
        (void)xEventGroupClearBits(s_weather_events,
                                   WEATHER_SYNC_BITS_ALL | WEATHER_SYNC_BIT_TIME);
    }
}

uint8_t Weather_HasCompletedSync(void)
{
    EventBits_t bits = (s_weather_events != NULL) ?
                           xEventGroupGetBits(s_weather_events) : 0U;
    return (uint8_t)((bits & WEATHER_SYNC_BITS_ALL) == WEATHER_SYNC_BITS_ALL);
}

uint16_t Weather_GetDisplayIcon(void)
{
    WeatherSnapshot_t snapshot;
    int icon;

    WeatherApp_GetSnapshot(&snapshot);
    icon = Weather_GetEffectiveIcon(&snapshot);

    if (icon <= 0)
    {
        return 0U;
    }

    return (uint16_t)icon;
}

WeatherScene_t Weather_GetScene(void)
{
    WeatherSnapshot_t snapshot;

    WeatherApp_GetSnapshot(&snapshot);
    return Weather_ClassifyIcon(Weather_GetEffectiveIcon(&snapshot));
}

#if WEATHER_DEMO_ENABLE
void Weather_FillDemoData(void)
{
    const WeatherDemoData_t *demo = &s_weather_demo_data[s_weather_demo_index];
    WeatherSnapshot_t snapshot = {0};
    RTC_TimeTypeDef time = {0};
    RTC_DateTypeDef date = {0};

    copy_token(snapshot.now.text, sizeof(snapshot.now.text), demo->text);
    snapshot.now.icon = demo->icon;
    snapshot.now.temp = demo->temp;
    snapshot.now.feelsLike = demo->temp;
    copy_token(snapshot.now.windDir, sizeof(snapshot.now.windDir), "East");
    snapshot.now.windScale = 2;
    snapshot.now.vis = 10;
    snapshot.now.humidity = 65;
    snapshot.now.aqi = demo->pm25 + 20;
    snapshot.now.pm10 = demo->pm25 + 8;
    snapshot.now.pm25 = demo->pm25;
    snapshot.now.no2 = 10;
    snapshot.now.so2 = 6;
    snapshot.now.co = 0.50f;
    snapshot.now.o3 = 70;

    snapshot.air.aqi = snapshot.now.aqi;
    snapshot.air.pm10 = snapshot.now.pm10;
    snapshot.air.pm25 = demo->pm25;
    snapshot.air.no2 = snapshot.now.no2;
    snapshot.air.so2 = snapshot.now.so2;
    snapshot.air.co = snapshot.now.co;
    snapshot.air.o3 = snapshot.now.o3;

    copy_token(snapshot.future[0].date, sizeof(snapshot.future[0].date), "2026-07-15");
    copy_token(snapshot.future[0].weather, sizeof(snapshot.future[0].weather), demo->forecast_text);
    snapshot.future[0].temp_high = demo->temp_high;
    snapshot.future[0].temp_low = demo->temp_low;
    snapshot.future[0].icon_id = demo->icon;

    strcpy(snapshot.future[1].date, "2026-07-16");
    strcpy(snapshot.future[1].weather, "Cloudy");
    snapshot.future[1].temp_high = 32;
    snapshot.future[1].temp_low = 21;
    snapshot.future[1].icon_id = 101;

    strcpy(snapshot.future[2].date, "2026-07-17");
    strcpy(snapshot.future[2].weather, "Cloudy");
    snapshot.future[2].temp_high = 33;
    snapshot.future[2].temp_low = 23;
    snapshot.future[2].icon_id = 101;

    strcpy(snapshot.future[3].date, "2026-07-18");
    strcpy(snapshot.future[3].weather, "Rain");
    snapshot.future[3].temp_high = 34;
    snapshot.future[3].temp_low = 24;
    snapshot.future[3].icon_id = 305;

    strcpy(snapshot.future[4].date, "2026-07-19");
    strcpy(snapshot.future[4].weather, "Rain");
    snapshot.future[4].temp_high = 34;
    snapshot.future[4].temp_low = 25;
    snapshot.future[4].icon_id = 305;

    strcpy(snapshot.future[5].date, "2026-07-20");
    strcpy(snapshot.future[5].weather, "MidRain");
    snapshot.future[5].temp_high = 30;
    snapshot.future[5].temp_low = 25;
    snapshot.future[5].icon_id = 306;

    strcpy(snapshot.future[6].date, "2026-07-21");
    strcpy(snapshot.future[6].weather, "Snow");
    snapshot.future[6].temp_high = 1;
    snapshot.future[6].temp_low = -6;
    snapshot.future[6].icon_id = 400;
    snapshot.valid = 1U;
    snapshot.last_sync_tick = xTaskGetTickCount();
    taskENTER_CRITICAL();
    s_weather_published = snapshot;
    taskEXIT_CRITICAL();

    date.Year = 26U;
    date.Month = RTC_MONTH_JULY;
    date.Date = 15U;
    date.WeekDay = RTC_WEEKDAY_WEDNESDAY;
    time.Hours = demo->hour;
    time.Minutes = demo->minute;
    time.Seconds = 0U;
    time.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
    time.StoreOperation = RTC_STOREOPERATION_RESET;
    if ((HAL_RTC_SetDate(&hrtc, &date, RTC_FORMAT_BIN) == HAL_OK) &&
        (HAL_RTC_SetTime(&hrtc, &time, RTC_FORMAT_BIN) == HAL_OK))
    {
        s_weather_module.time_synced = 1U;
    }

    log_printf("[WeatherDemo] %u/%u %s scene=%s icon=%d %02u:%02u",
               (unsigned int)(s_weather_demo_index + 1U),
               (unsigned int)(sizeof(s_weather_demo_data) / sizeof(s_weather_demo_data[0])),
               demo->name,
               demo->text,
               demo->icon,
               (unsigned int)demo->hour,
               (unsigned int)demo->minute);

    s_weather_demo_index++;
    if (s_weather_demo_index >= (sizeof(s_weather_demo_data) / sizeof(s_weather_demo_data[0])))
    {
        s_weather_demo_index = 0U;
    }
}
#endif

uint8_t stm32_calc_crc8(uint8_t *ptr, uint16_t len)
{
    return Protocol_Crc8(ptr, len);
}

static uint8_t RTC_IsLeapYear(int year)
{
    return (uint8_t)(((year % 4 == 0) && (year % 100 != 0)) ||
                     (year % 400 == 0));
}

static uint8_t RTC_GetDaysInMonth(int year, int month)
{
    static const uint8_t days_in_month[] = {
        31U, 28U, 31U, 30U, 31U, 30U,
        31U, 31U, 30U, 31U, 30U, 31U,
    };
    uint8_t days;

    if ((month < 1) || (month > 12))
    {
        return 0U;
    }

    days = days_in_month[month - 1];
    if ((month == 2) && (RTC_IsLeapYear(year) != 0U))
    {
        days++;
    }
    return days;
}

static uint8_t RTC_GetWeekDay(int year, int month, int day)
{
    static const uint8_t month_offset[] = {
        0U, 3U, 2U, 5U, 0U, 3U,
        5U, 1U, 4U, 6U, 2U, 4U,
    };
    int sunday_based;

    if (month < 3)
    {
        year--;
    }
    sunday_based = (year + year / 4 - year / 100 + year / 400 +
                    month_offset[month - 1] + day) % 7;
    return (sunday_based == 0) ? RTC_WEEKDAY_SUNDAY : (uint8_t)sunday_based;
}

static uint8_t RTC_SyncFromString(const char *time_str)
{
    RTC_TimeTypeDef time = {0};
    RTC_DateTypeDef date = {0};
    int year;
    int month;
    int day;
    int hour;
    int min;
    int sec;
    char tail;

    if ((time_str == NULL) ||
        (sscanf(time_str, "%d-%d-%d %d:%d:%d%c",
                &year, &month, &day, &hour, &min, &sec, &tail) != 6) ||
        (year < 2000) || (year > 2099) ||
        (month < 1) || (month > 12) ||
        (day < 1) || (day > RTC_GetDaysInMonth(year, month)) ||
        (hour < 0) || (hour > 23) ||
        (min < 0) || (min > 59) ||
        (sec < 0) || (sec > 59))
    {
        return 0U;
    }

    date.Year = (uint8_t)(year - 2000);
    date.Month = (uint8_t)month;
    date.Date = (uint8_t)day;
    date.WeekDay = RTC_GetWeekDay(year, month, day);
    time.Hours = (uint8_t)hour;
    time.Minutes = (uint8_t)min;
    time.Seconds = (uint8_t)sec;
    time.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
    time.StoreOperation = RTC_STOREOPERATION_RESET;

    if (HAL_RTC_SetDate(&hrtc, &date, RTC_FORMAT_BIN) != HAL_OK)
    {
        return 0U;
    }
    if (HAL_RTC_SetTime(&hrtc, &time, RTC_FORMAT_BIN) != HAL_OK)
    {
        return 0U;
    }
    return 1U;
}

static int Weather_ExtractInt(char *str)
{
    char *ptr = str;

    if (ptr == NULL)
    {
        return 0;
    }

    while ((*ptr != '\0') && ((*ptr < '0') || (*ptr > '9')) && (*ptr != '-'))
    {
        ptr++;
    }

    return (*ptr == '\0') ? 0 : atoi(ptr);
}

static uint8_t Weather_ParseNow(char *data, WeatherData_t *out)
{
    char *ptr;

    if ((data == NULL) || (out == NULL)) return 0U;
    memset(out, 0, sizeof(*out));

    ptr = strtok(data, ",");
    if (ptr != NULL) copy_token(out->text, sizeof(out->text), ptr);
    ptr = strtok(NULL, ",");
    if (ptr != NULL) out->icon = atoi(ptr);
    ptr = strtok(NULL, ",");
    if (ptr != NULL) out->temp = atoi(ptr);
    ptr = strtok(NULL, ",");
    if (ptr != NULL) out->feelsLike = Weather_ExtractInt(ptr);
    ptr = strtok(NULL, ",");
    if (ptr != NULL) copy_token(out->windDir, sizeof(out->windDir), ptr);
    ptr = strtok(NULL, ",");
    if (ptr != NULL) out->vis = Weather_ExtractInt(ptr);
    ptr = strtok(NULL, ",");
    if (ptr != NULL) out->humidity = atoi(ptr);
    ptr = strtok(NULL, ",");
    if (ptr != NULL) out->aqi = atoi(ptr);
    ptr = strtok(NULL, ",");
    if (ptr != NULL) out->pm10 = atoi(ptr);
    ptr = strtok(NULL, ",");
    if (ptr != NULL) out->pm25 = atoi(ptr);
    ptr = strtok(NULL, ",");
    if (ptr != NULL) out->no2 = atoi(ptr);
    ptr = strtok(NULL, ",");
    if (ptr != NULL) out->so2 = atoi(ptr);
    ptr = strtok(NULL, ",");
    if (ptr != NULL) out->co = (float)atof(ptr);
    ptr = strtok(NULL, ",");
    if (ptr != NULL) out->o3 = atoi(ptr);
    return 1U;
}

static uint8_t Weather_ParseAir(char *data, AirQuality_t *out)
{
    char *ptr;

    if ((data == NULL) || (out == NULL)) return 0U;
    memset(out, 0, sizeof(*out));

    ptr = strtok(data, ",");
    if (ptr != NULL) out->aqi = atoi(ptr);
    ptr = strtok(NULL, ",");
    if (ptr != NULL) out->pm10 = atoi(ptr);
    ptr = strtok(NULL, ",");
    if (ptr != NULL) out->pm25 = atoi(ptr);
    ptr = strtok(NULL, ",");
    if (ptr != NULL) out->no2 = atoi(ptr);
    ptr = strtok(NULL, ",");
    if (ptr != NULL) out->so2 = atoi(ptr);
    ptr = strtok(NULL, ",");
    if (ptr != NULL) out->co = (float)atof(ptr);
    ptr = strtok(NULL, ",");
    if (ptr != NULL) out->o3 = atoi(ptr);
    return 1U;
}

static uint8_t Weather_ParseFuture(char *data, uint8_t *index, WeatherDay_t *out)
{
    char *ptr;
    int parsed_index = -1;

    if ((data == NULL) || (index == NULL) || (out == NULL)) return 0U;
    memset(out, 0, sizeof(*out));

    ptr = strtok(data, "|");
    if (ptr != NULL) parsed_index = atoi(ptr);
    if ((parsed_index < 0) || (parsed_index >= 7)) return 0U;

    ptr = strtok(NULL, "|");
    if (ptr != NULL) copy_token(out->date, sizeof(out->date), ptr);
    ptr = strtok(NULL, "|");
    if (ptr != NULL) copy_token(out->weather, sizeof(out->weather), ptr);
    ptr = strtok(NULL, "|");
    if (ptr != NULL) out->temp_high = atoi(ptr);
    ptr = strtok(NULL, "|");
    if (ptr != NULL) out->temp_low = atoi(ptr);
    ptr = strtok(NULL, "|");
    if (ptr != NULL) out->icon_id = atoi(ptr);
    *index = (uint8_t)parsed_index;
    return 1U;
}

void process_protocol_data(uint8_t cmd, char *data)
{
    if (data == NULL)
    {
        return;
    }

    switch ((Weather_cmd_t)cmd)
    {
    case CMD_GET_TIME:
        if (RTC_SyncFromString(data))
        {
            s_weather_module.time_synced = 1U;
            (void)xEventGroupSetBits(s_weather_events, WEATHER_SYNC_BIT_TIME);
            log_printf("[Weather] time ok");
        }
        else log_printf("[Weather] time invalid");
        break;

    case CMD_GET_NOW_DETAIL:
        {
            WeatherData_t parsed;
            if (Weather_ParseNow(data, &parsed))
            {
                taskENTER_CRITICAL();
                s_weather_staging.now = parsed;
                taskEXIT_CRITICAL();
                (void)xEventGroupSetBits(s_weather_events, WEATHER_SYNC_BIT_NOW);
                log_printf("[Weather] now ok");
            }
            else log_printf("[Weather] now invalid");
        }
        break;

    case CMD_GET_AIR_DETAIL:
        {
            AirQuality_t parsed;
            if (Weather_ParseAir(data, &parsed))
            {
                taskENTER_CRITICAL();
                s_weather_staging.air = parsed;
                taskEXIT_CRITICAL();
                (void)xEventGroupSetBits(s_weather_events, WEATHER_SYNC_BIT_AIR);
                log_printf("[Weather] air ok");
            }
            else log_printf("[Weather] air invalid");
        }
        break;

    case CMD_GET_FUTURE_7DAY:
        if (strcmp(data, "LIST_START") == 0)
        {
            s_weather_future_list_started = 1U;
            log_printf("[Weather] list start");
        }
        else if (strcmp(data, "LIST_END") == 0)
        {
            (void)xEventGroupSetBits(s_weather_events, WEATHER_SYNC_BIT_FUTURE);
            log_printf("[Weather] list done");
        }
        else
        {
            WeatherDay_t parsed;
            uint8_t idx;
            if (Weather_ParseFuture(data, &idx, &parsed))
            {
                taskENTER_CRITICAL();
                s_weather_staging.future[idx] = parsed;
                s_weather_future_mask |= (uint8_t)(1U << idx);
                taskEXIT_CRITICAL();
            }
            else log_printf("[Weather] list row invalid");
        }
        break;

    default:
        break;
    }
}

void Weather_PowerOn(void)
{
    if (s_weather_module.power_on == 0U)
    {
        HAL_GPIO_WritePin(ESP32_EN_GPIO_Port, ESP32_EN_Pin, GPIO_PIN_SET);
        s_weather_module.power_on = 1U;
        s_weather_module.abort_requested = 0U;
        log_printf("[Weather] power on");
    }
}

void Weather_PowerOff(void)
{
    if (s_weather_module.power_on != 0U)
    {
        HAL_GPIO_WritePin(ESP32_EN_GPIO_Port, ESP32_EN_Pin, GPIO_PIN_RESET);
        s_weather_module.power_on = 0U;
        log_printf("[Weather] power off");
    }
}

void Weather_RequestAbortForSleep(void)
{
    s_weather_module.abort_requested = 1U;
    Weather_PowerOff();
}

uint8_t Weather_CanEnterStop(void)
{
    return (uint8_t)((s_weather_module.syncing == 0U) &&
                     (s_weather_module.power_on == 0U));
}

void Weather_SendCommand(Weather_cmd_t cmd)
{
    uint8_t frame[3] = {FRAME_HEAD, (uint8_t)cmd, FRAME_TAIL};

    (void)HAL_UART_Transmit(&huart2, frame, sizeof(frame), 100U);
    log_printf("[Weather] tx 0x%02X", (unsigned int)cmd);
}

void Weather_DebugPrintAll(void)
{
    WeatherSnapshot_t snapshot;

    WeatherApp_GetSnapshot(&snapshot);
    log_printf("[Weather] %s %dC aq=%d",
               snapshot.now.text,
               snapshot.now.temp,
               snapshot.air.aqi);
}
