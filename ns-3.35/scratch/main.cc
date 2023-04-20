#include <cstdlib>
#include <time.h>
#include <stdio.h>
#include <string>
#include <fstream>

#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/network-module.h"
#include "ns3/packet-sink.h"
#include "ns3/error-model.h"
#include "ns3/udp-header.h"
#include "ns3/enum.h"
#include "ns3/event-id.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"

#define MAPPER_COUNT 3

using namespace ns3;
using namespace std;
NS_LOG_COMPONENT_DEFINE("WifiTopology");

void ThroughputMonitor(FlowMonitorHelper *fmhelper, Ptr<FlowMonitor> flowMon, double em)
{
    uint16_t i = 0;

    std::map<FlowId, FlowMonitor::FlowStats> flowStats = flowMon->GetFlowStats();

    Ptr<Ipv4FlowClassifier> classing = DynamicCast<Ipv4FlowClassifier>(fmhelper->GetClassifier());
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator stats = flowStats.begin(); stats != flowStats.end(); ++stats)
    {
        Ipv4FlowClassifier::FiveTuple fiveTuple = classing->FindFlow(stats->first);

        std::cout << "Flow ID			: " << stats->first << " ; " << fiveTuple.sourceAddress << " -----> " << fiveTuple.destinationAddress << std::endl;
        std::cout << "Tx Packets = " << stats->second.txPackets << std::endl;
        std::cout << "Rx Packets = " << stats->second.rxPackets << std::endl;
        std::cout << "Duration		: " << (stats->second.timeLastRxPacket.GetSeconds() - stats->second.timeFirstTxPacket.GetSeconds()) << std::endl;
        std::cout << "Last Received Packet	: " << stats->second.timeLastRxPacket.GetSeconds() << " Seconds" << std::endl;
        std::cout << "Throughput: " << stats->second.rxBytes * 8.0 / (stats->second.timeLastRxPacket.GetSeconds() - stats->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps" << std::endl;

        i++;

        std::cout << "---------------------------------------------------------------------------" << std::endl;
    }

    Simulator::Schedule(Seconds(10), &ThroughputMonitor, fmhelper, flowMon, em);
}

void AverageDelayMonitor(FlowMonitorHelper *fmhelper, Ptr<FlowMonitor> flowMon, double em)
{
    uint16_t i = 0;

    std::map<FlowId, FlowMonitor::FlowStats> flowStats = flowMon->GetFlowStats();
    Ptr<Ipv4FlowClassifier> classing = DynamicCast<Ipv4FlowClassifier>(fmhelper->GetClassifier());
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator stats = flowStats.begin(); stats != flowStats.end(); ++stats)
    {
        Ipv4FlowClassifier::FiveTuple fiveTuple = classing->FindFlow(stats->first);
        std::cout << "Flow ID			: " << stats->first << " ; " << fiveTuple.sourceAddress << " -----> " << fiveTuple.destinationAddress << std::endl;
        std::cout << "Tx Packets = " << stats->second.txPackets << std::endl;
        std::cout << "Rx Packets = " << stats->second.rxPackets << std::endl;
        std::cout << "Duration		: " << (stats->second.timeLastRxPacket.GetSeconds() - stats->second.timeFirstTxPacket.GetSeconds()) << std::endl;
        std::cout << "Last Received Packet	: " << stats->second.timeLastRxPacket.GetSeconds() << " Seconds" << std::endl;
        std::cout << "Sum of e2e Delay: " << stats->second.delaySum.GetSeconds() << " s" << std::endl;
        std::cout << "Average of e2e Delay: " << stats->second.delaySum.GetSeconds() / stats->second.rxPackets << " s" << std::endl;

        i++;

        std::cout << "---------------------------------------------------------------------------" << std::endl;
    }

    Simulator::Schedule(Seconds(10), &AverageDelayMonitor, fmhelper, flowMon, em);
}

