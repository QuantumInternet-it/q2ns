/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/
/**
 * @file q2nsviz-entanglement-distribution-example.cc
 * @brief Pairwise Bell-pair distribution across 3 fully-connected nodes.
 *
 * Three nodes (Node1, Node2, Node3) share quantum and classical links in a
 * full-mesh topology. Bell pairs are distributed sequentially so that every
 * pair of nodes ends up sharing one entangled qubit:
 *
 *   Step 1 - Node1 creates Bell pair 0, keeps half, sends the other to Node2.
 *   Step 2 - Node1 creates Bell pair 1, keeps half, sends the other to Node3.
 *   Step 3 - Node2 creates Bell pair 2, keeps half, sends the other to Node3.
 *
 * After the protocol each link (1-2, 1-3, 2-3) carries one shared Bell pair.
 *
 * Timing model (illustrative):
 *   kSingleGate = 100 ns  (single-qubit gate)
 *   kTwoQGate   = 300 ns  (two-qubit gate)
 *   kQDelay     = 100 ns  (quantum channel propagation, ~20 m fiber)
 *
 * Steps start at T = 1, 3, 5 us for clear visualization; final trace at ~20 us.
 *
 * Visualization output is written to
 * examples/example_traces/q2nsviz-entanglement-distribution-example.json and can be loaded
 * in the q2nsviz viewer (src/q2ns/utils/q2nsviz-serve.sh).
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

#include <array>
#include <iostream>

using namespace ns3;
using namespace q2ns;

static const uint16_t kAckPort = 9100;

int main(int argc, char** argv) {
  std::cout << "[DEMO] Entanglement distribution (3 nodes) starting\n";

  RngSeedManager::SetSeed(1);
  RngSeedManager::SetRun(1);

  CommandLine cmd;
  cmd.Parse(argc, argv);

  TraceWriter::Instance().Open(
      "examples/example_traces/q2nsviz-entanglement-distribution-example.json");

  Trace("Entanglement Distribution with 3 nodes");

  Time::SetResolution(Time::NS);

  NetController net;
  net.SetQStateBackend(QStateBackend::Stab);

  // --- Nodes ---
  Ptr<QNode> node1 = net.CreateNode();
  Ptr<QNode> node2 = net.CreateNode();
  Ptr<QNode> node3 = net.CreateNode();

  TraceCreateNode("Node1", 85, 50);
  TraceCreateNode("Node2", 32, 80);
  TraceCreateNode("Node3", 32, 19);

  // --- Timing constants ---
  // Values are illustrative.
  const Time kSingleGate = NanoSeconds(100); // single-qubit gate
  const Time kTwoQGate = NanoSeconds(300);   // two-qubit gate
  const Time kQDelay = NanoSeconds(100);     // quantum link propagation (~20 m fiber)
  const Time kClassical = kQDelay;           // classical propagation = quantum (~20 m fiber)

  // --- Quantum links (full mesh) ---
  auto ch12 = net.InstallQuantumLink(node1, node2);
  auto ch13 = net.InstallQuantumLink(node1, node3);
  auto ch23 = net.InstallQuantumLink(node2, node3);
  ch12->SetAttribute("Delay", TimeValue(kQDelay));
  ch13->SetAttribute("Delay", TimeValue(kQDelay));
  ch23->SetAttribute("Delay", TimeValue(kQDelay));

  TraceCreateChannel("Node1", "Node2", "quantum");
  TraceCreateChannel("Node1", "Node2", "classical");
  TraceCreateChannel("Node1", "Node3", "quantum");
  TraceCreateChannel("Node1", "Node3", "classical");
  TraceCreateChannel("Node2", "Node3", "quantum");
  TraceCreateChannel("Node2", "Node3", "classical");

  // --- Classical network (full mesh p2p) ---
  InternetStackHelper internet;
  internet.Install(node1);
  internet.Install(node2);
  internet.Install(node3);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("100ns"));

  Ipv4AddressHelper ipv4;

  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  auto ifs12 = ipv4.Assign(p2p.Install(node1, node2));

  ipv4.SetBase("10.1.2.0", "255.255.255.0");
  auto ifs13 = ipv4.Assign(p2p.Install(node1, node3));

  ipv4.SetBase("10.1.3.0", "255.255.255.0");
  auto ifs23 = ipv4.Assign(p2p.Install(node2, node3));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // ---------------------------------------------------------------------------
  // UDP sockets for classical ACKs
  //   Node2 -> Node1  (Bell pair 0 confirmed)
  //   Node3 -> Node1  (Bell pair 1 confirmed)
  //   Node3 -> Node2  (Bell pair 2 confirmed)
  // ---------------------------------------------------------------------------

  // Node1 ACK-receive socket (receives ACKs from Node2 and Node3)
  auto node1AckRx = Socket::CreateSocket(node1, UdpSocketFactory::GetTypeId());
  node1AckRx->Bind(InetSocketAddress(Ipv4Address::GetAny(), kAckPort));
  int node1AcksReceived = 0;
  node1AckRx->SetRecvCallback([&](Ptr<Socket> sock) {
    Address from;
    while (sock->RecvFrom(from)) {
      ++node1AcksReceived;
      TraceNodeText("Node1", StrCat("ACK received (", node1AcksReceived, "/2 links confirmed)"));
      std::cout << "[NODE1] ACK received (" << node1AcksReceived << "/2)\n";
      if (node1AcksReceived == 2)
        Trace("Node1: both Bell pair links confirmed");
    }
  });

  // Node2 ACK-receive socket (receives ACK from Node3 for Bell pair 2)
  auto node2AckRx = Socket::CreateSocket(node2, UdpSocketFactory::GetTypeId());
  node2AckRx->Bind(InetSocketAddress(Ipv4Address::GetAny(), kAckPort));
  node2AckRx->SetRecvCallback([&](Ptr<Socket> sock) {
    Address from;
    while (sock->RecvFrom(from)) {
      TraceNodeText("Node2", "ACK from Node3: Bell pair 2 link confirmed");
      Trace("Node2: Bell pair 2 confirmed by Node3");
      std::cout << "[NODE2] ACK from Node3 received\n";
    }
  });

  // Pre-created transmit sockets: addresses from interface containers.
  // ifs12.GetAddress(0) = Node1 on the 1-2 subnet, etc.
  auto node2TxSock = Socket::CreateSocket(node2, UdpSocketFactory::GetTypeId());
  node2TxSock->Connect(InetSocketAddress(ifs12.GetAddress(0), kAckPort));

  auto node3ToNode1Sock = Socket::CreateSocket(node3, UdpSocketFactory::GetTypeId());
  node3ToNode1Sock->Connect(InetSocketAddress(ifs13.GetAddress(0), kAckPort));

  auto node3ToNode2Sock = Socket::CreateSocket(node3, UdpSocketFactory::GetTypeId());
  node3ToNode2Sock->Connect(InetSocketAddress(ifs23.GetAddress(0), kAckPort));

  // Node2 qubit-receive callback (Bell pair 0: q0b arrives from Node1)
  node2->SetRecvCallback([&](std::shared_ptr<Qubit> q) {
    TraceSetBitColor(q->GetLabel(), "#90EE90");
    TraceNodeText("Node2", StrCat("Qubit ", q->GetLabel(), " arrived - entangled with Node1"));
    std::cout << "[NODE2] Qubit " << q->GetLabel() << " arrived\n";
    node2TxSock->Send(Create<Packet>(1));
    TraceSendPacket("Node2", "Node1", Simulator::Now(), Simulator::Now() + kClassical,
                    StrCat("ACK: ", q->GetLabel(), " received"));
  });

  // Node3 qubit-receive callback (Bell pairs 1 and 2 arrive)
  // bell_1_b comes from Node1; bell_2_b comes from Node2.
  int node3QubitCount = 0;
  node3->SetRecvCallback([&](std::shared_ptr<Qubit> q) {
    ++node3QubitCount;
    TraceSetBitColor(q->GetLabel(), "#90EE90");
    TraceNodeText("Node3", StrCat("Qubit ", q->GetLabel(), " arrived (", node3QubitCount, "/2)"));
    std::cout << "[NODE3] Qubit " << q->GetLabel() << " arrived\n";
    if (node3QubitCount == 1) {
      node3ToNode1Sock->Send(Create<Packet>(1));
      TraceSendPacket("Node3", "Node1", Simulator::Now(), Simulator::Now() + kClassical,
                      StrCat("ACK: ", q->GetLabel(), " received"));
    } else {
      node3ToNode2Sock->Send(Create<Packet>(1));
      TraceSendPacket("Node3", "Node2", Simulator::Now(), Simulator::Now() + kClassical,
                      StrCat("ACK: ", q->GetLabel(), " received"));
    }
  });

  // --- Bell pair 0: Node1 <-> Node2, base T = 1 us ---
  Simulator::Schedule(MicroSeconds(1), [&] {
    Trace("Step 1: Node1 initializes Bell pair 0 (for Node2)");
    TraceNodeText("Node1", "Step 1: init Bell pair 0");

    auto q0a = node1->CreateQubit();
    q0a->SetLabel("bell_0_a");
    auto q0b = node1->CreateQubit();
    q0b->SetLabel("bell_0_b");

    TraceCreateBit("Node1", "bell_0_a", "quantum", "#88CCEE");
    TraceCreateBit("Node1", "bell_0_b", "quantum", "#88CCEE");

    // T + 100 ns: H(q0a)
    Simulator::Schedule(kSingleGate, [=]() {
      node1->Apply(gates::H(), {q0a});
      TraceSetBitColor("bell_0_a", "#BC71EB");
      TraceNodeText("Node1", "H(bell_0_a)");

      // T + 400 ns: CNOT(q0a, q0b) -> |Phi+>
      Simulator::Schedule(kTwoQGate, [=]() {
        node1->Apply(gates::CNOT(), {q0a, q0b});
        TraceEntangle({"bell_0_a", "bell_0_b"});
        TraceNodeText("Node1", "Bell pair 0 ready");

        // T + 500 ns: send q0b to Node2 (arrives T + 600 ns; ACK returns ~T + 1.6 ms)
        Simulator::Schedule(kSingleGate, [=]() {
          auto t0 = Simulator::Now();
          node1->Send(q0b, node2->GetId());
          TraceSendBit("bell_0_b", "Node1", "Node2", "quantum", t0, t0 + kQDelay);
          TraceNodeText("Node1", "bell_0_b in transit to Node2");
          TraceSetBitColor("bell_0_a", "#90EE90");
          std::cout << "[STEP 1] Bell pair 0 sent: Node1 -> Node2\n";
        });
      });
    });
  });

  // --- Bell pair 1: Node1 <-> Node3 ---
  // (Bell pair 0 finishes at ~1.65 us; 3 us gives clear temporal separation.)
  Simulator::Schedule(MicroSeconds(3), [&] {
    Trace("Step 2: Node1 initializes Bell pair 1 (for Node3)");
    TraceNodeText("Node1", "Step 2: init Bell pair 1");

    auto q1a = node1->CreateQubit();
    q1a->SetLabel("bell_1_a");
    auto q1b = node1->CreateQubit();
    q1b->SetLabel("bell_1_b");

    TraceCreateBit("Node1", "bell_1_a", "quantum", "#88CCEE");
    TraceCreateBit("Node1", "bell_1_b", "quantum", "#88CCEE");

    Simulator::Schedule(kSingleGate, [=]() {
      node1->Apply(gates::H(), {q1a});
      TraceSetBitColor("bell_1_a", "#BC71EB");
      TraceNodeText("Node1", "H(bell_1_a)");

      Simulator::Schedule(kTwoQGate, [=]() {
        node1->Apply(gates::CNOT(), {q1a, q1b});
        TraceEntangle({"bell_1_a", "bell_1_b"});
        TraceNodeText("Node1", "Bell pair 1 ready");

        Simulator::Schedule(kSingleGate, [=]() {
          auto t0 = Simulator::Now();
          node1->Send(q1b, node3->GetId());
          TraceSendBit("bell_1_b", "Node1", "Node3", "quantum", t0, t0 + kQDelay);
          TraceNodeText("Node1", "bell_1_b in transit to Node3");
          TraceSetBitColor("bell_1_a", "#90EE90");
          std::cout << "[STEP 2] Bell pair 1 sent: Node1 -> Node3\n";
        });
      });
    });
  });

  // --- Bell pair 2: Node2 <-> Node3 ---
  Simulator::Schedule(MicroSeconds(5), [&] {
    Trace("Step 3: Node2 initializes Bell pair 2 (for Node3)");
    TraceNodeText("Node2", "Step 3: init Bell pair 2");

    auto q2a = node2->CreateQubit();
    q2a->SetLabel("bell_2_a");
    auto q2b = node2->CreateQubit();
    q2b->SetLabel("bell_2_b");

    TraceCreateBit("Node2", "bell_2_a", "quantum", "#88CCEE");
    TraceCreateBit("Node2", "bell_2_b", "quantum", "#88CCEE");

    Simulator::Schedule(kSingleGate, [=]() {
      node2->Apply(gates::H(), {q2a});
      TraceSetBitColor("bell_2_a", "#BC71EB");
      TraceNodeText("Node2", "H(bell_2_a)");

      Simulator::Schedule(kTwoQGate, [=]() {
        node2->Apply(gates::CNOT(), {q2a, q2b});
        TraceEntangle({"bell_2_a", "bell_2_b"});
        TraceNodeText("Node2", "Bell pair 2 ready");

        Simulator::Schedule(kSingleGate, [=]() {
          auto t0 = Simulator::Now();
          node2->Send(q2b, node3->GetId());
          TraceSendBit("bell_2_b", "Node2", "Node3", "quantum", t0, t0 + kQDelay);
          TraceNodeText("Node2", "bell_2_b in transit to Node3");
          TraceSetBitColor("bell_2_a", "#90EE90");
          std::cout << "[STEP 3] Bell pair 2 sent: Node2 -> Node3\n";
        });
      });
    });
  });

  // --- Final trace at 20 us (all ACKs have arrived by ~16 us) ---
  Simulator::Schedule(MicroSeconds(20), [&] {
    Trace("All Bell pairs distributed and confirmed - full-mesh entanglement established");
    std::cout << "[DONE] All Bell pairs distributed\n";
  });

  Simulator::Stop(MilliSeconds(1));
  Simulator::Run();
  Simulator::Destroy();

  TraceWriter::Instance().Close();
  std::cout << "[DONE] Entanglement distribution finished\n";
  return 0;
}
