
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-header.h"
#include "ns3/tcp-socket-base.h"
#include "ns3/rtt-improved-estimator.h"
#include "ns3/rtt-proposed.h"

#include <fstream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <functional>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("PaperRtoProposedSweep");

static const uint32_t N_FLOWS       = 9;
static const double   RAMP_INTERVAL = 10.0;
static const double   SIM_DURATION  = 180.0;
static const double   PEAK_END      = (N_FLOWS - 1) * RAMP_INTERVAL + RAMP_INTERVAL;
static const uint32_t N_BUCKETS     = 21;

static const std::vector<uint32_t> WINDOW_SIZES = {1, 2, 3, 4};
static const std::vector<double>   GAMMA_VALUES  = {0.0, 0.25, 0.5, 0.75, 1.0};

static std::vector<double> g_rawRttBuffer;
static uint64_t            g_windowAcks = 0;
static uint64_t            g_windowRetx = 0;

static Ptr<RttImprovedEstimator> g_paperEst;

static std::vector<std::vector<Ptr<RttProposed>>> g_propEsts;

static bool g_isIncrease = true;
static int  g_seqCounter = 0;

struct ConfigRecord
{
    int    seq;
    double rtt_ms;
    double rto_paper_ms;
    double rto_prop_ms;
    double gamma_paper;
    double gamma_prop;
};

static std::vector<std::vector<std::vector<ConfigRecord>>> g_incRecs;
static std::vector<std::vector<std::vector<ConfigRecord>>> g_decRecs;

static void
RttChanged(Time /* old */, Time newRtt)
{
    if (newRtt > Seconds(0))
    {
        g_rawRttBuffer.push_back(newRtt.GetMilliSeconds());
        g_windowAcks++;
    }
}

static void
RetxOccurred(Ptr<const Packet> /* pkt */,
             const TcpHeader& /* hdr */,
             const Address&   /* local */,
             const Address&   /* remote */,
             Ptr<const TcpSocketBase> /* sock */)
{
    g_windowRetx++;
}

static void
SampleAndFeed()
{
    if (g_rawRttBuffer.empty())
        return;

    double sum = 0;
    for (double v : g_rawRttBuffer) sum += v;
    double avgRttMs = sum / g_rawRttBuffer.size();
    g_rawRttBuffer.clear();

    double lossRate = (g_windowAcks > 0)
                        ? std::min(1.0, static_cast<double>(g_windowRetx) / g_windowAcks)
                        : 0.0;
    g_windowAcks = 0;
    g_windowRetx = 0;

    Time rtt = MilliSeconds(avgRttMs);

    // Paper estimator
    g_paperEst->Measurement(rtt);
    double rtoPaper = (g_paperEst->GetEstimate()
                       + g_paperEst->GetVariation() * 4).GetMilliSeconds();
    double gammaPaper = (avgRttMs > 0)
                        ? 100.0 * std::abs(rtoPaper - avgRttMs) / avgRttMs : 0.0;

    g_seqCounter++;

    double minGamProp = 1e9, maxGamProp = -1e9;

    for (size_t w = 0; w < WINDOW_SIZES.size(); w++)
    {
        for (size_t g = 0; g < GAMMA_VALUES.size(); g++)
        {
            auto& est = g_propEsts[w][g];
            est->SetLossRate(lossRate);
            est->Measurement(rtt);

            double srttMs   = est->GetEstimate().GetMilliSeconds();
            double rttvarMs = est->GetVariation().GetMilliSeconds();
            double base     = srttMs + 4.0 * rttvarMs;
            double rtoProp  = (1.0 + GAMMA_VALUES[g] * lossRate) * base;
            double gammaProp = (avgRttMs > 0)
                               ? 100.0 * std::abs(rtoProp - avgRttMs) / avgRttMs : 0.0;

            minGamProp = std::min(minGamProp, gammaProp);
            maxGamProp = std::max(maxGamProp, gammaProp);

            ConfigRecord rec = {g_seqCounter, avgRttMs, rtoPaper, rtoProp,
                                gammaPaper, gammaProp};
            if (g_isIncrease)
                g_incRecs[w][g].push_back(rec);
            else
                g_decRecs[w][g].push_back(rec);
        }
    }

    NS_LOG_UNCOND("  [" << (g_isIncrease ? "INC" : "DEC") << " " << g_seqCounter
                  << "] RTT=" << avgRttMs << "ms"
                  << "  loss=" << lossRate * 100.0 << "%"
                  << "  γ(paper)=" << gammaPaper << "%"
                  << "  γ(prop)=[" << minGamProp << "%.." << maxGamProp << "%]");
}