class MyHeader : public Header
{
public:
    MyHeader();
    virtual ~MyHeader();
    void SetData(uint16_t data);
    uint16_t GetData(void) const;
    static TypeId GetTypeId(void);
    virtual TypeId GetInstanceTypeId(void) const;
    virtual void Print(std::ostream &os) const;
    virtual void Serialize(Buffer::Iterator start) const;
    virtual uint32_t Deserialize(Buffer::Iterator start);
    virtual uint32_t GetSerializedSize(void) const;

private:
    uint16_t m_data;
};

MyHeader::MyHeader()
{
}

MyHeader::~MyHeader()
{
}

TypeId
MyHeader::GetTypeId(void)
{
    static TypeId tid = TypeId("ns3::MyHeader")
                            .SetParent<Header>()
                            .AddConstructor<MyHeader>();
    return tid;
}

TypeId
MyHeader::GetInstanceTypeId(void) const
{
    return GetTypeId();
}

void MyHeader::Print(std::ostream &os) const
{
    os << "data = " << m_data << endl;
}

uint32_t
MyHeader::GetSerializedSize(void) const
{
    return 2;
}

void MyHeader::Serialize(Buffer::Iterator start) const
{
    start.WriteHtonU16(m_data);
}

uint32_t
MyHeader::Deserialize(Buffer::Iterator start)
{
    m_data = start.ReadNtohU16();

    return 2;
}

void MyHeader::SetData(uint16_t data)
{
    m_data = data;
}

uint16_t
MyHeader::GetData(void) const
{
    return m_data;
}

class master : public Application
{
public:
    master(uint16_t port, Ipv4InterfaceContainer &ip, Ipv4InterfaceContainer &targetIp);
    virtual ~master();

private:
    virtual void StartApplication(void);
    void HandleRead(Ptr<Socket> socket);
    void SendDataToMapper(Ptr<Socket> socket, uint16_t data);

    uint16_t port;
    Ipv4InterfaceContainer ip;
    Ipv4InterfaceContainer &targetIp;
    Ptr<Socket> socket;
    Ptr<Socket> mapper_socket;
};

class client : public Application
{
public:
    client(uint16_t port, Ipv4InterfaceContainer &ip, Ipv4InterfaceContainer &targetIp);
    virtual ~client();

private:
    virtual void StartApplication(void);
    void HandleRead(Ptr<Socket> socket);

    uint16_t port;
    Ipv4InterfaceContainer &ip;
    Ipv4InterfaceContainer &targetIp;
    Ptr<Socket> master_socket;
    Ptr<Socket> socket;
};

class mapper : public Application
{
public:
    mapper(uint16_t port, Ipv4InterfaceContainer &ip, map<int, char> decode_map, Ipv4InterfaceContainer &targetIp, int id);
    virtual ~mapper();

private:
    virtual void StartApplication(void);
    void HandleRead(Ptr<Socket> socket);
    void SendDataToClient(Ptr<Socket> socket, char data);

    uint16_t port;
    Ipv4InterfaceContainer ip;
    map<int, char> decode_map;
    Ipv4InterfaceContainer &targetIp;
    int id;
    Ptr<Socket> socket;
    Ptr<Socket> client_socket;
};

