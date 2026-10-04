#include <stdio.h>
#include <string.h>
#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_dummy.h>
#include <zephyr/ztest.h>
#include "sensorhub.h"

/* Runs the real shell commands from app/shell.c through the dummy backend */
static const struct shell *sh;
static const char *out; /* output of the last run() */

/* Call this before asserting on its result: zassert evaluates its message
 * arguments in the same call as the condition, in unspecified order.
 */
static int run(const char *cmd)
{
    size_t size;
    int ret;

    shell_backend_dummy_clear_output(sh);
    ret = shell_execute_cmd(sh, cmd);
    out = shell_backend_dummy_get_output(sh, &size);
    return ret;
}

static void *shell_setup(void)
{
    sh = shell_backend_dummy_get_ptr();
    WAIT_FOR(shell_ready(sh), 20000, k_msleep(1));
    zassert_true(shell_ready(sh), "timed out waiting for dummy shell backend");
    return NULL;
}

static void shell_before(void *fixture)
{
    ARG_UNUSED(fixture);
    zassert_ok(sensorhub_set_rate(SENSORHUB_RATE_DEFAULT_HZ));
}

ZTEST(shell_cmds, test_rate_set_changes_rate)
{
    int ret = run("rate set 250");

    zassert_ok(ret, "output: %s", out);
    zassert_equal(sensorhub_get_rate(), 250, "rate set did not change the rate");
    zassert_not_null(strstr(out, "rate=250 Hz"), "output: %s", out);

    ret = run("rate set 10");
    zassert_ok(ret, "output: %s", out);
    zassert_equal(sensorhub_get_rate(), 10);

    ret = run("rate set 500");
    zassert_ok(ret, "output: %s", out);
    zassert_equal(sensorhub_get_rate(), 500);
}

ZTEST(shell_cmds, test_rate_set_rejects_bad_input)
{
    static const char *const bad[] = {
        "rate set 9", "rate set 501", "rate set 0", "rate set -100",
        "rate set abc", "rate set 100abc", "rate set 4294967396",
        "rate set", "rate set 100 200",
    };

    ARRAY_FOR_EACH(bad, i) {
        int ret = run(bad[i]);

        zassert_not_equal(ret, 0, "'%s' was accepted: %s", bad[i], out);
        zassert_equal(sensorhub_get_rate(), SENSORHUB_RATE_DEFAULT_HZ,
                      "'%s' changed the rate", bad[i]);
    }
}

ZTEST(shell_cmds, test_rate_get)
{
    zassert_ok(sensorhub_set_rate(320));

    int ret = run("rate get");

    zassert_ok(ret, "output: %s", out);
    zassert_not_null(strstr(out, "rate=320 Hz"), "output: %s", out);
}

ZTEST(shell_cmds, test_sensor_read)
{
    int ret = run("sensor read temp");

    zassert_ok(ret, "output: %s", out);
    zassert_not_null(strstr(out, "temp: "), "output: %s", out);

    ret = run("sensor read imu");
    zassert_ok(ret, "output: %s", out);
    zassert_not_null(strstr(out, "imu: ax="), "output: %s", out);

    ret = run("sensor read gyro");
    zassert_not_equal(ret, 0, "unknown sensor accepted: %s", out);
}

ZTEST(shell_cmds, test_stats)
{
    struct queue_stats st;
    char expected[64];
    int ret = run("stats");

    zassert_ok(ret, "output: %s", out);
    sensorhub_get_qstats(&st);
    snprintf(expected, sizeof(expected), "q=%u/%u hwm=%u drops=%u",
             st.depth, st.capacity, st.high_watermark, st.drops);
    zassert_not_null(strstr(out, expected), "expected '%s' in: %s", expected, out);
}

ZTEST_SUITE(shell_cmds, NULL, shell_setup, shell_before, NULL, NULL);
