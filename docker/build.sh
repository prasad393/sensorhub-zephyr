#!/usr/bin/env bash
# Build the development image
set -euo pipefail
cd "$(dirname "$0")/.."
docker build -t sensorhub-zephyr -f docker/Dockerfile .