int main(int argc, char *argv[])
{
    double error = 0.000001;
    string bandwidth = "1Mbps";
    bool verbose = true;
    double duration = 60.0;
    bool tracing = false;

    std::map<int, char> decode_maps[3];
    // std::map<int, char> map2;
    // std::map<int, char> map3;

    for (int i = 0; i < 26; i++)
    {
        if (i % 3 == 0)
        {
            decode_maps[0].insert(pair<int, char>(i, 'a' + i));
        }
        if (i % 3 == 1)
        {
            decode_maps[1].insert(pair<int, char>(i, 'a' + i));
        }
        if (i % 3 == 2)
        {
            decode_maps[2].insert(pair<int, char>(i, 'a' + i));
        }
    }

    srand(time(NULL));

    CommandLine cmd(__FILE__);
    cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
    cmd.AddValue("tracing", "Enable pcap tracing", tracing);

    cmd.Parse(argc, argv);

    if (verbose)
    {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

    NodeContainer wifiStaNodeClient;
    wifiStaNodeClient.Create(1);

    NodeContainer wifiStaNodeMaster;
    wifiStaNodeMaster.Create(1);

    NodeContainer wifiStaNodeMappers;
    wifiStaNodeMappers.Create(3);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();

    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDeviceClient;
    staDeviceClient = wifi.Install(phy, mac, wifiStaNodeClient);

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));

    NetDeviceContainer staDeviceMaster;
    staDeviceMaster = wifi.Install(phy, mac, wifiStaNodeMaster);

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDeviceMappers;
    staDeviceMappers = wifi.Install(phy, mac, wifiStaNodeMappers);

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));

    Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
    em->SetAttribute("ErrorRate", DoubleValue(error));
    phy.SetErrorRateModel("ns3::YansErrorRateModel");

    MobilityHelper mobility;

    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
    mobility.Install(wifiStaNodeClient);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodeMaster);
    mobility.Install(wifiStaNodeMappers);

    InternetStackHelper stack;
    stack.Install(wifiStaNodeClient);
    stack.Install(wifiStaNodeMaster);
    stack.Install(wifiStaNodeMappers);

    Ipv4AddressHelper address;

    Ipv4InterfaceContainer staNodeClientInterface;
    Ipv4InterfaceContainer staNodesMasterInterface;
    Ipv4InterfaceContainer staNodesMapperInterface;

    address.SetBase("10.1.3.0", "255.255.255.0");

    staNodeClientInterface = address.Assign(staDeviceClient);
    staNodesMasterInterface = address.Assign(staDeviceMaster);

    staNodesMapperInterface = address.Assign(staDeviceMappers);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 1102;

    Ptr<client> clientApp = CreateObject<client>(port, staNodeClientInterface, staNodesMasterInterface);

    wifiStaNodeClient.Get(0)->AddApplication(clientApp);
    clientApp->SetStartTime(Seconds(0.0));
    clientApp->SetStopTime(Seconds(duration));

    Ptr<master> masterApp = CreateObject<master>(port, staNodesMasterInterface, staNodesMapperInterface);
    wifiStaNodeMaster.Get(0)->AddApplication(masterApp);
    masterApp->SetStartTime(Seconds(0.0));
    masterApp->SetStopTime(Seconds(duration));

    for (int i = 0; i < MAPPER_COUNT; i++)
    {
        Ptr<mapper> mapperApp = CreateObject<mapper>(port, staNodesMapperInterface, decode_maps[i], staNodeClientInterface, i + 1);
        wifiStaNodeMappers.Get(i)->AddApplication(mapperApp);
        mapperApp->SetStartTime(Seconds(0.0));
        mapperApp->SetStopTime(Seconds(duration));
    }

    NS_LOG_INFO("Run Simulation");

    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll();

    ThroughputMonitor(&flowHelper, flowMonitor, error);
    AverageDelayMonitor(&flowHelper, flowMonitor, error);

    Simulator::Stop(Seconds(duration));
    Simulator::Run();

    return 0;
}

client::client(uint16_t port, Ipv4InterfaceContainer &ip, Ipv4InterfaceContainer &targetIp)
    : port(port),
      ip(ip),
      targetIp(targetIp)
{
    std::srand(time(0));
}

client::~client()
{
}

static void GenerateTraffic(Ptr<Socket> socket, uint16_t data)
{
    Ptr<Packet> packet = new Packet();
    MyHeader m;
    m.SetData(data);

    packet->AddHeader(m);
    packet->Print(std::cout);
    socket->Send(packet);

    Simulator::Schedule(Seconds(0.1), &GenerateTraffic, socket, rand() % 26);
}

