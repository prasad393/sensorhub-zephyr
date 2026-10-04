# Development image: host tools plus a Zephyr workspace pinned by west.yml.
# The project itself is mounted at /workspace at run time (see run.sh).
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates git cmake ninja-build gcc g++ gcc-multilib g++-multilib \
        device-tree-compiler file make python3 python3-venv \
    && rm -rf /var/lib/apt/lists/*

# Zephyr's Python tooling lives in a virtualenv, as in the README
ENV VIRTUAL_ENV=/opt/venv PATH=/opt/venv/bin:$PATH
RUN python3 -m venv /opt/venv && pip install --no-cache-dir west

WORKDIR /opt/zephyr-ws
COPY west.yml manifest/west.yml
RUN west init -l manifest \
    && west update --narrow -o=--depth=1 \
    && west packages pip --install -- --no-cache-dir

# Outside a west workspace, west finds this one through ZEPHYR_BASE
ENV ZEPHYR_BASE=/opt/zephyr-ws/zephyr \
    ZEPHYR_TOOLCHAIN_VARIANT=host

WORKDIR /workspace
CMD ["/bin/bash"]
