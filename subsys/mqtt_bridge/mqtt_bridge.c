#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "sensorhub.h"

LOG_MODULE_REGISTER(sensorhub_mqtt, LOG_LEVEL_INF);

/* Stubbed: For demo, we just print what would be published.
 * In real use, enable an MQTT client and publish JSON to sensorhub/telemetry.
 * Controlled by Kconfig/CONFIG_SENSORHUB_MQTT.
 */
static void maybe_publish(const struct sample_msg *s)
{
#if defined(CONFIG_SENSORHUB_MQTT) && CONFIG_SENSORHUB_MQTT
    LOG_INF("[mqtt] publish sensorhub/telemetry "
            "{\"seq\":%u,\"t\":%.2f}", s->seq, s->temp.celsius);
#else
    ARG_UNUSED(s);
#endif
}

void mqtt_bridge_forward(const struct sample_msg *s);
void mqtt_bridge_forward(const struct sample_msg *s) { maybe_publish(s); }
