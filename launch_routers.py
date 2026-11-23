#!/usr/bin/env python3
import signal
import os
import sys
import subprocess
import json
import json5
import time
import traceback
from datetime import datetime

# Protocol to run in the containers: "zenoh" (default) or "mqtt". Set by script/run_experiment.sh.
PROTOCOL = os.environ.get("PROTOCOL", "zenoh")
MQTT_IMAGE = "eclipse-mosquitto:2.0.20"

def ep_ip(ep):
    return ep.split('/')[1].split(':')[0]  # "tcp/10.0.1.13:8003" -> "10.0.1.13"

def ep_subnet(ep):
    return ep_ip(ep).rsplit('.', 1)[0]     # "tcp/10.0.1.13:8003" -> "10.0.1"

# Phased start: after ns-3 is up, start routers, wait ROUTER_WAIT s, start subs, wait SUB_WAIT s, start pubs
ROUTER_WAIT = float(os.environ.get("ROUTER_WAIT", 10))
SUB_WAIT = float(os.environ.get("SUB_WAIT", 5))

def event(name, node=""):
    # Append "<unix_ns>,<event>,<node>" to events.csv in the run folder (same clock as the pcap)
    with open(f"{base_dir}/events.csv", "a") as f:
        f.write(f"{time.time_ns()},{name},{node}\n")

def wait_for_ns3():
    # A TAP has NO-CARRIER until a process opens it; ns-3's TapBridge opens all of them
    print("Waiting for ns-3 to attach to all TAPs...\n")
    ppid = os.getppid()
    while any("NO-CARRIER" in l for l in subprocess.check_output("ip -o link", shell=True, text=True).splitlines()
              if l.split(": ")[1].startswith("tap_")):
        if os.getppid() != ppid:
            raise RuntimeError("parent (run_experiment.sh) exited while waiting for ns-3")
        time.sleep(0.2)

# Packet capture of all bridges (one pcapng per run), started in main
capture = None
pcap = None

def stop_capture():
    if capture:
        subprocess.run("pkill -INT -x dumpcap", shell=True)  # SIGINT: dumpcap closes the file cleanly
        capture.wait(timeout=10)

def signal_handler(sig, frame):
    signal.signal(signal.SIGINT, signal.SIG_IGN)
    signal.signal(signal.SIGTERM, signal.SIG_IGN)
    print(f"\nReceived signal:{sig}, leaving...\n")
    stop_capture()  # before the cleanup deletes the bridges

    if node_list:
        # print("Cleaning up tmux sessions for all nodes...\n")
        # os.chdir("experiment_data")
        for node in node_list:
            try:
                node.cleanup()
                print(f"Successfully killed tmux session for Node {node.id}\n")
            except Exception as e:
                # print(f"Failed to kill tmux session for Node {node.id}: {e}\n")
                print(f"An error occurred while performing the cleanup for Node {node.id}: {e}\n")

    signal.signal(signal.SIGTERM, signal.SIG_DFL)
    os.killpg(process_group_id, signal.SIGTERM)
    sys.exit(0)


