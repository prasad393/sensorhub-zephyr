#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/logging/log.h>
#include "sensorhub.h"

LOG_MODULE_REGISTER(sensorhub_shell, LOG_LEVEL_INF);

/* forward decls from drivers */
int vs_temp_read(struct temp_sample *out);
int vs_imu_read(struct imu_sample *out);

/* sensor read <temp|imu> */
static int cmd_sensor_read(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 3) {
        shell_print(sh, "usage: sensor read <temp|imu>");
        return -EINVAL;
    }
    if (strcmp(argv[2], "temp") == 0) {
        struct temp_sample t;
        vs_temp_read(&t);
        shell_print(sh, "temp: %.2f C (avg=%.2f drift=%.3f noise=%.3f)", t.celsius, t.avg, t.drift, t.noise);
        return 0;
    } else if (strcmp(argv[2], "imu") == 0) {
        struct imu_sample s;
        vs_imu_read(&s);
        shell_print(sh, "imu: ax=%.2f ay=%.2f az=%.2f  gx=%.2f gy=%.2f gz=%.2f",
                    s.ax, s.ay, s.az, s.gx, s.gy, s.gz);
        return 0;
    }
    shell_print(sh, "unknown sensor: %s", argv[2]);
    return -EINVAL;
}

/* rate get|set */
static int cmd_rate(const struct shell *sh, size_t argc, char **argv)
{
    if (argc == 2) {
        shell_print(sh, "rate=%d Hz", sensorhub_get_rate());
        return 0;
    }
    if (argc >= 3 && strcmp(argv[1], "get") == 0) {
        shell_print(sh, "rate=%d Hz", sensorhub_get_rate());
        return 0;
    }
    if (argc >= 4 && strcmp(argv[1], "set") == 0) {
        int hz = atoi(argv[2]);
        if (hz < 10 || hz > 500) {
            shell_print(sh, "valid 10..500 Hz");
            return -EINVAL;
        }
        sensorhub_set_rate(hz);
        shell_print(sh, "ok rate=%d Hz", hz);
        return 0;
    }
    shell_print(sh, "usage: rate get | rate set <10..500>");
    return -EINVAL;
}

/* stats */
static int cmd_stats(const struct shell *sh, size_t argc, char **argv)
{
    struct queue_stats st;
    sensorhub_get_qstats(&st);
    shell_print(sh, "q=%u/%u hwm=%u drops=%u idle=%d%% rate=%dHz",
                st.depth, st.capacity, st.high_watermark, st.drops,
                atomic_get(&g_ctrl.idle_pct), sensorhub_get_rate());
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_sensor,
    SHELL_CMD(read, NULL, "sensor read <temp|imu>", cmd_sensor_read),
    SHELL_SUBCMD_SET_END /* Array terminated. */
);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_rate,
    SHELL_CMD(get, NULL, "rate get", cmd_rate),
    SHELL_CMD(set, NULL, "rate set <Hz>", cmd_rate),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(sensor, &sub_sensor, "sensor commands", NULL);
SHELL_CMD_REGISTER(rate, &sub_rate, "rate control", cmd_rate);
SHELL_CMD_REGISTER(stats, NULL, "queue/power stats", cmd_stats);
