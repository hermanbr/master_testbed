
#Build the topology defined in topology/<experiment>/zenoh.cc. 
#Moves the topology/<experiment>/zenoh.cc into ns3/scratch and builds it. 


if [ -z "$1" ]; then
    echo "usage: $0 <experiment_name>"
    exit 1
fi

EXPERIMENT_NAME=$1
SRC_DIR="script/topology/$EXPERIMENT_NAME"
NS3_SCRATCH_DIR="ns-3-dev/scratch"
ZENOH_DEPLOY_DIR="zenohd-auto-deploy"

if [ ! -f "$SRC_DIR/zenoh.cc" ]; then
    echo "ERROR: $SRC_DIR/zenoh.cc not found"
    exit 1
fi

if [ ! -f "$SRC_DIR/NETWORK_CONFIG.json5" ]; then
    echo "ERROR: $SRC_DIR/NETWORK_CONFIG.json5 not found"
    exit 1
fi

cp "$SRC_DIR/zenoh.cc" "$NS3_SCRATCH_DIR/"
cp "$SRC_DIR/NETWORK_CONFIG.json5" "$ZENOH_DEPLOY_DIR/"

cd ns-3-dev || exit
    ./ns3 configure --enable-examples --enable-tests --enable-sudo
    ./ns3 build
    
cd -