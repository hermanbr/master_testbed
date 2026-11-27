
#Build the topology defined in topology/<experiment>/ns3-topology.cc.
#Copies it into ns-3-dev/scratch (and NETWORK_CONFIG.json5 into deploy/) and builds it.


if [ -z "$1" ]; then
    echo "usage: $0 <experiment_name>"
    exit 1
fi

EXPERIMENT_NAME=$1
SRC_DIR="script/topology/$EXPERIMENT_NAME"
NS3_SCRATCH_DIR="ns-3-dev/scratch"
DEPLOY_DIR="deploy"

if [ ! -f "$SRC_DIR/ns3-topology.cc" ]; then
    echo "ERROR: $SRC_DIR/ns3-topology.cc not found"
    exit 1
fi

if [ ! -f "$SRC_DIR/NETWORK_CONFIG.json5" ]; then
    echo "ERROR: $SRC_DIR/NETWORK_CONFIG.json5 not found"
    exit 1
fi

rm -f "$NS3_SCRATCH_DIR/zenoh.cc"  # old name of the generated copy, so ns-3 does not build it too
cp "$SRC_DIR/ns3-topology.cc" "$NS3_SCRATCH_DIR/"
cp "$SRC_DIR/NETWORK_CONFIG.json5" "$DEPLOY_DIR/"

cd ns-3-dev || exit
    ./ns3 configure --enable-examples --enable-tests --enable-sudo
    ./ns3 build
    
cd -