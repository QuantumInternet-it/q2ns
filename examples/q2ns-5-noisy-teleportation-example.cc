/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/
/**
 * @file q2ns-5-noisy-teleportation-example.cc
 * @brief Teleportation experiment with UDP and Bob-side memory noise
 *
 * Demonstrates:
 *   - quantum teleportation over a quantum link
 *   - classical correction delivery over UDP
 *   - distance-dependent propagation delay
 *   - Bob-side depolarizing noise while waiting for classical corrections
 *   - repeated trials with fidelity / latency statistics
 *
 * Tunable parameters:
 *   --distanceKm   Alice-Bob distance in km
 *   --TDepMs       Bob memory characteristic time in ms (0 => ideal memory)
 *   --trials       number of repeated trials
 *   --verbose      print per-trial details
 *
 * Example:
 *   ./ns3 run q2ns-5-noisy-teleportation-example -- --distanceKm=50 --TDepMs=5 --trials=20
 *--verbose=1
 *---------------------------------------------------------------------------*/

#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/simulator.h"

#include "ns3/q2ns-analysis.h"
#include "ns3/q2ns-netcontroller.h"
#include "ns3/q2ns-qmap.h"
#include "ns3/q2ns-qnode.h"
#include "ns3/q2ns-qubit.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <numeric>
#include <vector>

using namespace ns3;
using namespace q2ns;

namespace {

struct RunningStats {
  std::vector<double> xs;

  void Add(double x) {
    xs.push_back(x);
  }

  double Mean() const {
    if (xs.empty()) {
      return 0.0;
    }
    return std::accumulate(xs.begin(), xs.end(), 0.0) / static_cast<double>(xs.size());
  }

  double StdDev() const {
    if (xs.size() < 2) {
      return 0.0;
    }
    const double mean = Mean();
    double acc = 0.0;
    for (double x : xs) {
      const double d = x - mean;
      acc += d * d;
    }
    return std::sqrt(acc / static_cast<double>(xs.size() - 1));
  }

