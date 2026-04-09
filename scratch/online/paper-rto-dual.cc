

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/rtt-improved-estimator.h"

#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <numeric>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("PaperRtoDual");

static const uint32_t N_FLOWS       = 9;
static const double   RAMP_INTERVAL = 10.0;   // s between flow add/remove
static const double   SIM_DURATION  = 180.0;
static const double   PEAK_END      = (N_FLOWS - 1) * RAMP_INTERVAL
                                      + RAMP_INTERVAL; // 90 s
static const uint32_t N_BUCKETS     = 21;

static std::vector<double> g_rawRttBuffer;  // in ms

static Ptr<RttMeanDeviation>    g_tradEst;
static Ptr<RttImprovedEstimator> g_imprEst;

struct OnlineRecord
{
    int    seq;
    double rtt_ms;
    double rto_trad_ms;
    double rto_impr_ms;
    double gamma_trad;
    double gamma_impr;
};

static std::vector<OnlineRecord> g_incRecords;  // increase phase
static std::vector<OnlineRecord> g_decRecords;  // decrease phase
static bool g_isIncrease = true;
static int  g_seqCounter = 0;

static void
RttChanged(Time /* oldRtt */, Time newRtt)
{
    if (newRtt > Seconds(0))
        g_rawRttBuffer.push_back(newRtt.GetMilliSeconds());
}


static void
SampleAndFeed()
{
    if (g_rawRttBuffer.empty())
        return;

    double sum = 0;
    for (double v : g_rawRttBuffer)
        sum += v;
    double avgRttMs = sum / g_rawRttBuffer.size();
    g_rawRttBuffer.clear();

    Time rtt = MilliSeconds(avgRttMs);
    g_tradEst->Measurement(rtt);
    g_imprEst->Measurement(rtt);

    double rtoTrad = (g_tradEst->GetEstimate() +
                      g_tradEst->GetVariation() * 4).GetMilliSeconds();
    double rtoImpr = (g_imprEst->GetEstimate() +
                      g_imprEst->GetVariation() * 4).GetMilliSeconds();

    double gammaTrad = (avgRttMs > 0)
                         ? 100.0 * std::abs(rtoTrad - avgRttMs) / avgRttMs
                         : 0.0;
    double gammaImpr = (avgRttMs > 0)
                         ? 100.0 * std::abs(rtoImpr - avgRttMs) / avgRttMs
                         : 0.0;

    g_seqCounter++;
    OnlineRecord rec = {g_seqCounter, avgRttMs, rtoTrad, rtoImpr,
                        gammaTrad, gammaImpr};

    if (g_isIncrease)
        g_incRecords.push_back(rec);
    else
        g_decRecords.push_back(rec);

    NS_LOG_UNCOND("  [" << (g_isIncrease ? "INC" : "DEC") << " "
                  << g_seqCounter << "] RTT=" << avgRttMs
                  << "ms  RTO=" << rtoTrad << "ms  RTO'=" << rtoImpr
                  << "ms  γ=" << gammaTrad << "%  γ'=" << gammaImpr << "%");
}

static void
StartDecreasePhase()
{
    NS_LOG_UNCOND("\n  --- Switching to DECREASE phase (t=" << PEAK_END << "s) ---");
    g_isIncrease = false;
    g_seqCounter = 0;
    g_rawRttBuffer.clear();
    g_tradEst->Reset();
    g_imprEst->Reset();
}

static void
ConnectTraces()
{
    Config::ConnectWithoutContext(
        "/NodeList/0/$ns3::TcpL4Protocol/SocketList/*/LastRTT",
        MakeCallback(&RttChanged));
    NS_LOG_UNCOND("  RTT trace connected on Node 0");
}

static void
WriteCsv(const std::string& fname, const std::vector<OnlineRecord>& data)
{
    std::ofstream f(fname);
    f << "seq,rtt_ms,rto_trad_ms,rto_impr_ms,gamma_trad,gamma_impr\n";
    for (auto& r : data)
        f << r.seq << "," << r.rtt_ms << "," << r.rto_trad_ms << ","
          << r.rto_impr_ms << "," << r.gamma_trad << "," << r.gamma_impr << "\n";
    f.close();
    NS_LOG_UNCOND("  Saved: " << fname << " (" << data.size() << " points)");
}

