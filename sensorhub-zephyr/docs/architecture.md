# Architecture

## Threads & Queues
```
[sensor_thread @ rate Hz] ---> [k_msgq bounded queue] ---> [logger_thread]
                                      |                         |
                                 (drops counted)          [optional MQTT]
```

- `sensor_thread` generates `struct sample_msg` payloads.
- `k_msgq` is bounded; on overflow, we increment `drops` and overwrite oldest (drop policy).
- `logger_thread` prints summaries and forwards to MQTT if enabled.

## Control & Shell
- `sensorhub_ctrl` holds the current rate (Hz), queue stats, and idle estimate.
- Shell commands call into `sensorhub_ctrl_*` APIs to read/modify state.

## Power / Idle Estimation
- On desktop, true low-power is not meaningful for `native_sim`. We simulate by measuring
  the proportion of time the system sleeps vs. works within a rolling window.
