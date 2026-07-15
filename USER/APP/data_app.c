#include "data_app.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "rtc.h"
#include "task.h"
#include "weather_app.h"

#define DATA_APP_TIME_BUF_NUM 2U
#define DATA_APP_HOME_STATUS_BUF_NUM 2U

TiltKey_t current_raw_direction = MSG_TILT_NONE;

static app_time_t s_time_buf[DATA_APP_TIME_BUF_NUM];
static volatile uint8_t s_time_write_idx = 0U;
static volatile uint8_t s_time_read_idx = 1U;
static uint8_t s_colon = 1U;
static DataApp_HomeStatus_t s_home_status_buf[DATA_APP_HOME_STATUS_BUF_NUM];
static volatile uint8_t s_home_status_read_idx = 0U;
static uint32_t s_home_status_version = 0U;

static uint8_t DataApp_Weekday(uint16_t year, uint8_t month, uint8_t day)
{
    static const uint8_t offsets[] = { 0U, 3U, 2U, 5U, 0U, 3U, 5U, 1U, 4U, 6U, 2U, 4U };
    uint16_t y = year;

    if ((year < 2000U) || (month < 1U) || (month > 12U) || (day < 1U) || (day > 31U))
    {
        return 0U;
    }

    if (month < 3U)
    {
        y--;
    }

    return (uint8_t)((y + (y / 4U) - (y / 100U) + (y / 400U) + offsets[month - 1U] + day) % 7U);
}

static int DataApp_FloatToX10(float value)
{
    if (value >= 0.0f)
    {
        return (int)((value * 10.0f) + 0.5f);
    }

    return (int)((value * 10.0f) - 0.5f);
}

static int DataApp_HumidityToInt(float humidity)
{
    if (humidity < 0.0f)
    {
        return 0;
    }
    if (humidity > 100.0f)
    {
        return 100;
    }
    return (int)(humidity + 0.5f);
}

static void DataApp_FormatEnv(char *buf, uint32_t buf_size)
{
    int temp_x10;
    int temp_abs;

    if ((buf == NULL) || (buf_size == 0U))
    {
        return;
    }

    temp_x10 = DataApp_FloatToX10(g_sensors_environment.temp);
    temp_abs = (temp_x10 < 0) ? -temp_x10 : temp_x10;
    (void)snprintf(buf,
                   buf_size,
                   "%s%d.%dC %d%%",
                   (temp_x10 < 0) ? "-" : "",
                   temp_abs / 10,
                   temp_abs % 10,
                   DataApp_HumidityToInt(g_sensors_environment.hum));
}

static uint8_t DataApp_HomeStatusEquals(const DataApp_HomeStatus_t *a, const DataApp_HomeStatus_t *b)
{
    if ((a == NULL) || (b == NULL))
    {
        return 0U;
    }

    return (uint8_t)((a->weather_icon_id == b->weather_icon_id) &&
                     (a->weather_scene == b->weather_scene) &&
                     (a->is_day == b->is_day) &&
                     (strcmp(a->time_text, b->time_text) == 0) &&
                     (strcmp(a->date_text, b->date_text) == 0) &&
                     (strcmp(a->week_text, b->week_text) == 0) &&
                     (strcmp(a->temp_range_text, b->temp_range_text) == 0) &&
                     (strcmp(a->pm25_text, b->pm25_text) == 0) &&
                     (strcmp(a->env_text, b->env_text) == 0));
}

void Time_Init(void)
{
    memset(s_time_buf, 0, sizeof(s_time_buf));
    s_time_write_idx = 0U;
    s_time_read_idx = 1U;
    s_colon = 1U;
}

void Time_BlinkUpdate(void)
{
    s_colon = (uint8_t)!s_colon;
}

void RTC_ReadToBuffer(void)
{
    RTC_TimeTypeDef time = {0};
    RTC_DateTypeDef date = {0};
    app_time_t *dst = &s_time_buf[s_time_write_idx];

    (void)HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN);
    (void)HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN);

    dst->year = (uint16_t)(2000U + date.Year);
    dst->month = date.Month;
    dst->date = date.Date;
    dst->hour = time.Hours;
    dst->min = time.Minutes;
    dst->sec = time.Seconds;
}

void Buffer_Swap(void)
{
    uint8_t tmp = s_time_write_idx;

    s_time_write_idx = s_time_read_idx;
    s_time_read_idx = tmp;
}

void Time_Get(app_time_t *out)
{
    if (out != NULL)
    {
        *out = s_time_buf[s_time_read_idx];
    }
}

