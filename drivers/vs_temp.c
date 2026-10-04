#include <zephyr/kernel.h>
#include <zephyr/random/random.h>
#include <math.h>
#include "sensorhub.h"

/* Simple virtual temperature: base + slow drift + random noise.
 * Both the sensor thread and the shell read it, so the state below is only
 * touched with the lock held.
 */
#define TWO_PI 6.28318531f

static struct k_spinlock lock;
static float avg;
static bool avg_valid;
static float drift_phase;

static float frand_unit(void)
{
    /* convert 32-bit to [0,1] */
    uint32_t r = sys_rand32_get();
    return (float)(r / (double)UINT32_MAX);
}

int vs_temp_read(struct temp_sample *out)
{
    float base = 24.5f;
    float noise = (frand_unit() - 0.5f) * 0.6f; /* ±0.3C */

    K_SPINLOCK(&lock) {
        /* keep the phase small so float rounding cannot stall it on long runs */
        drift_phase += 0.01f;
        if (drift_phase >= TWO_PI) {
            drift_phase -= TWO_PI;
        }
        float drift = 0.5f * sinf(drift_phase);

        float val = base + drift + noise;
        /* simple running average, seeded with the first reading */
        avg = avg_valid ? 0.95f * avg + 0.05f * val : val;
        avg_valid = true;

        out->celsius = val;
        out->avg = avg;
        out->drift = drift;
        out->noise = noise;
    }
    return 0;
}
