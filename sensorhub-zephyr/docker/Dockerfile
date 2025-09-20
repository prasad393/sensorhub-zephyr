FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    git python3 python3-pip cmake ninja-build g++ gcc make \
    libncurses5 wget file gdb \
    && rm -rf /var/lib/apt/lists/*

RUN pip3 install west

# Zephyr bootstrap
RUN useradd -ms /bin/bash builder
USER builder
WORKDIR /home/builder

RUN west init -m https://github.com/zephyrproject-rtos/zephyr zephyrproject && \
    cd zephyrproject && west update && west zephyr-export && \
    pip3 install -r zephyr/scripts/requirements.txt

WORKDIR /workspace
CMD ["/bin/bash"]