uint8_t Time_GetColon(void)
{
    return s_colon;
}

uint8_t Time_IsDaytime(void)
{
    app_time_t now;

    Time_Get(&now);
    if (now.year < 2020U)
    {
        return 1U;
    }

    return (uint8_t)((now.hour >= 7U) && (now.hour < 19U));
}

void Time_Format(char *out)
{
    app_time_t t;

    if (out == NULL)
    {
        return;
    }

    Time_Get(&t);
    if (Time_GetColon() != 0U)
    {
        (void)sprintf(out, "%02u:%02u", t.hour, t.min);
    }
    else
    {
        (void)sprintf(out, "%02u %02u", t.hour, t.min);
    }
}

void DataApp_HomeStatus_Update(void)
{
    static const char *const week_text[] = {
        "\345\221\250\346\227\245",
        "\345\221\250\344\270\200",
        "\345\221\250\344\272\214",
        "\345\221\250\344\270\211",
        "\345\221\250\345\233\233",
        "\345\221\250\344\272\224",
        "\345\221\250\345\205\255",
    };
    DataApp_HomeStatus_t next;
    const DataApp_HomeStatus_t *current = &s_home_status_buf[s_home_status_read_idx];
    const WeatherDay_t *today = &g_future_weather[0];
    app_time_t t;
    uint8_t weekday;
    uint8_t write_idx;

    memset(&next, 0, sizeof(next));
    Time_Get(&t);
    weekday = DataApp_Weekday(t.year, t.month, t.date);

    (void)snprintf(next.time_text, sizeof(next.time_text), "%02u:%02u", t.hour, t.min);
    (void)snprintf(next.date_text, sizeof(next.date_text), "%u\346\234\210%u\346\227\245", t.month, t.date);
    (void)snprintf(next.week_text, sizeof(next.week_text), "%s", week_text[weekday]);
    (void)snprintf(next.temp_range_text, sizeof(next.temp_range_text), "%d~%d\302\260C", today->temp_low, today->temp_high);
    (void)snprintf(next.pm25_text, sizeof(next.pm25_text), "PM2.5 %d", g_air_detail.pm25);
    DataApp_FormatEnv(next.env_text, sizeof(next.env_text));
    next.weather_icon_id = Weather_GetDisplayIcon();
    next.weather_scene = (uint8_t)Weather_GetScene();
    next.is_day = Time_IsDaytime();

    if (!DataApp_HomeStatusEquals(&next, current))
    {
        s_home_status_version++;
    }
    next.version = s_home_status_version;

    write_idx = (uint8_t)((s_home_status_read_idx + 1U) % DATA_APP_HOME_STATUS_BUF_NUM);
    s_home_status_buf[write_idx] = next;
    s_home_status_read_idx = write_idx;
}

void DataApp_HomeStatus_Get(DataApp_HomeStatus_t *out)
{
    if (out != NULL)
    {
        *out = s_home_status_buf[s_home_status_read_idx];
    }
}

void DataApp_Init(void)
{
    Time_Init();
    memset(s_home_status_buf, 0, sizeof(s_home_status_buf));
    s_home_status_read_idx = 0U;
    s_home_status_version = 0U;
    RTC_ReadToBuffer();
    Buffer_Swap();
    DataApp_HomeStatus_Update();
}

void DataApp_QuoteInvalidate(void)
{
}

void DataApp_QuoteServiceUpdate(TickType_t now)
{
    (void)now;
}

uint8_t DataApp_QuoteShowNext(DataApp_QuotePopupRequest_t *out)
{
    (void)out;
    return 0U;
}

uint8_t DataApp_QuotePopup_CopyFrame(DataApp_QuotePopupFrame_t *out)
{
    if (out != NULL)
    {
        memset(out, 0, sizeof(*out));
    }
    return 0U;
}

uint8_t DataApp_QuotePopup_PeekPending(DataApp_QuotePopupRequest_t *out)
{
    (void)out;
    return 0U;
}

void DataApp_QuotePopup_CommitPending(TickType_t now)
{
    (void)now;
}

