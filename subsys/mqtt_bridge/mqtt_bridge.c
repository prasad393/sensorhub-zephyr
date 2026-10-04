#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "sensorhub.h"

LOG_MODULE_REGISTER(sensorhub_mqtt, LOG_LEVEL_INF);

/* Stubbed: there is no broker connection yet. Instead of publishing each sample
 * to sensorhub/telemetry, log a once-per-second summary of what would have been
 * published, which keeps the console readable at high sample rates.
 * Built only with CONFIG_SENSORHUB_MQTT=y; called from the logger thread only.
 */
void mqtt_bridge_forward(const struct sample_msg *s)
{
    static uint32_t pending;
    static int64_t last_log_ms;

    pending++;
    if (k_uptime_get() - last_log_ms < 1000) {
        return;
    }

    LOG_INF("[mqtt stub] would publish %u msgs to sensorhub/telemetry, "
            "latest {\"seq\":%u,\"t\":%.2f}",
            pending, s->seq, (double)s->temp.celsius);
    pending = 0;
    last_log_ms = k_uptime_get();
}
