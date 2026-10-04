#pragma once
#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Producer rate limits, in Hz */
#define SENSORHUB_RATE_MIN_HZ     10
#define SENSORHUB_RATE_MAX_HZ     500
#define SENSORHUB_RATE_DEFAULT_HZ 100

/* Number of samples the producer -> consumer queue can hold */
#define SENSORHUB_QUEUE_CAPACITY 16

struct temp_sample {
    float celsius;
    float avg;     /* exponential moving average */
    float drift;
    float noise;
};

struct imu_sample {
    float ax, ay, az; /* g */
    float gx, gy, gz;
};

struct sample_msg {
    uint32_t seq;
    int64_t  ts_ms;
    struct temp_sample temp;
    struct imu_sample  imu;
};

/* A consistent snapshot of the queue statistics */
struct queue_stats {
    uint32_t depth;          /* samples queued right now */
    uint32_t capacity;
    uint32_t high_watermark; /* deepest the queue has been since the last reset */
    uint32_t drops;          /* samples discarded because the queue was full */
};

/* Start the producer (sensor) and consumer (logger) threads. */
int sensorhub_start(void);

/* Producer rate. sensorhub_set_rate() returns -EINVAL outside the limits above. */
int  sensorhub_set_rate(int hz);
int  sensorhub_get_rate(void);

/*
 * Bounded queue between the threads. sensorhub_enqueue() never blocks: when the
 * queue is full it discards the oldest sample and returns how many samples it
 * discarded (0 when there was room).
 */
int  sensorhub_enqueue(const struct sample_msg *msg);
int  sensorhub_dequeue(struct sample_msg *msg, k_timeout_t timeout);

/* Queue statistics. Safe to call from any thread. */
void sensorhub_get_qstats(struct queue_stats *out);
void sensorhub_reset_stats(void);

/* Idle-time estimate in percent, published by power.c */
void sensorhub_set_idle_pct(int pct);
int  sensorhub_get_idle_pct(void);

/* Virtual sensor drivers (drivers/). Safe to call from any thread. */
int vs_temp_read(struct temp_sample *out);
int vs_imu_read(struct imu_sample *out);

/* MQTT bridge (subsys/mqtt_bridge/), compiled in with CONFIG_SENSORHUB_MQTT */
#if defined(CONFIG_SENSORHUB_MQTT)
void mqtt_bridge_forward(const struct sample_msg *msg);
#else
static inline void mqtt_bridge_forward(const struct sample_msg *msg)
{
    ARG_UNUSED(msg);
}
#endif

#ifdef __cplusplus
}
#endif