void client::HandleRead(Ptr<Socket> socket)
{
    Ptr<Packet> packet;

    while ((packet = socket->Recv()))
    {
        if (packet->GetSize() == 0)
        {
            break;
        }

        MyHeader destinationHeader;

        packet->RemoveHeader(destinationHeader);
        std::cout << destinationHeader.GetData() << endl;
        std::cout << "Client received data:  " << (char)destinationHeader.GetData() << endl;
    }
}

void client::StartApplication(void)
{

    socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    InetSocketAddress local = InetSocketAddress(ip.GetAddress(0), port);
    socket->Bind(local);

    socket->SetRecvCallback(MakeCallback(&client::HandleRead, this));

    master_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    InetSocketAddress sockAddr(targetIp.GetAddress(0), port);
    master_socket->Connect(sockAddr);

    GenerateTraffic(master_socket, 0);
}

master::master(uint16_t port, Ipv4InterfaceContainer &ip, Ipv4InterfaceContainer &targetIp)
    : port(port),
      ip(ip),
      targetIp(targetIp)
{
    std::srand(time(0));
}

master::~master()
{
}

void master::StartApplication(void)
{
    socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    InetSocketAddress local = InetSocketAddress(ip.GetAddress(0), port);
    socket->Bind(local);

    // for (int i = 0; i < MAPPER_COUNT; i++)
    // {
    mapper_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    InetSocketAddress mapper_addr(targetIp.GetAddress(0), port);

    mapper_socket->Connect(mapper_addr);
    // }

    socket->SetRecvCallback(MakeCallback(&master::HandleRead, this));
}

void master::SendDataToMapper(Ptr<Socket> socket, uint16_t data)
{
    Ptr<Packet> packet = new Packet();
    MyHeader m;
    m.SetData(data);

    packet->AddHeader(m);
    packet->Print(std::cout);
    socket->Send(packet);
}

void master::HandleRead(Ptr<Socket> socket)
{
    Ptr<Packet> packet;

    while ((packet = socket->Recv()))
    {
        if (packet->GetSize() == 0)
        {
            break;
        }

        MyHeader destinationHeader;

        packet->RemoveHeader(destinationHeader);
        std::cout << destinationHeader.GetData() << endl;
        SendDataToMapper(mapper_socket, destinationHeader.GetData());
    }
}

mapper::mapper(uint16_t port, Ipv4InterfaceContainer &ip, map<int, char> decode_map, Ipv4InterfaceContainer &targetIp, int id)
    : port(port),
      ip(ip),
      decode_map(decode_map),
      targetIp(targetIp),
      id(id)
{
    std::srand(time(0));
}

mapper::~mapper()
{
}

void mapper::StartApplication(void)
{
    socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    InetSocketAddress local = InetSocketAddress(ip.GetAddress(0), port);
    socket->Bind(local);
    socket->Listen();

    client_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    InetSocketAddress sockAddr(targetIp.GetAddress(0), port);

    client_socket->Connect(sockAddr);

    socket->SetRecvCallback(MakeCallback(&mapper::HandleRead, this));
}

void mapper::SendDataToClient(Ptr<Socket> socket, char data)
{
    Ptr<Packet> packet = new Packet();
    MyHeader m;
    m.SetData(data);

    packet->AddHeader(m);
    packet->Print(std::cout);
    socket->Send(packet);
}

void mapper::HandleRead(Ptr<Socket> socket)
{
    Ptr<Packet> packet;

    while ((packet = socket->Recv()))
    {
        if (packet->GetSize() == 0)
        {
            break;
        }

        MyHeader destinationHeader;
        packet->RemoveHeader(destinationHeader);
        std::cout << "Mapper received data:  " << destinationHeader.GetData() << endl;

        if (decode_map[destinationHeader.GetData()])
        {
            SendDataToClient(client_socket, decode_map[destinationHeader.GetData()]);
        }
        else
        {
            cout << "YYYYYYYYYYYYYYYYYYYYYYYYYYYYYY  " << id << endl;
        }
    }
}