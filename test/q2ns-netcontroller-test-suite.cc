/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/
/**
 * @file q2ns-netcontroller-test-suite.cc
 * @brief NetController public API tests (topology + links + basic send/recv).
 *
 * These tests intentionally avoid internal plumbing types:
 * QProcessor, QNetworker, QNetDevice are not referenced.
 */

#include "ns3/log.h"
#include "ns3/rng-seed-manager.h"
#include "ns3/simulator.h"
#include "ns3/test.h"

#include "ns3/q2ns-netcontroller.h"
#include "ns3/q2ns-qchannel.h"
#include "ns3/q2ns-qgate.h"
#include "ns3/q2ns-qnode.h"
#include "ns3/q2ns-qubit.h"

#include <memory>

using namespace ns3;
using namespace q2ns;

NS_LOG_COMPONENT_DEFINE("Q2nsNetControllerTest");

/*-----------------------------------------------------------------------------
 * TestCase: CreateNode / GetNode
 *---------------------------------------------------------------------------*/
class NetControllerCreateNodeTestCase : public TestCase {
public:
  NetControllerCreateNodeTestCase()
      : TestCase("NetController CreateNode and GetNode return consistent pointers") {}

  void DoRun() override {
    Simulator::Destroy(); // ensure clean state

    NetController nc;

    auto a = nc.CreateNode();
    auto b = nc.CreateNode();

    NS_TEST_ASSERT_MSG_NE(a, nullptr, "CreateNode returned nullptr");
    NS_TEST_ASSERT_MSG_NE(b, nullptr, "CreateNode returned nullptr");
    NS_TEST_ASSERT_MSG_NE(a->GetId(), b->GetId(), "Nodes should have distinct ids");

    NS_TEST_ASSERT_MSG_EQ(nc.GetNode(a->GetId()), a, "GetNode should return the same Ptr");
    NS_TEST_ASSERT_MSG_EQ(nc.GetNode(b->GetId()), b, "GetNode should return the same Ptr");

    // Nonexistent id should return nullptr
    NS_TEST_ASSERT_MSG_EQ(nc.GetNode(999999), nullptr, "GetNode for missing id should be nullptr");

    Simulator::Destroy();
  }
};

/*-----------------------------------------------------------------------------
 * TestCase: InstallQuantumLink / GetChannel
 *---------------------------------------------------------------------------*/
class NetControllerInstallLinkTestCase : public TestCase {
public:
  NetControllerInstallLinkTestCase()
      : TestCase("NetController InstallQuantumLink creates retrievable channel") {}

  void DoRun() override {
    Simulator::Destroy();

    NetController nc;
    auto a = nc.CreateNode();
    auto b = nc.CreateNode();

    const Time d = NanoSeconds(10);
    auto ch = nc.InstallQuantumLink(a, b);
    ch->SetAttribute("Delay", TimeValue(d));

    NS_TEST_ASSERT_MSG_NE(ch, nullptr, "InstallQuantumLink returned nullptr channel");

    auto ch2 = nc.GetChannel(a, b);
    NS_TEST_ASSERT_MSG_EQ(ch2, ch, "GetChannel should return the installed channel");

    // Symmetry: reversed endpoints should return same channel
    auto ch3 = nc.GetChannel(b, a);
    NS_TEST_ASSERT_MSG_EQ(ch3, ch, "GetChannel should be symmetric in endpoints");

    Simulator::Destroy();
  }
};

/*-----------------------------------------------------------------------------
 * TestCase: InstallQuantumLink idempotence
 *---------------------------------------------------------------------------*/
class NetControllerInstallLinkIdempotentTestCase : public TestCase {
public:
  NetControllerInstallLinkIdempotentTestCase()
      : TestCase("InstallQuantumLink called twice returns existing channel") {}

  void DoRun() override {
    Simulator::Destroy();

    NetController nc;
    auto a = nc.CreateNode();
    auto b = nc.CreateNode();

    auto ch1 = nc.InstallQuantumLink(a, b);
    auto ch2 = nc.InstallQuantumLink(a, b);
    ch1->SetAttribute("Delay", TimeValue(NanoSeconds(10)));
    ch2->SetAttribute("Delay", TimeValue(NanoSeconds(999)));

    NS_TEST_ASSERT_MSG_NE(ch1, nullptr, "First channel was nullptr");
    NS_TEST_ASSERT_MSG_EQ(ch2, ch1, "Second InstallQuantumLink should return existing channel");

    Simulator::Destroy();
  }
};

