/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/
/**
 * @file q2ns-4-multipartite-scaling-example.cc
 * @brief Multipartite entanglement distribution and backend scaling demo
 *
 * Demonstrates:
 *   - creating a 1D cluster state on a central node
 *   - distributing one qubit to each remote node
 *   - comparing wall-clock runtime across backends
 *
 * Tunable parameters:
 *   --numNodes   total number of nodes (>= 2)
 *   --backend    ket | dm | stab
 *   --trials     repeated runs for mean/stdev wall-clock timing
 *
 * Example:
 *   ./ns3 run q2ns-4-multipartite-scaling-example -- --numNodes=16 --backend=stab --trials=5
 *---------------------------------------------------------------------------*/

#include "ns3/core-module.h"
#include "ns3/simulator.h"

#include "ns3/q2ns-netcontroller.h"
#include "ns3/q2ns-qnode.h"
#include "ns3/q2ns-qubit.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

using namespace ns3;
using namespace q2ns;

namespace {

struct RunningStats {
  std::vector<double> valuesMs;

  void Add(double x) {
    valuesMs.push_back(x);
  }

  double Mean() const {
    if (valuesMs.empty())
      return 0.0;
    double s = std::accumulate(valuesMs.begin(), valuesMs.end(), 0.0);
    return s / static_cast<double>(valuesMs.size());
  }

  double StdDev() const {
    if (valuesMs.size() < 2)
      return 0.0;
    const double mean = Mean();
    double accum = 0.0;
    for (double x : valuesMs) {
      const double d = x - mean;
      accum += d * d;
    }
    return std::sqrt(accum / static_cast<double>(valuesMs.size() - 1));
  }

  double Median() const {
    if (valuesMs.empty())
      return 0.0;

    std::vector<double> tmp = valuesMs;
    std::sort(tmp.begin(), tmp.end());

    const size_t n = tmp.size();
    if (n % 2 == 1) {
      return tmp[n / 2];
    }
    return 0.5 * (tmp[n / 2 - 1] + tmp[n / 2]);
  }
};


double RunOnce(uint32_t numNodes, const std::string& backend, bool verbose) {
  auto t0 = std::chrono::steady_clock::now();

  NetController net;
  net.SetQStateBackend(backend);

  std::vector<Ptr<QNode>> nodes;
  nodes.reserve(numNodes);

  for (uint32_t i = 0; i < numNodes; ++i) {
    nodes.push_back(net.CreateNode());
  }

  Ptr<QNode> center = nodes[0];

  for (uint32_t i = 1; i < numNodes; ++i) {
    auto ch = net.InstallQuantumLink(center, nodes[i]);
    ch->SetAttribute("Delay", TimeValue(NanoSeconds(10)));
  }


  for (uint32_t i = 1; i < numNodes; ++i) {
    Ptr<QNode> node = nodes[i];
    node->SetRecvCallback([node](std::shared_ptr<Qubit> q) { node->Measure(q); });
  }

  auto t1 = std::chrono::steady_clock::now();

  Simulator::Schedule(NanoSeconds(1), [center, &nodes, numNodes]() {
    std::vector<std::shared_ptr<Qubit>> qs;
    qs.reserve(numNodes);

    for (uint32_t i = 0; i < numNodes; ++i) {
      qs.push_back(center->CreateQubit());
    }

    // Prepare a 1D cluster state:
    for (uint32_t i = 0; i < numNodes; ++i) {
      center->Apply(gates::H(), {qs[i]});
    }

    for (uint32_t i = 0; i + 1 < numNodes; ++i) {
      center->Apply(gates::CZ(), {qs[i], qs[i + 1]});
    }

    // Distribute one qubit to each remote node
    for (uint32_t i = 1; i < numNodes; ++i) {
      const bool ok = center->Send(qs[i], nodes[i]->GetId());
      if (!ok) {
        std::cerr << "[WARN] Send to node " << i << " failed\n";
      }
    }

    // Center can measure its qubit after sending
    // This will run at 20 ns since the scheduled time is relative to the current simulation time
    // (Simulator::Now()). This overall lambda runs at 1 ns and then this one below for measurement
    // runs 19 ns after that.
    Simulator::Schedule(NanoSeconds(19), [centerQubit = qs[0], center]() {
      center->Measure(centerQubit, Basis::Z);
    });
  });

  // Leave plenty of time for all deliveries
  Simulator::Stop(MicroSeconds(10));
  Simulator::Run();
  Simulator::Destroy();

  auto t2 = std::chrono::steady_clock::now();

  const double configMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
  const double simMs = std::chrono::duration<double, std::milli>(t2 - t1).count();
  const double totalMs = std::chrono::duration<double, std::milli>(t2 - t0).count();

  if (verbose) {
    std::cout << "[RUN]  config=" << std::fixed << std::setprecision(3) << configMs << " ms"
              << "  sim=" << simMs << " ms"
              << "  total=" << totalMs << " ms\n";
  }

  return totalMs;
}

} // namespace

int main(int argc, char* argv[]) {
  uint32_t numNodes = 8;
  uint32_t trials = 3;
  std::string backend = "stab";
  uint32_t seed = 1;
  uint32_t run = 1;
  bool verbose = false;

  CommandLine cmd;
  cmd.AddValue("numNodes", "Total number of nodes (>= 2)", numNodes);
  cmd.AddValue("trials", "Number of repeated runs", trials);
  cmd.AddValue("backend", "ket | dm | stab", backend);
  cmd.AddValue("seed", "ns-3 RNG seed", seed);
  cmd.AddValue("run", "ns-3 RNG run number", run);
  cmd.AddValue("verbose", "Print per-trial timing details", verbose);
  cmd.Parse(argc, argv);

  if (numNodes < 2) {
    NS_ABORT_MSG("--numNodes must be at least 2");
  }

  RngSeedManager::SetSeed(seed);

  std::cout << "[DEMO] Multipartite entanglement distribution starting\n";
  std::cout << "  nodes    = " << numNodes << "\n";
  std::cout << "  backend  = " << backend << "\n";
  std::cout << "  trials   = " << trials << "\n";

  RunningStats totalStats;

  for (uint32_t t = 0; t < trials; ++t) {
    RngSeedManager::SetRun(run + t);
    if (verbose) {
      std::cout << "\n[TRIAL " << (t + 1) << "/" << trials << "]\n";
    }

    totalStats.Add(RunOnce(numNodes, backend, verbose));
  }

  std::cout << "\n[RESULT] backend=" << backend << "  nodes=" << numNodes
            << "  mean_total_ms=" << std::fixed << std::setprecision(3) << totalStats.Mean()
            << "  median_total_ms=" << totalStats.Median() << "  stddev_ms=" << totalStats.StdDev()
            << "\n";

  std::cout << "[DONE] Multipartite entanglement distribution finished\n";
  return 0;
}