#!/bin/bash
# Install the same toolchain as the ns3-sim Dockerfile directly on Ubuntu 24.04.
# Run as your normal user (NOT with sudo); the script calls sudo where needed.
#
# Usage: ./install_host.sh

set -euo pipefail

# ---- Settings ---------------------------------------------------------------
# Where Rust goes. /data is on sdb, which has plenty of space; the root disk does not.
RUST_DIR="/data/rust"
# -----------------------------------------------------------------------------

if [ "$EUID" -eq 0 ]; then
    echo "ERROR: run this as your normal user, not with sudo."
    exit 1
fi

. /etc/os-release
if [ "${VERSION_ID:-}" != "24.04" ]; then
    echo "WARNING: this script targets Ubuntu 24.04, you have ${PRETTY_NAME:-unknown}."
    read -rp "Continue anyway? [y/N] " ans
    [[ "$ans" =~ ^[Yy]$ ]] || exit 1
fi

# Free-space check on / (apt packages always install there)
FREE_MB=$(df -Pm / | awk 'NR==2 {print $4}')
if [ "$FREE_MB" -lt 3000 ]; then
    echo "WARNING: only ${FREE_MB} MB free on /. The apt packages need roughly 1.5-2.5 GB."
    echo "Free up space first (e.g. 'sudo apt clean', 'sudo journalctl --vacuum-size=100M')."
    read -rp "Continue anyway? [y/N] " ans
    [[ "$ans" =~ ^[Yy]$ ]] || exit 1
fi

echo "==> Installing system packages"
sudo apt-get update
sudo apt-get install -y --no-install-recommends \
    gcc g++ cmake ninja-build ccache pkg-config make \
    libsqlite3-dev libeigen3-dev libxml2-dev libboost-dev \
    libgtk-3-dev libgsl-dev \
    python3 python3-pip python3-venv python3-dev python3-json5 \
    iproute2 iptables bridge-utils net-tools \
    tmux git curl ca-certificates gnupg lsb-release tzdata

if command -v docker > /dev/null; then
    echo "==> Docker already installed, skipping"
else
    echo "==> Installing Docker CLI"
    sudo install -m 0755 -d /etc/apt/keyrings
    curl -fsSL https://download.docker.com/linux/ubuntu/gpg \
        | sudo gpg --dearmor --yes -o /etc/apt/keyrings/docker.gpg
    sudo chmod a+r /etc/apt/keyrings/docker.gpg
    echo "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.gpg] \
https://download.docker.com/linux/ubuntu $(lsb_release -cs) stable" \
        | sudo tee /etc/apt/sources.list.d/docker.list > /dev/null
    sudo apt-get update
    sudo apt-get install -y --no-install-recommends docker-ce-cli
fi

echo "==> Installing Rust into $RUST_DIR"
sudo mkdir -p "$RUST_DIR"
sudo chown "$USER":"$USER" "$RUST_DIR"
export RUSTUP_HOME="$RUST_DIR/rustup"
export CARGO_HOME="$RUST_DIR/cargo"
export PATH="$CARGO_HOME/bin:$PATH"

if ! command -v rustup > /dev/null; then
    curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs \
        | sh -s -- -y --default-toolchain stable --no-modify-path
fi
rustup target add x86_64-unknown-linux-musl
if ! command -v cross > /dev/null; then
    cargo install cross
fi
rm -rf "$CARGO_HOME/registry/cache"

# Make Rust available in new shells (only added once)
if ! grep -q "RUSTUP_HOME=$RUSTUP_HOME" ~/.bashrc; then
    {
        echo ""
        echo "# Rust (installed by install_host.sh)"
        echo "export RUSTUP_HOME=$RUSTUP_HOME"
        echo "export CARGO_HOME=$CARGO_HOME"
        echo "export PATH=$CARGO_HOME/bin:\$PATH"
    } >> ~/.bashrc
fi

echo ""
echo "==> Done. Versions:"
gcc --version | head -n1
cmake --version | head -n1
python3 -c "import json5; print('json5', json5.__version__)"
rustc --version
cross --version 2>/dev/null | head -n1 || true
echo ""
echo "Open a new terminal (or run 'source ~/.bashrc') to use Rust."
