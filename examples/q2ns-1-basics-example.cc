/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/
/**
 * @file q2ns-1-basics-example.cc
 * @brief Basic example demonstrating qubit creation, gate application, and measurement.
 */

#include "ns3/core-module.h"
#include "ns3/q2ns-netcontroller.h"
#include "ns3/q2ns-qgate.h"
#include "ns3/q2ns-qnode.h"
#include "ns3/q2ns-qstate.h"

using namespace ns3;
using namespace q2ns;

int main() {

  ns3::RngSeedManager::SetSeed(45);
  ns3::RngSeedManager::SetRun(15);

  NetController net;
  net.SetQStateBackend(QStateBackend::Ket);

  auto node = net.CreateNode();
  auto q = node->CreateQubit();
  auto state = net.GetState(q);
  std::cout << "Created node with ID: " << node->GetId() << "\n";
  std::cout << "Created qubit with ID: " << state->GetStateId() << "\n";
  std::cout << " and state: " << state << "\n";

  Simulator::Schedule(MicroSeconds(10), [node, q]() {
    node->Apply(gates::H(), {q});
    std::cout << "Applied Hadamard gate to qubit\n";
    std::cout << "State after gate application: " << node->GetState(q) << "\n";
  });

  Simulator::Schedule(MicroSeconds(20), [node, q]() {
    int result = node->Measure(q);
    std::cout << "Measurement result: " << result << "\n";
  });

  Simulator::Stop(MilliSeconds(10));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}