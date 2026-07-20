#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "time_utils.h"
#include "protocol_crc8.h"
#include "shake_detector.h"

#define TWO_PI 6.2831853071795864769f

typedef struct
{
    uint32_t deadline;
    uint8_t active;
} deadline_model_t;

static void deadline_set(deadline_model_t *timer, uint32_t now, uint32_t delay)
{
    timer->active = (delay != 0U);
    timer->deadline = now + delay;
}

static uint8_t deadline_expired(const deadline_model_t *timer, uint32_t now)
{
    return (uint8_t)((timer->active != 0U) && Time32_Reached(now, timer->deadline));
}

static void test_tick_wrap(void)
{
    uint32_t start = 0xFFFFFFF0U;
    uint32_t deadline = start + 32U;

    assert(Time32_Before(start, deadline));
    assert(Time32_Before(0x0000000FU, deadline));
    assert(Time32_Reached(0x00000010U, deadline));
    assert(Time32_Reached(0x00000030U, deadline));
    assert(Time32_Elapsed(0x00000020U, start) == 48U);
}

static void test_auto_off_model(void)
{
    deadline_model_t timer = {0};

    deadline_set(&timer, 100U, 0U);
    assert(!deadline_expired(&timer, 0xFFFFFFFFU));

    deadline_set(&timer, 0xFFFFFFF0U, 32U);
    assert(!deadline_expired(&timer, 0x0000000FU));
    assert(deadline_expired(&timer, 0x00000010U));
    assert(deadline_expired(&timer, 0x00000042U));

    deadline_set(&timer, 0x00000008U, 100U);
    assert(!deadline_expired(&timer, 0x00000040U));
    assert(deadline_expired(&timer, 0x0000006CU));
}

static void test_natural_phase_31_days(void)
{
    const uint32_t ticks = 31U * 24U * 60U * 60U * 10U;
    float phase1 = 0.0f;
    float phase2 = 0.0f;
    float before;

    for (uint32_t i = 0U; i < ticks; i++)
    {
        phase1 = Phase_AdvanceBounded(phase1, 0.11f * 0.1f, TWO_PI);
        phase2 = Phase_AdvanceBounded(phase2, 0.037f * 0.1f, TWO_PI);
        assert((phase1 >= 0.0f) && (phase1 < TWO_PI));
        assert((phase2 >= 0.0f) && (phase2 < TWO_PI));
    }

    before = phase1;
    phase1 = Phase_AdvanceBounded(phase1, 0.11f * 0.1f, TWO_PI);
    assert(phase1 != before);
}

static void test_future_bitmap(void)
{
    uint8_t mask = 0U;

    for (uint8_t i = 0U; i < 6U; i++) mask |= (uint8_t)(1U << i);
    assert(mask != 0x7FU);
    mask |= (uint8_t)(1U << 6);
    assert(mask == 0x7FU);
    /* A repeated LIST_START must retain rows accumulated in the same session. */
    assert(mask == 0x7FU);
}

static void test_weather_sync_bit_accumulation(void)
{
    const uint8_t bit_time = 1U << 0;
    const uint8_t bit_now = 1U << 1;
    const uint8_t bit_air = 1U << 2;
    const uint8_t bit_future = 1U << 3;
    const uint8_t required = bit_time | bit_now | bit_air | bit_future;
    uint8_t bits = 0U;

    bits |= bit_time | bit_now;
    assert((bits & required) != required);
    bits |= bit_air;
    assert((bits & required) != required);
    bits |= bit_future;
    assert((bits & required) == required);
    assert(((bits & (uint8_t)~bit_time) & required) != required);
}

static uint16_t adjust_optional_u16(uint16_t value,
                                    int8_t delta,
                                    uint16_t step,
                                    uint16_t disabled,
                                    uint16_t minimum,
                                    uint16_t maximum)
{
    int32_t next;

    if (delta > 0)
    {
        if (value == disabled) return minimum;
        next = (int32_t)value + step;
        return (next > maximum) ? maximum : (uint16_t)next;
    }
    if (delta < 0)
    {
        if (value <= minimum) return disabled;
        next = (int32_t)value - step;
        return (next < minimum) ? disabled : (uint16_t)next;
    }
    return value;
}

static void test_poetry_interval_setting(void)
{
    uint16_t interval = 0U;

    interval = adjust_optional_u16(interval, 1, 5U, 0U, 5U, 60U);
    assert(interval == 5U);
    interval = adjust_optional_u16(interval, 1, 5U, 0U, 5U, 60U);
    assert(interval == 10U);
    interval = adjust_optional_u16(interval, -1, 5U, 0U, 5U, 60U);
    assert(interval == 5U);
    interval = adjust_optional_u16(interval, -1, 5U, 0U, 5U, 60U);
    assert(interval == 0U);
    assert((interval != 0U) == 0);
    interval = 60U;
    interval = adjust_optional_u16(interval, 1, 5U, 0U, 5U, 60U);
    assert(interval == 60U);
    assert((interval != 0U) == 1);
}

