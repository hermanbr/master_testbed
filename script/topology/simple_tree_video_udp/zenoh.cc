/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

//
// This is an illustration of how one could use virtualization techniques to
// allow running applications on virtual machines talking over simulated
// networks.
//
// The actual steps required to configure the virtual machines can be rather
// involved, so we don't go into that here.  Please have a look at one of
// our HOWTOs on the nsnam wiki for more details about how to get the
// system configured.  For an example, have a look at "HOWTO Use Linux
// Containers to set up virtual networks" which uses this code as an
// example.
//
// The configuration you are after is explained in great detail in the
// HOWTO, but looks like the following:
//
//  +----------+                           +----------+
//  | virtual  |                           | virtual  |
//  |  Linux   |                           |  Linux   |
//  |   Host   |                           |   Host   |
//  |          |                           |          |
//  |   eth0   |                           |   eth0   |
//  +----------+                           +----------+
//       |                                      |
//  +----------+                           +----------+
//  |  Linux   |                           |  Linux   |
//  |  Bridge  |                           |  Bridge  |
//  +----------+                           +----------+
//       |                                      |
//  +------------+                       +-------------+
//  | "tap-left" |                       | "tap-right" |
//  +------------+                       +-------------+
//       |           n0            n1           |
//       |       +--------+    +--------+       |
//       +-------|  tap   |    |  tap   |-------+
//               | bridge |    | bridge |
//               +--------+    +--------+
//               |  wifi  |    |  wifi  |
//               +--------+    +--------+
//                   |             |
//                 ((*))         ((*))
//
//                       Wifi LAN
//
//                        ((*))
//                          |
//                     +--------+
//                     |  wifi  |
//                     +--------+
//                     | access |
//                     |  point |
//                     +--------+
//
#include "ns3/core-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/tap-bridge-module.h"
#include "ns3/wifi-module.h"

#include <fstream>
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TapWifiVirtualMachineExample");

int
main(int argc, char* argv[])
{
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    //
    // We are interacting with the outside, real, world.  This means we have to
    // interact in real-time and therefore means we have to use the real-time
    // simulator and take the time to calculate checksums.
    //
    GlobalValue::Bind("SimulatorImplementationType", StringValue("ns3::RealtimeSimulatorImpl"));
    GlobalValue::Bind("ChecksumEnabled", BooleanValue(true));

    //
    // Create three ghost nodes.  The two first will represent the clients, while the third will represent the router
    //
    NodeContainer nodes;
    nodes.Create(5);

    //
    // We're going to use 802.11 A so set up a wifi helper to reflect that.
    //
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode",
                                 StringValue("OfdmRate54Mbps"));

    //
    // No reason for pesky access points, so we'll use an ad-hoc network.
    //
    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    //
    // Configure the physical layer.
    //
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper wifiPhy;
    wifiPhy.SetChannel(wifiChannel.Create());

    //
    // Install the wireless devices onto our ghost nodes.
    //
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    //
    // Give each WiFi device its container's MAC (02:00:00:00:<node>:00, set by launch_routers.py).
    // TapBridge UseLocal later calls SetAddress with the learned MAC, but WifiMac::SetAddress doesn't
    // update the frame exchange manager (-> NS_ASSERT linkId.has_value()). Setting both here means
    // that later call doesn't change anything.
    //
    for (uint32_t i = 0; i < devices.GetN(); ++i)
    {
        char mac[18];
        std::snprintf(mac, sizeof(mac), "02:00:00:00:%02x:00", i);
        auto dev = DynamicCast<WifiNetDevice>(devices.Get(i));
        dev->SetAddress(Mac48Address(mac));
        dev->GetMac()->GetFrameExchangeManager()->SetAddress(Mac48Address(mac));
    }

    //
    // We need location information since we are talking about wifi, so add a
    // constant position to the ghost nodes.
    //
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    positionAlloc->Add(Vector(5.0, 0.0, 0.0));
    positionAlloc->Add(Vector(2.5, 5.0, 0.0));
    positionAlloc->Add(Vector(2.5, 5.0, 0.0));
    positionAlloc->Add(Vector(2.5, 5.0, 0.0));
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    //
    // Use the TapBridgeHelper to connect to the pre-configured tap devices for
    // the left side.  We go with "UseLocal" mode since the wifi devices do not
    // support promiscuous mode (because of their natures0.  This is a special
    // case mode that allows us to extend a linux bridge into ns-3 IFF we will
    // only see traffic from one other device on that bridge.  That is the case
    // for this configuration.
    //
    TapBridgeHelper tapBridge;
    tapBridge.SetAttribute("Mode", StringValue("UseLocal"));
    tapBridge.SetAttribute("DeviceName", StringValue("tap_0_0"));
    tapBridge.Install(nodes.Get(0), devices.Get(0));

    tapBridge.SetAttribute("DeviceName", StringValue("tap_1_0"));
    tapBridge.Install(nodes.Get(1), devices.Get(1));

    tapBridge.SetAttribute("DeviceName", StringValue("tap_2_0"));
    tapBridge.Install(nodes.Get(2), devices.Get(2));

    //
    // Connect the router tap tp the third wifi device ghost node
    //
    tapBridge.SetAttribute("DeviceName", StringValue("tap_3_0"));
    tapBridge.Install(nodes.Get(3), devices.Get(3));


     tapBridge.SetAttribute("DeviceName", StringValue("tap_4_0"));
    tapBridge.Install(nodes.Get(4), devices.Get(4));
    //
    // Run the simulation for ten minutes to give the user time to play around
    //
    Simulator::Stop(Seconds(600.));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