static void
StartDecreasePhase()
{
    NS_LOG_UNCOND("\n  --- Switching to DECREASE phase ---");
    g_isIncrease = false;
    g_seqCounter = 0;
    g_rawRttBuffer.clear();
    g_windowAcks = 0;
    g_windowRetx = 0;

    g_paperEst->Reset();
    for (auto& row : g_propEsts)
        for (auto& est : row)
            est->Reset();
}

static void
ConnectTraces()
{
    Config::ConnectWithoutContext(
        "/NodeList/0/$ns3::TcpL4Protocol/SocketList/*/LastRTT",
        MakeCallback(&RttChanged));
    Config::ConnectWithoutContext(
        "/NodeList/0/$ns3::TcpL4Protocol/SocketList/*/Retransmission",
        MakeCallback(&RetxOccurred));
    NS_LOG_UNCOND("  Traces connected on Node 0");
}

static void
WriteAllCsvs()
{
    int total = 0;
    for (size_t w = 0; w < WINDOW_SIZES.size(); w++)
    {
        for (size_t g = 0; g < GAMMA_VALUES.size(); g++)
        {
            std::ostringstream base;
            base << "prop-W" << WINDOW_SIZES[w]
                 << "-G" << std::fixed << std::setprecision(2) << GAMMA_VALUES[g];

        
            {
                std::ofstream f(base.str() + "-inc.csv");
                f << "seq,rtt_ms,rto_paper_ms,rto_prop_ms,gamma_paper,gamma_prop\n";
                for (const auto& r : g_incRecs[w][g])
                    f << r.seq << "," << r.rtt_ms << "," << r.rto_paper_ms << ","
                      << r.rto_prop_ms << "," << r.gamma_paper << "," << r.gamma_prop << "\n";
            }

            {
                std::ofstream f(base.str() + "-dec.csv");
                f << "seq,rtt_ms,rto_paper_ms,rto_prop_ms,gamma_paper,gamma_prop\n";
                for (const auto& r : g_decRecs[w][g])
                    f << r.seq << "," << r.rtt_ms << "," << r.rto_paper_ms << ","
                      << r.rto_prop_ms << "," << r.gamma_paper << "," << r.gamma_prop << "\n";
            }
            total += 2;
        }
    }
    NS_LOG_UNCOND("  Wrote " << total << " CSV files");
}

