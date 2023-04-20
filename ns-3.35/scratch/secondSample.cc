#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h" 
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-global-routing-helper.h" 
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h" 
#include "ns3/random-variable-stream.h" 
#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/uinteger.h"
#include "ns3/boolean.h"
#include "ns3/string.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/udp-echo-helper.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/constant-position-mobility-model.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/propagation-delay-model.h"
#include "ns3/on-off-helper.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-flow-classifier.h"

using namespace ns3;

// Run single 10 seconds experiment
void experiment()
{
  // 1. Create left, right, and load balancer nodes
  NodeContainer router_node; 
  router_node.Create (1);
  NodeContainer left_nodes; 
  left_nodes.Create (5);
  NodeContainer right_nodes; 
  right_nodes.Create (5);

  // 2. Place nodes somehow, this is required by every wireless simulation
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue (0.0),
                                  "MinY", DoubleValue (0.0),
                                  "DeltaX", DoubleValue (5.0),
                                  "DeltaY", DoubleValue (10.0),
                                  "GridWidth", UintegerValue (3),
                                  "LayoutType", StringValue ("RowFirst"));
                                  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (left_nodes);
  mobility.Install (router_node);
  mobility.Install (right_nodes);

  // 3. Create propagation loss matrix
  Ptr<MatrixPropagationLossModel> lossModel = CreateObject<MatrixPropagationLossModel> ();
  lossModel->SetDefaultLoss (60); // set default loss to 60 dB (no link)
  lossModel->SetLoss (left_nodes.Get (0)->GetObject<MobilityModel> (), router_node.Get (0)->GetObject<MobilityModel> (), 50); // set symmetric loss l0 <-> r to 50 dB
  lossModel->SetLoss (left_nodes.Get (1)->GetObject<MobilityModel> (), router_node.Get (0)->GetObject<MobilityModel> (), 50); // set symmetric loss l1 <-> r to 50 dB
  lossModel->SetLoss (left_nodes.Get (2)->GetObject<MobilityModel> (), router_node.Get (0)->GetObject<MobilityModel> (), 50); // set symmetric loss l2 <-> r to 50 dB
  lossModel->SetLoss (left_nodes.Get (3)->GetObject<MobilityModel> (), router_node.Get (0)->GetObject<MobilityModel> (), 50); // set symmetric loss l3 <-> r to 50 dB
  lossModel->SetLoss (left_nodes.Get (4)->GetObject<MobilityModel> (), router_node.Get (0)->GetObject<MobilityModel> (), 50); // set symmetric loss 14 <-> r to 50 dB
  lossModel->SetLoss (right_nodes.Get (0)->GetObject<MobilityModel> (), router_node.Get (0)->GetObject<MobilityModel> (), 50); // set symmetric loss r <-> r0 to 50 dB
  lossModel->SetLoss (right_nodes.Get (1)->GetObject<MobilityModel> (), router_node.Get (0)->GetObject<MobilityModel> (), 50); // set symmetric loss r <-> r1 to 50 dB
  lossModel->SetLoss (right_nodes.Get (2)->GetObject<MobilityModel> (), router_node.Get (0)->GetObject<MobilityModel> (), 50); // set symmetric loss r <-> r2 to 50 dB
  lossModel->SetLoss (right_nodes.Get (3)->GetObject<MobilityModel> (), router_node.Get (0)->GetObject<MobilityModel> (), 50); // set symmetric loss r <-> r3 to 50 dB
  lossModel->SetLoss (right_nodes.Get (4)->GetObject<MobilityModel> (), router_node.Get (0)->GetObject<MobilityModel> (), 50); // set symmetric loss r <-> r4 to 50 dB


  // 4. Create & setup wifi channel
  Ptr<YansWifiChannel> wifiChannel = CreateObject <YansWifiChannel> ();
  wifiChannel->SetPropagationLossModel (lossModel);
  wifiChannel->SetPropagationDelayModel (CreateObject <ConstantSpeedPropagationDelayModel> ());

  // 5. Install wireless devices
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211ac);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue ("OfdmRate54Mbps"),
                                "ControlMode", StringValue ("OfdmRate54Mbps"));
  YansWifiPhyHelper wifiPhy;
  wifiPhy.SetChannel (wifiChannel);
  wifiPhy.SetErrorRateModel ("ns3::NistErrorRateModel");
  WifiMacHelper staWifiMac, apWifiMac;
  staWifiMac.SetType ("ns3::StaWifiMac");
  apWifiMac.SetType ("ns3::ApWifiMac");
  NetDeviceContainer left_devices = wifi.Install (wifiPhy, staWifiMac, left_nodes);
  NetDeviceContainer router_device = wifi.Install (wifiPhy, apWifiMac, router_node);
  NetDeviceContainer right_devices = wifi.Install (wifiPhy, staWifiMac, right_nodes);

  // 6. Install TCP/IP stack & assign IP addresses
  InternetStackHelper internet;
  internet.Install (left_nodes);
  internet.Install (router_node);
  internet.Install (right_nodes);
  
  Ipv4AddressHelper ipv4_le, ipv4_ro, ipv4_ri;  
  ipv4_le.SetBase ("10.0.1.0", "255.255.255.0");
  ipv4_ro.SetBase ("10.0.0.0", "255.255.255.0");
  ipv4_ri.SetBase ("10.0.2.0", "255.255.255.0");
  Ipv4InterfaceContainer left_addresses = ipv4_le.Assign (left_devices);
  Ipv4InterfaceContainer router_address = ipv4_ro.Assign (router_device);
  Ipv4InterfaceContainer right_addresses = ipv4_ri.Assign (right_devices);

  uint32_t payloadSize = 1472;
  std::string dataRate = "10Mbps";
  OnOffHelper server ("ns3::TcpSocketFactory", (InetSocketAddress (router_address.GetAddress (0), 9)));
  server.SetAttribute ("PacketSize", UintegerValue (payloadSize));
  server.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  server.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  server.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
  ApplicationContainer serverApp = server.Install (router_node.Get(0));

  // Configure IPv4 address
   Ipv4Address addr;

   for(int i = 0 ; i < 5; i++) 
   {
     addr = left_addresses.GetAddress(i);
     std::cout << " Left Node " << i+1 << "\t "<< "IP Address "<< addr << std::endl; 
   }
   for(int i = 0 ; i < 5; i++)
   {
     addr = right_addresses.GetAddress(i);
     std::cout << " Right Node " << i+1 << "\t "<< "IP Address "<< addr << std::endl; 
   }
   addr = router_address.GetAddress(0);
   std::cout << "Internet Stack & IPv4 address configured.." << '\n';

  AsciiTraceHelper ascii;
  wifiPhy.EnableAsciiAll (ascii.CreateFileStream ("wifi-udp-load-balancer.tr"));


  ApplicationContainer cbrApps;
  // Create traffic generator (UDP)
    uint16_t cbrPort = 12345; 
    OnOffHelper onOffHelper ("ns3::TcpSocketFactory", InetSocketAddress (right_addresses.GetAddress ((rand()%5)), cbrPort));
    // OnOffHelper onOffHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address ("10.0.0.2"), cbrPort));
    onOffHelper.SetAttribute ("PacketSize", UintegerValue (1400));
    onOffHelper.SetAttribute ("OnTime",  StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
    cbrApps.Add (onOffHelper.Install (router_node.Get (0)));

    cbrPort = 12346;
    onOffHelper = OnOffHelper("ns3::UdpSocketFactory", InetSocketAddress (router_address.GetAddress (0), cbrPort));
    // flow 1:  left node 0 -> router
    onOffHelper.SetAttribute ("DataRate", StringValue ("3000000bps"));
    onOffHelper.SetAttribute ("StartTime", TimeValue (Seconds (1.000000)));
    cbrApps.Add (onOffHelper.Install (left_nodes.Get (0)));

    // flow 2:  left node 1 -> router
    onOffHelper.SetAttribute ("DataRate", StringValue ("3001100bps"));
    onOffHelper.SetAttribute ("StartTime", TimeValue (Seconds (1.001)));
    cbrApps.Add (onOffHelper.Install (left_nodes.Get (1)));

    // flow 3:  left node 2 -> router
    onOffHelper.SetAttribute ("DataRate", StringValue ("3001100bps"));
    onOffHelper.SetAttribute ("StartTime", TimeValue (Seconds (1.002)));
    cbrApps.Add (onOffHelper.Install (left_nodes.Get (2)));

    // flow 4:  left node 3 -> router
    onOffHelper.SetAttribute ("DataRate", StringValue ("3001100bps"));
    onOffHelper.SetAttribute ("StartTime", TimeValue (Seconds (1.003)));
    cbrApps.Add (onOffHelper.Install (left_nodes.Get (3)));

    // flow 5:  left node 4 -> router
    onOffHelper.SetAttribute ("DataRate", StringValue ("3001100bps"));
    onOffHelper.SetAttribute ("StartTime", TimeValue (Seconds (1.004)));
    cbrApps.Add (onOffHelper.Install (left_nodes.Get (4)));

    std::cout << "UDP traffic generated." << '\n';

  // 8. Install FlowMonitor on all nodes
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  //9. Run simulation for 10 seconds
  Simulator::Stop (Seconds (10.0+2));
  Simulator::Run ();

  //10. Print per flow statistics
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
        if (i->first > 0)
        {
            Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
            std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
            std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
            std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
            std::cout << "  TxOffered:  " << i->second.txBytes * 8.0 / 9.0 / 1000 / 1000  << " Mbps\n";
            std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
            std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
            std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / 9.0 / 1000 / 1000  << " Mbps\n";
            std::cout << "  Time First Tx Packet: " << i->second.timeFirstTxPacket << "\n";
            std::cout << "  Time Last Tx Packet: " << i->second.timeLastTxPacket << "\n";

        }
    }

  //11. Cleanup
  Simulator::Destroy ();
}

int main (int argc, char **argv)
{
  CommandLine cmd (__FILE__);
  cmd.Parse (argc, argv);
  std::cout << ("Running Simulation:\n") << std::endl;
  experiment ();
  
  return 0;
}
