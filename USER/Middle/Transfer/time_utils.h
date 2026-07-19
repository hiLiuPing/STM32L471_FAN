#ifndef __TIME_UTILS_H__
#define __TIME_UTILS_H__

#include <stdbool.h>
#include <stdint.h>

/* Valid for deadlines less than INT32_MAX ticks into the future. */
static inline bool Time32_Reached(uint32_t now, uint32_t deadline)
{
    return (int32_t)(now - deadline) >= 0;
}

static inline bool Time32_Before(uint32_t now, uint32_t deadline)
{
    return (int32_t)(now - deadline) < 0;
}

static inline uint32_t Time32_Elapsed(uint32_t now, uint32_t since)
{
    return now - since;
}

static inline float Phase_AdvanceBounded(float phase, float delta, float period)
{
    phase += delta;
    while (phase >= period) phase -= period;
    while (phase < 0.0f) phase += period;
    return phase;
}

#endif /* __TIME_UTILS_H__ */
