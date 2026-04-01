/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/
/**
 * @file q2ns-3-classical-comms-example.cc
 *
 * @brief Minimal ns-3 UDP communication example intended for q2ns users who may not already be
 *familiar with classical ns-3 networking.
 *
 * Demonstrates:
 *   - Creating nodes
 *   - Installing Internet stack
 *   - Creating a point-to-point link
 *   - Sending and receiving a UDP packet
 *---------------------------------------------------------------------------*/

#include "ns3/q2ns-netcontroller.h"
#include "ns3/q2ns-qnode.h"

#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

#include <iostream>

using namespace ns3;
using namespace q2ns;


int main() {

  std::cout << "[DEMO] Classical communication (A->B) starting\n";

  ns3::RngSeedManager::SetSeed(1);
  ns3::RngSeedManager::SetRun(1);

  // QNode inherits from ns3::Node, so it can be used directly with ordinary
  // ns-3 networking helpers (Internet stack, sockets, etc.).
  NetController net;
  auto A = net.CreateNode();
  auto B = net.CreateNode();


  // Classical networking setup
  // We install the ns-3 Internet stack so the nodes can send IP/UDP packets.
  InternetStackHelper internet;
  internet.Install(A);
  internet.Install(B);


  // Create a simple point-to-point classical link between A and B.
  // This gives us a direct network connection with fixed bandwidth and delay.
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("1ms"));
  NetDeviceContainer devices = p2p.Install(A, B);


  // Assign IPv4 addresses to the two ends of the link.
  // ns-3 requires this before we can send UDP packets.
  Ipv4AddressHelper ip;
  ip.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ip.Assign(devices);


  // UDP socket setup
  // We create a UDP receiver on B and a sender on A.
  const uint16_t port = 9000;

  // Create Bob's UDP socket, bind it to the chosen port, and attach the callback that will run
  // whenever a packet arrives.
  Ptr<Socket> bobSocket = Socket::CreateSocket(B, UdpSocketFactory::GetTypeId());
  InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), port);
  bobSocket->Bind(local);
  bobSocket->SetRecvCallback([](Ptr<Socket> socket) {
    while (Ptr<Packet> packet = socket->Recv()) {
      std::cout << "[B][classical] Received UDP packet at " << Simulator::Now().GetSeconds()
                << " s, size = " << packet->GetSize() << " bytes\n";
    }
  });

  // Create Alice's UDP socket and connect it to Bob's address and port.
  // interfaces.GetAddress(1) is Bob's IP address on the point-to-point link.
  // Now, aliceSocket->Send(...) will transmit packets to Bob.
  Ptr<Socket> aliceSocket = Socket::CreateSocket(A, UdpSocketFactory::GetTypeId());
  InetSocketAddress remote = InetSocketAddress(interfaces.GetAddress(1), port);
  aliceSocket->Connect(remote);



  // Simulation scheduling
  // Schedule a UDP packet to be sent from A to B after 1 second of simulation time.
  Simulator::Schedule(Seconds(1.0), [aliceSocket]() {
    Ptr<Packet> packet = Create<Packet>(4);
    aliceSocket->Send(packet);
    std::cout << "[A][classical] Sent UDP packet at " << Simulator::Now().GetSeconds() << " s\n";
  });

  Simulator::Stop(Seconds(10));
  Simulator::Run();

  std::cout << "[DONE] Classical communication (A->B) finished\n";

  Simulator::Destroy();
  return 0;
}