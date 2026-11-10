FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# System packages: C++ build tools, ns-3 dependencies, networking, utilities
RUN apt-get update && apt-get install -y --no-install-recommends \
    # C++ build toolchain
    gcc-12 g++-12 cmake ninja-build ccache pkg-config make \
    # ns-3 required libraries
    libsqlite3-dev libeigen3-dev libxml2-dev libboost-dev \
    libgtk-3-dev libgsl-dev \
    # Python
    python3 python3-pip python3-venv python3-dev python3-json5 \
    # Networking tools (TAP/bridge/netns)
    iproute2 iptables bridge-utils net-tools \
    # Utilities
    tmux sudo git curl ca-certificates gnupg lsb-release tzdata \
    && rm -rf /var/lib/apt/lists/*

# Set GCC 12 as default
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-12 100 \
    && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-12 100 \
    && update-alternatives --install /usr/bin/cc cc /usr/bin/gcc-12 100 \
    && update-alternatives --install /usr/bin/c++ c++ /usr/bin/g++-12 100

# Install Docker CLI (for Docker-in-Docker via socket mount)
RUN install -m 0755 -d /etc/apt/keyrings \
    && curl -fsSL https://download.docker.com/linux/ubuntu/gpg \
       | gpg --dearmor -o /etc/apt/keyrings/docker.gpg \
    && chmod a+r /etc/apt/keyrings/docker.gpg \
    && echo "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.gpg] \
       https://download.docker.com/linux/ubuntu $(lsb_release -cs) stable" \
       > /etc/apt/sources.list.d/docker.list \
    && apt-get update && apt-get install -y --no-install-recommends docker-ce-cli \
    && rm -rf /var/lib/apt/lists/*

# Install Rust via rustup
ENV RUSTUP_HOME=/usr/local/rustup \
    CARGO_HOME=/usr/local/cargo \
    PATH=/usr/local/cargo/bin:$PATH
RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs \
    | sh -s -- -y --default-toolchain stable \
    && rustup target add x86_64-unknown-linux-musl \
    && cargo install cross \
    && rm -rf /usr/local/cargo/registry/cache


WORKDIR /workspace

CMD ["bash"]
