# SensorHub (Zephyr on `native_sim`)

A hardware-free **Zephyr RTOS** project that simulates a multi-sensor hub using **virtual sensors** (temperature + IMU), a **bounded queue** between producer/consumer threads, a **Zephyr shell** for runtime control, optional **MQTT publishing**, and **ztest** unit tests — all runnable on your PC with `native_sim` (no hardware).

## Highlights
- Threads: producer (`sensor_thread`) and consumer (`logger_thread`) connected by a message queue.
- Shell commands: `sensor read <temp|imu>`, `rate get|set <Hz>`, `stats`.
- Backpressure: bounded queue, drop counters, high‑watermark tracking.
- Power sim: prints idle residency estimate to mimic low‑power behavior on desktop.
- Optional MQTT bridge: publish JSON to `sensorhub/telemetry` if a local broker is running.
- Tests: `ztest` covers sensor math bounds & queue behavior. CI builds and runs tests.

## Quick Start

### Option A: Install Zephyr SDK once (recommended)
```bash
# Install Zephyr (one-time). See official docs for your OS if needed.
# Minimal quick script (Linux/macOS):
python3 -m pip install --user west
west init -m https://github.com/zephyrproject-rtos/zephyr zephyrproject
cd zephyrproject && west update && west zephyr-export

# Build & run this app:
cd /path/to/sensorhub-zephyr
west build -b native_sim -s . -t run
# or
west build -b native_sim -s .
west run
```

You’ll see a Zephyr shell (UART). Try:
```
uart:~$ sensor read temp
uart:~$ sensor read imu
uart:~$ rate set 100
uart:~$ stats
```

### Option B: Docker (no local toolchain)
```bash
./docker/build.sh
./docker/run.sh
```
This builds the app inside a container and runs `native_sim` with an interactive shell.

> If Docker is not available on your system, use **Option A**.

## Shell Commands
- `sensor read temp` — Read a single temperature sample.
- `sensor read imu` — Read a single IMU sample (acc/gyro).
- `rate get` — Show current producer rate (Hz).
- `rate set <Hz>` — Change producer rate; valid: 10..500 Hz.
- `stats` — Show queue depth, drops, high‑watermark, and idle estimate.

## Architecture
- `sensor_thread` generates samples (virtual temp & IMU) at a configurable rate, enqueues to a bounded `k_msgq`.
- `logger_thread` dequeues and logs samples; also feeds optional MQTT bridge.
- Shell commands interact with a control block (`sensorhub_ctrl`) to query/adjust rate and stats.
- `power.c` computes a coarse CPU idle estimate based on sleep vs work timing on desktop.

See `docs/architecture.md` and `docs/sequences.md` for diagrams/notes.

## MQTT (optional)
Run a broker locally (e.g., Mosquitto on `localhost:1883`). If `CONFIG_SENSORHUB_MQTT=y` (enabled in `prj.conf`), samples will be published to the `sensorhub/telemetry` topic as JSON.

## Tests
We provide two `ztest` suites that build as Zephyr test apps:
- `tests/test_temp` — validates temp noise bounds and averaging logic.
- `tests/test_queue` — validates bounded queue behavior and drop counting.

Run (from repo root):
```bash
# Using Zephyr's Twister test runner
west twister -T tests -v -b native_sim
```

## CI
GitHub Actions workflow (`.github/workflows/ci.yml`) performs:
- Zephyr bootstrap with `west`
- Build & run unit tests via `twister`
- Build application for `native_sim`
- Static analysis: `cppcheck` (warning-only)

## Repo Layout
```
sensorhub-zephyr/
├─ docs/
├─ app/                  # threads, shell, power sim
├─ drivers/              # virtual sensors
├─ subsys/mqtt_bridge/   # optional MQTT publisher (CONFIG_SENSORHUB_MQTT)
├─ include/              # public headers
├─ tests/                # ztest apps
├─ scripts/              # helper scripts
├─ docker/               # containerized build/run
├─ prj.conf
├─ CMakeLists.txt
└─ .github/workflows/ci.yml
```

## What you’ll learn / showcase
- RTOS concurrency with Zephyr (threads, queues, timers).
- Device-driver patterns with simulated sensors.
- CLI tooling via Zephyr Shell; telemetry publishing.
- Unit testing with ztest; CI setup for embedded-style repos.

---

