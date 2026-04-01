/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/
/**
 * @file q2ns-3-teleportation-basic-example.cc
 * @brief Teleportation without any classical communication
 *
 * Demonstrates: Quantum links, CreateBellPair, Send, receive callbacks.
 * See docs/README-examples.md for detailed walkthrough.
 */

#include "ns3/q2ns-netcontroller.h"
#include "ns3/q2ns-qnode.h"
#include "ns3/q2ns-qubit.h"

#include "ns3/core-module.h"
#include "ns3/simulator.h"

#include <iostream>

using namespace ns3;
using namespace q2ns;

int main() {
  std::cout << "[DEMO] Teleportation (A->B) starting\n";

  ns3::RngSeedManager::SetSeed(1);
  ns3::RngSeedManager::SetRun(1);

  NetController net;
  net.SetQStateBackend(QStateBackend::Ket);

  auto A = net.CreateNode();
  auto B = net.CreateNode();

  auto ch = net.InstallQuantumLink(A, B);
  ch->SetAttribute("Delay", TimeValue(NanoSeconds(10)));

  // Schedule the send; the qubit arrives at B after the channel delay (10 ns)
  // Then Alice prepares her special state to teleport and performs a BSM
  std::pair<int, int> ms;
  Simulator::Schedule(NanoSeconds(1), [&]() {
    auto [qA, qBremote] = A->CreateBellPair();
    bool ok = A->Send(qBremote, B->GetId());
    std::cout << "[SEND][quantum] A->B: " << (ok ? "ok" : "failed") << "\n";

    auto qAToTeleport = A->CreateQubit();
    A->Apply(gates::H(), {qAToTeleport});
    ms = A->MeasureBell(qAToTeleport, qA);
    std::cout << "[A] BSM results: " << ms.first << ", " << ms.second << "\n";
  });

  // Bob applies corrections once he receives the qubit
  B->SetRecvCallback([&](std::shared_ptr<Qubit> q) {
    std::cout << "[RECV][quantum][B]: yes\n";
    std::cout << "[B] Applying corrections: Z^" << ms.first << " X^" << ms.second << "|state>\n";

    if (ms.second)
      B->Apply(gates::X(), {q});
    if (ms.first)
      B->Apply(gates::Z(), {q});

    // Verify: Bob should now have |+>
    // Measure in X-basis: |+> -> 0 deterministically
    int mx = B->Measure(q, Basis::X);
    std::cout << "[B][VERIFY] Final state is correct: " << ((mx == 0) ? "yes" : "no") << "\n";
  });


  Simulator::Stop(Seconds(10));
  Simulator::Run();

  std::cout << "[DONE] Teleportation (A->B) finished\n";

  Simulator::Destroy();
  return 0;
}
