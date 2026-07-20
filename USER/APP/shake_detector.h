#ifndef __SHAKE_DETECTOR_H__
#define __SHAKE_DETECTOR_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    int16_t x;
    int16_t y;
    int16_t z;
} ShakeDetectorSample_t;

typedef struct
{
    float gravity_x;
    float gravity_y;
    float gravity_z;
    float last_peak_x;
    float last_peak_y;
    float last_peak_z;
    float last_peak_magnitude;
    uint32_t sequence_started_ms;
    uint32_t last_peak_ms;
    uint32_t quiet_started_ms;
    uint8_t peak_count;
    uint8_t initialized;
    uint8_t locked;
    uint8_t quiet_tracking;
} ShakeDetector_t;

void ShakeDetector_Init(ShakeDetector_t *detector);
bool ShakeDetector_Update(ShakeDetector_t *detector,
                          const ShakeDetectorSample_t *sample,
                          uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* __SHAKE_DETECTOR_H__ */
