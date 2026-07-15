#include "weather_app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "rtc.h"
#include "usart.h"

AirQuality_t g_air_detail = {0};
WeatherData_t g_now_weather = {0};
WeatherDay_t g_future_weather[7] = {0};
uart_dma_t uart2_admin = {0};
uint8_t u2_dma_buf[UART_Transmit_DMA_RX_SIZE] = {0};
uint8_t u2_rb_buf[UART_Transmit_LWRB_SIZE] = {0};

volatile WeatherModule_t g_weather_module = {0};
volatile uint8_t g_weather_persist_dirty = 0U;

static uint8_t s_weather_cycle_has_now = 0U;
static uint8_t s_weather_cycle_has_air = 0U;
static uint8_t s_weather_cycle_has_future = 0U;

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

static int Weather_GetEffectiveIcon(void)
{
    int icon = g_now_weather.icon;
    int fallback_icon = g_future_weather[0].icon_id;

    if (((icon <= 0) || (Weather_ClassifyIcon(icon) == WEATHER_SCENE_UNKNOWN)) &&
        (fallback_icon > 0))
    {
        icon = fallback_icon;
    }

    return icon;
}

static int extract_int(char *str)
{
    char *p = str;

    if (p == NULL)
    {
        return 0;
    }

    while ((*p != '\0') && ((*p < '0') || (*p > '9')) && (*p != '-'))
    {
        p++;
    }

    return (*p == '\0') ? 0 : atoi(p);
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

void Weather_BeginSyncCycle(void)
{
    s_weather_cycle_has_now = 0U;
    s_weather_cycle_has_air = 0U;
    s_weather_cycle_has_future = 0U;
}

uint8_t Weather_HasCompletedSync(void)
{
    return (uint8_t)((s_weather_cycle_has_now != 0U) &&
                     (s_weather_cycle_has_air != 0U) &&
                     (s_weather_cycle_has_future != 0U));
}

uint16_t Weather_GetDisplayIcon(void)
{
    int icon = Weather_GetEffectiveIcon();

    if (icon <= 0)
    {
        return 0U;
    }

    return (uint16_t)icon;
}

WeatherScene_t Weather_GetScene(void)
{
    return Weather_ClassifyIcon(Weather_GetEffectiveIcon());
}

void Weather_FillDemoData(void)
{
    strcpy(g_now_weather.text, "Rain");
    g_now_weather.icon = 307;
    g_now_weather.temp = 22;
    g_now_weather.feelsLike = 24;
    strcpy(g_now_weather.windDir, "East");
    g_now_weather.windScale = 0;
    g_now_weather.vis = 2;
    g_now_weather.humidity = 15;
    g_now_weather.aqi = 95;
    g_now_weather.pm10 = 22;
    g_now_weather.pm25 = 13;
    g_now_weather.no2 = 10;
    g_now_weather.so2 = 6;
    g_now_weather.co = 5.00f;
    g_now_weather.o3 = 0;

    g_air_detail.aqi = 22;
    g_air_detail.pm10 = 13;
    g_air_detail.pm25 = 10;
    g_air_detail.no2 = 6;
    g_air_detail.so2 = 5;
    g_air_detail.co = 0.00f;
    g_air_detail.o3 = 70;

    strcpy(g_future_weather[0].date, "2026-06-13");
    strcpy(g_future_weather[0].weather, "HeavyRain");
    g_future_weather[0].temp_high = 23;
    g_future_weather[0].temp_low = 20;
    g_future_weather[0].icon_id = 307;

    strcpy(g_future_weather[1].date, "2026-06-14");
    strcpy(g_future_weather[1].weather, "Cloudy");
    g_future_weather[1].temp_high = 32;
    g_future_weather[1].temp_low = 21;
    g_future_weather[1].icon_id = 101;

    strcpy(g_future_weather[2].date, "2026-06-15");
    strcpy(g_future_weather[2].weather, "Cloudy");
    g_future_weather[2].temp_high = 33;
    g_future_weather[2].temp_low = 23;
    g_future_weather[2].icon_id = 101;

    strcpy(g_future_weather[3].date, "2026-06-16");
    strcpy(g_future_weather[3].weather, "Rain");
    g_future_weather[3].temp_high = 34;
    g_future_weather[3].temp_low = 24;
    g_future_weather[3].icon_id = 305;

    strcpy(g_future_weather[4].date, "2026-06-17");
    strcpy(g_future_weather[4].weather, "Rain");
    g_future_weather[4].temp_high = 34;
    g_future_weather[4].temp_low = 25;
    g_future_weather[4].icon_id = 305;

    strcpy(g_future_weather[5].date, "2026-06-18");
    strcpy(g_future_weather[5].weather, "MidRain");
    g_future_weather[5].temp_high = 30;
    g_future_weather[5].temp_low = 25;
    g_future_weather[5].icon_id = 306;

    strcpy(g_future_weather[6].date, "2026-06-19");
    strcpy(g_future_weather[6].weather, "MidRain");
    g_future_weather[6].temp_high = 29;
    g_future_weather[6].temp_low = 25;
    g_future_weather[6].icon_id = 306;
}

uint8_t stm32_calc_crc8(uint8_t *ptr, uint16_t len)
{
    uint8_t crc = 0x00U;

    while (len-- != 0U)
    {
        crc ^= *ptr++;
        for (uint8_t i = 8U; i > 0U; --i)
        {
            if ((crc & 0x80U) != 0U)
            {
                crc = (uint8_t)((crc << 1) ^ 0x31U);
            }
            else
            {
                crc <<= 1;
            }
        }
    }

    return crc;
}

static void RTC_SyncFromString(const char *time_str)
{
    RTC_TimeTypeDef time = {0};
    RTC_DateTypeDef date = {0};
    int year;
    int month;
    int day;
    int hour;
    int min;
    int sec;

    if ((time_str == NULL) ||
        (sscanf(time_str, "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &min, &sec) != 6))
    {
        return;
    }

    date.Year = (uint8_t)(year - 2000);
    date.Month = (uint8_t)month;
    date.Date = (uint8_t)day;
    time.Hours = (uint8_t)hour;
    time.Minutes = (uint8_t)min;
    time.Seconds = (uint8_t)sec;

    (void)HAL_RTC_SetDate(&hrtc, &date, RTC_FORMAT_BIN);
    (void)HAL_RTC_SetTime(&hrtc, &time, RTC_FORMAT_BIN);
}

void process_protocol_data(uint8_t cmd, char *data)
{
    char *ptr;

    if (data == NULL)
    {
        return;
    }

    switch ((Weather_cmd_t)cmd)
    {
    case CMD_GET_TIME:
        RTC_SyncFromString(data);
        log_printf("[Weather] time sync");
        break;

    case CMD_GET_NOW_DETAIL:
        ptr = strtok(data, ",");
        if (ptr != NULL) copy_token(g_now_weather.text, sizeof(g_now_weather.text), ptr);

        ptr = strtok(NULL, ",");
        if (ptr != NULL) g_now_weather.icon = atoi(ptr);
        ptr = strtok(NULL, ",");
        if (ptr != NULL) g_now_weather.temp = atoi(ptr);
        ptr = strtok(NULL, ",");
        if (ptr != NULL) g_now_weather.feelsLike = extract_int(ptr);
        ptr = strtok(NULL, ",");
        if (ptr != NULL) copy_token(g_now_weather.windDir, sizeof(g_now_weather.windDir), ptr);
        ptr = strtok(NULL, ",");
        if (ptr != NULL) g_now_weather.vis = extract_int(ptr);
        ptr = strtok(NULL, ",");
        if (ptr != NULL) g_now_weather.humidity = atoi(ptr);
        ptr = strtok(NULL, ",");
        if (ptr != NULL) g_now_weather.aqi = atoi(ptr);
        ptr = strtok(NULL, ",");
        if (ptr != NULL) g_now_weather.pm10 = atoi(ptr);
        ptr = strtok(NULL, ",");
        if (ptr != NULL) g_now_weather.pm25 = atoi(ptr);
        ptr = strtok(NULL, ",");
        if (ptr != NULL) g_now_weather.no2 = atoi(ptr);
        ptr = strtok(NULL, ",");
        if (ptr != NULL) g_now_weather.so2 = atoi(ptr);
        ptr = strtok(NULL, ",");
        if (ptr != NULL) g_now_weather.co = (float)atof(ptr);
        ptr = strtok(NULL, ",");
        if (ptr != NULL) g_now_weather.o3 = atoi(ptr);

        s_weather_cycle_has_now = 1U;
        log_printf("[Weather] now ok");
        break;

    case CMD_GET_AIR_DETAIL:
        ptr = strtok(data, ",");
        if (ptr != NULL) g_air_detail.aqi = atoi(ptr);
        ptr = strtok(NULL, ",");
        if (ptr != NULL) g_air_detail.pm10 = atoi(ptr);
        ptr = strtok(NULL, ",");
        if (ptr != NULL) g_air_detail.pm25 = atoi(ptr);
        ptr = strtok(NULL, ",");
        if (ptr != NULL) g_air_detail.no2 = atoi(ptr);
        ptr = strtok(NULL, ",");
        if (ptr != NULL) g_air_detail.so2 = atoi(ptr);
        ptr = strtok(NULL, ",");
        if (ptr != NULL) g_air_detail.co = (float)atof(ptr);
        ptr = strtok(NULL, ",");
        if (ptr != NULL) g_air_detail.o3 = atoi(ptr);

        s_weather_cycle_has_air = 1U;
        log_printf("[Weather] air ok");
        break;

    case CMD_GET_FUTURE_7DAY:
        if (strcmp(data, "LIST_START") == 0)
        {
            log_printf("[Weather] list start");
        }
        else if (strcmp(data, "LIST_END") == 0)
        {
            s_weather_cycle_has_future = 1U;
            log_printf("[Weather] list done");
        }
        else
        {
            int idx = -1;

            ptr = strtok(data, "|");
            if (ptr != NULL)
            {
                idx = atoi(ptr);
            }

            if ((idx >= 0) && (idx < 7))
            {
                ptr = strtok(NULL, "|");
                if (ptr != NULL) copy_token(g_future_weather[idx].date, sizeof(g_future_weather[idx].date), ptr);
                ptr = strtok(NULL, "|");
                if (ptr != NULL) copy_token(g_future_weather[idx].weather, sizeof(g_future_weather[idx].weather), ptr);
                ptr = strtok(NULL, "|");
                if (ptr != NULL) g_future_weather[idx].temp_high = atoi(ptr);
                ptr = strtok(NULL, "|");
                if (ptr != NULL) g_future_weather[idx].temp_low = atoi(ptr);
                ptr = strtok(NULL, "|");
                if (ptr != NULL) g_future_weather[idx].icon_id = atoi(ptr);
            }
        }
        break;

    default:
        break;
    }
}

void Weather_PowerOn(void)
{
    if (g_weather_module.power_on == 0U)
    {
        HAL_GPIO_WritePin(ESP32_EN_GPIO_Port, ESP32_EN_Pin, GPIO_PIN_SET);
        g_weather_module.power_on = 1U;
        g_weather_module.abort_requested = 0U;
        log_printf("[Weather] power on");
    }
}

void Weather_PowerOff(void)
{
    if (g_weather_module.power_on != 0U)
    {
        HAL_GPIO_WritePin(ESP32_EN_GPIO_Port, ESP32_EN_Pin, GPIO_PIN_RESET);
        g_weather_module.power_on = 0U;
        log_printf("[Weather] power off");
    }
}

void Weather_RequestAbortForSleep(void)
{
    g_weather_module.abort_requested = 1U;
    Weather_PowerOff();
}

uint8_t Weather_CanEnterStop(void)
{
    return (uint8_t)((g_weather_module.syncing == 0U) &&
                     (g_weather_module.power_on == 0U));
}

void Weather_SendCommand(Weather_cmd_t cmd)
{
    uint8_t frame[3] = {FRAME_HEAD, (uint8_t)cmd, FRAME_TAIL};

    (void)HAL_UART_Transmit(&huart2, frame, sizeof(frame), 100U);
    log_printf("[Weather] tx 0x%02X", (unsigned int)cmd);
}

void Weather_DebugPrintAll(void)
{
    log_printf("[Weather] %s %dC aq=%d",
               g_now_weather.text,
               g_now_weather.temp,
               g_air_detail.aqi);
}
