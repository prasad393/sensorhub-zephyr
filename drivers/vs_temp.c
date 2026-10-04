#include <zephyr/kernel.h>
#include <zephyr/random/random.h>
#include <math.h>
#include "sensorhub.h"

/* Simple virtual temperature: base + slow drift + random noise */
static float avg;
static float drift_phase;

static float frand_unit(void)
{
    /* convert 32-bit to [0,1) */
    uint32_t r = sys_rand32_get();
    return (float)(r / (double)UINT32_MAX);
}

int vs_temp_read(struct temp_sample *out)
{
    float base = 24.5f;
    drift_phase += 0.01f;
    float drift = 0.5f * sinf(drift_phase);
    float noise = (frand_unit() - 0.5f) * 0.6f; /* ±0.3C */

    float val = base + drift + noise;
    /* simple running average */
    avg = 0.95f * avg + 0.05f * val;

    out->celsius = val;
    out->avg = avg;
    out->drift = drift;
    out->noise = noise;
    return 0;
}
