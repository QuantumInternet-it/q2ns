/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/
/**
 * @file q2ns-3-teleportation-comms-example.cc
 * @brief Teleportation with classical communication (UDP)
 *
 * Demonstrates:
 *   - Quantum link + Bell pair distribution
 *   - Bell-state measurement at Alice
 *   - UDP transmission of the two classical correction bits
 *   - Bob waiting for both the qubit and the classical bits before correcting
 */

#include "ns3/q2ns-netcontroller.h"
#include "ns3/q2ns-qnode.h"
#include "ns3/q2ns-qubit.h"

#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/simulator.h"

#include <iostream>

using namespace ns3;
using namespace q2ns;

namespace {


// Bob's local information about the teleportation as it arrives.
// He learns the remote qubit through the quantum channel and the
// Bell-measurement outcomes through the classical channel.
struct BobInfo {
  std::shared_ptr<Qubit> qBremote;
  bool qubitArrived = false;
  bool bitsArrived = false;
  int m1 = 0;
  int m2 = 0;
};


// Bob can complete teleportation only after he has both:
//   1) the received qubit, and
//   2) Alice's two classical correction bits.
void TryCorrections(Ptr<QNode> bob, BobInfo& bobInfo) {
  if (!bobInfo.qubitArrived || !bobInfo.bitsArrived) {
    return;
  }

  std::cout << "[B] Applying corrections: Z^" << bobInfo.m1 << " X^" << bobInfo.m2 << "|state>\n";

  if (bobInfo.m2) {
    bob->Apply(gates::X(), {bobInfo.qBremote});
  }
  if (bobInfo.m1) {
    bob->Apply(gates::Z(), {bobInfo.qBremote});
  }

  int mx = bob->Measure(bobInfo.qBremote, Basis::X);
  std::cout << "[B][VERIFY] Final state is correct: " << ((mx == 0) ? "yes" : "no") << "\n";
}


} // namespace

int main() {
  std::cout << "[DEMO] Teleportation (A->B) with classical communication starting\n";

  ns3::RngSeedManager::SetSeed(1);
  ns3::RngSeedManager::SetRun(1);

  NetController net;
  net.SetQStateBackend(QStateBackend::Ket);

  auto A = net.CreateNode();
  auto B = net.CreateNode();



  // Networking setup
  // Create the quantum channel used to distribute Bob's half of the Bell pair.
  auto ch = net.InstallQuantumLink(A, B);
  ch->SetAttribute("Delay", TimeValue(NanoSeconds(10)));

  // Install a simple IP/UDP network between Alice and Bob for the two
  // correction bits that complete the teleportation protocol.
  InternetStackHelper internet;
  internet.Install(A);
  internet.Install(B);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("1ms"));
  NetDeviceContainer devices = p2p.Install(A, B);

  Ipv4AddressHelper ip;
  ip.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ip.Assign(devices);

  const uint16_t port = 9000;

  Ptr<Socket> bobSocket = Socket::CreateSocket(B, UdpSocketFactory::GetTypeId());
  bobSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), port));

  Ptr<Socket> aliceSocket = Socket::CreateSocket(A, UdpSocketFactory::GetTypeId());
  aliceSocket->Connect(InetSocketAddress(interfaces.GetAddress(1), port));


  // Bob's classical and quantum receive callbacks
  // Create a BobInfo instance to track Bob's local information updates
  BobInfo bobInfo;

  // When Bob receives the remote Bell-pair half, he records that the quantum
  // part has arrived. He then tries to finish teleportation, which succeeds
  // only if the classical correction packet has already arrived too.
  B->SetRecvCallback([B, &bobInfo](std::shared_ptr<Qubit> q) {
    std::cout << "[RECV][quantum][B]: yes\n";
    bobInfo.qubitArrived = true;
    bobInfo.qBremote = q;

    TryCorrections(B, bobInfo);
  });


  // When the classical correction packet arrives at Bob, decode the two bits
  // and check whether Bob now has everything needed to finish teleportation.
  bobSocket->SetRecvCallback([B, &bobInfo](Ptr<Socket> socket) {
    while (Ptr<Packet> packet = socket->Recv()) {
      bobInfo.bitsArrived = true;

      uint8_t bytes[2] = {0, 0};
      packet->CopyData(bytes, 2);

      bobInfo.m1 = bytes[0] & 1;
      bobInfo.m2 = bytes[1] & 1;

      std::cout << "[RECV][classical][B] m1=" << bobInfo.m1 << ", m2=" << bobInfo.m2 << "\n";

      TryCorrections(B, bobInfo);
    }
  });



  // Scheduling the protocol
  // Alice first sends Bob's Bell-pair half over the quantum channel.
  // She then prepares |+>, performs the Bell-state measurement locally,
  // and sends the two resulting correction bits to Bob over UDP.
  Simulator::Schedule(NanoSeconds(1), [&]() {
    auto [qA, qBremote] = A->CreateBellPair();
    bool ok = A->Send(qBremote, B->GetId());
    std::cout << "[SEND][quantum] A->B: " << (ok ? "ok" : "failed") << "\n";

    auto qAToTeleport = A->CreateQubit();
    A->Apply(gates::H(), {qAToTeleport});
    auto [m1, m2] = A->MeasureBell(qAToTeleport, qA);
    std::cout << "[A] BSM results: " << m1 << ", " << m2 << "\n";

    uint8_t bytes[2] = {static_cast<uint8_t>(m1), static_cast<uint8_t>(m2)};
    aliceSocket->Send(Create<Packet>(bytes, 2));
    std::cout << "[SEND][classical] A->B: m1=" << m1 << ", m2=" << m2 << "\n";
  });



  Simulator::Stop(Seconds(10));
  Simulator::Run();

  std::cout << "[DONE] Teleportation (A->B) with classical communication finished\n";

  Simulator::Destroy();
  return 0;
}