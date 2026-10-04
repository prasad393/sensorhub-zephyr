#!/usr/bin/env bash
# Build the development image. It is always linux/amd64: native_sim's default
# 32-bit build needs x86 multilib (Docker emulates amd64 on Apple Silicon).
set -euo pipefail
cd "$(dirname "$0")/.."
docker build --platform linux/amd64 -t sensorhub-zephyr -f docker/Dockerfile .
