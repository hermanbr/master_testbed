"""Per-node timeline from NETWORK_CONFIG: when each node starts, stops, pauses, resumes.

    "0": { ..., timeline: [ {at: 15, do: "start"}, {at: 40, do: "stop"} ] }

`at` = seconds after ns-3 is up. A node without a timeline starts at 0.
Actions are methods on launch_routers.Node with the same name.
"""
import sys
import time

ACTIONS = ("start", "stop", "pause", "resume")


def create_timeline(nodes):
    """nodes = {id: config} -> [(at, node_id, action)] sorted by time. Exits on a bad entry."""
    timeline = []
    for node_id, config in nodes.items():
        for step in config.get("timeline", [{"at": 0, "do": "start"}]):
            at, action = step.get("at"), step.get("do")
            if action not in ACTIONS:
                sys.exit(f"node {node_id}: unknown action {action!r} (allowed: {', '.join(ACTIONS)})")
            if not isinstance(at, (int, float)) or at < 0:
                sys.exit(f"node {node_id}: 'at' must be a number >= 0, got {at!r}")
            timeline.append((at, node_id, action))
    return sorted(timeline, key=lambda e: e[0])  # stable: same-time events keep config order


def run_scheduler(timeline, nodes, log):
    """nodes = {id: Node}; log(action, node_id) writes events.csv. Blocks until the last event."""
    t0 = time.monotonic()
    for at, node_id, action in timeline:
        time.sleep(max(0, t0 + at - time.monotonic()))
        print(f"{time.monotonic() - t0:8.3f}s (planned {at:g}s) node {node_id} {action}\n")
        log(action, node_id)
        try:
            getattr(nodes[node_id], action)()
        except Exception as e:  # one failed action shouldn't stop the rest of the experiment
            print(f"!! node {node_id} {action} failed: {e}\n")
