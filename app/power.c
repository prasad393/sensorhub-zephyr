#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "sensorhub.h"

LOG_MODULE_REGISTER(sensorhub_power, LOG_LEVEL_INF);

/* Coarse idle estimate: a background thread sleeps and marks idle time. */
static void idle_estimator(void *a, void *b, void *c)
{
    ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);
    const int window_ms = 1000;
    while (1) {
        int64_t t0 = k_uptime_get();
        /* simulate 'busy' sampling; in reality, just sleep small slices */
        int busy_ms = 0;
        for (int i = 0; i < 10; ++i) {
            k_busy_wait(200); /* ~0.2 ms busy * 10 = ~2 ms busy per second */
            busy_ms += 1;     /* small fudge to avoid 0% busy */
        }
        int64_t t1 = k_uptime_get();
        int elapsed = (int)(t1 - t0);
        if (elapsed <= 0) elapsed = 1;
        int idle_ms = window_ms - busy_ms;
        if (idle_ms < 0) idle_ms = 0;
        int pct = (idle_ms * 100) / window_ms;
        atomic_set(&g_ctrl.idle_pct, pct);
        k_msleep(MAX(0, window_ms - elapsed));
    }
}

K_THREAD_STACK_DEFINE(idle_stack, 1024);
static struct k_thread idle_tid;

SYS_INIT(init_power, APPLICATION, 90);
int init_power(const struct device *dev)
{
    ARG_UNUSED(dev);
    k_thread_create(&idle_tid, idle_stack, K_THREAD_STACK_SIZEOF(idle_stack),
                    idle_estimator, NULL, NULL, NULL, 7, 0, K_NO_WAIT);
    k_thread_name_set(&idle_tid, "idle_est");
    return 0;
}
