# master_testbed
Testbed for running network emulations for my master thesis. Our thesis aims at comparing service discovery protocols for embedded devices on 5G SA and WiFi. 

### Dependencies
ubuntu:24.04
docker
docker-compose
python3

### Structure
* ns-3-dev
    * Network simulator. Here used with real-time and connects with network programs over TAP bridges to emulate setup
* scripts
    * Defines topology and scripts to build and run testbed
* zenoh
    * network protocol
* zenohd-auto-deploy
    * Program that deploys zenoh containers with TAP interfaces to connect to ns-3 ghost nodes



### How the project is built
To define a topology one must create one file for the ns-3 network, and one for the network nodes. These are placed in ```scripts/topology/<experiment_name>```.


The ns-3 topology is made by moving a zenoh.cc file into the ns-3-dev/scratch folder so its autumatically compiled when building with ```./ns3 build```. 