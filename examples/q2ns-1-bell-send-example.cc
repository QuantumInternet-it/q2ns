/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/
/**
 * @file q2ns-1-bell-send-example.cc
 * @brief Bell pair creation and qubit transmission between two nodes.
 *
 * Demonstrates: Quantum links, CreateBellPair, Send, receive callbacks.
 * See docs/README-examples.md for detailed walkthrough.
 */
#include "ns3/core-module.h"
#include "ns3/q2ns-netcontroller.h"
#include "ns3/q2ns-qnode.h"
#include "ns3/q2ns-qubit.h"
#include "ns3/simulator.h"

#include <iostream>

using namespace ns3;
using namespace q2ns;

int main(int, char**) {
  std::cout << "[DEMO] Bell send (A->B) starting\n";

  ns3::RngSeedManager::SetSeed(1);
  ns3::RngSeedManager::SetRun(2);

  NetController net;
  net.SetQStateBackend(QStateBackend::Stab);

  auto A = net.CreateNode();
  auto B = net.CreateNode();

  auto ch = net.InstallQuantumLink(A, B);
  ch->SetAttribute("Delay", TimeValue(NanoSeconds(10)));

  auto [qA, qBremote] = A->CreateBellPair();

  B->SetRecvCallback([&](std::shared_ptr<Qubit>) { std::cout << "[RECV][quantum][B]: yes\n"; });

  // Schedule the send; the qubit arrives at B after the channel delay (10 ns)
  Simulator::Schedule(NanoSeconds(1), [&]() {
    bool ok = A->Send(qBremote, B->GetId());
    std::cout << "[SEND][quantum] A->B: " << (ok ? "ok" : "failed") << "\n";
  });

  // Schedule measurements after send time + channel delay (1 + 10 + margin = 20 ns)
  Simulator::Schedule(NanoSeconds(20), [&]() {
    int mA = A->Measure(qA, q2ns::Basis::Z);
    int mB = B->Measure(qBremote, q2ns::Basis::Z);
    std::cout << "[VERIFY] Z-measurements: A=" << mA << " B=" << mB << " (correlated expected)\n";
  });

  Simulator::Stop(MilliSeconds(1));
  Simulator::Run();

  std::cout << "[DONE] Bell send (A->B) finished\n";

  Simulator::Destroy();
  return 0;
}
