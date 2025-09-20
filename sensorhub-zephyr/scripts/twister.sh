#!/usr/bin/env bash
# Helper script to run tests via Twister
set -e
source zephyrproject/zephyr/zephyr-env.sh
west twister -T tests -v -b native_sim