TiltKey_t TiltKey_Update(motion_module_t *motion)
{
    enum
    {
        TILT_SHAKE_AXIS_NONE = 0,
        TILT_SHAKE_AXIS_HORIZONTAL,
        TILT_SHAKE_AXIS_VERTICAL
    };

    const TickType_t shake_window_ticks = pdMS_TO_TICKS(2000U);
    const TickType_t shake_lock_ticks = pdMS_TO_TICKS(600U);
    const TickType_t single_tilt_delay_ticks = pdMS_TO_TICKS(400U);
    uint8_t buf;
    int16_t raw_x;
    int16_t raw_y;
    int16_t raw_z;
    float ax;
    float ay;
    float az;
    float pitch;
    float roll;
    static float fx = 0.0f;
    static float fy = 0.0f;
    static float fz = 0.0f;
    static TiltKey_t last_stable_key = MSG_TILT_NONE;
    static uint8_t stable_cnt = 0U;
    static TiltKey_t output_locked_key = MSG_TILT_NONE;
    static TiltKey_t last_shake_horizontal_key = MSG_TILT_NONE;
    static TiltKey_t last_shake_vertical_key = MSG_TILT_NONE;
    static uint8_t horizontal_shake_cnt = 0U;
    static uint8_t vertical_shake_cnt = 0U;
    static TickType_t horizontal_shake_start = 0U;
    static TickType_t vertical_shake_start = 0U;
    static TickType_t horizontal_shake_locked_until = 0U;
    static TickType_t vertical_shake_locked_until = 0U;
    static TiltKey_t pending_output_key = MSG_TILT_NONE;
    static TickType_t pending_output_start = 0U;

    if (motion == NULL)
    {
        return MSG_TILT_NONE;
    }

    buf = motion->buf_idx;
    raw_x = motion->x[buf];
    raw_y = motion->y[buf];
    raw_z = motion->z[buf];

    ax = raw_x * 0.001f;
    ay = raw_y * 0.001f;
    az = raw_z * 0.001f;

    fx = (fx * 0.7f) + (ax * 0.3f);
    fy = (fy * 0.7f) + (ay * 0.3f);
    fz = (fz * 0.7f) + (az * 0.3f);

    pitch = atan2f(fx, sqrtf((fy * fy) + (fz * fz))) * RAD2DEG;
    roll = atan2f(fy, sqrtf((fx * fx) + (fz * fz))) * RAD2DEG;

    current_raw_direction = MSG_TILT_NONE;
    if (pitch > ANGLE_TH)
    {
        current_raw_direction = MSG_TILT_LEFT;
    }
    else if (pitch < -ANGLE_TH)
    {
        current_raw_direction = MSG_TILT_RIGHT;
    }
    else if (roll > ANGLE_TH)
    {
        current_raw_direction = MSG_TILT_UP;
    }
    else if (roll < -ANGLE_TH)
    {
        current_raw_direction = MSG_TILT_DOWN;
    }

    if (current_raw_direction == last_stable_key)
    {
        if (stable_cnt < HOLD_COUNT)
        {
            stable_cnt++;
        }
    }
    else
    {
        stable_cnt = 0U;
        last_stable_key = current_raw_direction;
    }

    if ((stable_cnt >= HOLD_COUNT) && (current_raw_direction != MSG_TILT_NONE))
    {
        TickType_t now = xTaskGetTickCount();
        uint8_t axis = TILT_SHAKE_AXIS_NONE;
        TiltKey_t shake_event = MSG_TILT_NONE;

        if ((current_raw_direction == MSG_TILT_LEFT) || (current_raw_direction == MSG_TILT_RIGHT))
        {
            axis = TILT_SHAKE_AXIS_HORIZONTAL;
        }
        else if ((current_raw_direction == MSG_TILT_UP) || (current_raw_direction == MSG_TILT_DOWN))
        {
            axis = TILT_SHAKE_AXIS_VERTICAL;
        }

        if (axis == TILT_SHAKE_AXIS_HORIZONTAL)
        {
            if ((horizontal_shake_locked_until != 0U) &&
                ((TickType_t)(now - horizontal_shake_locked_until) < shake_lock_ticks))
            {
                return MSG_TILT_NONE;
            }

            if (last_shake_horizontal_key == MSG_TILT_NONE)
            {
                last_shake_horizontal_key = current_raw_direction;
                horizontal_shake_cnt = 1U;
                horizontal_shake_start = now;
            }
            else if (current_raw_direction != last_shake_horizontal_key)
            {
                if ((TickType_t)(now - horizontal_shake_start) > shake_window_ticks)
                {
                    horizontal_shake_cnt = 1U;
                    horizontal_shake_start = now;
                }
                else if (horizontal_shake_cnt < 255U)
                {
                    horizontal_shake_cnt++;
                }

                last_shake_horizontal_key = current_raw_direction;
                if (horizontal_shake_cnt >= 3U)
                {
                    shake_event = MSG_TILT_SHAKE_HORIZONTAL;
                    horizontal_shake_cnt = 0U;
                    last_shake_horizontal_key = MSG_TILT_NONE;
                    horizontal_shake_start = now;
                    horizontal_shake_locked_until = now;
                }
            }
        }
        else if (axis == TILT_SHAKE_AXIS_VERTICAL)
        {
            if ((vertical_shake_locked_until != 0U) &&
                ((TickType_t)(now - vertical_shake_locked_until) < shake_lock_ticks))
            {
                return MSG_TILT_NONE;
            }

            if (last_shake_vertical_key == MSG_TILT_NONE)
            {
                last_shake_vertical_key = current_raw_direction;
                vertical_shake_cnt = 1U;
                vertical_shake_start = now;
            }
            else if (current_raw_direction != last_shake_vertical_key)
            {
                if ((TickType_t)(now - vertical_shake_start) > shake_window_ticks)
                {
                    vertical_shake_cnt = 1U;
                    vertical_shake_start = now;
                }
                else if (vertical_shake_cnt < 255U)
                {
                    vertical_shake_cnt++;
                }

                last_shake_vertical_key = current_raw_direction;
                if (vertical_shake_cnt >= 3U)
                {
                    shake_event = MSG_TILT_SHAKE_VERTICAL;
                    vertical_shake_cnt = 0U;
                    last_shake_vertical_key = MSG_TILT_NONE;
                    vertical_shake_start = now;
                    vertical_shake_locked_until = now;
                }
            }
        }

        if (shake_event != MSG_TILT_NONE)
        {
            output_locked_key = shake_event;
            pending_output_key = MSG_TILT_NONE;
            return shake_event;
        }

        if (output_locked_key == MSG_TILT_NONE)
        {
            if (pending_output_key == MSG_TILT_NONE)
            {
                pending_output_key = current_raw_direction;
                pending_output_start = now;
            }
            else if (pending_output_key != current_raw_direction)
            {
                pending_output_key = MSG_TILT_NONE;
                pending_output_start = now;
            }
            else if ((TickType_t)(now - pending_output_start) >= single_tilt_delay_ticks)
            {
                output_locked_key = current_raw_direction;
                pending_output_key = MSG_TILT_NONE;
                return current_raw_direction;
            }
        }
    }

    if ((fabsf(pitch) < ANGLE_HYST) && (fabsf(roll) < ANGLE_HYST))
    {
        output_locked_key = MSG_TILT_NONE;
        last_shake_horizontal_key = MSG_TILT_NONE;
        last_shake_vertical_key = MSG_TILT_NONE;
        pending_output_key = MSG_TILT_NONE;
    }

    return MSG_TILT_NONE;
}

