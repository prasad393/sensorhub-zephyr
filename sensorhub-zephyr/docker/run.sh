#!/usr/bin/env bash
set -e
docker run --rm -it -v "$(pwd)":/workspace sensorhub-zephyr bash -lc '\
  source /home/builder/zephyrproject/zephyr/zephyr-env.sh && \
  cd /workspace && west build -b native_sim -s . && west run \
'
