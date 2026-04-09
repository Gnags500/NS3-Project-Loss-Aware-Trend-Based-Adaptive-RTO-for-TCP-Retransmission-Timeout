
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/rtt-improved-estimator.h"
#include "ns3/rtt-proposed.h"
#include "ns3/error-model.h"

#include <cmath>
#include <fstream>
#include <sys/stat.h>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WiredSim");

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
    std::string algo       = "paper";
    std::string outputFile = "report/csv/wired-results.csv";
    double      simTime    = 40.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("nodes",   "Total end-nodes (senders + receivers)", nNodes);
    cmd.AddValue("flows",   "Number of TCP flows",                   nFlows);
    cmd.AddValue("pps",     "Packets per second per flow",           pps);
    cmd.AddValue("algo",    "RTO algorithm: paper | proposed",       algo);
    cmd.AddValue("output",  "Output CSV file",                       outputFile);
    cmd.AddValue("simtime", "Simulation duration (s)",               simTime);
    cmd.Parse(argc, argv);

    NS_LOG_UNCOND("[WiredSim] algo=" << algo
                  << "  nodes=" << nNodes
                  << "  flows=" << nFlows
                  << "  pps="   << pps);

    EnsureDir("report");
    EnsureDir("report/csv");

    // ── RTO algorithm selection ───────────────────────────────────────────────
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

    uint32_t nSenders   = std::max(1u, nNodes / 2);
    uint32_t nReceivers = std::max(1u, nNodes / 2);

    NodeContainer senders, routers, receivers;
    senders.Create(nSenders);
    routers.Create(2);
    receivers.Create(nReceivers);

    InternetStackHelper internet;
    internet.Install(senders);
    internet.Install(routers);
    internet.Install(receivers);

    PointToPointHelper access;
    access.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    access.SetChannelAttribute("Delay",   StringValue("1ms"));

    PointToPointHelper bottle;
    bottle.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    bottle.SetChannelAttribute("Delay",   StringValue("10ms"));
    bottle.SetQueue("ns3::DropTailQueue<Packet>",
                    "MaxSize", QueueSizeValue(QueueSize("100p")));
    bottle.SetDeviceAttribute("ReceiveErrorModel",
    PointerValue(CreateObjectWithAttributes<RateErrorModel>(
        "ErrorRate", DoubleValue(0.08),   
        "ErrorUnit", StringValue("ERROR_UNIT_PACKET"))));

    Ipv4AddressHelper ipv4;
    std::vector<Ipv4Address> dstAddr(nReceivers);

    for (uint32_t i = 0; i < nSenders; i++)
    {
        std::ostringstream sub;
        sub << "10.1." << (i % 254 + 1) << ".0";
        ipv4.SetBase(sub.str().c_str(), "255.255.255.0");
        ipv4.Assign(access.Install(NodeContainer(senders.Get(i), routers.Get(0))));
    }

    ipv4.SetBase("10.2.1.0", "255.255.255.0");
    ipv4.Assign(bottle.Install(NodeContainer(routers.Get(0), routers.Get(1))));

    for (uint32_t i = 0; i < nReceivers; i++)
    {
        std::ostringstream sub;
        sub << "10.3." << (i % 254 + 1) << ".0";
        ipv4.SetBase(sub.str().c_str(), "255.255.255.0");
        auto dev   = access.Install(NodeContainer(routers.Get(1), receivers.Get(i)));
        dstAddr[i] = ipv4.Assign(dev).GetAddress(1);
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    const uint16_t basePort = 9000;
    const uint32_t pktSize  = 512; 

    for (uint32_t f = 0; f < nFlows; f++)
    {
        uint32_t srcIdx = f % nSenders;
        uint32_t dstIdx = f % nReceivers;
        uint16_t port   = basePort + f;

        PacketSinkHelper sink("ns3::TcpSocketFactory",
                              InetSocketAddress(Ipv4Address::GetAny(), port));
        sink.Install(receivers.Get(dstIdx)).Start(Seconds(0.0));

        double   bps = static_cast<double>(pps) * pktSize * 8.0;
        std::ostringstream rateStr;
        rateStr << static_cast<uint64_t>(bps) << "bps";

        OnOffHelper src("ns3::TcpSocketFactory",
                        InetSocketAddress(dstAddr[dstIdx], port));
        src.SetAttribute("DataRate",   StringValue(rateStr.str()));
        src.SetAttribute("PacketSize", UintegerValue(pktSize));
        src.SetAttribute("OnTime",
                         StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        src.SetAttribute("OffTime",
                         StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        auto app = src.Install(senders.Get(srcIdx));
        app.Start(Seconds(1.0));
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

    double measureTime   = simTime - 1.0; // exclude warm-up second
    double throughput    = (totalBytes * 8.0) / (measureTime * 1e6); // Mbps
    double delay_ms      = (cntDelay > 0.0) ? sumDelay / cntDelay : 0.0;
    double pdr           = (totalTx > 0.0) ? totalRx / totalTx : 0.0;
    double drop_ratio    = (totalTx > 0.0) ? totalDrop / totalTx : 0.0;

    Simulator::Destroy();

    NS_LOG_UNCOND("  → thr=" << throughput << "Mbps  delay=" << delay_ms
                  << "ms  pdr=" << pdr << "  drop=" << drop_ratio);

    bool newFile = !std::ifstream(outputFile).good();
    std::ofstream csv(outputFile, std::ios::app);
    if (newFile)
        csv << "algo,nodes,flows,pps,throughput_mbps,delay_ms,pdr,drop_ratio\n";

    csv << algo       << ","
        << nNodes     << ","
        << nFlows     << ","
        << pps        << ","
        << throughput << ","
        << delay_ms   << ","
        << pdr        << ","
        << drop_ratio << "\n";

    return 0;
}
