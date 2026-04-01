/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/
/**
 * @file q2nsviz-teleportation-example.cc
 * @brief Quantum teleportation of a |+> state from Alice to Bob.
 *
 * Timing model (illustrative):
 *   kSingleGate = 100 ns  (single-qubit gate)
 *   kTwoQGate   = 300 ns  (two-qubit gate)
 *   kQDelay     =  10 ns  (quantum channel propagation, ~2 m fiber)
 *
 * Visualization output is written to
 * examples/example_traces/q2nsviz-teleportation-example.json and can be loaded in the
 * q2nsviz viewer (src/q2ns/utils/q2nsviz-serve.sh).
 *
 * See docs/tutorials/tutorial-00.md for a detailed walkthrough.
 */

#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/simulator.h"

#include "ns3/q2ns-netcontroller.h"
#include "ns3/q2ns-qgate.h"
#include "ns3/q2ns-qnode.h"
#include "ns3/q2ns-qubit.h"

#include "ns3/q2nsviz-trace-writer.h"
#include "ns3/q2nsviz-trace.h"

#include <iostream>

using namespace ns3;
using namespace q2ns;

static const uint16_t kCtrlPort = 9000;

int main(int argc, char** argv) {
  std::cout << "[DEMO] Quantum teleportation starting\n";

  RngSeedManager::SetSeed(1);
  RngSeedManager::SetRun(1);

  CommandLine cmd;
  cmd.Parse(argc, argv);

  TraceWriter::Instance().Open("examples/example_traces/q2nsviz-teleportation-example.json");

  Time::SetResolution(Time::NS);

  // --- Create quantum network ---
  NetController net;

  auto alice = net.CreateNode();
  auto bob = net.CreateNode();

  // --- Timing constants ---
  // Values are illustrative and platform-neutral (see file header for ranges).
  const Time kSingleGate = NanoSeconds(100); // single-qubit gate
  const Time kTwoQGate = NanoSeconds(300);   // two-qubit gate
  const Time kQDelay = NanoSeconds(10);      // quantum link propagation (~2 m fiber)
  const Time kClassical = kQDelay;           // classical propagation = quantum (~2 m fiber)

  // Quantum link: Alice <-> Bob
  auto ch = net.InstallQuantumLink(alice, bob);
  ch->SetAttribute("Delay", TimeValue(kQDelay));

  TraceCreateNode("Alice", 25, 25);
  TraceCreateNode("Bob", 75, 25);
  TraceCreateChannel("Alice", "Bob", "quantum");

  // --- Classical network (UDP for correction bits) ---
  InternetStackHelper internet;
  internet.Install(alice);
  internet.Install(bob);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("10ns"));
  NetDeviceContainer devices = p2p.Install(StaticCast<Node>(alice), StaticCast<Node>(bob));

  Ipv4AddressHelper ip;
  ip.SetBase("10.1.1.0", "255.255.255.0");
  auto ifs = ip.Assign(devices);
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();
  TraceCreateChannel("Alice", "Bob", "classical");

  // --- Sockets ---
  Ptr<Socket> bobRxSocket = Socket::CreateSocket(bob, UdpSocketFactory::GetTypeId());
  bobRxSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), kCtrlPort));

  Ptr<Socket> aliceTxSocket = Socket::CreateSocket(alice, UdpSocketFactory::GetTypeId());
  aliceTxSocket->Connect(InetSocketAddress(ifs.GetAddress(1), kCtrlPort));

  // --- Bob's shared context (filled by quantum + classical callbacks) ---
  struct BobCtx {
    std::shared_ptr<Qubit> eprB;
    bool qubitArrived = false;
    bool bitsArrived = false;
    int m1 = 0; // BSM bit 1: Z-correction coefficient
    int m2 = 0; // BSM bit 2: X-correction coefficient
  };
  BobCtx bobCtx;

  // Local helper: apply corrections and verify once both streams have arrived.
  auto tryCorrections = [&]() {
    if (!bobCtx.qubitArrived || !bobCtx.bitsArrived)
      return;

    Trace("Bob applies Pauli corrections Z^m", bobCtx.m1, " X^m", bobCtx.m2);

    // Apply Z correction (if needed) as an explicit gate step
    if (bobCtx.m1) {
      bob->Apply(gates::Z(), {bobCtx.eprB});
      TraceNodeText("Bob", "Z correction applied (m1=1)");
    } else {
      TraceNodeText("Bob", "Z correction skipped (m1=0)");
    }

    // Apply X correction (if needed) as an explicit gate step
    if (bobCtx.m2) {
      bob->Apply(gates::X(), {bobCtx.eprB});
      TraceNodeText("Bob", "X correction applied (m2=1)");
    } else {
      TraceNodeText("Bob", "X correction skipped (m2=0)");
    }

    // Teleportation complete: bob_epr now holds the teleported psi state.
    TraceSetBitColor("bob_epr", "#DDCC77");
    TraceNodeText("Bob", "Teleportation complete - eprB holds psi state");

    // Verify: |+> -> X-measurement should always yield 0
    Trace("Bob verifies teleported state (X-basis)");
    int mx = bob->Measure(bobCtx.eprB, Basis::X);
    TraceMeasure("bob_epr", "X");
    TraceNodeText("Bob", StrCat("X-measure: ", mx, " (expected 0 -> |+> received)"));
    std::cout << "[VERIFY] Bob's X-measure: " << mx << " (expected 0)\n";

    Simulator::Stop();
  };

  // Bob's quantum receive callback: eprB has arrived from Alice.
  bob->SetRecvCallback([&](std::shared_ptr<Qubit> q) {
    Trace("Bob receives eprB");
    bobCtx.eprB = q;
    bobCtx.qubitArrived = true;
    TraceSetBitColor("bob_epr", "#117733");
    TraceNodeText("Bob", "eprB arrived - half of Bell pair received");
    std::cout << "[B][quantum] eprB arrived at t=" << Simulator::Now().GetNanoSeconds() << " ns\n";
    tryCorrections();
  });

  // Bob's classical receive callback: correction bits have arrived from Alice.
  bobRxSocket->SetRecvCallback([&](Ptr<Socket> sock) {
    Address from;
    while (Ptr<Packet> p = sock->RecvFrom(from)) {
      uint8_t bits[2] = {0, 0};
      p->CopyData(bits, 2);
      bobCtx.m1 = bits[0] & 1;
      bobCtx.m2 = bits[1] & 1;
      bobCtx.bitsArrived = true;

      Trace("Bob receives classical corrections m1=", bobCtx.m1, " m2=", bobCtx.m2);
      TraceNodeText("Bob", StrCat("Corrections received: m1=", bobCtx.m1, " m2=", bobCtx.m2));
      std::cout << "[B][CTRL] Corrections arrived: m1=" << bobCtx.m1 << " m2=" << bobCtx.m2 << "\n";
      tryCorrections();
    }
  });

  // ---------------------------------------------------------------------------
  // Phase 1: Entanglement preparation and distribution
  // ---------------------------------------------------------------------------
  Simulator::Schedule(MicroSeconds(1), [&]() {
    Trace("Phase 1: Alice creates eprA and eprB");
    TraceNodeText("Alice", "Phase 1: preparing Bell pair");

    auto eprA = alice->CreateQubit();
    auto eprB = alice->CreateQubit();
    TraceCreateBit("Alice", "alice_epr", "quantum", "#88CCEE");
    TraceCreateBit("Alice", "bob_epr", "quantum", "#88CCEE");
    std::cout << "[A] eprA, eprB created in |0>\n";

    // H on eprA
    Simulator::Schedule(kSingleGate, [&, eprA, eprB]() {
      Trace("Alice applies H to eprA");
      alice->Apply(gates::H(), {eprA});
      TraceSetBitColor("alice_epr", "#bc71eb");
      TraceNodeText("Alice", "H(eprA)");
      std::cout << "[A] H applied to eprA\n";

      //  + 400 ns: CNOT(eprA, eprB) -> |Phi+>
      Simulator::Schedule(kTwoQGate, [&, eprA, eprB]() {
        Trace("Alice applies CNOT(eprA, eprB) -> Bell pair |Phi+>");
        alice->Apply(gates::CNOT(), {eprA, eprB});
        TraceEntangle({"alice_epr", "bob_epr"});
        TraceSetBitColor("alice_epr", "#117733");
        TraceSetBitColor("bob_epr", "#117733");
        TraceNodeText("Alice", "CNOT(eprA, eprB): Bell pair |Phi+> ready");
        std::cout << "[A] Bell pair |Phi+> prepared\n";

        // Send eprB to Bob
        Simulator::Schedule(kSingleGate, [&, eprA, eprB]() {
          Trace("Alice sends eprB to Bob");
          TraceSendBit("bob_epr", "Alice", "Bob", "quantum", Simulator::Now(),
                       Simulator::Now() + kQDelay);
          bool ok = alice->Send(eprB, bob->GetId());
          TraceNodeText("Alice", "eprB sent - in transit to Bob (+10ns)");
          std::cout << "[A][quantum] eprB sent: " << (ok ? "ok" : "failed") << "\n";
          // eprA is retained at Alice for the BSM in Phase 2.

          // ---------------------------------------------------------------
          // Phase 2: Teleportation
          // ---------------------------------------------------------------
          Simulator::Schedule(MicroSeconds(2), [&, eprA]() { // absolute T = 3 us
            Trace("Phase 2: Teleportation begins");
            TraceNodeText("Alice", "Phase 2: teleportation (entanglement pre-shared)");

            auto psi = alice->CreateQubit();
            TraceCreateBit("Alice", "psi", "quantum", "#DDCC77");
            std::cout << "[A] psi created in |0>\n";

            // prepare |+> (the state to teleport)
            Simulator::Schedule(kSingleGate, [&, eprA, psi]() {
              Trace("Alice applies H to psi -> |+> (state to teleport)");
              alice->Apply(gates::H(), {psi});
              TraceSetBitColor("psi", "#DDCC77");
              TraceNodeText("Alice", "H(psi): state to teleport is |+>");
              std::cout << "[A] psi prepared as |+>\n";

              // BSM step 1 - CNOT(psi, eprA)
              Simulator::Schedule(kTwoQGate, [&, eprA, psi]() {
                Trace("Alice BSM step 1: CNOT(psi, eprA)");
                alice->Apply(gates::CNOT(), {psi, eprA});
                TraceNodeText("Alice", "BSM step 1: CNOT(psi, eprA)");
                std::cout << "[A][BSM] CNOT(psi, eprA) applied\n";

                // BSM step 2 - H(psi)
                Simulator::Schedule(kSingleGate, [&, eprA, psi]() {
                  Trace("Alice BSM step 2: H(psi)");
                  alice->Apply(gates::H(), {psi});
                  TraceNodeText("Alice", "BSM step 2: H(psi)");
                  std::cout << "[A][BSM] H(psi) applied\n";

                  // BSM step 3 - Z-measure psi and eprA
                  Simulator::Schedule(kSingleGate, [&, eprA, psi]() {
                    Trace("Alice BSM step 3: Z-measure psi and eprA");
                    TraceMeasure("psi");
                    TraceMeasure("alice_epr");

                    int m1 = alice->Measure(psi);  // Z-basis -> Z correction at Bob
                    int m2 = alice->Measure(eprA); // Z-basis -> X correction at Bob

                    Trace("BSM outcome: m1=", m1, " m2=", m2);
                    TraceSetBitColor("psi", m1 == 0 ? "#FFFFFF" : "#000000");
                    TraceSetBitColor("alice_epr", m2 == 0 ? "#FFFFFF" : "#000000");
                    TraceNodeText("Alice", StrCat("BSM complete: m1=", m1, " m2=", m2));
                    std::cout << "[A][BSM] m1=" << m1 << " m2=" << m2 << "\n";

                    // Send classical corrections to Bob
                    uint8_t corrBits[2] = {(uint8_t) (m1 & 1), (uint8_t) (m2 & 1)};
                    aliceTxSocket->Send(Create<Packet>(corrBits, 2));
                    TraceSendPacket("Alice", "Bob", Simulator::Now(), Simulator::Now() + kClassical,
                                    StrCat("Corrections m1=", m1, " m2=", m2));
                    TraceNodeText("Alice", StrCat("Corrections sent: m1=", m1, " m2=", m2));
                    std::cout << "[A][CTRL] Corrections sent (~5 us delay)\n";

                    // psi and eprA are consumed by measurement
                    TraceRemoveBit("psi");
                    TraceRemoveBit("alice_epr");
                  });
                });
              });
            });
          });
        });
      });
    });
  });

  // --- Run simulation ---
  Simulator::Stop(MilliSeconds(100));
  Simulator::Run();
  Simulator::Destroy();

  TraceWriter::Instance().Close();

  std::cout << "[DONE] Quantum teleportation finished\n";
  return 0;
}
