#!/bin/bash
# Usage: script/run_experiment.sh <zenoh|mqtt> <topology>
set -e
[[ "$1" =~ ^(zenoh|mqtt)$ && -n "$2" ]] || { echo "usage: $0 <zenoh|mqtt> <topology>"; exit 1; }
cd "$(dirname "$0")/.."
export SUDO_PASSWORD=""  # ./ns3 --enable-sudo won't prompt (passwordless sudo on this host)
export PROTOCOL="$1"     # read by deploy/launch_nodes.py

script/build_ns3.sh "$2"

# remove old containers and tap stuff if any
docker ps -a --format '{{.Names}}' | grep -E '^node_[0-9]+$' | xargs -r docker rm -f >/dev/null || true  # only our node_<id>
for i in $(ip -br link | awk '/^(br_|tap_|internal_)/{sub(/@.*/,"",$1); print $1}'); do sudo ip link del "$i"; done
pkill -x dumpcap || true

(cd deploy && exec python3 launch_nodes.py) &
LAUNCHER_PID=$!
trap 'kill -TERM $LAUNCHER_PID 2>/dev/null; wait $LAUNCHER_PID' EXIT
# Wait until the launcher has created all bridges and started the capture
until pgrep -x dumpcap >/dev/null; do kill -0 $LAUNCHER_PID; sleep 1; done

script/run_ns3.sh