TiltKey_t FallDetect_Check(motion_module_t *motion)
{
    uint8_t buf;
    float ax;
    float ay;
    float az;
    float acc_mag;
    static float fx = 0.0f;
    static float fy = 0.0f;
    static float fz = 0.0f;
    static uint8_t fall_cnt = 0U;
    static uint16_t init_cnt = 0U;
    static uint8_t fall_trig_lock = 0U;

    if (motion == NULL)
    {
        return MSG_TILT_NONE;
    }

    buf = motion->buf_idx;
    ax = motion->x[buf] * 0.001f;
    ay = motion->y[buf] * 0.001f;
    az = motion->z[buf] * 0.001f;

    fx = (fx * 0.7f) + (ax * 0.3f);
    fy = (fy * 0.7f) + (ay * 0.3f);
    fz = (fz * 0.7f) + (az * 0.3f);
    acc_mag = sqrtf((fx * fx) + (fy * fy) + (fz * fz));

    if (init_cnt < 20U)
    {
        init_cnt++;
        fall_cnt = 0U;
        fall_trig_lock = 0U;
        return MSG_TILT_NONE;
    }

    if ((acc_mag < 0.3f) || (acc_mag > 1.5f))
    {
        fall_cnt = 0U;
        return MSG_TILT_NONE;
    }

    if (acc_mag < FALL_THRESHOLD)
    {
        fall_cnt++;
        if ((fall_cnt >= FALL_TRIG_CNT) && (fall_trig_lock == 0U))
        {
            fall_trig_lock = 1U;
            fall_cnt = 0U;
            return MSG_FALL_DOWN;
        }
    }
    else
    {
        fall_cnt = 0U;
        fall_trig_lock = 0U;
    }

    return MSG_TILT_NONE;
}
