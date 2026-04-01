/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/
/**
 * @file q2ns-2-fidelity-example.cc
 * @brief Fidelity sweep over depolarizing probability.
 *
 * Demonstrates q2ns::analysis::Fidelity by sending |+> through a
 * DepolarizingQMap at several probability values and printing the
 * fidelity against the ideal |+> reference for each trial.
 *
 * Because DepolarizingQMap is a trajectory model (it applies one
 * specific Pauli per shot), the per-trial fidelity is 0 or 1.
 * Run multiple times with different seeds to see the stochastic nature.
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"

#include "ns3/q2ns-analysis.h"
#include "ns3/q2ns-netcontroller.h"
#include "ns3/q2ns-qmap.h"
#include "ns3/q2ns-qnode.h"
#include "ns3/q2ns-qstate.h"
#include "ns3/q2ns-qubit.h"
#include "ns3/simulator.h"

#include <iomanip>
#include <iostream>
#include <random>
#include <vector>

using namespace ns3;
using namespace q2ns;

int main() {
  RngSeedManager::SetSeed(std::random_device{}() | 1u);

  NetController net;

  // Build the ideal |+> reference
  auto ref_node = net.CreateNode();
  auto qref = ref_node->CreateQubit();
  ref_node->Apply(gates::H(), {qref});
  const auto ideal = net.GetState(qref);

  // Experiment topology: A -> [DepolarizingQMap] -> B
  auto A = net.CreateNode();
  auto B = net.CreateNode();

  auto depol = CreateObject<DepolarizingQMap>();
  auto ch = net.InstallQuantumLink(A, B);
  ch->SetAttribute("Delay", TimeValue(NanoSeconds(10)));
  ch->SetAttribute("QMap", PointerValue(depol));

  // Receive callback installed once; `received` is reset at the start of each trial.
  std::shared_ptr<QState> received;
  B->SetRecvCallback([&net, &received](std::shared_ptr<Qubit> q) { received = net.GetState(q); });

  const std::vector<double> probs = {0.0, 0.10, 0.25, 0.50, 0.75, 1.0};

  std::cout << std::fixed << std::setprecision(4);
  std::cout << "Depolarizing fidelity sweep (one trial per point)\n";
  std::cout << "  p       F\n";

  for (double p : probs) {
    received = nullptr;
    depol->SetAttribute("Probability", DoubleValue(p));

    auto q = A->CreateQubit();
    A->Apply(gates::H(), {q});

    Simulator::Schedule(NanoSeconds(1), [A, B, q]() { A->Send(q, B->GetId()); });

    // Advance the simulation by 1 ms -- well past the 10 ns channel delay.
    Simulator::Stop(Simulator::Now() + MilliSeconds(1));
    Simulator::Run();

    const double f = received ? q2ns::analysis::Fidelity(*ideal, *received) : -1.0;
    std::cout << "  " << p << "   " << f << "\n";
  }

  Simulator::Destroy();
  return 0;
}
