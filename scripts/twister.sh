#!/usr/bin/env bash
# Build and run every test, plus the app smoke tests, the same way CI does.
# Extra arguments go to twister, e.g. -s sensorhub.shell to run one suite.
set -euo pipefail
cd "$(dirname "$0")/.."

export ZEPHYR_TOOLCHAIN_VARIANT=${ZEPHYR_TOOLCHAIN_VARIANT:-host}
west twister -T . -p native_sim -p native_sim/native/64 -v --inline-logs "$@"
