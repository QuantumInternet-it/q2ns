/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/
/**
 * @file q2ns-2-basis-measurement-example.cc
 * @brief Measurement bases, custom circuits, and custom gate definitions in q2ns.
 *
 * Demonstrates:
 * - Preparing eigenstates and measuring in X, Y, and Z bases using the ns-3 scheduler
 * - Defining a custom gate by composing H and S via matrix product
 * - Defining the same gate entry-by-entry using MakeMatrix and Complex{re, im}
 */

#include "ns3/core-module.h"
#include "ns3/q2ns-netcontroller.h"
#include "ns3/q2ns-qgate.h"
#include "ns3/q2ns-qnode.h"
#include "ns3/q2ns-types.h"
#include "ns3/rng-seed-manager.h"
#include <iostream>

using namespace ns3;
using namespace q2ns;

int main(int, char**) {
  RngSeedManager::SetSeed(42);
  RngSeedManager::SetRun(1);

  NetController net;
  net.SetQStateBackend(QStateBackend::Ket);
  auto N = net.CreateNode();

  // Allocate all qubits before the simulation starts
  auto q1 = N->CreateQubit();
  auto q2 = N->CreateQubit();
  auto q3 = N->CreateQubit();
  auto q4 = N->CreateQubit();
  auto q5 = N->CreateQubit();

  // Pre-compute the custom gate matrices once, before scheduling.
  // MatrixS() * MatrixH() = S*H  (H is rightmost, so it is applied first)
  auto HS = gates::Custom(MatrixS() * MatrixH());

  // S*H = (1/sqrt(2)) [[1, 1], [i, -i]]  -- same gate, written entry-by-entry
  const double s = 1.0 / std::sqrt(2.0);
  auto HS_explicit = gates::Custom(MakeMatrix({
      {Complex{s, 0.0}, Complex{s, 0.0}},
      {Complex{0.0, s}, Complex{0.0, -s}},
  }));

  // Test 1 at t=10 us: H|0> -> |+>, measure in X-basis -> always 0
  Simulator::Schedule(MicroSeconds(10), [N, q1]() {
    N->Apply(gates::H(), {q1});
    std::cout << "Test 1  H|0> in X-basis:           " << N->Measure(q1, Basis::X)
              << "  (expect 0)\n";
  });

  // Test 2 at t=20 us: X then H -- |0> -> |1> -> |->, measure in X-basis -> always 1
  // |-> = (|0> - |1>)/sqrt(2) is the -1 eigenstate of X
  Simulator::Schedule(MicroSeconds(20), [N, q2]() {
    N->Apply(gates::X(), {q2}); // |0> -> |1>
    N->Apply(gates::H(), {q2}); // |1> -> |->
    std::cout << "Test 2  H|1> in X-basis:           " << N->Measure(q2, Basis::X)
              << "  (expect 1)\n";
  });

  // Test 3 at t=30 us: H then S -- |0> -> |+> -> |+i>, measure in Y-basis -> always 0
  // S maps |+> to (|0> + i|1>)/sqrt(2) = |+i>, the +1 eigenstate of Y
  Simulator::Schedule(MicroSeconds(30), [N, q3]() {
    N->Apply(gates::H(), {q3}); // |0> -> |+>
    N->Apply(gates::S(), {q3}); // |+> -> |+i>
    std::cout << "Test 3  S*H|0> in Y-basis:         " << N->Measure(q3, Basis::Y)
              << "  (expect 0)\n";
  });

  // Test 4 at t=40 us: same result using a single custom gate (matrix product form)
  Simulator::Schedule(MicroSeconds(40), [N, q4, HS]() {
    N->Apply(HS, {q4});
    std::cout << "Test 4  Custom HS (S*H)|0> in Y:   " << N->Measure(q4, Basis::Y)
              << "  (same as Test 3)\n";
  });

  // Test 5 at t=50 us: same result using a single custom gate (explicit matrix form)
  Simulator::Schedule(MicroSeconds(50), [N, q5, HS_explicit]() {
    N->Apply(HS_explicit, {q5});
    std::cout << "Test 5  Custom HS (explicit)|0> Y: " << N->Measure(q5, Basis::Y)
              << "  (same as Test 3)\n";
  });

  Simulator::Stop(MicroSeconds(100));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}