# AGENTS.md

Guidance for AI coding agents (Claude Code, Codex, Cursor, etc.) working in this repository.

## Project overview

Master-thesis testbed for **comparing service discovery protocols for embedded devices over 5G SA (incl. 5G MBS / multicast-broadcast) and WiFi**.

- **ns-3** runs in real-time mode (`RealtimeSimulatorImpl`) and models the radio/network topology.
- Real protocol implementations run in **Docker containers**, attached to ns-3 "ghost nodes" via **TAP bridges** (`tap-bridge` module, `UseBridge` mode).
- **Zenoh** is the first protocol under test; more service discovery protocols will be added.
- Measurements/logs are written to `zenohd-auto-deploy/experiment_data/<experiment>/<timestamp>/` (`<experiment>` is the `experiment` field in NETWORK_CONFIG, not the folder name).

## Repository layout

| Path | What it is |
|------|------------|
| `script/` | Build/run scripts. Always run from the repo root. |
| `script/topology/<experiment>/` | One directory per experiment: `zenoh.cc` (ns-3 topology) + `NETWORK_CONFIG.json5` (container nodes/links). Existing: `dumbell`, `test`. |
| `ns-3-dev/` | Git submodule, **upstream** ns-3 (gitlab.com/nsnam). Has its own `AGENTS.md`. |
| `zenoh/` | Git submodule, **upstream** Eclipse Zenoh (Rust). |
| `zenohd-auto-deploy/` | Git submodule, separate repo (`hermanbr/zenohd-auto-deploy`). `launch_routers.py` creates containers, TAPs, bridges, veths, tmux sessions. |
| `container/` | `Dockerfile` + `docker-compose.yml` for the build/sim environment (Ubuntu 24.04, gcc-12, Rust + `cross`, privileged, host network, docker socket mounted). |
| `install_host.sh` | Installs the same toolchain directly on the host (Rust into `/data/rust`). |

## Workflow

Run everything from the repo root:

```bash
./script/build_cross_zenoh.sh            # build static musl Zenoh binaries (cross, --profile fast)
./script/build_ns3.sh <experiment>       # copy topology files into place, ./ns3 configure + build
./script/run_zenoh.sh                    # start containers + TAP/bridges (needs sudo + docker, runs in tmux)
./script/run_ns3.sh                      # ./ns3 run scratch/zenoh --no-build
```

Order: build → start Zenoh nodes → run ns-3. Teardown: Ctrl-C in `launch_routers.py` cleans up containers, bridges, TAPs, and tmux sessions.

`build_ns3.sh` configures ns-3 with `--enable-examples --enable-tests --enable-sudo`.

### Generated files: never edit directly

`build_ns3.sh` / `run_experiment.sh` **overwrite** these on every build:

- `ns-3-dev/scratch/zenoh.cc` ← `script/topology/<experiment>/zenoh.cc`
- `zenohd-auto-deploy/NETWORK_CONFIG.json5` ← `script/topology/<experiment>/NETWORK_CONFIG.json5`

Always edit the source under `script/topology/<experiment>/`.

## Edit boundaries (important)

**Only modify the files the task requires.** Don't reformat, rename, "clean up", bump dependencies, or refactor unrelated code. Don't create new top-level directories, READMEs, or docs unless asked.

**Allowed to edit:**
- `script/**` (including new experiments under `script/topology/<new_experiment>/`)
- `container/**`
- `install_host.sh`, `README.md`, `AGENTS.md`, `CLAUDE.md`
- `zenohd-auto-deploy/**` (the user's own repo; except `experiment_data/` and the generated `NETWORK_CONFIG.json5`). Changes there must be committed in that repo, so mention them to the user.

**Read-only unless the user explicitly asks for a change in that specific location:**
- `ns-3-dev/**`: upstream ns-3. New ns-3 modules (e.g. 5G-LENA `nr` for 5G NR/MBS) belong in `ns-3-dev/contrib/`, and only when requested.
- `zenoh/**`: upstream Zenoh.

**Never edit:**
- `.gitmodules`, `LICENSE`
- Build outputs: `ns-3-dev/build/`, `zenoh/target/`
- `zenohd-auto-deploy/experiment_data/`: measurement results are research data

**Git:** don't commit, push, run `git submodule update`, check out other revisions inside submodules, or run `git clean` / `git reset --hard` unless the user asks.

## System-level actions: ask first

The testbed changes host networking state. **Ask before** running anything that:
- uses `sudo`
- creates or deletes TAP, bridge, veth, or netns interfaces
- starts or stops or removes Docker containers or images (`run_zenoh.sh`, `launch_routers.py`, `docker run/rm`)
- runs `install_host.sh` or modifies `~/.bashrc` or system packages

Read-only inspection (`ip link`, `ip addr`, `docker ps`, `tmux ls`) is fine.

## Research integrity

- Topology parameters (data rates, delays, node counts, IP plans, Wi-Fi/NR PHY/MAC settings) and protocol configs **define the experiments**. Never change them as a side effect of another task. Call out any change explicitly.
- Keep experiments reproducible: one directory per experiment, and don't silently mutate an existing experiment to create a new one. Copy it to a new directory instead.
- IP addresses/subnets in `zenoh.cc` must match `NETWORK_CONFIG.json5` for the same experiment.
- Don't fabricate or "fix" results. Report failures as they are.

## Conventions

- ns-3 C++ follows ns-3 style (`ns-3-dev/.clang-format`). See `ns-3-dev/AGENTS.md` for ns-3 build/test details.
- Shell scripts assume the repo root as the working directory.
- Write new code comments in English. Existing topology files contain Chinese comments inherited from an upstream example.
- `NETWORK_CONFIG` schema: see `zenohd-auto-deploy/README.md` and `NETWORK_CONFIG_DEFAULT.json5`.