/*-----------------------------------------------------------------------------
 * TestCase: SetQChannelParameters updates per-direction delay/jitter/map
 *
 * This assumes your QChannel exposes GetDelayAB/GetDelayBA etc. If it does not,
 * you can keep this as a behavior test (send timing) or add minimal getters.
 *---------------------------------------------------------------------------*/
class NetControllerSetQChannelParametersTestCase : public TestCase {
public:
  NetControllerSetQChannelParametersTestCase()
      : TestCase("SetQChannelParameters updates channel attributes") {}

  void DoRun() override {
    Simulator::Destroy();

    NetController nc;
    auto a = nc.CreateNode();
    auto b = nc.CreateNode();

    auto ch = nc.InstallQuantumLink(a, b);
    ch->SetAttribute("Delay", TimeValue(NanoSeconds(10)));
    NS_TEST_ASSERT_MSG_NE(ch, nullptr, "Channel is nullptr");

    // Update asymmetrically
    const Time dAB = NanoSeconds(50);
    const Time dBA = NanoSeconds(70);
    const Time jAB = NanoSeconds(3);
    const Time jBA = NanoSeconds(5);

    ch->SetDelayAB(dAB);
    ch->SetDelayBA(dBA);
    ch->SetJitterAB(jAB);
    ch->SetJitterBA(jBA);

    NS_TEST_ASSERT_MSG_EQ(ch->GetDelayAB(), dAB, "DelayAB not updated");
    NS_TEST_ASSERT_MSG_EQ(ch->GetDelayBA(), dBA, "DelayBA not updated");
    NS_TEST_ASSERT_MSG_EQ(ch->GetJitterAB(), jAB, "JitterAB not updated");
    NS_TEST_ASSERT_MSG_EQ(ch->GetJitterBA(), jBA, "JitterBA not updated");

    Simulator::Destroy();
  }
};

/*-----------------------------------------------------------------------------
 * TestCase: Send/Receive path works via NetController-installed link
 *---------------------------------------------------------------------------*/
class NetControllerSendReceiveTestCase : public TestCase {
public:
  NetControllerSendReceiveTestCase()
      : TestCase("Send from A to B via InstallQuantumLink triggers receive callback") {}

  void DoRun() override {
    Simulator::Destroy();

    RngSeedManager::SetSeed(1);
    RngSeedManager::SetRun(1);

    NetController nc;
    nc.SetQStateBackend(QStateBackend::Ket); // pick one; suite doesn't care which

    auto a = nc.CreateNode();
    auto b = nc.CreateNode();

    auto ch = nc.InstallQuantumLink(a, b);
    ch->SetAttribute("Delay", TimeValue(NanoSeconds(10)));

    bool recvB = false;
    b->SetRecvCallback([&](std::shared_ptr<Qubit>) { recvB = true; });

    auto q = a->CreateQubit();
    bool ok = a->Send(q, b->GetId());
    NS_TEST_ASSERT_MSG_EQ(ok, true, "Send should succeed with an installed link");

    Simulator::Stop(MilliSeconds(1));
    Simulator::Run();

    NS_TEST_ASSERT_MSG_EQ(recvB, true, "Receive callback on B should fire");

    Simulator::Destroy();
  }
};

/*-----------------------------------------------------------------------------
 * TestCase: AssignStreams can be called before Run, and twice, without breaking
 *---------------------------------------------------------------------------*/
class NetControllerAssignStreamsTestCase : public TestCase {
public:
  NetControllerAssignStreamsTestCase()
      : TestCase("AssignStreams is safe to call before Run and multiple times") {}

