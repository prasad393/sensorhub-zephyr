#!/usr/bin/env bash
# Build the app for native_sim and run it with the Zephyr shell in this
# terminal. Ctrl-C quits. Extra arguments go to zephyr.exe, e.g. -stop_at=10.
set -euo pipefail
cd "$(dirname "$0")/.."

export ZEPHYR_TOOLCHAIN_VARIANT=${ZEPHYR_TOOLCHAIN_VARIANT:-host}
BUILD_DIR=${BUILD_DIR:-build-stdinout}

west build -b native_sim -d "$BUILD_DIR" . -- -DEXTRA_CONF_FILE=overlay-stdinout.conf

# Hand keystrokes straight to the Zephyr shell (it echoes and completes them
# itself) while keeping Ctrl-C, and restore the terminal afterwards.
if [ -t 0 ]; then
    saved=$(stty -g)
    trap 'stty "$saved"' EXIT
    stty -icanon -echo
fi
"$BUILD_DIR/zephyr/zephyr.exe" "$@"
