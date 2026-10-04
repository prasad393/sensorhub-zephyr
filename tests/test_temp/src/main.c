#include <zephyr/ztest.h>
#include "sensorhub.h"

ZTEST(temp_suite, test_temp_noise_bounds)
{
    struct temp_sample t;
    float last_avg = 0.0f;
    for (int i = 0; i < 200; ++i) {
        zassert_ok(vs_temp_read(&t), "read fail");
        zassert_true(t.celsius > 20.0f && t.celsius < 30.0f, "temp out of bounds");
        last_avg = t.avg;
    }
    zassert_true(last_avg > 20.0f && last_avg < 30.0f, "avg out of bounds");
}

ZTEST_SUITE(temp_suite, NULL, NULL, NULL, NULL, NULL);