  void DoRun() override {
    Simulator::Destroy();

    RngSeedManager::SetSeed(3);
    RngSeedManager::SetRun(7);

    NetController nc;
    nc.SetQStateBackend(QStateBackend::DM);

    auto a = nc.CreateNode();
    auto b = nc.CreateNode();
    auto ch = nc.InstallQuantumLink(a, b);
    ch->SetAttribute("Delay", TimeValue(NanoSeconds(10)));

    // Call AssignStreams before any run
    int64_t used1 = nc.AssignStreams(0);
    NS_TEST_ASSERT_MSG_EQ(used1 >= 0, true, "AssignStreams should return non-negative");

    // Call again; allowed (should not crash)
    int64_t used2 = nc.AssignStreams(0);
    NS_TEST_ASSERT_MSG_EQ(used2 >= 0, true, "AssignStreams (again) should return non-negative");

    // Basic operation still works
    bool recvB = false;
    b->SetRecvCallback([&](std::shared_ptr<Qubit>) { recvB = true; });

    auto q = a->CreateQubit();
    bool ok = a->Send(q, b->GetId());
    NS_TEST_ASSERT_MSG_EQ(ok, true, "Send should still succeed after AssignStreams");

    Simulator::Stop(MilliSeconds(1));
    Simulator::Run();
    NS_TEST_ASSERT_MSG_EQ(recvB, true, "Receive should still work after AssignStreams");

    Simulator::Destroy();
  }
};

/**
 * \brief Test InstallQuantumChain() creates links only between adjacent nodes
 * \note For nodes [A, B, C, D], expected links are A<->B, B<->C, C<->D only
 * \see NetController::InstallQuantumChain(), NetController::GetChannel()
 */
class QChannelInstallQuantumChainCase : public TestCase {
public:
  QChannelInstallQuantumChainCase()
      : TestCase("NetController: InstallQuantumChain installs adjacent links only") {}

private:
  void DoRun() override {
    NetController nc;
    Ptr<QNode> nodeA = nc.CreateNode();
    Ptr<QNode> nodeB = nc.CreateNode();
    Ptr<QNode> nodeC = nc.CreateNode();
    Ptr<QNode> nodeD = nc.CreateNode();

    std::vector<Ptr<QNode>> nodes = {nodeA, nodeB, nodeC, nodeD};
    auto channels = nc.InstallQuantumChain(nodes);
    for (auto& ch : channels) {
      ch->SetAttribute("Delay", ns3::TimeValue(ns3::NanoSeconds(10)));
    }

    NS_TEST_ASSERT_MSG_EQ(channels.size(), 3u,
                          "InstallQuantumChain should create N-1 channels for N nodes");

    NS_TEST_ASSERT_MSG_NE(channels[0], nullptr, "Channel A<->B should be valid");
    NS_TEST_ASSERT_MSG_NE(channels[1], nullptr, "Channel B<->C should be valid");
    NS_TEST_ASSERT_MSG_NE(channels[2], nullptr, "Channel C<->D should be valid");

    NS_TEST_ASSERT_MSG_NE(nc.GetChannel(nodeA, nodeB), nullptr, "A<->B should exist");
    NS_TEST_ASSERT_MSG_NE(nc.GetChannel(nodeB, nodeC), nullptr, "B<->C should exist");
    NS_TEST_ASSERT_MSG_NE(nc.GetChannel(nodeC, nodeD), nullptr, "C<->D should exist");

    NS_TEST_ASSERT_MSG_EQ(nc.GetChannel(nodeA, nodeC), nullptr,
                          "A<->C should not exist in a chain");
    NS_TEST_ASSERT_MSG_EQ(nc.GetChannel(nodeA, nodeD), nullptr,
                          "A<->D should not exist in a chain");
    NS_TEST_ASSERT_MSG_EQ(nc.GetChannel(nodeB, nodeD), nullptr,
                          "B<->D should not exist in a chain");

    bool receivedAtB = false;
    nodeB->SetRecvCallback([&](std::shared_ptr<Qubit>) { receivedAtB = true; });

    auto q = nodeA->CreateQubit("qChain");
    const bool okSend = nodeA->Send(q, nodeB->GetId());
    NS_TEST_ASSERT_MSG_EQ(okSend, true, "Send over installed chain edge A->B should succeed");

    Simulator::Run();
    Simulator::Destroy();

    NS_TEST_ASSERT_MSG_EQ(receivedAtB, true, "Node B should receive qubit from A over chain edge");
  }
};


