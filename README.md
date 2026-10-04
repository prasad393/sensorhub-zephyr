# SensorHub (Zephyr on `native_sim`)

[![CI](https://github.com/prasad393/sensorhub-zephyr/actions/workflows/ci.yml/badge.svg)](https://github.com/prasad393/sensorhub-zephyr/actions/workflows/ci.yml)

A hardware-free **Zephyr RTOS** project that simulates a multi-sensor hub. Virtual temperature and IMU sensors feed a producer thread, a **bounded queue** decouples it from a consumer thread, and a **Zephyr shell** lets you change the sample rate and inspect the queue while it runs. Everything runs on your PC with Zephyr's `native_sim` board, and CI builds and tests every change.

## Highlights
- **Threads**: producer (`sensor_thread`) and consumer (`logger_thread`) connected by a bounded `k_msgq`.
- **Backpressure**: the producer never blocks. When the queue is full it drops the oldest sample, counts the drop, and tracks the queue's high-watermark.
- **Thread-safe statistics**: the drop count and high-watermark are only touched under a spinlock, so the shell and logger always read a consistent snapshot.
- **Rate control**: 10 to 500 Hz, scheduled against absolute microsecond deadlines so the delivered rate matches the requested one.
- **Shell**: `sensor read`, `rate get|set` and `stats`, with input validation and tab completion.
- **Tests**: `ztest` suites for the queue logic, sensor models and shell commands, plus smoke tests that boot the app, all run by Twister on 32- and 64-bit `native_sim`.
- **Reproducible setup**: Zephyr is pinned to v4.4.2 by a west manifest, and a Docker image covers machines without a local toolchain.

## Quick Start

### Option A: Linux
`native_sim` runs on Linux and builds with your host GCC, so you don't need the Zephyr SDK. On Ubuntu 24.04 or newer (Zephyr 4.4 needs Python 3.12+):
```bash
sudo apt install --no-install-recommends git cmake ninja-build gcc g++ \
    gcc-multilib g++-multilib device-tree-compiler python3-venv
```

Set up a west workspace once. It takes about 1.5 GB of disk, mostly Zephyr's Python tooling. The repository's `west.yml` pins the Zephyr version:
```bash
mkdir sensorhub-ws && cd sensorhub-ws
python3 -m venv .venv
echo 'export ZEPHYR_TOOLCHAIN_VARIANT=host' >> .venv/bin/activate   # build with the host GCC
source .venv/bin/activate
pip install west
git clone https://github.com/prasad393/sensorhub-zephyr.git
west init -l sensorhub-zephyr        # use this repo's west.yml
west update --narrow -o=--depth=1    # fetch Zephyr v4.4.2
west packages pip --install          # Zephyr's Python dependencies
```
In a new terminal, activate the environment again with `source sensorhub-ws/.venv/bin/activate`.

Build and run:
```bash
cd sensorhub-zephyr
scripts/run.sh    # builds, then runs with the shell in this terminal; Ctrl-C quits
```

### Option B: Docker (no local toolchain)
```bash
git clone https://github.com/prasad393/sensorhub-zephyr.git && cd sensorhub-zephyr
docker/build.sh                     # once: tools and a Zephyr workspace in an image
docker/run.sh                       # build and run the app; Ctrl-C quits
docker/run.sh scripts/twister.sh    # build and run all tests
```
The image is built for `linux/amd64`, because `native_sim`'s default 32-bit build needs x86 multilib. On Apple Silicon, Docker runs it under emulation.

## Try It
A session from `scripts/run.sh`:
```
uart:~$ rate set 250
ok rate=250 Hz
[00:00:01.880,000] <inf> sensorhub: sensor rate: 250 Hz
[00:00:03.000,000] <inf> sensorhub: rate=250Hz rx=250 q=0/16 hwm=1 drops=0 idle=99%
uart:~$ stats
q=0/16 hwm=1 drops=0 idle=99% rate=250Hz
uart:~$ sensor read temp
temp: 24.89 C (avg=24.97 drift=0.497 noise=-0.107)
uart:~$ sensor read imu
imu: ax=-0.05 ay=-0.01 az=1.01  gx=0.26 gy=-0.43 gz=0.19
uart:~$ rate set 600
invalid rate '600': expected 10..500 Hz
```

Once per second the consumer logs a telemetry line:

| Field   | Meaning                                                     |
|---------|-------------------------------------------------------------|
| `rate`  | Configured producer rate                                    |
| `rx`    | Samples the consumer received during the last second        |
| `q`     | Samples queued now / queue capacity                         |
| `hwm`   | High-watermark: the deepest the queue has been              |
| `drops` | Samples discarded because the queue was full                |
| `idle`  | Simulated idle-time estimate (see [Limitations](#limitations)) |

### Shell on a separate terminal
By default Zephyr puts the `native_sim` shell on its own pseudo-terminal. This keeps the telemetry log and the shell in separate windows:
```bash
west build -b native_sim
west build -t run      # prints "uart connected to pseudotty: /dev/pts/N"
screen /dev/pts/N      # in a second terminal (Ctrl-A K quits), or any serial terminal
```

## Shell Commands
| Command            | Description                                                      |
|--------------------|------------------------------------------------------------------|
| `sensor read temp` | Read one temperature sample (value, moving average, drift, noise) |
| `sensor read imu`  | Read one IMU sample (accelerometer in g, gyro)                    |
| `rate get`         | Show the producer rate                                            |
| `rate set <Hz>`    | Change the producer rate. Integers from 10 to 500 only            |
| `stats`            | Show queue depth, high-watermark, drops, idle estimate and rate   |

## Tests
```bash
scripts/twister.sh                      # everything CI runs, on native_sim and native_sim/native/64
scripts/twister.sh -s sensorhub.shell   # one suite
```

| Suite                       | Covers                                                                  |
|-----------------------------|-------------------------------------------------------------------------|
| `tests/pipeline`            | FIFO order, drop-oldest on overflow, stats reset, rate limits, and a producer/consumer run in which every sample must be accounted for (consumed + dropped + queued = produced) |
| `tests/sensors`             | Temperature model (seeded average, noise and drift bounds, a full drift cycle) and IMU ranges |
| `tests/shell`               | The real shell commands, run through Zephyr's dummy shell backend, including rejected input |
| `sample.yaml` (app)         | Boots the app and checks its telemetry, with and without the MQTT stub  |

Twister treats compiler warnings as errors.

## Configuration
- **MQTT bridge (stub)**: `CONFIG_SENSORHUB_MQTT` (see `Kconfig`, off by default). When enabled, the consumer forwards every sample to `subsys/mqtt_bridge/`, which logs once per second what it would publish to `sensorhub/telemetry`. It does not connect to a broker yet. Try it with `west build -b native_sim -- -DCONFIG_SENSORHUB_MQTT=y`.
- **Shell on stdin/stdout**: `overlay-stdinout.conf` moves the shell onto the launching terminal. `scripts/run.sh` builds with it.
- **Kernel tick**: `prj.conf` sets 100 µs ticks (`native_sim` defaults to 10 ms), so samples stay evenly spaced up to 500 Hz.

## Architecture
```
[sensor_thread @ rate Hz] ---> [k_msgq bounded queue] ---> [logger_thread]
                                      |                         |
                          (full: drop oldest, count)      [optional MQTT]
```
- `sensor_thread` reads the virtual sensors at the configured rate and enqueues one sample per period.
- `logger_thread` dequeues samples, counts them, forwards them to the MQTT stub, and logs a telemetry line every second.
- Shell commands go through the API in `include/sensorhub.h`. The control state stays private to `app/sensorhub.c`.
- The pipeline lives in `app/sensorhub.c`, separate from `main()`, so the tests can link it.

See `docs/architecture.md` and `docs/sequences.md` for details.

## CI
The GitHub Actions workflow (`.github/workflows/ci.yml`) runs on every push and pull request:
- Sets up the west workspace with the same west commands as the Quick Start above.
- Runs Twister on the app and every test suite for `native_sim` and `native_sim/native/64`.
- Uploads the Twister reports.

## Repo Layout
```
sensorhub-zephyr/
├─ app/                  # main, pipeline (threads, queue, stats), shell, idle estimator
├─ drivers/              # virtual temperature and IMU sensors
├─ subsys/mqtt_bridge/   # MQTT bridge stub (CONFIG_SENSORHUB_MQTT)
├─ include/              # sensorhub.h: shared types and API
├─ tests/                # ztest suites: pipeline, sensors, shell
├─ docs/                 # architecture and sequence notes
├─ scripts/              # run.sh, twister.sh
├─ docker/               # development image and run scripts
├─ west.yml              # west manifest pinning Zephyr
├─ Kconfig               # app options
├─ prj.conf              # app configuration
├─ overlay-stdinout.conf # shell on the launching terminal
├─ sample.yaml           # Twister smoke tests for the app
└─ .github/workflows/ci.yml
```

## Limitations
- **MQTT** is a stub: no network stack or broker connection yet.
- **Idle estimate** is simulated. On `native_sim`, simulated time only advances while the CPU sleeps or busy-waits, so real CPU load can't be measured. `app/power.c` models a fixed busy time instead.
- **Targets**: `native_sim` only for now. The code uses only portable Zephyr APIs, but no hardware board is configured.

## What This Project Demonstrates
- RTOS concurrency with Zephyr: threads, message queues, spinlocks, absolute timeouts.
- Device-driver patterns with simulated sensors.
- CLI tooling with the Zephyr shell, and testing it in CI.
- Unit and integration testing with `ztest` and Twister, and CI for an embedded project.