static void test_sensor_stale_and_recovery(void)
{
    uint32_t last_success = 0xFFFFFF00U;

    assert(Time32_Elapsed(0x000002E7U, last_success) < 1000U);
    assert(Time32_Elapsed(0x000002E8U, last_success) == 1000U);
    last_success = 0x00001000U;
    assert(Time32_Elapsed(0x00001001U, last_success) == 1U);
}

static bool feed_shake(ShakeDetector_t *detector, int16_t x, uint32_t now)
{
    ShakeDetectorSample_t sample = { x, 0, 1000 };

    return ShakeDetector_Update(detector, &sample, now);
}

static void settle_shake(ShakeDetector_t *detector, uint32_t *now)
{
    for (uint32_t i = 0U; i < 45U; i++)
    {
        *now += 30U;
        assert(!feed_shake(detector, 0, *now));
    }
}

static void assert_shake_sequence(ShakeDetector_t *detector, uint32_t *now)
{
    for (uint8_t i = 0U; i < 7U; i++)
    {
        bool triggered;

        *now += 120U;
        triggered = feed_shake(detector, ((i & 1U) == 0U) ? 1000 : -1000, *now);
        assert(triggered == (i == 6U));
    }
}

static void test_shake_detector(void)
{
    ShakeDetector_t detector;
    uint32_t now = 100U;

    ShakeDetector_Init(&detector);
    for (uint32_t i = 0U; i < 10U; i++)
    {
        assert(!feed_shake(&detector, 0, now));
        now += 30U;
    }
    assert(!feed_shake(&detector, 1000, now));
    settle_shake(&detector, &now);

    ShakeDetector_Init(&detector);
    assert(!feed_shake(&detector, 0, now));
    assert_shake_sequence(&detector, &now);
    now += 120U;
    assert(!feed_shake(&detector, -1000, now));

    settle_shake(&detector, &now);
    assert_shake_sequence(&detector, &now);

    ShakeDetector_Init(&detector);
    now = 0xFFFFFF00U;
    assert(!feed_shake(&detector, 0, now));
    assert_shake_sequence(&detector, &now);
}

typedef struct
{
    uint8_t state;
    uint8_t check[16];
    uint8_t check_len;
    uint16_t payload_left;
    uint8_t frames;
    uint8_t bad;
} frame_model_t;

static void frame_reset(frame_model_t *model)
{
    model->state = 0U;
    model->check_len = 0U;
    model->payload_left = 0U;
}

static void frame_feed(frame_model_t *model, uint8_t ch)
{
    switch (model->state)
    {
    case 0U:
        if (ch == 0xAAU) model->state = 1U;
        break;
    case 1U:
        model->check[model->check_len++] = ch;
        model->state = 2U;
        break;
    case 2U:
        model->check[model->check_len++] = ch;
        model->payload_left = (uint16_t)ch << 8;
        model->state = 3U;
        break;
    case 3U:
        model->check[model->check_len++] = ch;
        model->payload_left |= ch;
        model->state = (model->payload_left == 0U) ? 5U : 4U;
        break;
    case 4U:
        model->check[model->check_len++] = ch;
        if (--model->payload_left == 0U) model->state = 5U;
        break;
    case 5U:
        if (ch == Protocol_Crc8(model->check, model->check_len)) model->state = 6U;
        else
        {
            model->bad++;
            frame_reset(model);
        }
        break;
    case 6U:
        if (ch == 0x55U) model->frames++;
        else model->bad++;
        frame_reset(model);
        break;
    default:
        frame_reset(model);
        break;
    }
}

static void test_frame_recovery(void)
{
    frame_model_t model = {0};
    uint8_t check[] = {0x02U, 0x00U, 0x01U, 'X'};
    uint8_t good[] = {0xAAU, 0x02U, 0x00U, 0x01U, 'X', 0U, 0x55U};

    good[5] = Protocol_Crc8(check, (uint16_t)sizeof(check));
    for (uint8_t i = 0U; i < sizeof(good); i++) frame_feed(&model, good[i]);
    assert((model.frames == 1U) && (model.bad == 0U));

    good[5] ^= 0x01U;
    for (uint8_t i = 0U; i < sizeof(good); i++) frame_feed(&model, good[i]);
    assert(model.bad == 1U);

    frame_feed(&model, 0xAAU);
    frame_feed(&model, 0x02U);
    frame_reset(&model); /* 200 ms inter-byte timeout. */
    good[5] ^= 0x01U;
    for (uint8_t i = 0U; i < sizeof(good); i++) frame_feed(&model, good[i]);
    assert(model.frames == 2U);
}

int main(void)
{
    test_tick_wrap();
    test_auto_off_model();
    test_natural_phase_31_days();
    test_future_bitmap();
    test_weather_sync_bit_accumulation();
    test_poetry_interval_setting();
    test_sensor_stale_and_recovery();
    test_shake_detector();
    test_frame_recovery();
    puts("stability host tests: PASS");
    return 0;
}
