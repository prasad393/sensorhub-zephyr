# Architecture

## Threads & Queues
```
                        rate (Hz)                       stats snapshot
  shell ─────────────────────┐            ┌──────────────────────────── shell, logger
                             ▼            │
  [sensor_thread, prio 5] ──put──▶ [k_msgq, 16 samples] ──get──▶ [logger_thread, prio 6]
   reads vs_temp + vs_imu          full: drop oldest,             counts rx, logs once/s,
   at the configured rate          count drop, track hwm          forwards to MQTT stub
```

| Thread          | Priority | Started by                 | Role                                          |
|-----------------|----------|----------------------------|-----------------------------------------------|
| `sensor`        | 5        | `sensorhub_start()`        | Producer: one sample per period               |
| `logger`        | 6        | `sensorhub_start()`        | Consumer: telemetry line every second         |
| `idle_est`      | 7        | `SYS_INIT` in `power.c`    | Publishes the idle-time estimate              |
| shell           | default  | Zephyr shell subsystem     | Runs the `sensor`, `rate` and `stats` commands |

- `sensor_thread` fills a `struct sample_msg` (sequence number, timestamp,
  temperature and IMU readings) and calls `sensorhub_enqueue()`.
- The queue is a bounded `k_msgq`. The producer never blocks: when the queue is
  full, `sensorhub_enqueue()` discards the oldest sample and counts a drop, so
  the consumer always gets the freshest data. A drop is only counted when a
  sample was really discarded (the consumer may free a slot in the meantime).
- `logger_thread` dequeues with a 100 ms timeout, counts what it received, and
  once per second logs `rate=… rx=… q=depth/capacity hwm=… drops=… idle=…%`.

## Rate scheduling
The producer sleeps until an absolute deadline kept in microseconds, so the
average rate is exact even when the period isn't a whole number of ticks
(e.g. 300 Hz = 3333 µs). The app uses 100 µs kernel ticks
(`CONFIG_SYS_CLOCK_TICKS_PER_SEC=10000`; native_sim defaults to 10 ms), so
samples are evenly spaced up to the 500 Hz limit. If the thread is late it
doesn't sleep, and after a stall of more than 100 ms it resynchronises instead
of bursting to catch up.

## Control & Shell
The control state is private to `app/sensorhub.c`; everything else goes
through the API in `include/sensorhub.h`:

| Function                         | Notes                                                  |
|----------------------------------|--------------------------------------------------------|
| `sensorhub_set_rate()` / `_get_rate()` | Atomic; `set` returns `-EINVAL` outside 10..500 Hz |
| `sensorhub_get_qstats()`         | Consistent snapshot: depth, capacity, high-watermark, drops |
| `sensorhub_reset_stats()`        | Clears drops; high-watermark restarts from the current depth |
| `sensorhub_enqueue()` / `_dequeue()` | The queue operations the threads (and tests) use    |
| `sensorhub_get_idle_pct()`       | Latest idle estimate from `power.c`                   |

The drop count and high-watermark are updated by the producer while the logger
and shell read or reset them, so they are only touched while holding a
`k_spinlock`; readers get a snapshot in which both fields belong together.

The shell commands in `app/shell.c` are static subcommands declared with
`SHELL_CMD_ARG`, so the shell itself rejects missing or extra arguments and
offers tab completion. `rate set` parses its value with `shell_strtol()` and
rejects anything that isn't an integer in range.

## Virtual sensors
- `vs_temp_read()`: 24.5 °C base, a slow ±0.5 °C sine drift, ±0.3 °C uniform
  noise, and an exponential moving average (α = 0.05) seeded with the first
  reading. The shell and the producer both call it, so its state is locked.
- `vs_imu_read()`: a stationary IMU, ~0 g on X/Y and ~1 g on Z with small jitter,
  plus a small random gyro bias. It keeps no state.

## Power / Idle Estimation
On desktop, true low-power is not meaningful for `native_sim`: simulated time
only advances while the CPU sleeps or busy-waits, so CPU load can't be
measured. `power.c` models a fixed amount of busy time per one-second window
and publishes the rest as idle (about 99%). It is a placeholder for a real
measurement such as `k_thread_runtime_stats_all_get()` on hardware.

## MQTT bridge (stub)
With `CONFIG_SENSORHUB_MQTT=y` the logger forwards every sample to
`mqtt_bridge_forward()`. There is no network connection yet: the stub logs,
at most once per second, how many messages it would have published to
`sensorhub/telemetry` and the latest payload. With the option off, the call
compiles to an empty inline function and the bridge isn't built.

## Tests
| Suite                 | What it covers                                                         |
|-----------------------|------------------------------------------------------------------------|
| `tests/pipeline`      | FIFO order, drop-oldest on overflow, stats reset, rate limits, and a producer/consumer run where every sample must be accounted for |
| `tests/sensors`       | Temperature model (seeded average, bounds, full drift cycle), IMU ranges |
| `tests/shell`         | The real shell commands through Zephyr's dummy shell backend          |
| `sample.yaml`         | Boots the app and checks its telemetry, with and without the MQTT stub |

The pipeline lives in `sensorhub.c` rather than `main.c` so the tests can link
it without a second `main()`.
