/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

// Default Network Topology
//
//       10.1.1.0
// n0 -------------- n1
//    point-to-point
//

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FirstScriptExample");

int
main(int argc, char* argv[])
{
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    //NetDevice এর attribute পরিবর্তন (example: DataRate)
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));//

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);//NetDevices + Channel দুই node এর মধ্যে install করা
    //nodes → [Node0, Node1]

// pointToPoint.Install(nodes) → creates:

// Node0 → NetDevice0
// Node1 → NetDevice1

// NetDeviceContainer devices → [NetDevice0, NetDevice1]

    InternetStackHelper stack;
    stack.Install(nodes);
// কী হচ্ছে step by step?

// stack তৈরি হলো

// .Install(nodes) → দুই node-এ TCP/IP stack install হলো

// এখন nodes-এ IP assign করা এবং UDP/TCP application চালানো যাবে
//Stack = layered system inside node, যা সব network protocols (TCP/UDP/IP)
// handle করে এবং application কে network communication করতে দেয়।
// Client Application wants to send packet
//         │
//         ▼
//   TCP/UDP layer (Transport) → adds headers, manages reliability
//         │
//         ▼
//          IP layer (Network) → adds IP addresses, decides route
//         │
//         ▼
// NetDevice + Channel (Link + Physical) → actually sends bits to other node
//         │
//         ▼
// Receiver node stack → IP → UDP → Server Application

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    UdpEchoServerHelper echoServer(9);//9 port number

    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(1));
    serverApps.Stop(Seconds(10));

  UdpEchoClientHelper echoClient(interfaces.GetAddress(1), 9);
  //interfaces.GetAddress(1) → destination IP
// Client এখন Node0-এ install হবে, কিন্তু packet পাঠাবে Node1 IP-এ
    echoClient.SetAttribute("MaxPackets", UintegerValue(1));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
//     3️⃣ Key points

// SetAttribute এ প্রথম argument must match helper/application class-এর attribute name

// শুধু random string দেওয়া যাবে না

// Attributes এর list → class header বা ns-3 documentation এ দেখা যায়

// উদাহরণ (UdpEchoClient attributes):

// Attribute name	Type	Purpose
// MaxPackets	UintegerValue	কত packet পাঠাবে
// Interval	TimeValue	Interval between packets
// PacketSize	UintegerValue	Size of each packet

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2));
    clientApps.Stop(Seconds(10));

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