int
main(int argc, char* argv[])
{
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    NS_LOG_UNCOND("============================================================");
    NS_LOG_UNCOND("Multi-Config Sweep: Paper vs Proposed");
    NS_LOG_UNCOND("  Windows W  : 1, 2, 3, 4");
    NS_LOG_UNCOND("  Loss factor γ: 0.00, 0.25, 0.50, 0.75, 1.00");
    NS_LOG_UNCOND("  Total proposed configs: 20");
    NS_LOG_UNCOND("============================================================");

    // ── Create estimators ─────────────────────────────────────────────────────
    g_paperEst = CreateObject<RttImprovedEstimator>();

    g_propEsts.resize(WINDOW_SIZES.size());
    g_incRecs.resize(WINDOW_SIZES.size());
    g_decRecs.resize(WINDOW_SIZES.size());

    for (size_t w = 0; w < WINDOW_SIZES.size(); w++)
    {
        g_propEsts[w].resize(GAMMA_VALUES.size());
        g_incRecs[w].resize(GAMMA_VALUES.size());
        g_decRecs[w].resize(GAMMA_VALUES.size());
        for (size_t g = 0; g < GAMMA_VALUES.size(); g++)
        {
            auto est = CreateObject<RttProposed>();
            est->SetAttribute("WindowSize", UintegerValue(WINDOW_SIZES[w]));
            est->SetAttribute("Gamma",      DoubleValue(GAMMA_VALUES[g]));
            g_propEsts[w][g] = est;
        }
    }

    Config::SetDefault("ns3::TcpL4Protocol::SocketType",
                       StringValue("ns3::TcpNewReno"));
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1000));
    Config::SetDefault("ns3::TcpSocket::SndBufSize",  UintegerValue(1u << 22));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize",  UintegerValue(1u << 22));

    NodeContainer allNodes;
    allNodes.Create(20);

    NodeContainer senders, receivers;
    for (uint32_t i = 0; i < N_FLOWS; i++) senders.Add(allNodes.Get(i));
    NodeContainer routers;
    routers.Add(allNodes.Get(9));
    routers.Add(allNodes.Get(10));
    for (uint32_t i = 0; i < N_FLOWS; i++) receivers.Add(allNodes.Get(i + 11));

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
        auto dev = accessLink.Install(NodeContainer(senders.Get(i), routers.Get(0)));
        ipv4.Assign(dev);
    }

    ipv4.SetBase("10.2.1.0", "255.255.255.0");
    ipv4.Assign(bottleLink.Install(NodeContainer(routers.Get(0), routers.Get(1))));

    for (uint32_t i = 0; i < N_FLOWS; i++)
    {
        std::ostringstream sub;
        sub << "10.3." << (i + 1) << ".0";
        ipv4.SetBase(sub.str().c_str(), "255.255.255.0");
        auto dev = accessLink.Install(NodeContainer(routers.Get(1), receivers.Get(i)));
        recvAddrs[i] = ipv4.Assign(dev).GetAddress(1);
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t basePort = 5000;
    for (uint32_t i = 0; i < N_FLOWS; i++)
    {
        uint16_t port = basePort + i;
        double startT = static_cast<double>(i) * RAMP_INTERVAL;
        double stopT  = (i == 0) ? SIM_DURATION
                                 : std::min(SIM_DURATION,
                                            PEAK_END + static_cast<double>(N_FLOWS - i) * RAMP_INTERVAL);

        if (i == 0)
        {
            PacketSinkHelper sink("ns3::TcpSocketFactory",
                                  InetSocketAddress(Ipv4Address::GetAny(), port));
            sink.Install(receivers.Get(i)).Start(Seconds(0.0));
            BulkSendHelper bulk("ns3::TcpSocketFactory",
                                InetSocketAddress(recvAddrs[i], port));
            bulk.SetAttribute("MaxBytes", UintegerValue(0));
            bulk.SetAttribute("SendSize", UintegerValue(1000));
            auto app = bulk.Install(senders.Get(i));
            app.Start(Seconds(startT)); app.Stop(Seconds(stopT));
        }
        else
        {
            PacketSinkHelper sink("ns3::UdpSocketFactory",
                                  InetSocketAddress(Ipv4Address::GetAny(), port));
            sink.Install(receivers.Get(i)).Start(Seconds(std::max(0.0, startT - 0.1)));
            OnOffHelper onoff("ns3::UdpSocketFactory",
                              InetSocketAddress(recvAddrs[i], port));
            onoff.SetConstantRate(DataRate("2500kbps"), 1000);
            auto app = onoff.Install(senders.Get(i));
            app.Start(Seconds(startT)); app.Stop(Seconds(stopT));
        }
    }
    Simulator::Schedule(Seconds(0.1), &ConnectTraces);

    double incStart = 2.0, incEnd = PEAK_END - 2.0;
    double incStep  = (incEnd - incStart) / (N_BUCKETS - 1);
    for (uint32_t i = 0; i < N_BUCKETS; i++)
        Simulator::Schedule(Seconds(incStart + i * incStep), &SampleAndFeed);

    Simulator::Schedule(Seconds(PEAK_END), &StartDecreasePhase);

    double decStart = PEAK_END + 2.0, decEnd = SIM_DURATION - 2.0;
    double decStep  = (decEnd - decStart) / (N_BUCKETS - 1);
    for (uint32_t i = 0; i < N_BUCKETS; i++)
        Simulator::Schedule(Seconds(decStart + i * decStep), &SampleAndFeed);

    Simulator::Stop(Seconds(SIM_DURATION));
    NS_LOG_UNCOND("\nRunning " << SIM_DURATION << "s simulation...\n");
    Simulator::Run();
    Simulator::Destroy();

    NS_LOG_UNCOND("\n=== Writing CSV files ===");
    WriteAllCsvs();

    NS_LOG_UNCOND("\n=== Average Gamma Summary (paper vs proposed) ===");
    for (size_t w = 0; w < WINDOW_SIZES.size(); w++)
    {
        for (size_t g = 0; g < GAMMA_VALUES.size(); g++)
        {
            auto avg = [](const std::vector<ConfigRecord>& v,
                          std::function<double(const ConfigRecord&)> fn) {
                double s = 0; for (const auto& r : v) s += fn(r);
                return v.empty() ? 0.0 : s / v.size();
            };
            double ip = avg(g_incRecs[w][g], [](const ConfigRecord& r){ return r.gamma_paper; });
            double ipr= avg(g_incRecs[w][g], [](const ConfigRecord& r){ return r.gamma_prop; });
            double dp = avg(g_decRecs[w][g], [](const ConfigRecord& r){ return r.gamma_paper; });
            double dpr= avg(g_decRecs[w][g], [](const ConfigRecord& r){ return r.gamma_prop; });
            NS_LOG_UNCOND("  W=" << WINDOW_SIZES[w]
                          << " γ=" << std::fixed << std::setprecision(2) << GAMMA_VALUES[g]
                          << "  Inc paper=" << std::setprecision(1) << ip
                          << "% prop=" << ipr
                          << "%  Dec paper=" << dp << "% prop=" << dpr << "%");
        }
    }

    NS_LOG_UNCOND("\n=== Done ===");
    NS_LOG_UNCOND("python3 scratch/proposed/paper-rto-proposed-plot.py");
    return 0;
}
