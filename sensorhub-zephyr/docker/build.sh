#!/usr/bin/env bash
set -e
docker build -t sensorhub-zephyr -f docker/Dockerfile .
