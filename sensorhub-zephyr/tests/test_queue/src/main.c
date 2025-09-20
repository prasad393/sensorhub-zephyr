#include <zephyr/ztest.h>
#include "sensorhub.h"

extern void sensor_thread(void*, void*, void*);
extern void logger_thread(void*, void*, void*);

ZTEST(queue_suite, test_rate_set)
{
    sensorhub_set_rate(200);
    zassert_equal(sensorhub_get_rate(), 200, "rate not set");
}

ZTEST_SUITE(queue_suite, NULL, NULL, NULL, NULL, NULL);