//══════════════════════════════════════════════════════════════════════════
int
main(int argc, char* argv[])
{
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    NS_LOG_UNCOND("============================================================");
    NS_LOG_UNCOND("Paper RTO — Online Dual-Estimator Simulation");
    NS_LOG_UNCOND("  Traditional: ns3::RttMeanDeviation (Jacobson, α=1/8, β=1/4)");
    NS_LOG_UNCOND("  Improved:    ns3::RttImprovedEstimator (adaptive α, β)");
    NS_LOG_UNCOND("  Method: 21 samples per phase, each from averaged raw RTTs");
    NS_LOG_UNCOND("============================================================");

    g_tradEst = CreateObject<RttMeanDeviation>();
    g_imprEst = CreateObject<RttImprovedEstimator>();

    Config::SetDefault("ns3::TcpL4Protocol::SocketType",
                       StringValue("ns3::TcpNewReno"));
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1000));
    Config::SetDefault("ns3::TcpSocket::SndBufSize",  UintegerValue(1u << 22));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize",  UintegerValue(1u << 22));

    NodeContainer allNodes;
    allNodes.Create(20);

    NodeContainer senders, receivers;
    for (uint32_t i = 0; i < N_FLOWS; i++)
        senders.Add(allNodes.Get(i));
    NodeContainer routers;
    routers.Add(allNodes.Get(9));
    routers.Add(allNodes.Get(10));
    for (uint32_t i = 0; i < N_FLOWS; i++)
        receivers.Add(allNodes.Get(i + 11));

    InternetStackHelper internet;
    internet.Install(allNodes);

    PointToPointHelper accessLink;
    accessLink.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    accessLink.SetChannelAttribute("Delay", StringValue("2.5ms"));
    accessLink.SetQueue("ns3::DropTailQueue<Packet>",
                        "MaxSize", QueueSizeValue(QueueSize("20p")));

    
    PointToPointHelper bottleLink;
    bottleLink.SetDeviceAttribute("DataRate", StringValue("20Mbps"));
    bottleLink.SetChannelAttribute("Delay", StringValue("15ms"));
    bottleLink.SetQueue("ns3::DropTailQueue<Packet>",
                        "MaxSize", QueueSizeValue(QueueSize("320p")));

    Ipv4AddressHelper ipv4;
    std::vector<Ipv4Address> recvAddrs(N_FLOWS);

    for (uint32_t i = 0; i < N_FLOWS; i++)
    {
        std::ostringstream sub;
        sub << "10.1." << (i + 1) << ".0";
        ipv4.SetBase(sub.str().c_str(), "255.255.255.0");
        NetDeviceContainer dev = accessLink.Install(
            NodeContainer(senders.Get(i), routers.Get(0)));
        ipv4.Assign(dev);
    }

    ipv4.SetBase("10.2.1.0", "255.255.255.0");
    NetDeviceContainer bnDev = bottleLink.Install(
        NodeContainer(routers.Get(0), routers.Get(1)));
    ipv4.Assign(bnDev);

    for (uint32_t i = 0; i < N_FLOWS; i++)
    {
        std::ostringstream sub;
        sub << "10.3." << (i + 1) << ".0";
        ipv4.SetBase(sub.str().c_str(), "255.255.255.0");
        NetDeviceContainer dev = accessLink.Install(
            NodeContainer(routers.Get(1), receivers.Get(i)));
        Ipv4InterfaceContainer iface = ipv4.Assign(dev);
        recvAddrs[i] = iface.GetAddress(1);
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t basePort = 5000;
    NS_LOG_UNCOND("\n=== Flow Schedule ===");
    for (uint32_t i = 0; i < N_FLOWS; i++)
    {
        uint16_t port = basePort + i;
        double startT = static_cast<double>(i) * RAMP_INTERVAL;
        double stopT;

        if (i == 0)
            stopT = SIM_DURATION;
        else
        {
            stopT = PEAK_END
                    + static_cast<double>(N_FLOWS - i) * RAMP_INTERVAL;
            if (stopT > SIM_DURATION)
                stopT = SIM_DURATION;
        }

        if (i == 0)
        {
            PacketSinkHelper sink("ns3::TcpSocketFactory",
                                  InetSocketAddress(Ipv4Address::GetAny(), port));
            ApplicationContainer sinkApp = sink.Install(receivers.Get(i));
            sinkApp.Start(Seconds(0.0));
            sinkApp.Stop(Seconds(stopT + 5.0));

            BulkSendHelper bulk("ns3::TcpSocketFactory",
                                InetSocketAddress(recvAddrs[i], port));
            bulk.SetAttribute("MaxBytes", UintegerValue(0));
            bulk.SetAttribute("SendSize", UintegerValue(1000));
            ApplicationContainer sApp = bulk.Install(senders.Get(i));
            sApp.Start(Seconds(startT));
            sApp.Stop(Seconds(stopT));
        }
        else
        {
            PacketSinkHelper sink("ns3::UdpSocketFactory",
                                  InetSocketAddress(Ipv4Address::GetAny(), port));
            ApplicationContainer sinkApp = sink.Install(receivers.Get(i));
            sinkApp.Start(Seconds(std::max(0.0, startT - 0.1)));
            sinkApp.Stop(Seconds(stopT + 5.0));

            OnOffHelper onoff("ns3::UdpSocketFactory",
                              InetSocketAddress(recvAddrs[i], port));
            onoff.SetConstantRate(DataRate("2500kbps"), 1000);
            ApplicationContainer sApp = onoff.Install(senders.Get(i));
            sApp.Start(Seconds(startT));
            sApp.Stop(Seconds(stopT));
        }

        NS_LOG_UNCOND("  Flow " << i << ": " << startT << "s → " << stopT
                       << "s" << (i == 0 ? "  [TCP/FTP]" : "  [UDP/CBR]"));
    }

    Simulator::Schedule(Seconds(0.1), &ConnectTraces);

    double incStart = 2.0;
    double incEnd   = PEAK_END - 2.0;  // 88s
    double incStep  = (incEnd - incStart) / (N_BUCKETS - 1);
    NS_LOG_UNCOND("\n=== Increase phase: " << N_BUCKETS << " samples from "
                  << incStart << "s to " << incEnd << "s (step=" << incStep << "s)");
    for (uint32_t i = 0; i < N_BUCKETS; i++)
    {
        double t = incStart + i * incStep;
        Simulator::Schedule(Seconds(t), &SampleAndFeed);
    }

    
    Simulator::Schedule(Seconds(PEAK_END), &StartDecreasePhase);

    double decStart = PEAK_END + 2.0;   // 92s
    double decEnd   = SIM_DURATION - 2.0;  // 178s
    double decStep  = (decEnd - decStart) / (N_BUCKETS - 1);
    NS_LOG_UNCOND("=== Decrease phase: " << N_BUCKETS << " samples from "
                  << decStart << "s to " << decEnd << "s (step=" << decStep << "s)");
    for (uint32_t i = 0; i < N_BUCKETS; i++)
    {
        double t = decStart + i * decStep;
        Simulator::Schedule(Seconds(t), &SampleAndFeed);
    }

    Simulator::Stop(Seconds(SIM_DURATION));
    NS_LOG_UNCOND("\nRunning " << SIM_DURATION << "s simulation...\n");
    Simulator::Run();
    Simulator::Destroy();

    NS_LOG_UNCOND("\n=== Results ===");
    WriteCsv("paper-dual-increase-trad.csv", g_incRecords);
    WriteCsv("paper-dual-decrease-trad.csv", g_decRecords);
    WriteCsv("paper-dual-increase-impr.csv", g_incRecords);
    WriteCsv("paper-dual-decrease-impr.csv", g_decRecords);

    auto avgGamma = [](const std::vector<OnlineRecord>& recs, bool trad) {
        double sum = 0;
        for (auto& r : recs)
            sum += trad ? r.gamma_trad : r.gamma_impr;
        return recs.empty() ? 0.0 : sum / recs.size();
    };

    NS_LOG_UNCOND("\n=== Gamma Averages ===");
    NS_LOG_UNCOND("  Increase — Traditional: " << avgGamma(g_incRecords, true)
                  << "%  Improved: " << avgGamma(g_incRecords, false) << "%");
    NS_LOG_UNCOND("  Decrease — Traditional: " << avgGamma(g_decRecords, true)
                  << "%  Improved: " << avgGamma(g_decRecords, false) << "%");

    NS_LOG_UNCOND("\n============================================================");
    NS_LOG_UNCOND("Next: python3 scratch/paper-rto-dual-plot.py");
    NS_LOG_UNCOND("============================================================");

    return 0;
}
