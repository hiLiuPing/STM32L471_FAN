#include "shake_detector.h"

#include <math.h>
#include <string.h>

#define SHAKE_DYNAMIC_HIGH_G       0.75f
#define SHAKE_DYNAMIC_QUIET_G      0.12f


#define SHAKE_SEQUENCE_WINDOW_MS   1500U
#define SHAKE_MIN_PEAK_GAP_MS      80U
#define SHAKE_REARM_QUIET_MS       300U
#define SHAKE_GRAVITY_ALPHA        0.08f
#define SHAKE_REVERSE_DOT_LIMIT    (-0.45f)

static uint32_t ShakeDetector_Elapsed(uint32_t now, uint32_t since)
{
    return (uint32_t)(now - since);
}

void ShakeDetector_Init(ShakeDetector_t *detector)
{
    if (detector != NULL)
    {
        memset(detector, 0, sizeof(*detector));
    }
}

bool ShakeDetector_Update(ShakeDetector_t *detector,
                          const ShakeDetectorSample_t *sample,
                          uint32_t now_ms)
{
    float ax;
    float ay;
    float az;
    float lx;
    float ly;
    float lz;
    float magnitude_sq;
    float magnitude;

    if ((detector == NULL) || (sample == NULL))
    {
        return false;
    }

    ax = (float)sample->x * 0.001f;
    ay = (float)sample->y * 0.001f;
    az = (float)sample->z * 0.001f;

    if (detector->initialized == 0U)
    {
        detector->gravity_x = ax;
        detector->gravity_y = ay;
        detector->gravity_z = az;
        detector->initialized = 1U;
        detector->quiet_started_ms = now_ms;
        detector->quiet_tracking = 1U;
        return false;
    }

    detector->gravity_x += SHAKE_GRAVITY_ALPHA * (ax - detector->gravity_x);
    detector->gravity_y += SHAKE_GRAVITY_ALPHA * (ay - detector->gravity_y);
    detector->gravity_z += SHAKE_GRAVITY_ALPHA * (az - detector->gravity_z);

    lx = ax - detector->gravity_x;
    ly = ay - detector->gravity_y;
    lz = az - detector->gravity_z;
    magnitude_sq = (lx * lx) + (ly * ly) + (lz * lz);
    magnitude = sqrtf(magnitude_sq);

    if (detector->locked != 0U)
    {
        if (magnitude <= SHAKE_DYNAMIC_QUIET_G)
        {
            if (detector->quiet_tracking == 0U)
            {
                detector->quiet_started_ms = now_ms;
                detector->quiet_tracking = 1U;
            }
            else if (ShakeDetector_Elapsed(now_ms, detector->quiet_started_ms) >=
                     SHAKE_REARM_QUIET_MS)
            {
                detector->locked = 0U;
                detector->quiet_tracking = 0U;
                detector->peak_count = 0U;
                detector->last_peak_magnitude = 0.0f;
            }
        }
        else
        {
            detector->quiet_tracking = 0U;
        }
        return false;
    }

    if (detector->peak_count != 0U &&
        ShakeDetector_Elapsed(now_ms, detector->sequence_started_ms) >
            SHAKE_SEQUENCE_WINDOW_MS)
    {
        detector->peak_count = 0U;
        detector->last_peak_magnitude = 0.0f;
    }

    if (magnitude < SHAKE_DYNAMIC_HIGH_G)
    {
        return false;
    }

    if ((detector->peak_count != 0U) &&
        (ShakeDetector_Elapsed(now_ms, detector->last_peak_ms) <
         SHAKE_MIN_PEAK_GAP_MS))
    {
        return false;
    }

    if (detector->peak_count == 0U)
    {
        detector->peak_count = 1U;
        detector->sequence_started_ms = now_ms;
        detector->last_peak_ms = now_ms;
        detector->last_peak_x = lx;
        detector->last_peak_y = ly;
        detector->last_peak_z = lz;
        detector->last_peak_magnitude = magnitude;
        return false;
    }

    {
        float last_magnitude = detector->last_peak_magnitude;
        float dot = (lx * detector->last_peak_x) +
                    (ly * detector->last_peak_y) +
                    (lz * detector->last_peak_z);
        float normalized_dot = dot / (magnitude * last_magnitude);

        if (normalized_dot > SHAKE_REVERSE_DOT_LIMIT)
        {
            if (magnitude > last_magnitude)
            {
                detector->last_peak_x = lx;
                detector->last_peak_y = ly;
                detector->last_peak_z = lz;
                detector->last_peak_magnitude = magnitude;
            }
            return false;
        }
    }

    detector->peak_count++;
    detector->last_peak_ms = now_ms;
    detector->last_peak_x = lx;
    detector->last_peak_y = ly;
    detector->last_peak_z = lz;
    detector->last_peak_magnitude = magnitude;

    if (detector->peak_count >= 7U)
    {
        detector->locked = 1U;
        detector->quiet_tracking = 0U;
        detector->peak_count = 0U;
        return true;
    }

    return false;
}
