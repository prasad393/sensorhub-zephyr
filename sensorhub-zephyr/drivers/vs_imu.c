#include <zephyr/kernel.h>
#include <zephyr/random/random.h>
#include "sensorhub.h"

static float frand_range(float lo, float hi)
{
    uint32_t r = sys_rand32_get();
    float u = (float)(r / (double)UINT32_MAX);
    return lo + (hi - lo) * u;
}

int vs_imu_read(struct imu_sample *out)
{
    /* stationary with small jitter; gravity on Z ~1g */
    out->ax = frand_range(-0.05f, 0.05f);
    out->ay = frand_range(-0.05f, 0.05f);
    out->az = 1.0f + frand_range(-0.02f, 0.02f);

    /* gyro small bias */
    out->gx = frand_range(-0.5f, 0.5f);
    out->gy = frand_range(-0.5f, 0.5f);
    out->gz = frand_range(-0.5f, 0.5f);
    return 0;
}
