/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/
/**
 * @file q2nsviz-ghz-distribution-example.cc
 * @brief GHZ state generation and distribution across 4 nodes.
 *
 * Node1 locally prepares a 4-qubit GHZ state:
 *
 *   |GHZ> = (|0000> + |1111>) / sqrt(2)
 *
 * using a Hadamard gate on q0 followed by three sequential CNOT(q0, qi) for
 * i in {1,2,3} (serialized because all share q0 as control).  It then keeps
 * q0 and distributes q1->Node2, q2->Node3, q3->Node4 over quantum links,
 * completing the shared entangled resource.
 *
 * Timing model (illustrative, platform-neutral):
 *   kSingleGate = 100 ns  (single-qubit gate)
 *   kTwoQGate   = 300 ns  (two-qubit gate)
 *   kQDelay     = 100 ns  (quantum channel propagation, ~20 m fiber)
 *
 * Visualization output is written to
 * examples/example_traces/q2nsviz-ghz-distribution-example.json and can be loaded in the
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

#include <atomic>
#include <iostream>

using namespace ns3;
using namespace q2ns;

static const uint16_t kAckPort = 9100;

int main(int argc, char** argv) {
  std::cout << "[DEMO] GHZ state distribution (4 nodes) starting\n";

  RngSeedManager::SetSeed(1);
  RngSeedManager::SetRun(1);

  CommandLine cmd;
  cmd.Parse(argc, argv);

  // Visualization output
  TraceWriter::Instance().Open("examples/example_traces/q2nsviz-ghz-distribution-example.json");

  Trace("GHZ State Distribution with 4 nodes");

  Time::SetResolution(Time::NS);

  NetController net;
  net.SetQStateBackend(QStateBackend::Stab);

  // --- Nodes ---
  Ptr<QNode> node1 = net.CreateNode();
  Ptr<QNode> node2 = net.CreateNode();
  Ptr<QNode> node3 = net.CreateNode();
  Ptr<QNode> node4 = net.CreateNode();

  TraceCreateNode("Node1", 85, 50);
  TraceCreateNode("Node2", 50, 85);
  TraceCreateNode("Node3", 15, 50);
  TraceCreateNode("Node4", 49, 15);

  // --- Timing constants ---
  // Values are illustrative
  const Time kSingleGate = NanoSeconds(100); // single-qubit gate
  const Time kTwoQGate = NanoSeconds(300);   // two-qubit gate
  const Time kQDelay = NanoSeconds(100);     // quantum link propagation (~20 m fiber)
  const Time kClassical = kQDelay;           // classical propagation = quantum (~20 m fiber)

  // --- Quantum links (star from Node1) ---
  auto ch12 = net.InstallQuantumLink(node1, node2);
  auto ch13 = net.InstallQuantumLink(node1, node3);
  auto ch14 = net.InstallQuantumLink(node1, node4);
  ch12->SetAttribute("Delay", TimeValue(kQDelay));
  ch13->SetAttribute("Delay", TimeValue(kQDelay));
  ch14->SetAttribute("Delay", TimeValue(kQDelay));

  // Trace star topology channels (only the physical links that carry this protocol)
  TraceCreateChannel("Node1", "Node2", "quantum");
  TraceCreateChannel("Node1", "Node2", "classical");
  TraceCreateChannel("Node1", "Node3", "quantum");
  TraceCreateChannel("Node1", "Node3", "classical");
  TraceCreateChannel("Node1", "Node4", "quantum");
  TraceCreateChannel("Node1", "Node4", "classical");

  // --- Classical network (star p2p from Node1) ---
  InternetStackHelper internet;
  internet.Install(node1);
  internet.Install(node2);
  internet.Install(node3);
  internet.Install(node4);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("100ns"));

  Ipv4AddressHelper ipv4;

  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  auto ifs12 = ipv4.Assign(p2p.Install(node1, node2));

  ipv4.SetBase("10.1.2.0", "255.255.255.0");
  auto ifs13 = ipv4.Assign(p2p.Install(node1, node3));

  ipv4.SetBase("10.1.3.0", "255.255.255.0");
  auto ifs14 = ipv4.Assign(p2p.Install(node1, node4));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // ---------------------------------------------------------------------------
  // UDP ACK infrastructure
  // Each of Node2/3/4 sends a 1-byte ACK to Node1 upon qubit arrival.
  // Node1 aggregates them and fires a final summary trace when all three arrive.
  // ---------------------------------------------------------------------------
  // Note that ifs12.GetAddress(0) is Node1's IP on the 1-2 subnet; with global routing all
  // remote nodes can reach it regardless of which subnet they are on.
  const Ipv4Address kNode1Addr = ifs12.GetAddress(0);

  // Node1 ACK-receive socket
  auto node1AckRx = Socket::CreateSocket(node1, UdpSocketFactory::GetTypeId());
  node1AckRx->Bind(InetSocketAddress(Ipv4Address::GetAny(), kAckPort));
  int acksReceived = 0;
  node1AckRx->SetRecvCallback([&](Ptr<Socket> sock) {
    Address from;
    while (sock->RecvFrom(from)) {
      ++acksReceived;
      TraceNodeText("Node1", StrCat("ACK received (", acksReceived, "/3 GHZ nodes confirmed)"));
      std::cout << "[NODE1] ACK " << acksReceived << "/3 received\n";
      if (acksReceived == 3) {
        Trace("GHZ resource confirmed by all remote nodes");
        TraceSetBitColor("q0", "#44AA99");
      }
    }
  });

  // Pre-create one ACK transmit socket per remote node (connected before simulation).
  auto node2TxSock = Socket::CreateSocket(node2, UdpSocketFactory::GetTypeId());
  node2TxSock->Connect(InetSocketAddress(kNode1Addr, kAckPort));

  auto node3TxSock = Socket::CreateSocket(node3, UdpSocketFactory::GetTypeId());
  node3TxSock->Connect(InetSocketAddress(ifs13.GetAddress(0), kAckPort));

  auto node4TxSock = Socket::CreateSocket(node4, UdpSocketFactory::GetTypeId());
  node4TxSock->Connect(InetSocketAddress(ifs14.GetAddress(0), kAckPort));

  // Node2 receive callback: q1 arrives, send ACK to Node1
  node2->SetRecvCallback([&](std::shared_ptr<Qubit> q) {
    TraceSetBitColor(q->GetLabel(), "#90EE90");
    TraceNodeText("Node2", StrCat("GHZ qubit ", q->GetLabel(), " arrived"));
    std::cout << "[NODE2] " << q->GetLabel() << " arrived\n";
    node2TxSock->Send(Create<Packet>(1));
    TraceSendPacket("Node2", "Node1", Simulator::Now(), Simulator::Now() + kClassical,
                    StrCat("ACK: ", q->GetLabel(), " received"));
  });

  // Node3 receive callback: q2 arrives, send ACK to Node1
  node3->SetRecvCallback([&](std::shared_ptr<Qubit> q) {
    TraceSetBitColor(q->GetLabel(), "#90EE90");
    TraceNodeText("Node3", StrCat("GHZ qubit ", q->GetLabel(), " arrived"));
    std::cout << "[NODE3] " << q->GetLabel() << " arrived\n";
    node3TxSock->Send(Create<Packet>(1));
    TraceSendPacket("Node3", "Node1", Simulator::Now(), Simulator::Now() + kClassical,
                    StrCat("ACK: ", q->GetLabel(), " received"));
  });

  // Node4 receive callback: q3 arrives, send ACK to Node1
  node4->SetRecvCallback([&](std::shared_ptr<Qubit> q) {
    TraceSetBitColor(q->GetLabel(), "#90EE90");
    TraceNodeText("Node4", StrCat("GHZ qubit ", q->GetLabel(), " arrived"));
    std::cout << "[NODE4] " << q->GetLabel() << " arrived\n";
    node4TxSock->Send(Create<Packet>(1));
    TraceSendPacket("Node4", "Node1", Simulator::Now(), Simulator::Now() + kClassical,
                    StrCat("ACK: ", q->GetLabel(), " received"));
  });

  // ---------------------------------------------------------------------------
  // Simulation events
  // ---------------------------------------------------------------------------
  Simulator::Schedule(MicroSeconds(1), [=] {
    auto q0 = node1->CreateQubit("q0");
    auto q1 = node1->CreateQubit("q1");
    auto q2 = node1->CreateQubit("q2");
    auto q3 = node1->CreateQubit("q3");

    TraceCreateBit("Node1", "q0", "quantum", "#88CCEE");
    TraceCreateBit("Node1", "q1", "quantum", "#88CCEE");
    TraceCreateBit("Node1", "q2", "quantum", "#88CCEE");
    TraceCreateBit("Node1", "q3", "quantum", "#88CCEE");
    TraceNodeText("Node1", "q0-q3 initialized");

    // -- Prepare GHZ state on Node1 ---
    Simulator::Schedule(kSingleGate, [=] {
      Trace("Applying H to q0");
      node1->Apply(q2ns::gates::H(), {q0});
      TraceSetBitColor("q0", "#BC71EB");
      TraceNodeText("Node1", "H(q0)");

      Simulator::Schedule(kTwoQGate, [=] {
        Trace("CNOT(q0, q1)");
        node1->Apply(q2ns::gates::CNOT(), {q0, q1});
        TraceEntangle({"q0", "q1"});
        TraceNodeText("Node1", "CNOT(q0, q1)");

        Simulator::Schedule(kTwoQGate, [=] {
          Trace("CNOT(q0, q2)");
          node1->Apply(q2ns::gates::CNOT(), {q0, q2});
          TraceEntangle({"q0", "q2"});
          TraceNodeText("Node1", "CNOT(q0, q2)");

          Simulator::Schedule(kTwoQGate, [=] {
            Trace("CNOT(q0, q3) - GHZ state ready");
            node1->Apply(q2ns::gates::CNOT(), {q0, q3});
            TraceEntangle({"q0", "q3"});
            TraceNodeText("Node1", "GHZ state ready");
            std::cout << "[NODE1] GHZ state prepared: (|0000> + |1111>)/sqrt(2)\n";

            // Distribute qubits to nodes
            // Sends are staggered by 50 ns each for visual clarity
            // (three independent quantum channels, could be simultaneous physically).
            Simulator::Schedule(kSingleGate, [=] {
              Trace("Distributing GHZ qubits to nodes");
              TraceNodeText("Node1", "Sending q1/q2/q3 to nodes 2/3/4");

              const Time kSendGap = NanoSeconds(50);
              auto t0 = Simulator::Now();

              node1->Send(q1, node2->GetId());
              TraceSendBit("q1", "Node1", "Node2", "quantum", t0, t0 + kQDelay);

              Simulator::Schedule(kSendGap, [=] {
                auto t1 = Simulator::Now();
                node1->Send(q2, node3->GetId());
                TraceSendBit("q2", "Node1", "Node3", "quantum", t1, t1 + kQDelay);

                Simulator::Schedule(kSendGap, [=] {
                  auto t2 = Simulator::Now();
                  node1->Send(q3, node4->GetId());
                  TraceSendBit("q3", "Node1", "Node4", "quantum", t2, t2 + kQDelay);
                  TraceNodeText("Node1", "q1/q2/q3 in transit - awaiting ACKs");

                  Simulator::Schedule(kQDelay + NanoSeconds(50), [=] {
                    Trace("GHZ quantum distribution complete - waiting for classical ACKs");
                    std::cout << "[NODE1] GHZ qubits in flight, waiting for ACKs\n";
                  });
                });
              });
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
  std::cout << "[DONE] GHZ distribution finished\n";
  return 0;
}
