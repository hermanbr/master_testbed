# Deploy

Part of master_testbed (was the separate repo `zenohd-auto-deploy`, imported with its history).

Automatically deploys the protocol nodes (routers/brokers, publishers, subscribers; Zenoh or MQTT, set by `PROTOCOL`) as Docker containers `node_<id>`, with TAP/veth network interfaces for ns-3 integration.

## How It Works

`launch_nodes.py` reads `NETWORK_CONFIG.json5` and for each node:

1. Creates a tmux session
2. Starts a Docker container (`--network none`)
3. Creates TAP interfaces, bridges, and veth pairs to connect the container to ns-3
4. Assigns IP addresses inside the container
5. Runs each node's `timeline` (`scheduler.py`): starts the program for its role and protocol (Zenoh: `zenohd` / `z_pub` / `z_sub` or `apps/zenoh_apps`; MQTT: `mosquitto` / clients or `apps/mqtt_apps`)

On shutdown (Ctrl-C / SIGTERM), all containers, bridges, TAP interfaces, and tmux sessions are cleaned up automatically.

## Usage

This script is typically invoked via the parent project:

```bash
# From the master_testbed root
./script/run_nodes.sh
```

Or run directly:

```bash
cd deploy
python3 launch_nodes.py
```

The script reads `NETWORK_CONFIG.json5` in the current directory. This file is copied from `script/topology/<experiment_name>/` by `build_ns3.sh`.

## Configuration

See `NETWORK_CONFIG_DEFAULT.json5` for the template with documentation. Key fields:

| Field | Description |
|-------|-------------|
| `experiment` | Experiment name. Logs are saved under `experiment_data/<protocol>/<experiment>/<timestamp>/` as `node_<id>.log` |
| `docker_image.tag` | Docker image to use for containers |
| `docker_image.clean_first` | Pull a fresh image before launching |
| `volume` | Path to Zenoh binaries, mounted into the container |
| `nodes.<id>.role` | `"pub"`, `"sub"`, or omit for router |
| `nodes.<id>.zid` | Zenoh runtime identifier (128-bit hex; Zenoh only) |
| `nodes.<id>.timeline` | Optional, e.g. `[{at: 15, do: "start"}, {at: 40, do: "stop"}]`; `at` = s after ns-3 is up. Actions (`scheduler.py`): `start`, `stop` (kill the program, container + network stay), `pause`/`resume` (`docker pause`, node goes silent). Default: start at 0 |
| `nodes.<id>.listen_endpoints` | List of `"tcp/<ip>:<port>"` endpoints |
| `links` | Connections between nodes (`a`, `a_idx`, `b`, `b_idx`, optional `cap` in Mbps) |

## Requirements

- Python 3 with `json5` module
- Docker
- `sudo` (for TAP/bridge/netns operations)
- `tmux`