class Node():
    def __init__(self, id, config):
        self.id = id
        self.config = config

        if config.get('zid').get('set'):
            self.zid = config.get('zid').get('value')
        else:
            self.zid = False
            
        self.listen_endpoints = config.get('listen_endpoints') 
        self.container_name = f"zenohd_{self.id}"
        self.volume = self.config.get('volume') or network_config.get('volume')
        self.role = self.config.get('role', 'router') 
        
        self.cleanup()
        
        self.run_docker()
        # docker run is sent to tmux asynchronously; wait until the container exists
        while subprocess.run(f"docker inspect {self.container_name}", shell=True,
                             capture_output=True).returncode != 0:
            time.sleep(0.5)
        
        self.setup_network_interfaces()
        # The program is started later by launch(), once ns-3 is up (see main)

    def launch(self):
        event("start", self.id)
        if PROTOCOL == "mqtt":
            self.launch_mqtt()
        else:
            self.launch_zenoh()

    def launch_mqtt(self):
        launch_cmd = f"docker exec -it {self.container_name} "

        if self.role in ("pub", "sub"):
            # MQTT has no discovery: use the broker (router node) on the same /24 as this node
            net = ep_subnet(self.listen_endpoints[0])
            broker = next(ep_ip(ep) for n in nodes.values() if n.get("role", "router") == "router"
                          for ep in n["listen_endpoints"] if ep_subnet(ep) == net)
            app = self.config.get("app")  # optional: own binary from apps/mqtt_apps (same names as apps/zenoh_apps)
            if app:
                print(f"Launching {app} ({self.role}) {self.container_name} -> {broker}")
                launch_cmd += f"/apps/mqtt_apps/target/x86_64-unknown-linux-musl/release/{app} -h {broker}"
                if self.role == "sub":
                    launch_cmd += f" -o /out/{self.container_name}_video.ts"  # host: <run folder>/zenohd_<id>_video.ts
            elif self.role == "pub":
                print(f"Launching MQTT publisher {self.container_name} -> {broker}")
                launch_cmd += (f'mosquitto_pub -d -h {broker} -t demo/example/mqtt-pub -m "Pub from MQTT {self.id}"'
                               f' --repeat 1000000 --repeat-delay 1')
            else:
                print(f"Launching MQTT subscriber {self.container_name} -> {broker}")
                launch_cmd += f'mosquitto_sub -h {broker} -t "demo/example/#" -F "%U %t %p"'
            # mosquitto clients exit if the first connect fails (e.g. before ns-3 runs); retry every second
            launch_cmd = f"until {launch_cmd}; do sleep 1; done"
        else:
            # Broker: listen on all interfaces, bridge to each router this node is side "a" of in links
            conf = "listener 1883\nallow_anonymous true\nlog_dest stdout\n"
            for link in links:
                peer = nodes[link["b"]]
                if link.get("a") == str(self.id) and peer.get("role", "router") == "router":
                    peer_ip = ep_ip(peer["listen_endpoints"][link["b_idx"]])
                    conf += f"\nconnection bridge_{self.id}_{link['b']}\naddress {peer_ip}:1883\ntopic # both 0\n"
            conf_path = f"{base_dir}/{self.container_name}.conf"
            with open(conf_path, "w") as f:
                f.write(conf)
            # absolute path: docker cp would read the ':' in the timestamp dir as "container:path"
            self.run_shell_command(f"docker cp {os.path.abspath(conf_path)} {self.container_name}:/mosquitto.conf")
            print(f"Launching MQTT broker {self.container_name}")
            launch_cmd += "mosquitto -c /mosquitto.conf -v"

        launch_cmd += f" > >(tee ./{self.container_name}.log) 2> >(tee ./{self.container_name}_err.log >&2)"
        self.run_shell_command(f"tmux send-keys -t {self.container_name} '{launch_cmd}' C-m")

    def launch_zenoh(self):
        launch_cmd = f"docker exec -e RUST_LOG=debug -it {self.container_name} ./zenoh/target/x86_64-unknown-linux-musl/fast/"

        app = self.config.get("app")  # optional: own binary from apps/zenoh_apps instead of z_pub/z_sub
        if app:
            print(f"Launching {app} ({self.role}) {self.container_name}")
            launch_cmd = (f"docker exec -e RUST_LOG=info -it {self.container_name} "
                          f"/apps/zenoh_apps/target/x86_64-unknown-linux-musl/release/{app}")
            if self.role == "sub":
                launch_cmd += f" -o /out/{self.container_name}_video.ts"  # host: <run folder>/zenohd_<id>_video.ts
        elif self.role == "pub":
            print(f"Launching publisher {self.container_name}")
            launch_cmd += "examples/z_pub"
        elif self.role == "sub":
            print(f"Launching subscriber {self.container_name}")
            launch_cmd += "examples/z_sub"
        else:
            print(f"Lauching zenohd {self.container_name}")
            launch_cmd += "zenohd"
            launch_cmd += " --adminspace-permissions rw"
            
            if self.zid:
                launch_cmd += f" -i {self.zid}"
        
        if self.role != "router" and self.config.get("mode") == "client":
            # Client: no listening/scouting, connect only to the router on the same /24
            net = ep_subnet(self.listen_endpoints[0])
            router = next(ep for n in nodes.values() if n.get("role", "router") == "router"
                          for ep in n["listen_endpoints"] if ep_subnet(ep) == net)
            launch_cmd += f" -m client -e {router}"
        else:
            # Ports to listen to
            for ep in self.listen_endpoints:
                launch_cmd += f" -l {ep}"


        if self.role == "router":    
            peer_capacities = {} # 儲存 {Target ZID: Capacity}
            
            rid = str(self.id)     
            peer_eps = []
            for link in links:
                target_node_id = None
                capacity = link.get("cap")
                
                if link.get("a") == rid:
                    target_node_id = link["b"]
                    eps = nodes[target_node_id]["listen_endpoints"]
                    idx = link["b_idx"]
                    peer_eps.append(eps[idx])
                elif link.get("b") == rid:
                    target_node_id = link["a"]
                    eps = nodes[target_node_id]["listen_endpoints"]
                    idx = link["a_idx"]
                    peer_eps.append(eps[idx])
                
                if target_node_id is not None and capacity is not None:
                    target_node_config = nodes.get(target_node_id)
                    target_zid = target_node_config.get("zid").get("value")  
                    peer_capacities[target_zid] = int(capacity)
                    

            if peer_capacities:
                capacities_json_str = json.dumps(peer_capacities)
                launch_cmd += f" --cfg='peer_caps:{capacities_json_str}'"

            for ep in peer_eps:
                launch_cmd += f" -e {ep}"
                
        launch_cmd += f" > >(tee ./{self.container_name}.log) 2> >(tee ./{self.container_name}_err.log >&2)"
        tmux_cmd = f"tmux send-keys -t {self.container_name} '{launch_cmd}' C-m"
        
        self.run_shell_command(tmux_cmd)
        
        
    def setup_netns_veth(self, tid, addr):
        name = f"{self.id}_{tid}"
        
        TAP_NAME = f'tap_{name}'
        MAC_ADDRESS = f'02:00:00:00:{int(self.id):02x}:{tid:02x}'

        self.run_shell_command(f"sudo ip tuntap add {TAP_NAME} mode tap")
        # No IPv6 on the TAP itself: its own MLD/RS frames bypass the bridge (and ebtables), and
        # TapBridge UseLocal would learn the TAP's MAC instead of the container's
        self.run_shell_command(f"sudo sysctl -qw net.ipv6.conf.{TAP_NAME}.disable_ipv6=1")
        self.run_shell_command(f"sudo ip link set tap_{name} promisc on up")
        
        self.run_shell_command(f"sudo ip link add name br_{name} type bridge")
        self.run_shell_command(f"sudo ip link set br_{name} up")       
        self.run_shell_command(f"sudo ip link set {TAP_NAME} master br_{name}")
        
        self.run_shell_command(f"sudo iptables -I FORWARD -m physdev --physdev-is-bridged -i br_{name}  -j ACCEPT")

        pid = subprocess.check_output(f"docker inspect --format '{{{{ .State.Pid }}}}' {self.container_name}", shell=True).decode().strip()
        
        self.run_shell_command("sudo mkdir -p /var/run/netns")
        self.run_shell_command(f"sudo ln -sf /proc/{pid}/ns/net  /var/run/netns/{pid}") 
              
        self.run_shell_command(f"sudo ip link add internal_{name}  type veth peer name external_{name}")       
        self.run_shell_command(f"sudo ip link set internal_{name}  master br_{name}")       
        self.run_shell_command(f"sudo ip link set internal_{name}  up")       
        self.run_shell_command(f"sudo ip link set external_{name}  netns {pid}")       

        self.run_shell_command(f"sudo ip netns exec {pid}  ip link set dev external_{name} name eth{tid}")
        # Fixed MAC 02:00:00:00:<node>:<idx>; WiFi STAs in ns-3 use the same one (UseLocal)
        self.run_shell_command(f"sudo ip netns exec {pid}  ip link set eth{tid} address {MAC_ADDRESS}")
        self.run_shell_command(f"sudo ip netns exec {pid}  ip link set eth{tid} up")       
        self.run_shell_command(f"sudo ip netns exec {pid}  ip addr add {addr}/24 dev eth{tid}") 

        # Only allow the nodes own mac address to even enter the TAP bridge, otherwise it messes with NS-3's tap bridge
        EBTABLES_RULE_NAME = f"EBTABLES_{TAP_NAME}"

        self.run_shell_command(f"sudo ebtables -N {EBTABLES_RULE_NAME}")
        self.run_shell_command(f"sudo ebtables -A {EBTABLES_RULE_NAME} -s {MAC_ADDRESS} -j ACCEPT")
        # Explicit DROP: "-N ... -P DROP" leaves the chain policy at RETURN, so nothing would be dropped
        self.run_shell_command(f"sudo ebtables -A {EBTABLES_RULE_NAME} -j DROP")
        
        # Set the rules on the TAP
        self.run_shell_command(f"sudo ebtables -A FORWARD -o {TAP_NAME} -j {EBTABLES_RULE_NAME}")
        self.run_shell_command(f"sudo ebtables -A OUTPUT -o {TAP_NAME} -j DROP")

    def setup_network_interfaces(self):
        for idx, ep in enumerate(self.listen_endpoints):
            addr = ep.split('/')[1].split(':')[0]  # e.g. 10.0.1.1
            self.setup_netns_veth(idx, addr)


    def run_shell_command(self, command):
        print(f"Running command: {command}\n")
        subprocess.run(command, shell=True, check=True)

    def cleanup_netns_veth(self, tid):
        name = f"{self.id}_{tid}"
        
        self.run_shell_command(f"sudo iptables -D FORWARD -m physdev --physdev-is-bridged -i br_{name} -j ACCEPT 2>/dev/null || true")

        # ebtables rules from setup_netns_veth (loop: earlier runs may have left duplicates)
        tap = f"tap_{name}"
        self.run_shell_command(f"while sudo ebtables -D FORWARD -o {tap} -j EBTABLES_{tap} 2>/dev/null; do :; done")
        self.run_shell_command(f"while sudo ebtables -D OUTPUT -o {tap} -j DROP 2>/dev/null; do :; done")
        self.run_shell_command(f"sudo ebtables -F EBTABLES_{tap} 2>/dev/null && sudo ebtables -X EBTABLES_{tap} || true")
        
        self.run_shell_command(f"sudo ip link del br_{name} 2>/dev/null || true")
        self.run_shell_command(f"sudo ip link del tap_{name} 2>/dev/null || true")
        self.run_shell_command(f"sudo ip link del internal_{name} 2>/dev/null || true")
        self.run_shell_command(f"sudo ip link del external_{name} 2>/dev/null || true")

    def cleanup_stop_container(self):
        print(f"Stopping container for Node {self.id}...\n")
        self.run_shell_command(f"docker container rm -f {self.container_name} 2>/dev/null || true")
    
    def cleanup(self):
        print(f"Cleaning up for Node {self.id}...\n")
        
        for idx in range(len(self.listen_endpoints)):
            self.cleanup_netns_veth(idx)
        self.run_shell_command("sudo rm -f /var/run/netns/* 2>/dev/null || true")
        self.run_shell_command(f"docker container rm -f {self.container_name} 2>/dev/null || true")
        self.run_shell_command(f"tmux kill-session -t {self.container_name} 2>/dev/null || true")

    def run_docker(self):
        print(f"Running docker {self.id}...\n")
        
        chdir_command = f"mkdir -p {base_dir} && cd {base_dir} && "
        volume_arg = ""
        if self.volume:
            host_path = os.path.abspath(self.volume)
            host_project_dir = os.environ.get('HOST_PROJECT_DIR')
            if host_project_dir:
                # Remap container path to host path for Docker socket mount
                container_project_dir = os.path.abspath(os.path.join(os.getcwd(), '..'))
                host_path = host_path.replace(container_project_dir, host_project_dir, 1)
            volume_arg = f"-v {host_path}:/zenoh"
        volume_arg += f" -v {os.path.abspath('apps')}:/apps"  # own apps (apps/zenoh_apps)
        # run folder as /out for app output files (--mount: the ':' in the timestamp breaks -v)
        volume_arg += f" --mount type=bind,src={os.path.abspath(base_dir)},dst=/out"

        docker_run_cmd = f"docker run -dit --name {self.container_name} --network none --rm --entrypoint /bin/sh {volume_arg} {image}"
        if image_clean:
            clean_image = f"docker rmi {image} 2>/dev/null || true && "
            docker_run_cmd = clean_image + chdir_command + docker_run_cmd
        else:
            docker_run_cmd = chdir_command + docker_run_cmd
        base_command = f"tmux new-session -d -s {self.container_name} && tmux send-keys -t {self.container_name} '{docker_run_cmd}"
        base_command += "; echo $? > /tmp/exit_code' C-m"

        self.run_shell_command(base_command)