  double Median() const {
    if (xs.empty()) {
      return 0.0;
    }
    std::vector<double> tmp = xs;
    std::sort(tmp.begin(), tmp.end());
    const size_t n = tmp.size();
    if (n % 2 == 1) {
      return tmp[n / 2];
    }
    return 0.5 * (tmp[n / 2 - 1] + tmp[n / 2]);
  }
};

struct BobInfo {
  std::shared_ptr<Qubit> qRemote;
  bool qubitArrived = false;
  bool bitsArrived = false;
  int m1 = 0;
  int m2 = 0;
  Time tQubitArrive = Seconds(0);
  Time tBitsArrive = Seconds(0);
  bool done = false;
  double finalFidelity = std::numeric_limits<double>::quiet_NaN();
  double completionMs = std::numeric_limits<double>::quiet_NaN();
};

void TryCorrections(Ptr<QNode> bob, NetController& net, BobInfo& info, Ptr<DepolarizingQMap> depol,
                    Ptr<UniformRandomVariable> u, std::shared_ptr<Qubit> qIdealPlus, Time tStart,
                    bool verbose) {
  if (info.done || !info.qubitArrived || !info.bitsArrived) {
    return;
  }

  // Bob's qubit sits in memory from quantum arrival until the classical bits arrive.
  const Time holdTime = info.tBitsArrive - info.tQubitArrive;

  QMapContext ctx;
  ctx.elapsedTime = holdTime;

  QMapInstance instance = depol->Sample(u, ctx);
  instance(*bob, info.qRemote);

  // Standard teleportation correction: X^m2 Z^m1
  if (info.m2) {
    bob->Apply(gates::X(), {info.qRemote});
  }
  if (info.m1) {
    bob->Apply(gates::Z(), {info.qRemote});
  }

  const auto finalState = net.GetState(info.qRemote);
  const auto idealState = net.GetState(qIdealPlus);

  info.finalFidelity = q2ns::analysis::Fidelity(*finalState, *idealState);
  info.completionMs = (Simulator::Now() - tStart).GetSeconds() * 1000.0;
  info.done = true;

  if (verbose) {
    std::cout << "[B] corrections applied"
              << "  hold_ms=" << std::fixed << std::setprecision(3)
              << holdTime.GetSeconds() * 1000.0 << "  fidelity=" << std::setprecision(4)
              << info.finalFidelity << "\n";
  }

  Simulator::Stop();
}

struct TrialResult {
  double fidelity = std::numeric_limits<double>::quiet_NaN();
  double completionMs = std::numeric_limits<double>::quiet_NaN();
};

TrialResult RunOnce(double distanceKm, double TDepMs, bool verbose) {
  const uint16_t port = 9000;

  // Approximate propagation in fiber: 5 microseconds / km
  const Time qDelay = MicroSeconds(static_cast<int64_t>(std::llround(5.0 * distanceKm)));
  const Time cDelay = MicroSeconds(static_cast<int64_t>(std::llround(5.0 * distanceKm)));

  BobInfo bobInfo;
  Time tStart = Seconds(0);

  NetController net;
  net.SetQStateBackend(QStateBackend::Ket);

  auto A = net.CreateNode();
  auto B = net.CreateNode();

  // Reference node for the ideal |+> state
  auto ref = net.CreateNode();
  auto qIdealPlus = ref->CreateQubit();
  ref->Apply(gates::H(), {qIdealPlus});

  // Bob memory-noise map: p = 1 - exp(-t / Tdep)
  auto depol = CreateObject<DepolarizingQMap>();
  if (TDepMs > 0.0) {
    const double ratePerSecond = 1000.0 / TDepMs; // TDepMs is in ms
    depol->SetAttribute("Rate", DoubleValue(ratePerSecond));
  } else {
    depol->SetAttribute("Probability", DoubleValue(0.0));
  }

  auto u = CreateObject<UniformRandomVariable>();

  // Quantum link
  auto ch = net.InstallQuantumLink(A, B);
  ch->SetAttribute("Delay", TimeValue(qDelay));

  // Classical UDP link
  InternetStackHelper internet;
  internet.Install(A);
  internet.Install(B);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
  p2p.SetChannelAttribute("Delay", TimeValue(cDelay));
  NetDeviceContainer devices = p2p.Install(A, B);

  Ipv4AddressHelper ip;
  ip.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ip.Assign(devices);

  Ptr<Socket> bobSocket = Socket::CreateSocket(B, UdpSocketFactory::GetTypeId());
  bobSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), port));

  Ptr<Socket> aliceSocket = Socket::CreateSocket(A, UdpSocketFactory::GetTypeId());
  aliceSocket->Connect(InetSocketAddress(interfaces.GetAddress(1), port));

  // Bob's classical receive callback
  bobSocket->SetRecvCallback(
      [B, &bobInfo, &net, depol, u, qIdealPlus, &tStart, verbose](Ptr<Socket> socket) {
        while (Ptr<Packet> packet = socket->Recv()) {
          uint8_t bytes[2] = {0, 0};
          const uint32_t n = packet->CopyData(bytes, 2);

          if (n < 2) {
            continue;
          }

          bobInfo.bitsArrived = true;
          bobInfo.m1 = bytes[0] & 1;
          bobInfo.m2 = bytes[1] & 1;
          bobInfo.tBitsArrive = Simulator::Now();

          if (verbose) {
            std::cout << "[RECV][classical][B][udp] t=" << std::fixed << std::setprecision(6)
                      << Simulator::Now().GetSeconds() << " s"
                      << "  m1=" << bobInfo.m1 << "  m2=" << bobInfo.m2 << "\n";
          }

          TryCorrections(B, net, bobInfo, depol, u, qIdealPlus, tStart, verbose);
        }
      });

  // Bob's quantum receive callback
  B->SetRecvCallback(
      [B, &bobInfo, &net, depol, u, qIdealPlus, &tStart, verbose](std::shared_ptr<Qubit> q) {
        bobInfo.qRemote = q;
        bobInfo.qubitArrived = true;
        bobInfo.tQubitArrive = Simulator::Now();

        if (verbose) {
          std::cout << "[RECV][quantum][B] t=" << std::fixed << std::setprecision(6)
                    << Simulator::Now().GetSeconds() << " s\n";
        }

        TryCorrections(B, net, bobInfo, depol, u, qIdealPlus, tStart, verbose);
      });

  // Teleportation protocol
  Simulator::Schedule(NanoSeconds(1), [&]() {
    tStart = Simulator::Now();

    auto [qA, qBRemote] = A->CreateBellPair();
    const bool ok = A->Send(qBRemote, B->GetId());

    if (verbose) {
      std::cout << "[SEND][quantum] A->B: " << (ok ? "ok" : "failed") << "\n";
    }

    auto psi = A->CreateQubit();
    A->Apply(gates::H(), {psi}); // teleport |+>

    auto [m1, m2] = A->MeasureBell(psi, qA);

    if (verbose) {
      std::cout << "[A] BSM results: " << m1 << ", " << m2 << "\n";
    }

    uint8_t bytes[2] = {
        static_cast<uint8_t>(m1),
        static_cast<uint8_t>(m2),
    };

    aliceSocket->Send(Create<Packet>(bytes, 2));

    if (verbose) {
      std::cout << "[SEND][classical][udp] A->B\n";
    }
  });

  Simulator::Stop(Seconds(5));
  Simulator::Run();
  Simulator::Destroy();

  TrialResult r;
  r.fidelity = bobInfo.finalFidelity;
  r.completionMs = bobInfo.completionMs;
  return r;
}

} // namespace

