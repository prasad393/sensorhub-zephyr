#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/random.h>
#include "sensorhub.h"

LOG_MODULE_REGISTER(sensorhub, LOG_LEVEL_INF);

/* Queue */
#define MSGQ_CAPACITY 16
K_MSGQ_DEFINE(sample_q, sizeof(struct sample_msg), MSGQ_CAPACITY, 4);

struct sensorhub_ctrl g_ctrl = {
    .rate_hz = ATOMIC_INIT(100),
    .idle_pct = ATOMIC_INIT(0),
};

static uint16_t q_high_watermark;
static uint32_t q_drops;
static uint32_t seq_no;

/* Prototypes from drivers */
int vs_temp_read(struct temp_sample *out);
int vs_imu_read(struct imu_sample *out);

/* Producer thread */
void sensor_thread(void *a, void *b, void *c)
{
    ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);
    int last_rate = -1;
    int64_t next = k_uptime_get();

    while (1) {
        int rate = sensorhub_get_rate();
        if (rate < 10) rate = 10;
        if (rate > 500) rate = 500;
        if (rate != last_rate) {
            LOG_INF("sensor rate: %d Hz", rate);
            last_rate = rate;
        }

        struct sample_msg msg = {0};
        msg.seq = seq_no++;
        msg.ts_ms = k_uptime_get();

        vs_temp_read(&msg.temp);
        vs_imu_read(&msg.imu);

        if (k_msgq_put(&sample_q, &msg, K_NO_WAIT) != 0) {
            /* drop oldest */
            struct sample_msg dump;
            k_msgq_get(&sample_q, &dump, K_NO_WAIT);
            k_msgq_put(&sample_q, &msg, K_NO_WAIT);
            q_drops++;
        }

        /* update q stats */
        uint32_t used = k_msgq_num_used_get(&sample_q);
        if (used > q_high_watermark) q_high_watermark = used;

        /* sleep to maintain rate */
        int32_t period_ms = 1000 / rate;
        next += period_ms;
        int32_t to_sleep = (int32_t)(next - k_uptime_get());
        if (to_sleep < 0) to_sleep = 0;
        k_msleep(to_sleep);
    }
}

/* Consumer thread */
void logger_thread(void *a, void *b, void *c)
{
    ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);
    int64_t last_print = 0;

    while (1) {
        struct sample_msg msg;
        if (k_msgq_get(&sample_q, &msg, K_MSEC(100)) == 0) {
            /* In a real system we would do more here; keep it light for demo */
        }

        if (k_uptime_get() - last_print > 1000) {
            struct queue_stats st;
            sensorhub_get_qstats(&st);
            LOG_INF("rate=%dHz q=%u/%u hwm=%u drops=%u idle=%d%%",
                    sensorhub_get_rate(), st.depth, st.capacity, st.high_watermark,
                    st.drops, atomic_get(&g_ctrl.idle_pct));
            last_print = k_uptime_get();
        }
    }
}

K_THREAD_STACK_DEFINE(sensor_stack, 2048);
K_THREAD_STACK_DEFINE(logger_stack, 2048);
static struct k_thread sensor_tid, logger_tid;

void main(void)
{
    /* spawn threads */
    k_thread_create(&sensor_tid, sensor_stack, K_THREAD_STACK_SIZEOF(sensor_stack),
                    sensor_thread, NULL, NULL, NULL, 5, 0, K_NO_WAIT);
    k_thread_name_set(&sensor_tid, "sensor");

    k_thread_create(&logger_tid, logger_stack, K_THREAD_STACK_SIZEOF(logger_stack),
                    logger_thread, NULL, NULL, NULL, 6, 0, K_NO_WAIT);
    k_thread_name_set(&logger_tid, "logger");
}

/* Control / stats API */
void sensorhub_set_rate(int hz) { atomic_set(&g_ctrl.rate_hz, hz); }
int  sensorhub_get_rate(void) { return atomic_get(&g_ctrl.rate_hz); }
void sensorhub_reset_stats(void) { q_high_watermark = 0; q_drops = 0; }

void sensorhub_get_qstats(struct queue_stats *out)
{
    out->capacity = MSGQ_CAPACITY;
    out->depth = k_msgq_num_used_get(&sample_q);
    out->high_watermark = q_high_watermark;
    out->drops = q_drops;
}