if __name__ == "__main__":
    process_group_id = os.getpgid(os.getpid())
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)


    try:
        # Load configuration from the JSON5 file
        with open('NETWORK_CONFIG.json5', 'r') as config_file:
            network_config = json5.load(config_file)
        experiment_name = network_config.get('experiment')
        image_config = network_config.get('docker_image')
        image = MQTT_IMAGE if PROTOCOL == "mqtt" else image_config.get('tag')
        image_clean = image_config.get('clean_first')
        user_name = network_config.get('user_name')
        nodes = network_config.get('nodes', {})
        links = network_config.get('links', [])

        run_ts = datetime.now().strftime('%Y-%m-%d_%H:%M:%S')
        base_dir = f"experiment_data/{PROTOCOL}/{experiment_name}/{run_ts}"
        os.makedirs(base_dir, exist_ok=True)
        # os.chdir(base_dir)
        
        node_list = []    
        for node_id, node_config in nodes.items():
            node_list.append(Node(node_id, node_config))

        # Capture all bridges (one per container interface) into one pcapng for this run
        pcap = os.path.abspath(f"{base_dir}/{PROTOCOL}_{experiment_name}_{run_ts.replace(':', '-')}.pcapng")
        # os.chmod(path=pcap, mode=stat.S_IRWXO)
        bridges = sorted(l.split()[0] for l in subprocess.check_output("ip -br link", shell=True, text=True).splitlines()
                         if l.startswith("br_"))
        # No sudo: dumpcap has capture capabilities itself (user must be in the 'wireshark' group)
        capture = subprocess.Popen(["dumpcap", "-q", "-w", pcap] + [a for br in bridges for a in ("-i", br)])
        print(f"Capturing {len(bridges)} bridges to {pcap}\n")
        event("capture_start")

        wait_for_ns3()
        event("ns3_up")
        print("ns-3 is up.\n")

        for role, wait in (("router", ROUTER_WAIT), ("sub", SUB_WAIT), ("pub", 0)):
            print(f"Starting {role} nodes, then waiting {wait:g} s\n")
            for node in node_list:
                if node.role == role:
                    node.launch()
            time.sleep(wait)

        print("All nodes have been launched.\n")

        signal.pause()
    
    except Exception:
        traceback.print_exc()  # show what failed before cleaning up
        stop_capture()
        for node in node_list:
            print("ERROR: CLEANING UP")
            node.cleanup()