int main(int argc, char* argv[]) {
  double distanceKm = 50.0;
  double TDepMs = 5.0;
  uint32_t trials = 10;
  uint32_t seed = 1;
  uint32_t run = 1;
  bool verbose = false;

  CommandLine cmd;
  cmd.AddValue("distanceKm", "Distance between Alice and Bob in km", distanceKm);
  cmd.AddValue("TDepMs", "Bob memory characteristic time in ms (0 => ideal memory)", TDepMs);
  cmd.AddValue("trials", "Number of repeated trials", trials);
  cmd.AddValue("seed", "ns-3 RNG seed", seed);
  cmd.AddValue("run", "ns-3 RNG run number", run);
  cmd.AddValue("verbose", "Print per-trial details", verbose);
  cmd.Parse(argc, argv);

  RngSeedManager::SetSeed(seed);

  RunningStats fidelityStats;
  RunningStats latencyStats;

  if (verbose) {
    std::cout << "[DEMO] Noisy teleportation experiment starting\n";
    std::cout << "  distanceKm = " << distanceKm << "\n";
    std::cout << "  TDepMs     = " << TDepMs << "\n";
    std::cout << "  trials     = " << trials << "\n";
  }

  for (uint32_t t = 0; t < trials; ++t) {
    RngSeedManager::SetRun(run + t);

    if (verbose) {
      std::cout << "\n[TRIAL " << (t + 1) << "/" << trials << "]\n";
    }

    TrialResult r = RunOnce(distanceKm, TDepMs, verbose);
    fidelityStats.Add(r.fidelity);
    latencyStats.Add(r.completionMs);
  }

  std::cout << "\n[RESULT]"
            << " distance_km=" << std::fixed << std::setprecision(3) << distanceKm
            << " TDep_ms=" << TDepMs << " trials=" << trials
            << " fidelity_mean=" << std::setprecision(4) << fidelityStats.Mean()
            << " fidelity_median=" << fidelityStats.Median()
            << " fidelity_stddev=" << fidelityStats.StdDev()
            << " latency_mean_ms=" << std::setprecision(4) << latencyStats.Mean()
            << " latency_median_ms=" << latencyStats.Median()
            << " latency_stddev_ms=" << latencyStats.StdDev() << "\n";

  if (verbose) {
    std::cout << "[DONE] Noisy teleportation experiment finished\n";
  }

  return 0;
}