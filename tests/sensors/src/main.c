#include <math.h>
#include <zephyr/ztest.h>
#include "sensorhub.h"

#define EPS 1e-5f

/*
 * All temperature checks live in one test: the driver keeps its moving average
 * between calls, and the first check needs a driver that has never been read.
 */
ZTEST(temp, test_temp_model)
{
    struct temp_sample t;
    float min_drift = 1.0f, max_drift = -1.0f;

    /* The moving average starts from the first reading instead of 0 C */
    zassert_ok(vs_temp_read(&t), "read fail");
    zassert_equal(t.avg, t.celsius, "average not seeded by the first reading");

    /* 24.5 C base, +-0.5 C drift, +-0.3 C noise; 1000 reads > one drift period */
    for (int i = 0; i < 1000; ++i) {
        zassert_ok(vs_temp_read(&t), "read fail");
        zassert_true(fabsf(t.noise) <= 0.3f + EPS, "noise %f out of range", (double)t.noise);
        zassert_true(fabsf(t.drift) <= 0.5f + EPS, "drift %f out of range", (double)t.drift);
        zassert_within(t.celsius, 24.5f + t.drift + t.noise, EPS, "celsius != base + drift + noise");
        zassert_true(t.avg > 23.7f && t.avg < 25.3f, "avg %f out of range", (double)t.avg);
        min_drift = MIN(min_drift, t.drift);
        max_drift = MAX(max_drift, t.drift);
    }
    zassert_true(min_drift < -0.45f && max_drift > 0.45f, "drift did not swing through a full cycle");
}

ZTEST(imu, test_imu_ranges)
{
    struct imu_sample s;

    for (int i = 0; i < 1000; ++i) {
        zassert_ok(vs_imu_read(&s), "read fail");
        /* stationary: ~0 g on X/Y, ~1 g on Z, small gyro bias */
        zassert_true(fabsf(s.ax) <= 0.05f + EPS && fabsf(s.ay) <= 0.05f + EPS, "accel x/y");
        zassert_within(s.az, 1.0f, 0.02f + EPS, "accel z");
        zassert_true(fabsf(s.gx) <= 0.5f + EPS && fabsf(s.gy) <= 0.5f + EPS &&
                     fabsf(s.gz) <= 0.5f + EPS, "gyro");
    }
}

ZTEST_SUITE(temp, NULL, NULL, NULL, NULL, NULL);
ZTEST_SUITE(imu, NULL, NULL, NULL, NULL, NULL);
