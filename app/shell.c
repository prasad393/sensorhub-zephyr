#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include "sensorhub.h"

/*
 * Note: a subcommand handler receives argv[0] = the subcommand's own name, so
 * for "rate set 100" the handler sees argc = 2 and argv = { "set", "100" }.
 * SHELL_CMD_ARG() makes the shell reject a wrong argument count before the
 * handler runs.
 */

/* sensor read temp */
static int cmd_sensor_read_temp(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc); ARG_UNUSED(argv);
    struct temp_sample t;

    vs_temp_read(&t);
    shell_print(sh, "temp: %.2f C (avg=%.2f drift=%.3f noise=%.3f)",
                (double)t.celsius, (double)t.avg, (double)t.drift, (double)t.noise);
    return 0;
}

/* sensor read imu */
static int cmd_sensor_read_imu(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc); ARG_UNUSED(argv);
    struct imu_sample s;

    vs_imu_read(&s);
    shell_print(sh, "imu: ax=%.2f ay=%.2f az=%.2f  gx=%.2f gy=%.2f gz=%.2f",
                (double)s.ax, (double)s.ay, (double)s.az,
                (double)s.gx, (double)s.gy, (double)s.gz);
    return 0;
}

/* rate get */
static int cmd_rate_get(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc); ARG_UNUSED(argv);

    shell_print(sh, "rate=%d Hz", sensorhub_get_rate());
    return 0;
}

/* rate set <Hz> */
static int cmd_rate_set(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    int err = 0;
    /* shell_strtol() rejects empty strings and trailing junk such as "100abc" */
    long hz = shell_strtol(argv[1], 10, &err);

    if (err != 0 || hz < SENSORHUB_RATE_MIN_HZ || hz > SENSORHUB_RATE_MAX_HZ) {
        shell_error(sh, "invalid rate '%s': expected %d..%d Hz",
                    argv[1], SENSORHUB_RATE_MIN_HZ, SENSORHUB_RATE_MAX_HZ);
        return -EINVAL;
    }

    sensorhub_set_rate((int)hz);
    shell_print(sh, "ok rate=%d Hz", (int)hz);
    return 0;
}

/* stats */
static int cmd_stats(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc); ARG_UNUSED(argv);
    struct queue_stats st;

    sensorhub_get_qstats(&st);
    shell_print(sh, "q=%u/%u hwm=%u drops=%u idle=%d%% rate=%dHz",
                st.depth, st.capacity, st.high_watermark, st.drops,
                sensorhub_get_idle_pct(), sensorhub_get_rate());
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_sensor_read,
    SHELL_CMD_ARG(temp, NULL, "Read one temperature sample.", cmd_sensor_read_temp, 1, 0),
    SHELL_CMD_ARG(imu, NULL, "Read one IMU sample (accelerometer + gyro).",
                  cmd_sensor_read_imu, 1, 0),
    SHELL_SUBCMD_SET_END /* Array terminated. */
);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_sensor,
    SHELL_CMD(read, &sub_sensor_read, "sensor read <temp|imu>", NULL),
    SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_rate,
    SHELL_CMD_ARG(get, NULL, "rate get", cmd_rate_get, 1, 0),
    SHELL_CMD_ARG(set, NULL,
                  "rate set <Hz>, " STRINGIFY(SENSORHUB_RATE_MIN_HZ) ".."
                  STRINGIFY(SENSORHUB_RATE_MAX_HZ),
                  cmd_rate_set, 2, 0),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(sensor, &sub_sensor, "sensor commands", NULL);
SHELL_CMD_REGISTER(rate, &sub_rate, "rate control", NULL);
SHELL_CMD_ARG_REGISTER(stats, NULL, "queue/power stats", cmd_stats, 1, 0);
