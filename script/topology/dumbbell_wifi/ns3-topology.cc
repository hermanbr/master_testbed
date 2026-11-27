#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/tap-bridge-module.h"
#include "ns3/wifi-module.h"

#include <cstdio>

using namespace ns3;

/*
DUMBBELL TOPOLOGY, WiFi access (802.11ax, 5 GHz)

 N0 (pub) \  BSS "left"                    BSS "right" / N5 (pub)
           \ 10.0.1.0/24       10.0.3.0/24 10.0.2.0/24 /
 N1 (sub) ---- N3 (AP+router) <-- CSMA --> N4 (AP+router) ---- N6 (sub)
           /  ch 36                              ch 44   \
 N2 (sub) /                                               \ N7 (sub)

Leaves are WiFi stations, N3/N4 are the APs. Each BSS has its own channel
object, so the two sides never interfere. The router-router bottleneck is wired.

TapBridge modes:
- STAs can't use UseBridge (StaWifiMac has no SendFrom), so they use UseLocal.
  The AP only accepts frames from associated MACs, so the STA MAC must equal the
  container MAC from the start: both use 02:00:00:00:<node>:<endpoint idx>
  (the launcher sets the container side).
- APs and the bottleneck use UseBridge.

TAP names: tap_<node>_<endpoint index>, must match NETWORK_CONFIG.json5.
*/

NS_LOG_COMPONENT_DEFINE("DumbbellWifi");

// Give a STA a fixed MAC (must be done before association, i.e. before Run)
static void
SetStaMac(Ptr<NetDevice> dev, uint32_t node, uint32_t idx)
{
    char buf[18];
    std::snprintf(buf, sizeof(buf), "02:00:00:00:%02x:%02x", node, idx);
    Mac48Address mac(buf);
    Ptr<WifiMac> wifiMac = DynamicCast<WifiNetDevice>(dev)->GetMac();
    wifiMac->SetAddress(mac);
    wifiMac->GetFrameExchangeManager()->SetAddress(mac);
}

int
main(int argc, char* argv[])
{
    std::string bottleneckRate = "10Mbps";
    uint32_t delayMs = 1;
    double simTime = 600.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("bottleneckRate", "Data rate of the router-router link", bottleneckRate);
    cmd.AddValue("delayMs", "One-way delay of the router-router link (ms)", delayMs);
    cmd.AddValue("simTime", "Simulation time (s)", simTime);
    cmd.Parse(argc, argv);

    GlobalValue::Bind("SimulatorImplementationType", StringValue("ns3::RealtimeSimulatorImpl"));
    GlobalValue::Bind("ChecksumEnabled", BooleanValue(true));

    NodeContainer n;
    n.Create(8); // 0..7, ids match NETWORK_CONFIG.json5

    // Positions: APs at x=0 / x=100, STAs 5 m from their AP
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> pos = CreateObject<ListPositionAllocator>();
    pos->Add(Vector(5, 0, 0));    // N0
    pos->Add(Vector(0, 5, 0));    // N1
    pos->Add(Vector(-5, 0, 0));   // N2
    pos->Add(Vector(0, 0, 0));    // N3 (AP left)
    pos->Add(Vector(100, 0, 0));  // N4 (AP right)
    pos->Add(Vector(105, 0, 0));  // N5
    pos->Add(Vector(100, 5, 0));  // N6
    pos->Add(Vector(95, 0, 0));   // N7
    mobility.SetPositionAllocator(pos);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(n);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211ax); // default rate manager: IdealWifiManager
    WifiMacHelper mac;

    // 10.0.1.0/24: left BSS (AP N3, STAs N0, N1, N2) on its own channel
    YansWifiPhyHelper phyLeft;
    phyLeft.SetChannel(YansWifiChannelHelper::Default().Create());
    phyLeft.Set("ChannelSettings", StringValue("{36, 20, BAND_5GHZ, 0}"));
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(Ssid("left")));
    NetDeviceContainer apLeft = wifi.Install(phyLeft, mac, n.Get(3));
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(Ssid("left")));
    NetDeviceContainer staLeft = wifi.Install(phyLeft, mac, NodeContainer(n.Get(0), n.Get(1), n.Get(2)));

    // 10.0.2.0/24: right BSS (AP N4, STAs N5, N6, N7) on its own channel
    YansWifiPhyHelper phyRight;
    phyRight.SetChannel(YansWifiChannelHelper::Default().Create());
    phyRight.Set("ChannelSettings", StringValue("{44, 20, BAND_5GHZ, 0}"));
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(Ssid("right")));
    NetDeviceContainer apRight = wifi.Install(phyRight, mac, n.Get(4));
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(Ssid("right")));
    NetDeviceContainer staRight = wifi.Install(phyRight, mac, NodeContainer(n.Get(5), n.Get(6), n.Get(7)));

    for (uint32_t i = 0; i < 3; i++)
    {
        SetStaMac(staLeft.Get(i), i, 0);      // N0..N2
        SetStaMac(staRight.Get(i), i + 5, 0); // N5..N7
    }

    // 10.0.3.0/24: bottleneck (N3, N4)
    CsmaHelper bottleneck;
    bottleneck.SetChannelAttribute("DataRate", StringValue(bottleneckRate));
    bottleneck.SetChannelAttribute("Delay", TimeValue(MilliSeconds(delayMs)));
    NetDeviceContainer mid = bottleneck.Install(NodeContainer(n.Get(3), n.Get(4)));

    TapBridgeHelper tb;

    // Leaves (STAs): UseLocal
    tb.SetAttribute("Mode", StringValue("UseLocal"));
    tb.SetAttribute("DeviceName", StringValue("tap_0_0")); tb.Install(n.Get(0), staLeft.Get(0));
    tb.SetAttribute("DeviceName", StringValue("tap_1_0")); tb.Install(n.Get(1), staLeft.Get(1));
    tb.SetAttribute("DeviceName", StringValue("tap_2_0")); tb.Install(n.Get(2), staLeft.Get(2));
    tb.SetAttribute("DeviceName", StringValue("tap_5_0")); tb.Install(n.Get(5), staRight.Get(0));
    tb.SetAttribute("DeviceName", StringValue("tap_6_0")); tb.Install(n.Get(6), staRight.Get(1));
    tb.SetAttribute("DeviceName", StringValue("tap_7_0")); tb.Install(n.Get(7), staRight.Get(2));

    // Routers: endpoint 0 = AP, endpoint 1 = bottleneck. UseBridge
    tb.SetAttribute("Mode", StringValue("UseBridge"));
    tb.SetAttribute("DeviceName", StringValue("tap_3_0")); tb.Install(n.Get(3), apLeft.Get(0));
    tb.SetAttribute("DeviceName", StringValue("tap_3_1")); tb.Install(n.Get(3), mid.Get(0));
    tb.SetAttribute("DeviceName", StringValue("tap_4_0")); tb.Install(n.Get(4), apRight.Get(0));
    tb.SetAttribute("DeviceName", StringValue("tap_4_1")); tb.Install(n.Get(4), mid.Get(1));

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
