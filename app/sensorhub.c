#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "sensorhub.h"

LOG_MODULE_REGISTER(sensorhub, LOG_LEVEL_INF);

/* Queue */
K_MSGQ_DEFINE(sample_q, sizeof(struct sample_msg), SENSORHUB_QUEUE_CAPACITY, 4);

static atomic_t rate_hz = ATOMIC_INIT(SENSORHUB_RATE_DEFAULT_HZ);
static atomic_t idle_pct;

/*
 * The producer updates these while the logger and shell threads read or reset
 * them, so they are only touched with stats_lock held.
 */
static struct k_spinlock stats_lock;
static uint32_t q_high_watermark;
static uint32_t q_drops;

int sensorhub_enqueue(const struct sample_msg *msg)
{
    int dropped = 0;

    while (k_msgq_put(&sample_q, msg, K_NO_WAIT) != 0) {
        /* Queue full: discard the oldest sample to make room. The consumer may
         * have freed a slot in the meantime, so only count real discards.
         */
        struct sample_msg oldest;

        if (k_msgq_get(&sample_q, &oldest, K_NO_WAIT) == 0) {
            dropped++;
        }
    }

    uint32_t used = k_msgq_num_used_get(&sample_q);

    K_SPINLOCK(&stats_lock) {
        q_drops += dropped;
        if (used > q_high_watermark) {
            q_high_watermark = used;
        }
    }

    return dropped;
}

int sensorhub_dequeue(struct sample_msg *msg, k_timeout_t timeout)
{
    return k_msgq_get(&sample_q, msg, timeout);
}

void sensorhub_get_qstats(struct queue_stats *out)
{
    out->capacity = SENSORHUB_QUEUE_CAPACITY;
    out->depth = k_msgq_num_used_get(&sample_q);

    K_SPINLOCK(&stats_lock) {
        out->high_watermark = q_high_watermark;
        out->drops = q_drops;
    }
}

void sensorhub_reset_stats(void)
{
    /* Restart the high-watermark from the current depth, not zero, so it never
     * reads lower than what is in the queue right now.
     */
    uint32_t used = k_msgq_num_used_get(&sample_q);

    K_SPINLOCK(&stats_lock) {
        q_high_watermark = used;
        q_drops = 0;
    }
}

int sensorhub_set_rate(int hz)
{
    if (hz < SENSORHUB_RATE_MIN_HZ || hz > SENSORHUB_RATE_MAX_HZ) {
        return -EINVAL;
    }
    atomic_set(&rate_hz, hz);
    return 0;
}

int sensorhub_get_rate(void)
{
    return (int)atomic_get(&rate_hz);
}

void sensorhub_set_idle_pct(int pct)
{
    atomic_set(&idle_pct, CLAMP(pct, 0, 100));
}

int sensorhub_get_idle_pct(void)
{
    return (int)atomic_get(&idle_pct);
}

/* Producer thread */
static void sensor_thread(void *a, void *b, void *c)
{
    ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);
    int last_rate = -1;
    uint32_t seq_no = 0;
    int64_t next_us = k_ticks_to_us_floor64(k_uptime_ticks());

    while (1) {
        int rate = sensorhub_get_rate();
        if (rate != last_rate) {
            LOG_INF("sensor rate: %d Hz", rate);
            last_rate = rate;
        }

        struct sample_msg msg = {0};
        msg.seq = seq_no++;
        msg.ts_ms = k_uptime_get();

        vs_temp_read(&msg.temp);
        vs_imu_read(&msg.imu);

        sensorhub_enqueue(&msg);

        /* Sleep until the next sample is due. An absolute deadline in
         * microseconds keeps the average rate exact even when the period is
         * not a whole number of ticks. Skip the sleep when already late (even
         * a past deadline costs a full tick), and resync after a long stall
         * (e.g. a debugger pause) instead of bursting to catch up.
         */
        int64_t now_us = k_ticks_to_us_floor64(k_uptime_ticks());

        next_us += USEC_PER_SEC / rate;
        if (now_us - next_us > 100 * USEC_PER_MSEC) {
            next_us = now_us;
        }
        if (next_us > now_us) {
            k_sleep(K_TIMEOUT_ABS_US(next_us));
        }
    }
}

/* Consumer thread */
static void logger_thread(void *a, void *b, void *c)
{
    ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);
    int64_t last_print = k_uptime_get();
    uint32_t received = 0;

    while (1) {
        struct sample_msg msg;
        if (sensorhub_dequeue(&msg, K_MSEC(100)) == 0) {
            received++;
            mqtt_bridge_forward(&msg);
        }

        if (k_uptime_get() - last_print >= 1000) {
            struct queue_stats st;
            sensorhub_get_qstats(&st);
            LOG_INF("rate=%dHz rx=%u q=%u/%u hwm=%u drops=%u idle=%d%%",
                    sensorhub_get_rate(), received, st.depth, st.capacity,
                    st.high_watermark, st.drops, sensorhub_get_idle_pct());
            received = 0;
            last_print = k_uptime_get();
        }
    }
}

K_THREAD_STACK_DEFINE(sensor_stack, 2048);
K_THREAD_STACK_DEFINE(logger_stack, 2048);
static struct k_thread sensor_tid, logger_tid;

int sensorhub_start(void)
{
    static atomic_t started;

    if (atomic_set(&started, 1) != 0) {
        return -EALREADY;
    }

    k_thread_create(&sensor_tid, sensor_stack, K_THREAD_STACK_SIZEOF(sensor_stack),
                    sensor_thread, NULL, NULL, NULL, 5, 0, K_NO_WAIT);
    k_thread_name_set(&sensor_tid, "sensor");

    k_thread_create(&logger_tid, logger_stack, K_THREAD_STACK_SIZEOF(logger_stack),
                    logger_thread, NULL, NULL, NULL, 6, 0, K_NO_WAIT);
    k_thread_name_set(&logger_tid, "logger");

    return 0;
}
