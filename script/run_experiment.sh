#!/bin/bash
# Usage: script/run_experiment.sh <zenoh|mqtt> <topology>
set -e
[[ "$1" =~ ^(zenoh|mqtt)$ && -n "$2" ]] || { echo "usage: $0 <zenoh|mqtt> <topology>"; exit 1; }
cd "$(dirname "$0")/.."
export SUDO_PASSWORD=""  # ./ns3 --enable-sudo won't prompt (passwordless sudo on this host)
export PROTOCOL="$1"     # read by zenohd-auto-deploy/launch_routers.py

script/build_ns3.sh "$2"

# remove old containers and tap stuff if any
docker rm -f $(docker ps -aq --filter name=zenohd_) >/dev/null 2>&1 || true
for i in $(ip -br link | awk '/^(br_|tap_|internal_)/{sub(/@.*/,"",$1); print $1}'); do sudo ip link del "$i"; done

(cd zenohd-auto-deploy && exec python3 launch_routers.py) &
ZENOH_PID=$!
trap 'kill -TERM $ZENOH_PID 2>/dev/null; wait $ZENOH_PID' EXIT
# Wait until the launcher has created one bridge per TAP that ns-3 will use
N=$(grep -c DeviceName ns-3-dev/scratch/zenoh.cc)
until [ "$(ip -br link | grep -c '^br_')" -ge "$N" ]; do kill -0 $ZENOH_PID; sleep 1; done

script/run_ns3.sh
