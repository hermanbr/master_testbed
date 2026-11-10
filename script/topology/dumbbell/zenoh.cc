#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/network-module.h"
#include "ns3/tap-bridge-module.h"

using namespace ns3;

/*
DUMBBELL TOPOLOGY

 N0 (pub) \                                          / N5 (pub)
           \     10.0.1.0/24          10.0.2.0/24   /
 N1 (sub) ---- N3 (router) <--------> N4 (router) ---- N6 (sub)
           /            10.0.3.0/24 (bottleneck)    \
 N2 (sub) /                                          \ N7 (sub)

Each side is one shared CSMA segment (like a switch/LAN), so leaves find
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
    n.Create(8); // 0..7, ids match NETWORK_CONFIG.json5

    CsmaHelper lan;
    lan.SetChannelAttribute("DataRate", StringValue(lanRate));
    lan.SetChannelAttribute("Delay", TimeValue(MilliSeconds(delayMs)));

    CsmaHelper bottleneck;
    bottleneck.SetChannelAttribute("DataRate", StringValue(bottleneckRate));
    bottleneck.SetChannelAttribute("Delay", TimeValue(MilliSeconds(delayMs)));

    // 10.0.1.0/24: left LAN (N0, N1, N2, N3)
    NetDeviceContainer left = lan.Install(NodeContainer(n.Get(0), n.Get(1), n.Get(2), n.Get(3)));
    // 10.0.2.0/24: right LAN (N4, N5, N6, N7)
    NetDeviceContainer right = lan.Install(NodeContainer(n.Get(4), n.Get(5), n.Get(6), n.Get(7)));
    // 10.0.3.0/24: bottleneck (N3, N4)
    NetDeviceContainer mid = bottleneck.Install(NodeContainer(n.Get(3), n.Get(4)));

    TapBridgeHelper tb;
    tb.SetAttribute("Mode", StringValue("UseBridge"));

    // Left leaves
    tb.SetAttribute("DeviceName", StringValue("tap_0_0")); tb.Install(n.Get(0), left.Get(0));
    tb.SetAttribute("DeviceName", StringValue("tap_1_0")); tb.Install(n.Get(1), left.Get(1));
    tb.SetAttribute("DeviceName", StringValue("tap_2_0")); tb.Install(n.Get(2), left.Get(2));

    // Router N3: endpoint 0 = left LAN, endpoint 1 = bottleneck
    tb.SetAttribute("DeviceName", StringValue("tap_3_0")); tb.Install(n.Get(3), left.Get(3));
    tb.SetAttribute("DeviceName", StringValue("tap_3_1")); tb.Install(n.Get(3), mid.Get(0));

    // Router N4: endpoint 0 = right LAN, endpoint 1 = bottleneck
    tb.SetAttribute("DeviceName", StringValue("tap_4_0")); tb.Install(n.Get(4), right.Get(0));
    tb.SetAttribute("DeviceName", StringValue("tap_4_1")); tb.Install(n.Get(4), mid.Get(1));

    // Right leaves
    tb.SetAttribute("DeviceName", StringValue("tap_5_0")); tb.Install(n.Get(5), right.Get(1));
    tb.SetAttribute("DeviceName", StringValue("tap_6_0")); tb.Install(n.Get(6), right.Get(2));
    tb.SetAttribute("DeviceName", StringValue("tap_7_0")); tb.Install(n.Get(7), right.Get(3));

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
