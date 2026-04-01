/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/
/**
 * @file q2nsviz-graphstate-gen-example.cc
 * @brief Distributed graph state generation across an orchestrator and three clients.
 *
 * An orchestrator locally builds a 5-qubit linear cluster state (graph state):
 *   q0 - q1 - q2 - q3 - q4
 * It then distributes the even-indexed qubits (q0, q2, q4) to three clients
 * and measures the odd-indexed qubits (q1, q3) in the X-basis, effectively
 * transmitting entanglement onto the clients via single-qubit measurements.
 *
 * Timing model (illustrative):
 *   kSingleGate = 100 ns  (single-qubit gate)
 *   kTwoQGate   = 300 ns  (two-qubit gate)
 *   kQDelay     =  10 ns  (quantum channel propagation, ~2 m fiber)
 *
 * CZ gate scheduling uses two non-overlapping layers:
 *   Layer 1 (non-overlapping): CZ(q0,q1), CZ(q2,q3)
 *   Layer 2 (non-overlapping): CZ(q1,q2), CZ(q3,q4)
 * The 5 Hadamards (independent qubits) can be applied in parallel.
 *
 * Visualization output is written to
 * examples/example_traces/q2nsviz-graphstate-gen-example.json and can be loaded in the
 * q2nsviz viewer (src/q2ns/utils/q2nsviz-serve.sh).
 *
 * See docs/tutorials/tutorial-01.md for a detailed walkthrough.
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
#include <vector>

using namespace ns3;
using namespace q2ns;

static const uint16_t kCtrlPort = 9200;

int main(int argc, char** argv) {
  std::cout << "[DEMO] Distributed graph state generation starting\n";

  RngSeedManager::SetSeed(1);
  RngSeedManager::SetRun(1);

  CommandLine cmd;
  cmd.Parse(argc, argv);

  // Visualization output
  TraceWriter::Instance().Open("examples/example_traces/q2nsviz-graphstate-gen-example.json");

  Time::SetResolution(Time::NS);

  NetController net;
  net.SetQStateBackend(QStateBackend::Ket);

  // --- Instantiate nodes ---
  Ptr<QNode> orch = net.CreateNode();
  Ptr<QNode> client1 = net.CreateNode();
  Ptr<QNode> client2 = net.CreateNode();
  Ptr<QNode> client3 = net.CreateNode();

  TraceCreateNode("Orchestrator", 50, 50);
  TraceCreateNode("Client1", 25, 20);
  TraceCreateNode("Client2", 75, 20);
  TraceCreateNode("Client3", 50, 85);

  // --- Timing constants ---
  // Values are illustrative
  const Time kSingleGate = NanoSeconds(100); // single-qubit gate (H)
  const Time kTwoQGate = NanoSeconds(300);   // two-qubit gate (CZ)
  const Time kQDelay = NanoSeconds(10);      // quantum link propagation (~2 m fiber)
  const Time kClassical = kQDelay;           // classical propagation = quantum (~2 m fiber)

  // --- Quantum links ---
  auto ch1 = net.InstallQuantumLink(orch, client1);
  auto ch2 = net.InstallQuantumLink(orch, client2);
  auto ch3 = net.InstallQuantumLink(orch, client3);
  ch1->SetAttribute("Delay", TimeValue(kQDelay));
  ch2->SetAttribute("Delay", TimeValue(kQDelay));
  ch3->SetAttribute("Delay", TimeValue(kQDelay));

  TraceCreateChannel("Orchestrator", "Client1", "quantum");
  TraceCreateChannel("Orchestrator", "Client2", "quantum");
  TraceCreateChannel("Orchestrator", "Client3", "quantum");

  // --- Classical network ---
  InternetStackHelper internet;
  internet.Install(orch);
  internet.Install(client1);
  internet.Install(client2);
  internet.Install(client3);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("10ns"));

  auto dev1 = p2p.Install(orch, client1);
  auto dev2 = p2p.Install(orch, client2);
  auto dev3 = p2p.Install(orch, client3);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  auto ifs1 = ipv4.Assign(dev1);
  ipv4.SetBase("10.1.2.0", "255.255.255.0");
  auto ifs2 = ipv4.Assign(dev2);
  ipv4.SetBase("10.1.3.0", "255.255.255.0");
  auto ifs3 = ipv4.Assign(dev3);
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();
  const Ipv4Address kClient1Addr = ifs1.GetAddress(1);
  const Ipv4Address kClient2Addr = ifs2.GetAddress(1);
  const Ipv4Address kClient3Addr = ifs3.GetAddress(1);

  TraceCreateChannel("Orchestrator", "Client1", "classical");
  TraceCreateChannel("Orchestrator", "Client2", "classical");
  TraceCreateChannel("Orchestrator", "Client3", "classical");

  // ---------------------------------------------------------------------------
  // Classical control: Orchestrator sends X-basis measurement outcomes to the
  // two neighbors of each measured qubit so clients can apply byproduct corrections.
  //   q1 measured -> outcome m1 sent to Client1 (holds q0) and Client2 (holds q2)
  //   q3 measured -> outcome m3 sent to Client2 (holds q2) and Client3 (holds q4)
  // ---------------------------------------------------------------------------

  // Client receive sockets for classical outcome messages from Orchestrator
  auto c1RxSock = Socket::CreateSocket(client1, UdpSocketFactory::GetTypeId());
  c1RxSock->Bind(InetSocketAddress(Ipv4Address::GetAny(), kCtrlPort));
  c1RxSock->SetRecvCallback([&](Ptr<Socket> sock) {
    Address from;
    while (Ptr<Packet> p = sock->RecvFrom(from)) {
      uint8_t outcome;
      p->CopyData(&outcome, 1);
      int m = outcome & 1;
      TraceNodeText("Client1", StrCat("q1 outcome m=", m, " received - q0 ready"));
      TraceSetBitColor("q0", "#44AA99");
      std::cout << "[CLIENT1] q1 outcome m=" << m << " - q0 ready\n";
    }
  });

  int c2OutcomesRxd = 0;
  auto c2RxSock = Socket::CreateSocket(client2, UdpSocketFactory::GetTypeId());
  c2RxSock->Bind(InetSocketAddress(Ipv4Address::GetAny(), kCtrlPort));
  c2RxSock->SetRecvCallback([&](Ptr<Socket> sock) {
    Address from;
    while (Ptr<Packet> p = sock->RecvFrom(from)) {
      uint8_t outcome;
      p->CopyData(&outcome, 1);
      int m = outcome & 1;
      ++c2OutcomesRxd;
      TraceNodeText("Client2", StrCat("outcome m=", m, " received (", c2OutcomesRxd, "/2)"));
      if (c2OutcomesRxd == 2) {
        TraceNodeText("Client2", "Both outcomes received - q2 ready");
        TraceSetBitColor("q2", "#44AA99");
      }
      std::cout << "[CLIENT2] outcome m=" << m << " (" << c2OutcomesRxd << "/2) - q2\n";
    }
  });

  auto c3RxSock = Socket::CreateSocket(client3, UdpSocketFactory::GetTypeId());
  c3RxSock->Bind(InetSocketAddress(Ipv4Address::GetAny(), kCtrlPort));
  c3RxSock->SetRecvCallback([&](Ptr<Socket> sock) {
    Address from;
    while (Ptr<Packet> p = sock->RecvFrom(from)) {
      uint8_t outcome;
      p->CopyData(&outcome, 1);
      int m = outcome & 1;
      TraceNodeText("Client3", StrCat("q3 outcome m=", m, " received - q4 ready"));
      TraceSetBitColor("q4", "#44AA99");
      std::cout << "[CLIENT3] q3 outcome m=" << m << " - q4 ready\n";
    }
  });

  // Pre-create orchestrator transmit sockets before simulation runs.
  // orchToC2 handles two sends (m1 and m3) as independent UDP datagrams.
  auto orchToC1 = Socket::CreateSocket(orch, UdpSocketFactory::GetTypeId());
  orchToC1->Connect(InetSocketAddress(kClient1Addr, kCtrlPort));
  auto orchToC2 = Socket::CreateSocket(orch, UdpSocketFactory::GetTypeId());
  orchToC2->Connect(InetSocketAddress(kClient2Addr, kCtrlPort));
  auto orchToC3 = Socket::CreateSocket(orch, UdpSocketFactory::GetTypeId());
  orchToC3->Connect(InetSocketAddress(kClient3Addr, kCtrlPort));

  // Client qubit arrival callbacks: trace arrival and set pending color.
  // Corrections are triggered by the classical outcome messages.
  // The real corrections are NOT implemented in this example
  client1->SetRecvCallback([&](std::shared_ptr<Qubit> q) {
    TraceSetBitColor(q->GetLabel(), "#AACCFF");
    TraceNodeText("Client1", StrCat(q->GetLabel(), " arrived - awaiting correction basis"));
    std::cout << "[CLIENT1] " << q->GetLabel() << " arrived\n";
  });
  client2->SetRecvCallback([&](std::shared_ptr<Qubit> q) {
    TraceSetBitColor(q->GetLabel(), "#AACCFF");
    TraceNodeText("Client2", StrCat(q->GetLabel(), " arrived - awaiting correction basis"));
    std::cout << "[CLIENT2] " << q->GetLabel() << " arrived\n";
  });
  client3->SetRecvCallback([&](std::shared_ptr<Qubit> q) {
    TraceSetBitColor(q->GetLabel(), "#AACCFF");
    TraceNodeText("Client3", StrCat(q->GetLabel(), " arrived - awaiting correction basis"));
    std::cout << "[CLIENT3] " << q->GetLabel() << " arrived\n";
  });

  // --- Qubit storage ---
  static const int kNumQubits = 5;
  std::vector<std::shared_ptr<Qubit>> qubits;

  // --- Simulation events ---

  // --- Create qubits ---
  Simulator::Schedule(MicroSeconds(1), [&] {
    Trace("Orchestrator creates ", kNumQubits, " qubits");
    TraceNodeText("Orchestrator", "Preparing 5-qubit linear cluster state");
    for (int i = 0; i < kNumQubits; ++i) {
      auto q = orch->CreateQubit();
      q->SetLabel("q" + std::to_string(i));
      qubits.push_back(q);
      TraceCreateBit("Orchestrator", q->GetLabel(), "quantum", "#88CCEE");
    }

    // Hadamard on all qubits (parallelizable)
    Simulator::Schedule(kSingleGate, [&] {
      Trace("Orchestrator applies H to all qubits");
      TraceNodeText("Orchestrator", "H layer (all qubits, parallel)");
      for (const auto& q : qubits) {
        orch->Apply(q2ns::gates::H(), {q});
        TraceSetBitColor(q->GetLabel(), "#bc71eb");
      }

      // CZ layer 1 - CZ(q0,q1) and CZ(q2,q3)
      Simulator::Schedule(kTwoQGate, [&] {
        Trace("CZ layer 1: CZ(q0,q1) CZ(q2,q3)");
        TraceNodeText("Orchestrator", "CZ layer 1");
        orch->Apply(q2ns::gates::CZ(), {qubits[0], qubits[1]});
        TraceEntangle({qubits[0]->GetLabel(), qubits[1]->GetLabel()});
        orch->Apply(q2ns::gates::CZ(), {qubits[2], qubits[3]});
        TraceEntangle({qubits[2]->GetLabel(), qubits[3]->GetLabel()});

        // CZ layer 2 - CZ(q1,q2) and CZ(q3,q4)
        Simulator::Schedule(kTwoQGate, [&] {
          Trace("CZ layer 2: CZ(q1,q2) CZ(q3,q4) - cluster state ready");
          TraceNodeText("Orchestrator", "CZ layer 2 - cluster state ready");
          orch->Apply(q2ns::gates::CZ(), {qubits[1], qubits[2]});
          TraceEntangle({qubits[1]->GetLabel(), qubits[2]->GetLabel()});
          orch->Apply(q2ns::gates::CZ(), {qubits[3], qubits[4]});
          TraceEntangle({qubits[3]->GetLabel(), qubits[4]->GetLabel()});

          // Send even-indexed qubits to clients
          Simulator::Schedule(kSingleGate, [&] {
            Trace("Orchestrator sends q0, q2, q4 to clients");
            TraceNodeText("Orchestrator", "Sending q0/q2/q4 to clients");
            orch->Send(qubits[0], client1->GetId());
            orch->Send(qubits[2], client2->GetId());
            orch->Send(qubits[4], client3->GetId());

            auto t0 = Simulator::Now();
            auto t1 = t0 + kQDelay;
            TraceSendBit("q0", "Orchestrator", "Client1", "quantum", t0, t1);
            TraceSendBit("q2", "Orchestrator", "Client2", "quantum", t0, t1);
            TraceSendBit("q4", "Orchestrator", "Client3", "quantum", t0, t1);

            // Measure q1 and q3 in X-basis once client qubits have arrived
            Simulator::Schedule(kQDelay + kSingleGate, [&] {
              Trace("Orchestrator measures q1, q3 in X-basis");
              TraceNodeText("Orchestrator", "Measuring q1, q3 (X-basis)");

              // Measure q1: neighbors are q0 (Client1) and q2 (Client2)
              auto m1 = orch->Measure(qubits[1], Basis::X);
              Trace("q1 X-measure outcome = ", m1);
              TraceGraphMeasure("q1", "X");
              TraceRemoveBit("q1");

              // Measure q3: neighbors are q2 (Client2) and q4 (Client3)
              auto m3 = orch->Measure(qubits[3], Basis::X);
              Trace("q3 X-measure outcome = ", m3);
              TraceGraphMeasure("q3", "X");
              TraceRemoveBit("q3");

              TraceNodeText("Orchestrator", "Measurements complete - sending outcomes to clients");
              const auto tNow = Simulator::Now();

              uint8_t m1b = (uint8_t) (m1 & 1);
              uint8_t m3b = (uint8_t) (m3 & 1);

              // Send m1 to Client1 (q0 is neighbor of q1)
              orchToC1->Send(Create<Packet>(&m1b, 1));
              TraceSendPacket("Orchestrator", "Client1", tNow, tNow + kClassical,
                              StrCat("q1 outcome: m=", m1));

              // Send m1 to Client2 (q2 is neighbor of q1)
              orchToC2->Send(Create<Packet>(&m1b, 1));
              TraceSendPacket("Orchestrator", "Client2", tNow, tNow + kClassical,
                              StrCat("q1 outcome: m=", m1));

              // Send m3 to Client2 (q2 is neighbor of q3)
              orchToC2->Send(Create<Packet>(&m3b, 1));
              TraceSendPacket("Orchestrator", "Client2", tNow, tNow + kClassical,
                              StrCat("q3 outcome: m=", m3));

              // Send m3 to Client3 (q4 is neighbor of q3)
              orchToC3->Send(Create<Packet>(&m3b, 1));
              TraceSendPacket("Orchestrator", "Client3", tNow, tNow + kClassical,
                              StrCat("q3 outcome: m=", m3));
            });
          });
        });
      });
    });
  });

  Simulator::Stop(MilliSeconds(1));
  Simulator::Run();
  Simulator::Destroy();

  TraceWriter::Instance().Close();
  std::cout << "[DONE] Distributed graph state generation finished\n";
  return 0;
}
