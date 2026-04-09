
 
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/energy-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/rtt-improved-estimator.h"
#include "ns3/rtt-proposed.h"
#include "ns3/wifi-radio-energy-model-helper.h"
#include "ns3/yans-wifi-helper.h"
 
#include <cmath>
#include <fstream>
#include <sys/stat.h>
 
using namespace ns3;
using namespace ns3::energy;
 
NS_LOG_COMPONENT_DEFINE("WirelessSim");
 
static void
EnsureDir(const std::string& path)
{
    struct stat st{};
    if (stat(path.c_str(), &st) != 0)
        mkdir(path.c_str(), 0755);
}
 
int
main(int argc, char* argv[])
{
    uint32_t    nNodes     = 40;
    uint32_t    nFlows     = 20;
    uint32_t    pps        = 200;
    double      coverage   = 500.0;   
    std::string algo       = "paper";
    std::string outputFile = "report/csv/wireless-results.csv";
    double      simTime    = 40.0;
 
    CommandLine cmd(__FILE__);
    cmd.AddValue("nodes",    "Total nodes",                         nNodes);
    cmd.AddValue("flows",    "Number of TCP flows",                 nFlows);
    cmd.AddValue("pps",      "Packets per second per flow",         pps);
    cmd.AddValue("coverage", "Grid side in metres (Tx_range=250m)", coverage);
    cmd.AddValue("algo",     "RTO algorithm: paper | proposed",     algo);
    cmd.AddValue("output",   "Output CSV file",                     outputFile);
    cmd.AddValue("simtime",  "Simulation duration (s)",             simTime);
    cmd.Parse(argc, argv);
 
    NS_LOG_UNCOND("[WirelessSim] algo=" << algo
                  << "  nodes="    << nNodes
                  << "  flows="    << nFlows
                  << "  pps="      << pps
                  << "  coverage=" << coverage << "m");
 
    EnsureDir("report");
    EnsureDir("report/csv");
 
    if (algo == "paper")
    {
        Config::SetDefault("ns3::TcpL4Protocol::RttEstimatorType",
                           TypeIdValue(RttImprovedEstimator::GetTypeId()));
    }
    else
    {
        Config::SetDefault("ns3::TcpL4Protocol::RttEstimatorType",
                           TypeIdValue(RttProposed::GetTypeId()));
        Config::SetDefault("ns3::RttProposed::WindowSize", UintegerValue(2));
        Config::SetDefault("ns3::RttProposed::Gamma",      DoubleValue(0.25));
    }
 
    Config::SetDefault("ns3::TcpL4Protocol::SocketType",
                       StringValue("ns3::TcpNewReno"));
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(512));
    Config::SetDefault("ns3::TcpSocket::SndBufSize",  UintegerValue(1 << 20));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize",  UintegerValue(1 << 20));
 
    Config::SetDefault("ns3::aodv::RoutingProtocol::HelloInterval",
                       TimeValue(Seconds(3.0)));
 
    Config::SetDefault("ns3::aodv::RoutingProtocol::MaxQueueLen",
                       UintegerValue(500));
    Config::SetDefault("ns3::aodv::RoutingProtocol::MaxQueueTime",
                       TimeValue(Seconds(10.0)));
 
    NodeContainer nodes;
    nodes.Create(nNodes);
 
    YansWifiChannelHelper channel;
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss("ns3::FriisPropagationLossModel");
    channel.AddPropagationLoss("ns3::NakagamiPropagationLossModel",
                               "m0", DoubleValue(1.0),
                               "m1", DoubleValue(1.0),
                               "m2", DoubleValue(1.0));
 
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());
    phy.Set("TxPowerStart",   DoubleValue(30.0));
    phy.Set("TxPowerEnd",     DoubleValue(30.0));
    phy.Set("RxNoiseFigure",  DoubleValue(12.0));
 
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");
 
    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");
 
    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);
 
    uint32_t gridCols = static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<double>(nNodes))));
    double   spacing  = (gridCols > 1) ? coverage / static_cast<double>(gridCols - 1) : 0.0;
 
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX",      DoubleValue(0.0),
                                  "MinY",      DoubleValue(0.0),
                                  "DeltaX",    DoubleValue(spacing),
                                  "DeltaY",    DoubleValue(spacing),
                                  "GridWidth", UintegerValue(gridCols),
                                  "LayoutType",StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);
 
    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv);
    internet.Install(nodes);
 
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);
 
    const double INITIAL_ENERGY_J = 100.0;
 
    BasicEnergySourceHelper energyHelper;
    energyHelper.Set("BasicEnergySourceInitialEnergyJ",
                     DoubleValue(INITIAL_ENERGY_J));
    EnergySourceContainer energySources = energyHelper.Install(nodes);
 
    WifiRadioEnergyModelHelper radioEnergyHelper;
    radioEnergyHelper.Install(devices, energySources);
 
    const uint16_t basePort = 9000;
    const uint32_t pktSize  = 512;
 
    double startSpread = (nFlows > 1) ? 1.0 / static_cast<double>(nFlows) : 0.0;
 
    for (uint32_t f = 0; f < nFlows; f++)
    {
        uint32_t srcIdx = f % nNodes;
        uint32_t dstIdx = (f + nNodes / 2 + 1) % nNodes;
        if (dstIdx == srcIdx)
            dstIdx = (dstIdx + 1) % nNodes;
        if (dstIdx == srcIdx)   // only when nNodes == 1
            continue;
 
        uint16_t port = basePort + f;
 
        
        PacketSinkHelper sink("ns3::TcpSocketFactory",
                              InetSocketAddress(Ipv4Address::GetAny(), port));
        sink.Install(nodes.Get(dstIdx)).Start(Seconds(0.0));
 
        
        double   bps = static_cast<double>(pps) * pktSize * 8.0;
        std::ostringstream rateStr;
        rateStr << static_cast<uint64_t>(bps) << "bps";
 
        OnOffHelper src("ns3::TcpSocketFactory",
                        InetSocketAddress(interfaces.GetAddress(dstIdx), port));
        src.SetAttribute("DataRate",   StringValue(rateStr.str()));
        src.SetAttribute("PacketSize", UintegerValue(pktSize));
        src.SetAttribute("OnTime",
                         StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        src.SetAttribute("OffTime",
                         StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        auto app = src.Install(nodes.Get(srcIdx));
        double appStart = 3.0 + static_cast<double>(f) * startSpread;
        app.Start(Seconds(appStart));
        app.Stop(Seconds(simTime));
    }
 
    FlowMonitorHelper fmHelper;
    auto monitor = fmHelper.InstallAll();
 
    Simulator::Stop(Seconds(simTime + 1.0));
    Simulator::Run();
    monitor->CheckForLostPackets();
    auto stats = monitor->GetFlowStats();
 
    double totalTx    = 0.0;
    double totalRx    = 0.0;
    double totalDrop  = 0.0;
    double totalBytes = 0.0;
    double sumDelay   = 0.0;
    double cntDelay   = 0.0;
 
    for (auto& kv : stats)
    {
        totalTx    += static_cast<double>(kv.second.txPackets);
        totalRx    += static_cast<double>(kv.second.rxPackets);
        totalDrop  += static_cast<double>(kv.second.lostPackets);
        totalBytes += static_cast<double>(kv.second.rxBytes);
        if (kv.second.rxPackets > 0)
        {
            sumDelay += kv.second.delaySum.GetSeconds() * 1000.0; // ms
            cntDelay += static_cast<double>(kv.second.rxPackets);
        }
    }
 
    double measureTime = simTime - 2.0; // exclude AODV warm-up
    double throughput  = (totalBytes * 8.0) / (measureTime * 1e6); // Mbps
    double delay_ms    = (cntDelay > 0.0) ? sumDelay / cntDelay : 0.0;
    double pdr         = (totalTx > 0.0) ? totalRx / totalTx : 0.0;
    double drop_ratio  = (totalTx > 0.0) ? totalDrop / totalTx : 0.0;
 
    double totalEnergy = 0.0;
    for (uint32_t i = 0; i < energySources.GetN(); i++)
    {
        auto src = DynamicCast<BasicEnergySource>(energySources.Get(i));
        if (src)
            totalEnergy += (INITIAL_ENERGY_J - src->GetRemainingEnergy());
    }
 
    Simulator::Destroy();
 
    NS_LOG_UNCOND("  → thr=" << throughput << "Mbps  delay=" << delay_ms
                  << "ms  pdr=" << pdr << "  drop=" << drop_ratio
                  << "  energy=" << totalEnergy << "J");
 
    bool newFile = !std::ifstream(outputFile).good();
    std::ofstream csv(outputFile, std::ios::app);
    if (newFile)
        csv << "algo,nodes,flows,pps,coverage_m,"
               "throughput_mbps,delay_ms,pdr,drop_ratio,energy_j\n";
 
    csv << algo        << ","
        << nNodes      << ","
        << nFlows      << ","
        << pps         << ","
        << coverage    << ","
        << throughput  << ","
        << delay_ms    << ","
        << pdr         << ","
        << drop_ratio  << ","
        << totalEnergy << "\n";
 
    return 0;
}
 