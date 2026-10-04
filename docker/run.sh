#!/usr/bin/env bash
# Run a command in the development container, with the repo at /workspace.
#   docker/run.sh                       build and run the app (Ctrl-C quits)
#   docker/run.sh scripts/twister.sh    build and run all tests
set -euo pipefail
cd "$(dirname "$0")/.."
docker run --rm -it --platform linux/amd64 \
    --user "$(id -u):$(id -g)" -e HOME=/tmp -e BUILD_DIR=build-docker \
    -v "$PWD":/workspace sensorhub-zephyr \
    "${@:-scripts/run.sh}"
