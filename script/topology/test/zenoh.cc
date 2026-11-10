#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/network-module.h"
#include "ns3/tap-bridge-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TapCsmaVirtualMachineExample");

int main(int argc, char* argv[]) {
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  // 實時模擬 + 啟用檢查碼
  GlobalValue::Bind("SimulatorImplementationType", StringValue("ns3::RealtimeSimulatorImpl"));
  GlobalValue::Bind("ChecksumEnabled", BooleanValue(true));

  // 節點對應 Docker 節點編號：0..6
  // 0↔1(10.0.9.*), 1↔2(10.0.1.*), 1↔4(10.0.4.*),
  // 2↔6(10.0.2.*), 3↔6(10.0.6.*), 3↔4(10.0.5.*), 3↔5(10.0.3.*)
  NodeContainer n;
  n.Create(7); // 0..6

  // ------------------------------
  // L2 連線（CSMA）
  // 可依你先前設定保留帶寬/延遲：10.0.4.*、10.0.5.* 用 5Mbps；其餘 100Mbps
  // ------------------------------

  // 10.0.9.* : 0 <-> 1
  CsmaHelper csma01;
  csma01.SetChannelAttribute("DataRate", StringValue("100Mbps"));
  csma01.SetChannelAttribute("Delay", TimeValue(MilliSeconds(1)));
  NetDeviceContainer d01 = csma01.Install(NodeContainer(n.Get(0), n.Get(1)));

  // 10.0.1.* : 1 <-> 2
  CsmaHelper csma12;
  csma12.SetChannelAttribute("DataRate", StringValue("100Mbps"));
  csma12.SetChannelAttribute("Delay", TimeValue(MilliSeconds(1)));
  NetDeviceContainer d12 = csma12.Install(NodeContainer(n.Get(1), n.Get(2)));

  // 10.0.4.* : 1 <-> 4
  CsmaHelper csma14;
  csma14.SetChannelAttribute("DataRate", StringValue("100Mbps"));
  csma14.SetChannelAttribute("Delay", TimeValue(MilliSeconds(1)));
  NetDeviceContainer d14 = csma14.Install(NodeContainer(n.Get(1), n.Get(4)));

  // 10.0.2.* : 2 <-> 6
  CsmaHelper csma26;
  csma26.SetChannelAttribute("DataRate", StringValue("100Mbps"));
  csma26.SetChannelAttribute("Delay", TimeValue(MilliSeconds(1)));
  NetDeviceContainer d26 = csma26.Install(NodeContainer(n.Get(2), n.Get(6)));

  // 10.0.6.* : 3 <-> 6
  CsmaHelper csma36;
  csma36.SetChannelAttribute("DataRate", StringValue("100Mbps"));
  csma36.SetChannelAttribute("Delay", TimeValue(MilliSeconds(1)));
  NetDeviceContainer d36 = csma36.Install(NodeContainer(n.Get(3), n.Get(6)));

  // 10.0.5.* : 3 <-> 4
  CsmaHelper csma34;
  csma34.SetChannelAttribute("DataRate", StringValue("3Mbps"));
  csma34.SetChannelAttribute("Delay", TimeValue(MilliSeconds(1)));
  NetDeviceContainer d34 = csma34.Install(NodeContainer(n.Get(3), n.Get(4)));

  // 10.0.3.* : 3 <-> 5
  CsmaHelper csma35;
  csma35.SetChannelAttribute("DataRate", StringValue("100Mbps"));
  csma35.SetChannelAttribute("Delay", TimeValue(MilliSeconds(1)));
  NetDeviceContainer d35 = csma35.Install(NodeContainer(n.Get(3), n.Get(5)));

  // ------------------------------
  // TapBridge 對接（UseBridge 模式）
  // tap 名稱要與 run_zenoh.sh 一致
  // ------------------------------
  TapBridgeHelper tb;
  tb.SetAttribute("Mode", StringValue("UseBridge"));

  // Node 0 (10.0.9.*)
  tb.SetAttribute("DeviceName", StringValue("tap_0_0")); tb.Install(n.Get(0), d01.Get(0));

  // Node 1 (10.0.1.*, 10.0.4.*, 10.0.9.*)  對應 tap_1_0 / tap_1_1 / tap_1_2
  tb.SetAttribute("DeviceName", StringValue("tap_1_0")); tb.Install(n.Get(1), d12.Get(0)); // 10.0.1.*
  tb.SetAttribute("DeviceName", StringValue("tap_1_1")); tb.Install(n.Get(1), d14.Get(0)); // 10.0.4.*
  tb.SetAttribute("DeviceName", StringValue("tap_1_2")); tb.Install(n.Get(1), d01.Get(1)); // 10.0.9.*

  // Node 2 (10.0.1.*, 10.0.2.*)  對應 tap_2_0 / tap_2_1
  tb.SetAttribute("DeviceName", StringValue("tap_2_0")); tb.Install(n.Get(2), d12.Get(1)); // 10.0.1.*
  tb.SetAttribute("DeviceName", StringValue("tap_2_1")); tb.Install(n.Get(2), d26.Get(0)); // 10.0.2.*

  // Node 3 (10.0.6.*, 10.0.3.*, 10.0.5.*)  對應 tap_3_0 / tap_3_1 / tap_3_2
  tb.SetAttribute("DeviceName", StringValue("tap_3_0")); tb.Install(n.Get(3), d36.Get(0)); // 10.0.6.*
  tb.SetAttribute("DeviceName", StringValue("tap_3_1")); tb.Install(n.Get(3), d35.Get(0)); // 10.0.3.*
  tb.SetAttribute("DeviceName", StringValue("tap_3_2")); tb.Install(n.Get(3), d34.Get(0)); // 10.0.5.*

  // Node 4 (10.0.4.*, 10.0.5.*)  對應 tap_4_0 / tap_4_1
  tb.SetAttribute("DeviceName", StringValue("tap_4_0")); tb.Install(n.Get(4), d14.Get(1)); // 10.0.4.*
  tb.SetAttribute("DeviceName", StringValue("tap_4_1")); tb.Install(n.Get(4), d34.Get(1)); // 10.0.5.*

  // Node 5 (10.0.3.*)  對應 tap_5_0
  tb.SetAttribute("DeviceName", StringValue("tap_5_0")); tb.Install(n.Get(5), d35.Get(1)); // 10.0.3.*

  // Node 6 (10.0.2.*, 10.0.6.*)  對應 tap_6_0 / tap_6_1
  tb.SetAttribute("DeviceName", StringValue("tap_6_0")); tb.Install(n.Get(6), d26.Get(1)); // 10.0.2.*
  tb.SetAttribute("DeviceName", StringValue("tap_6_1")); tb.Install(n.Get(6), d36.Get(1)); // 10.0.6.*

  // 模擬 10 分鐘
  Simulator::Stop(Seconds(600.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}