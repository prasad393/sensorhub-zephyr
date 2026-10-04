#pragma once
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#ifdef __cplusplus
extern "C" {
#endif

struct temp_sample {
    float celsius;
    float avg;     /* simple moving average */
    float drift;
    float noise;
};

struct imu_sample {
    float ax, ay, az;
    float gx, gy, gz;
};

struct sample_msg {
    uint32_t seq;
    int64_t  ts_ms;
    struct temp_sample temp;
    struct imu_sample  imu;
};

struct queue_stats {
    uint16_t depth;
    uint16_t capacity;
    uint16_t high_watermark;
    uint32_t drops;
};

struct sensorhub_ctrl {
    atomic_t rate_hz;
    struct queue_stats qstats;
    atomic_t idle_pct; /* 0..100 */
};

extern struct sensorhub_ctrl g_ctrl;

/* API used by shell/tests */
void sensorhub_set_rate(int hz);
int  sensorhub_get_rate(void);
void sensorhub_get_qstats(struct queue_stats *out);
int  sensorhub_read_temp(struct temp_sample *out);
int  sensorhub_read_imu(struct imu_sample *out);

/* test helpers */
void sensorhub_reset_stats(void);

#ifdef __cplusplus
}
#endif
