/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/
/**
 * @file q2ns-2-qmap-example.cc
 * @brief Example of using different quantum maps.
 *
 * Demonstrates the usage of various quantum maps in q2ns.
 * Run with different --mode values to see different maps in action:
 * - loss+depol: a composition of LossQMap and DepolarizingQMap
 * - randomgate: a RandomGateQMap with X and Z gates
 * - randomunitary: a RandomUnitaryQMap applying Haar-random unitaries
 * - conditional: a ConditionalQMap that applies loss only if flight time > 20 ns
 * - lambda: a QMap from a simple lambda that applies S gate
 * - lambda-random: a QMap from a lambda that applies X with probability based on a rate and flight
 * time
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"

#include "ns3/q2ns-netcontroller.h"
#include "ns3/q2ns-qmap.h"
#include "ns3/q2ns-qnode.h"
#include "ns3/q2ns-qstate.h"
#include "ns3/q2ns-qubit.h"
#include "ns3/simulator.h"

#include <iostream>
#include <random>
#include <string>

using namespace ns3;
using namespace q2ns;

int main(int argc, char* argv[]) {

  RngSeedManager::SetSeed(std::random_device{}() | 1u);

  std::string mode = "loss+depol"; // default demo
  double lossP = 0.5;
  double depolRate = 1e8;

  CommandLine cmd;
  cmd.AddValue(
      "mode",
      "Demo mode: loss+depol | randomgate | randomunitary | conditional | lambda | lambda-random",
      mode);
  cmd.AddValue("lossP", "Loss probability used in loss+depol mode", lossP);
  cmd.AddValue("depolRate", "Depolarizing rate [1/s] used in loss+depol mode", depolRate);
  cmd.Parse(argc, argv);

  NetController net;
  auto A = net.CreateNode();
  auto B = net.CreateNode();

  B->SetRecvCallback([&net](std::shared_ptr<Qubit> q) {
    std::cout << "Qubit received at " << Simulator::Now() << "\n";
    std::cout << "In state: " << net.GetState(q) << "\n";
  });

  Ptr<QMap> map;
  std::cout << "Running " << mode << "\n";

  if (mode == "loss+depol") {
    auto loss = CreateObject<LossQMap>();
    loss->SetAttribute("Probability", DoubleValue(lossP));

    auto depol = CreateObject<DepolarizingQMap>();
    depol->SetAttribute("Rate", DoubleValue(depolRate));

    map = QMap::Compose({loss, depol});
  } else if (mode == "randomgate") {
    auto rg = CreateObject<RandomGateQMap>();
    rg->AddGate(gates::X(), 1.0);
    rg->AddGate(gates::Z(), 2.0);
    rg->SetAttribute("Probability", DoubleValue(1.0));
    map = rg;
  } else if (mode == "randomunitary") {
    auto ru = CreateObject<RandomUnitaryQMap>();
    ru->SetAttribute("Probability", DoubleValue(1.0));
    map = ru;
  } else if (mode == "conditional") {
    auto loss = CreateObject<LossQMap>();
    loss->SetAttribute("Probability", DoubleValue(1.0));

    auto cond = CreateObject<ConditionalQMap>();
    cond->SetQMap(loss);
    cond->SetCondition([](const std::shared_ptr<Qubit>&, const QMapContext& ctx) {
      return ctx.elapsedTime > NanoSeconds(20);
    });
    map = cond;
  } else if (mode == "lambda") {
    map = QMap::FromLambda(
        [](QNode& node, std::shared_ptr<Qubit>& q) { node.Apply(gates::S(), {q}); });
  } else if (mode == "lambda-random") {
    const double rate = 5e6; // 1/s
    map = QMap::FromLambda([rate](QNode& node, std::shared_ptr<Qubit>& q,
                                  Ptr<UniformRandomVariable> u, const QMapContext& ctx) {
      const double p = QMap::RateToProb(rate, ctx.elapsedTime);
      if (p > 0.0 && u->GetValue(0.0, 1.0) < p) {
        node.Apply(gates::X(), {q});
      }
    });
  } else {
    NS_ABORT_MSG("Unknown mode: " << mode);
  }

  auto ch = net.InstallQuantumLink(A, B);
  ch->SetAttribute("Delay", TimeValue(NanoSeconds(10)));
  ch->SetAttribute("QMap", PointerValue(map));

  // Allocate and prepare the qubit before the simulation starts
  auto q = A->CreateQubit();
  A->Apply(gates::H(), {q}); // prepare |+>

  // Schedule the send; the QMap fires at B after the link delay
  Simulator::Schedule(NanoSeconds(1), [A, B, q]() { A->Send(q, B->GetId()); });

  Simulator::Stop(MilliSeconds(1));
  Simulator::Run();

  const bool lost = (q->GetLocation().type == LocationType::Lost);
  std::cout << "Qubit status: " << (lost ? "LOST" : "delivered") << "\n";

  Simulator::Destroy();
  return 0;
}