/**
 * \brief Test InstallQuantumAllToAll() creates links for every unordered node pair
 * \note For nodes [A, B, C, D], expected links are all 6 unordered pairs
 * \see NetController::InstallQuantumAllToAll(), NetController::GetChannel()
 */
class QChannelInstallQuantumAllToAllCase : public TestCase {
public:
  QChannelInstallQuantumAllToAllCase()
      : TestCase("NetController: InstallQuantumAllToAll installs full pairwise connectivity") {}

private:
  void DoRun() override {
    NetController nc;
    Ptr<QNode> nodeA = nc.CreateNode();
    Ptr<QNode> nodeB = nc.CreateNode();
    Ptr<QNode> nodeC = nc.CreateNode();
    Ptr<QNode> nodeD = nc.CreateNode();

    std::vector<Ptr<QNode>> nodes = {nodeA, nodeB, nodeC, nodeD};
    auto channels = nc.InstallQuantumAllToAll(nodes);
    for (auto& ch : channels) {
      ch->SetAttribute("Delay", ns3::TimeValue(ns3::NanoSeconds(10)));
    }

    NS_TEST_ASSERT_MSG_EQ(channels.size(), 6u,
                          "InstallQuantumAllToAll should create N*(N-1)/2 channels for N=4");

    for (const auto& ch : channels) {
      NS_TEST_ASSERT_MSG_NE(ch, nullptr, "Each all-to-all channel should be valid");
    }

    NS_TEST_ASSERT_MSG_NE(nc.GetChannel(nodeA, nodeB), nullptr, "A<->B should exist");
    NS_TEST_ASSERT_MSG_NE(nc.GetChannel(nodeA, nodeC), nullptr, "A<->C should exist");
    NS_TEST_ASSERT_MSG_NE(nc.GetChannel(nodeA, nodeD), nullptr, "A<->D should exist");
    NS_TEST_ASSERT_MSG_NE(nc.GetChannel(nodeB, nodeC), nullptr, "B<->C should exist");
    NS_TEST_ASSERT_MSG_NE(nc.GetChannel(nodeB, nodeD), nullptr, "B<->D should exist");
    NS_TEST_ASSERT_MSG_NE(nc.GetChannel(nodeC, nodeD), nullptr, "C<->D should exist");

    bool receivedAtD = false;
    nodeD->SetRecvCallback([&](std::shared_ptr<Qubit>) { receivedAtD = true; });

    auto q = nodeA->CreateQubit("qClique");
    const bool okSend = nodeA->Send(q, nodeD->GetId());
    NS_TEST_ASSERT_MSG_EQ(okSend, true, "Send over direct all-to-all edge A->D should succeed");

    Simulator::Run();
    Simulator::Destroy();

    NS_TEST_ASSERT_MSG_EQ(receivedAtD, true,
                          "Node D should receive qubit from A over direct all-to-all edge");
  }
};

/*-----------------------------------------------------------------------------
 * TestSuite registration
 *---------------------------------------------------------------------------*/
class NetControllerTestSuite : public TestSuite {
public:
  NetControllerTestSuite() : TestSuite("q2ns-netcontroller", Type::UNIT) {
    AddTestCase(new NetControllerCreateNodeTestCase, TestCase::Duration::QUICK);
    AddTestCase(new NetControllerInstallLinkTestCase, TestCase::Duration::QUICK);
    AddTestCase(new NetControllerInstallLinkIdempotentTestCase, TestCase::Duration::QUICK);
    AddTestCase(new NetControllerSetQChannelParametersTestCase, TestCase::Duration::QUICK);
    AddTestCase(new NetControllerSendReceiveTestCase, TestCase::Duration::QUICK);
    AddTestCase(new NetControllerAssignStreamsTestCase, TestCase::Duration::QUICK);

    // NetController helper topologies
    AddTestCase(new QChannelInstallQuantumChainCase, TestCase::Duration::QUICK);
    AddTestCase(new QChannelInstallQuantumAllToAllCase, TestCase::Duration::QUICK);
  }
};

static NetControllerTestSuite g_netControllerTestSuite;