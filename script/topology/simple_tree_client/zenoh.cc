#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/network-module.h"
#include "ns3/tap-bridge-module.h"

using namespace ns3;

/*
DUMBBELL TOPOLOGY

 N0 (pub) \ WiFi                                 
           \     10.0.1.0/24
 N1 (sub) ---- N3 (router) 
           / WiFi                                   
 N2 (sub) /                                          

Each side is connected to a WiFi AP (like a switch/LAN), so leaves find
their router with Zenoh multicast scouting. Only the routers are linked in
NETWORK_CONFIG.json5.

TAP names: tap_<node>_<endpoint index>, must match NETWORK_CONFIG.json5.
*/

NS_LOG_COMPONENT_DEFINE("ZenohDumbbell");

int
main(int argc, char* argv[])
{
    std::string lanRate = "100Mbps";
    std::string bottleneckRate = "10Mbps";
    uint32_t delayMs = 1;
    double simTime = 600.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("lanRate", "Data rate of the two edge LANs", lanRate);
    cmd.AddValue("bottleneckRate", "Data rate of the router-router link", bottleneckRate);
    cmd.AddValue("delayMs", "One-way delay of every link (ms)", delayMs);
    cmd.AddValue("simTime", "Simulation time (s)", simTime);
    cmd.Parse(argc, argv);

    GlobalValue::Bind("SimulatorImplementationType", StringValue("ns3::RealtimeSimulatorImpl"));
    GlobalValue::Bind("ChecksumEnabled", BooleanValue(true));

    NodeContainer n;
    n.Create(4); // 0..3, ids match NETWORK_CONFIG.json5

    CsmaHelper lan;
    lan.SetChannelAttribute("DataRate", StringValue(lanRate));
    lan.SetChannelAttribute("Delay", TimeValue(MilliSeconds(delayMs)));

    // 10.0.1.0/24: left LAN (N0, N1, N2, N3)
    NetDeviceContainer left = lan.Install(NodeContainer(n.Get(0), n.Get(1), n.Get(2), n.Get(3)));

    TapBridgeHelper tb;
    tb.SetAttribute("Mode", StringValue("UseBridge"));

    // Left leaves
    tb.SetAttribute("DeviceName", StringValue("tap_0_0")); 
    tb.Install(n.Get(0), left.Get(0));
    tb.SetAttribute("DeviceName", StringValue("tap_1_0")); 
    tb.Install(n.Get(1), left.Get(1));
    tb.SetAttribute("DeviceName", StringValue("tap_2_0")); 
    tb.Install(n.Get(2), left.Get(2));

    // Router N3: endpoint 0 = left LAN, endpoint 1 = bottleneck
    tb.SetAttribute("DeviceName", StringValue("tap_3_0")); 
    tb.Install(n.Get(3), left.Get(3));


    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
