/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/

#include "ns3/q2ns-netcontroller.h"
#include "ns3/q2ns-qchannel.h"
#include "ns3/q2ns-qgate.h"
#include "ns3/q2ns-qmap.h"
#include "ns3/q2ns-qnode.h"
#include "ns3/q2ns-qubit.h"

#include "ns3/core-module.h"
#include "ns3/ptr.h"
#include "ns3/simulator.h"
#include "ns3/test.h"

using namespace ns3;
using namespace q2ns;

/*-----------------------------------------------------------------------------
 * Test Suite: QChannel - Basic Configuration
 *---------------------------------------------------------------------------*/

/**
 * \brief Test connecting two quantum network devices to a channel
 * \see QChannel::Connect(), QChannel::GetNDevices(), QChannel::GetDevice()
 */
class QChannelConnectDevicesCase : public TestCase {
public:
  QChannelConnectDevicesCase() : TestCase("QChannel: connect two devices") {}

private:
  void DoRun() override {
    NetController nc;
    Ptr<QNode> nodeA = nc.CreateNode();
    Ptr<QNode> nodeB = nc.CreateNode();

    auto ch = nc.InstallQuantumLink(nodeA, nodeB);
    ch->SetAttribute("Delay", TimeValue(NanoSeconds(1)));
    NS_TEST_ASSERT_MSG_NE(ch, nullptr, "InstallQuantumLink should return a valid channel");
    NS_TEST_ASSERT_MSG_EQ(ch->GetNDevices(), 2u,
                          "Channel should have exactly 2 devices after InstallQuantumLink()");
    NS_TEST_ASSERT_MSG_NE(ch->GetDevice(0), nullptr, "Device at index 0 should be non-null");
    NS_TEST_ASSERT_MSG_NE(ch->GetDevice(1), nullptr, "Device at index 1 should be non-null");
    NS_TEST_ASSERT_MSG_EQ(ch->GetDevice(2), nullptr,
                          "GetDevice() with out-of-range index should return nullptr");

    Simulator::Run();
    Simulator::Destroy();
  }
};

/**
 * \brief Test setting symmetric propagation delay for both directions
 * \note Symmetric delay applies the same value to A->B and B->A directions
 * \see QChannel::SetDelay(), QChannel::GetDelay()
 */
class QChannelSymmetricDelayCase : public TestCase {
public:
  QChannelSymmetricDelayCase() : TestCase("QChannel: set symmetric delay") {}

private:
  void DoRun() override {
    auto ch = CreateObject<QChannel>();

    ch->SetDelay(NanoSeconds(100));
    NS_TEST_ASSERT_MSG_EQ(ch->GetDelay(), NanoSeconds(100),
                          "Symmetric delay should be 100ns for both directions");
  }
};


/**
 * \brief Test setting independent delays for A->B and B->A directions
 * \note Asymmetric delays model channels with different propagation times per direction
 * \see QChannel::SetDelayAB(), QChannel::SetDelayBA()
 */
class QChannelAsymmetricDelayCase : public TestCase {
public:
  QChannelAsymmetricDelayCase() : TestCase("QChannel: set asymmetric delays") {}

private:
  void DoRun() override {
    NetController nc;
    Ptr<QNode> nodeA = nc.CreateNode();
    Ptr<QNode> nodeB = nc.CreateNode();

    auto ch = nc.InstallQuantumLink(nodeA, nodeB);
    NS_TEST_ASSERT_MSG_NE(ch, nullptr, "InstallQuantumLink should return a valid channel");

    ch->SetDelayAB(MicroSeconds(1));
    ch->SetDelayBA(MicroSeconds(5));

    Time receivedTimeAB = Seconds(-1);
    Time receivedTimeBA = Seconds(-1);

    nodeB->SetRecvCallback([&](std::shared_ptr<Qubit>) { receivedTimeAB = Simulator::Now(); });
    nodeA->SetRecvCallback([&](std::shared_ptr<Qubit>) { receivedTimeBA = Simulator::Now(); });

    auto qAB = nodeA->CreateQubit("qAB");
    auto qBA = nodeB->CreateQubit("qBA");
    nodeA->Send(qAB, nodeB->GetId());
    nodeB->Send(qBA, nodeA->GetId());

    Simulator::Run();
    Simulator::Destroy();

    NS_TEST_ASSERT_MSG_EQ(receivedTimeAB, MicroSeconds(1), "A to B should arrive at 1us");
    NS_TEST_ASSERT_MSG_EQ(receivedTimeBA, MicroSeconds(5), "B to A should arrive at 5us");
  }
};

/*-----------------------------------------------------------------------------
 * Test Suite: QChannel - Transmission Tests
 *---------------------------------------------------------------------------*/

/**
 * \brief Test delivering a qubit with zero propagation delay
 * \note Zero delay means qubit arrives immediately at simulation time 0
 * \see QChannel::SendFrom(), QChannel::SetDelay()
 */
class QChannelZeroDelayCase : public TestCase {
public:
  QChannelZeroDelayCase() : TestCase("QChannel: deliver qubit with zero delay") {}

private:
  void DoRun() override {
    NetController nc;
    Ptr<QNode> nodeA = nc.CreateNode();
    Ptr<QNode> nodeB = nc.CreateNode();
    auto ch = nc.InstallQuantumLink(nodeA, nodeB);
    NS_TEST_ASSERT_MSG_NE(ch, nullptr, "InstallQuantumLink should return a valid channel");

    bool received = false;
    nodeB->SetRecvCallback([&](std::shared_ptr<Qubit>) { received = true; });

    auto q = nodeA->CreateQubit("q");
    nodeA->Send(q, nodeB->GetId());

    Simulator::Run();
    Simulator::Destroy();

    NS_TEST_ASSERT_MSG_EQ(received, true, "qubit should be received");
  }
};

/**
 * \brief Test delivering a qubit with finite propagation delay
 * \note Verifies qubit arrival time matches configured channel delay
 * \see QChannel::SendFrom(), QChannel::SetDelay()
 */
class QChannelFiniteDelayCase : public TestCase {
public:
  QChannelFiniteDelayCase() : TestCase("QChannel: deliver qubit with finite delay") {}

private:
  void DoRun() override {
    NetController nc;
    Ptr<QNode> nodeA = nc.CreateNode();
    Ptr<QNode> nodeB = nc.CreateNode();

    auto ch = nc.InstallQuantumLink(nodeA, nodeB);
    ch->SetAttribute("Delay", TimeValue(MilliSeconds(10)));
    NS_TEST_ASSERT_MSG_NE(ch, nullptr, "InstallQuantumLink should return a valid channel");

    Time receivedTime = Seconds(-1);
    nodeB->SetRecvCallback([&](std::shared_ptr<Qubit>) { receivedTime = Simulator::Now(); });

    auto q = nodeA->CreateQubit("q");
    nodeA->Send(q, nodeB->GetId());

    Simulator::Run();
    Simulator::Destroy();

    NS_TEST_ASSERT_MSG_EQ(receivedTime, MilliSeconds(10), "qubit should arrive at 10ms");
  }
};

/**
 * \brief Test simultaneous bidirectional qubit transmission
 * \note Verifies full-duplex capability with concurrent A->B and B->A transfers
 * \see QChannel::SendFrom()
 */
class QChannelBidirectionalCase : public TestCase {
public:
  QChannelBidirectionalCase() : TestCase("QChannel: bidirectional qubit exchange") {}

private:
  void DoRun() override {
    NetController nc;
    Ptr<QNode> nodeA = nc.CreateNode();
    Ptr<QNode> nodeB = nc.CreateNode();

    auto ch = nc.InstallQuantumLink(nodeA, nodeB);
    ch->SetAttribute("Delay", TimeValue(NanoSeconds(10)));
    NS_TEST_ASSERT_MSG_NE(ch, nullptr, "InstallQuantumLink should return a valid channel");

    bool aReceived = false;
    bool bReceived = false;

    nodeA->SetRecvCallback([&](std::shared_ptr<Qubit> q) { aReceived = true; });
    nodeB->SetRecvCallback([&](std::shared_ptr<Qubit> q) { bReceived = true; });

    auto qAB = nodeA->CreateQubit("qAB");
    auto qBA = nodeB->CreateQubit("qBA");

    nodeA->Send(qAB, nodeB->GetId());
    nodeB->Send(qBA, nodeA->GetId());

    Simulator::Run();
    Simulator::Destroy();

    NS_TEST_ASSERT_MSG_EQ(bReceived, true, "node B should receive qubit from A");
    NS_TEST_ASSERT_MSG_EQ(aReceived, true, "node A should receive qubit from B");
  }
};

/*-----------------------------------------------------------------------------
 * Test Suite: QChannel - Loss and Noise
 *---------------------------------------------------------------------------*/

/**
 * \brief Test perfect loss (probability = 1.0) ensures no qubit delivery
 * \note Deterministic test: loss probability of 1.0 guarantees packet drop
 * \see QChannel::SetLossProbability(), QChannel::SendFrom()
 */
class QChannelPerfectLossCase : public TestCase {
public:
  QChannelPerfectLossCase() : TestCase("QChannel: perfect loss probability 1.0") {}

private:
  void DoRun() override {
    NetController nc;
    Ptr<QNode> nodeA = nc.CreateNode();
    Ptr<QNode> nodeB = nc.CreateNode();

    auto ch = nc.InstallQuantumLink(nodeA, nodeB);
    ch->SetAttribute("Delay", TimeValue(NanoSeconds(10)));
    NS_TEST_ASSERT_MSG_NE(ch, nullptr, "InstallQuantumLink should return a valid channel");

    auto loss = ns3::CreateObject<LossQMap>();
    loss->SetAttribute("Probability", ns3::DoubleValue(1.0)); // 100% loss
    ch->SetQMap(loss);

    bool received = false;
    nodeB->SetRecvCallback([&](std::shared_ptr<Qubit>) { received = true; });

    auto q = nodeA->CreateQubit("q");
    nodeA->Send(q, nodeB->GetId());

    Simulator::Run();
    Simulator::Destroy();

    NS_TEST_ASSERT_MSG_EQ(received, false, "qubit should be lost with p=1.0");
  }
};

/**
 * \brief Test independent loss probabilities for A->B and B->A directions
 * \note Models asymmetric fiber attenuation or directional loss characteristics
 * \see QChannel::SetLossProbabilityAB(), QChannel::SetLossProbabilityBA()
 */
class QChannelAsymmetricLossCase : public TestCase {
public:
  QChannelAsymmetricLossCase() : TestCase("QChannel: asymmetric loss probabilities") {}

private:
  void DoRun() override {
    NetController nc;
    Ptr<QNode> nodeA = nc.CreateNode();
    Ptr<QNode> nodeB = nc.CreateNode();

    auto ch = nc.InstallQuantumLink(nodeA, nodeB);
    ch->SetAttribute("Delay", TimeValue(NanoSeconds(10)));
    NS_TEST_ASSERT_MSG_NE(ch, nullptr, "InstallQuantumLink should return a valid channel");

    auto lossAB = ns3::CreateObject<LossQMap>();
    lossAB->SetAttribute("Probability", ns3::DoubleValue(0.0)); // 0% loss probability
    ch->SetQMapAB(lossAB);

    auto lossBA = ns3::CreateObject<LossQMap>();
    lossBA->SetAttribute("Probability", ns3::DoubleValue(1.0)); // 100% loss probability
    ch->SetQMapBA(lossBA);

    bool aReceived = false;
    bool bReceived = false;

    nodeA->SetRecvCallback([&](std::shared_ptr<Qubit>) { aReceived = true; });
    nodeB->SetRecvCallback([&](std::shared_ptr<Qubit>) { bReceived = true; });

    auto qAB = nodeA->CreateQubit("qAB");
    auto qBA = nodeB->CreateQubit("qBA");

    nodeA->Send(qAB, nodeB->GetId());
    nodeB->Send(qBA, nodeA->GetId());

    Simulator::Run();
    Simulator::Destroy();

    NS_TEST_ASSERT_MSG_EQ(bReceived, true, "A->B should succeed (loss p=0.0)");
    NS_TEST_ASSERT_MSG_EQ(aReceived, false, "B->A should fail (loss p=1.0)");
  }
};

/**
 * \brief Test that a qubit lost in transmission is marked LOST and becomes unusable for
 *        meaningful operations (send/measure/apply-gate) while remaining as an inspectable handle.
 *
 * \note Assumes:
 *   - Loss probability 1.0 in QChannel causes q->SetLocationLost() and registry location becomes
 * Lost
 *   - QNetworker::Send rejects lost qubits
 *   - QProcessor::Measure rejects non-local or lost qubits (via location check)
 *   - QProcessor::Apply rejects non-local qubits (or lost via location/locality guards)
 */
class QChannelLostQubitInaccessibleCase : public TestCase {
public:
  QChannelLostQubitInaccessibleCase()
      : TestCase("QChannel: lost qubit marked LOST and is inaccessible") {}

private:
  void DoRun() override {
    NetController nc;
    Ptr<QNode> nodeA = nc.CreateNode();
    Ptr<QNode> nodeB = nc.CreateNode();

    auto ch = nc.InstallQuantumLink(nodeA, nodeB);
    ch->SetAttribute("Delay", TimeValue(NanoSeconds(10)));
    NS_TEST_ASSERT_MSG_NE(ch, nullptr, "InstallQuantumLink should return a valid channel");

    auto loss = ns3::CreateObject<LossQMap>();
    loss->SetAttribute("Probability", ns3::DoubleValue(1.0));
    ch->SetQMap(loss);

    bool received = false;
    nodeB->SetRecvCallback([&](std::shared_ptr<Qubit> /*q*/) { received = true; });

    auto q = nodeA->CreateQubit("qLost");

    // Sanity: qubit should be local before send
    NS_TEST_ASSERT_MSG_NE(nodeA->GetQubit("qLost"), nullptr,
                          "Sender node should have qLost locally before send");

    const bool okFirstSend = nodeA->Send(q, nodeB->GetId());
    NS_TEST_ASSERT_MSG_EQ(okFirstSend, true,
                          "First send request should be accepted (loss happens in channel)");

    Simulator::Run();
    Simulator::Destroy();

    // Receiver should never get it
    NS_TEST_ASSERT_MSG_EQ(received, false,
                          "Receiver should not receive qubit when loss prob = 1.0");

    // Sender processor should no longer claim it as local (it was sent)
    NS_TEST_ASSERT_MSG_EQ(nodeA->GetQubit("qLost"), nullptr,
                          "Sender node should not keep qLost locally after send");

    // Registry location should be LOST
    auto loc = q->GetLocation();
    NS_TEST_ASSERT_MSG_EQ(loc.type != LocationType::Unset, true, "Lost qubit should not be Unset");
    NS_TEST_ASSERT_MSG_EQ(static_cast<int>(loc.type), static_cast<int>(LocationType::Lost),
                          "Lost qubit must be marked LocationType::Lost");

    // Try to send again: should be rejected
    const bool okSecondSend = nodeA->Send(q, nodeB->GetId());
    NS_TEST_ASSERT_MSG_EQ(okSecondSend, false, "Sending a LOST qubit must be rejected");

    // Try to measure: should be rejected (nonlocal/lost)
    const int bit = nodeA->Measure(q, Basis::Z);
    NS_TEST_ASSERT_MSG_EQ(bit, -1, "Measuring a LOST qubit must be rejected");

    // Try to apply a gate: should be rejected (not local)
    const bool gateOk = nodeA->Apply(gates::H(), {q});
    NS_TEST_ASSERT_MSG_EQ(gateOk, false,
                          "Applying a gate to a LOST/nonlocal qubit must be rejected");
  }
};


/*-----------------------------------------------------------------------------
 * Test Suite: QChannel - Quantum Maps
 *---------------------------------------------------------------------------*/

/**
 * \brief Test identity transit map (nullptr = no operation during transmission)
 * \note Identity map leaves quantum state unchanged during channel propagation
 * \see QChannel::SetMap()
 */
class QChannelIdentityMapCase : public TestCase {
public:
  QChannelIdentityMapCase() : TestCase("QChannel: identity transit map") {}

private:
  void DoRun() override {
    NetController nc;
    Ptr<QNode> nodeA = nc.CreateNode();
    Ptr<QNode> nodeB = nc.CreateNode();

    auto ch = nc.InstallQuantumLink(nodeA, nodeB);
    ch->SetAttribute("Delay", TimeValue(NanoSeconds(10)));
    NS_TEST_ASSERT_MSG_NE(ch, nullptr, "InstallQuantumLink should return a valid channel");

    bool received = false;
    nodeB->SetRecvCallback([&](std::shared_ptr<Qubit>) { received = true; });

    auto q = nodeA->CreateQubit("q");
    nodeA->Send(q, nodeB->GetId());

    Simulator::Run();
    Simulator::Destroy();

    NS_TEST_ASSERT_MSG_EQ(received, true, "qubit should be received with identity map");
  }
};

/**
 * \brief Test transit map that applies a quantum gate during transmission
 * \note Models channel-induced transformations (e.g., phase shifts, depolarization)
 * \see QChannel::SetMap()
 */
class QChannelGateMapCase : public TestCase {
public:
  QChannelGateMapCase() : TestCase("QChannel: transit map applies gate") {}

private:
  void DoRun() override {
    NetController nc;
    Ptr<QNode> nodeA = nc.CreateNode();
    Ptr<QNode> nodeB = nc.CreateNode();

    auto ch = nc.InstallQuantumLink(nodeA, nodeB);
    ch->SetAttribute("Delay", TimeValue(NanoSeconds(10)));
    NS_TEST_ASSERT_MSG_NE(ch, nullptr, "InstallQuantumLink should return a valid channel");

    // Map applies X gate during transit
    ch->SetQMap(QMap::FromLambda(
        [](QNode& node, std::shared_ptr<Qubit>& q) { node.Apply(gates::X(), {q}); }));

    std::shared_ptr<Qubit> receivedQubit;
    nodeB->SetRecvCallback([&](std::shared_ptr<Qubit> q) { receivedQubit = q; });

    auto q = nodeA->CreateQubit("q"); // starts as |0>
    nodeA->Send(q, nodeB->GetId());

    Simulator::Run();

    NS_TEST_ASSERT_MSG_NE(receivedQubit, nullptr, "qubit should be received");

    // Measure the received qubit (should be |1> after X gate)
    int bit = nodeB->Measure(receivedQubit, Basis::Z);

    Simulator::Destroy();

    NS_TEST_ASSERT_MSG_EQ(bit, 1, "qubit should be in state one after transit X gate");
  }
};

/**
 * \brief Test independent quantum maps for A->B and B->A directions
 * \note Models direction-dependent channel imperfections or transformations
 * \see QChannel::SetMapAB(), QChannel::SetMapBA()
 */
class QChannelAsymmetricMapCase : public TestCase {
public:
  QChannelAsymmetricMapCase() : TestCase("QChannel: asymmetric transit maps") {}

private:
  void DoRun() override {
    NetController nc;
    Ptr<QNode> nodeA = nc.CreateNode();
    Ptr<QNode> nodeB = nc.CreateNode();

    auto ch = nc.InstallQuantumLink(nodeA, nodeB);
    ch->SetAttribute("Delay", TimeValue(NanoSeconds(10)));
    NS_TEST_ASSERT_MSG_NE(ch, nullptr, "InstallQuantumLink should return a valid channel");

    // A->B: apply X gate
    ch->SetQMapAB(QMap::FromLambda(
        [](QNode& node, std::shared_ptr<Qubit>& q) { node.Apply(gates::X(), {q}); }));

    // B->A: identity (no operation)


    std::shared_ptr<Qubit> receivedAtB;
    std::shared_ptr<Qubit> receivedAtA;

    nodeA->SetRecvCallback([&](std::shared_ptr<Qubit> q) { receivedAtA = q; });
    nodeB->SetRecvCallback([&](std::shared_ptr<Qubit> q) { receivedAtB = q; });

    auto qAB = nodeA->CreateQubit("qAB"); // |0>
    auto qBA = nodeB->CreateQubit("qBA"); // |0>

    nodeA->Send(qAB, nodeB->GetId());
    nodeB->Send(qBA, nodeA->GetId());

    Simulator::Run();

    NS_TEST_ASSERT_MSG_NE(receivedAtB, nullptr, "qubit should be received at B");
    NS_TEST_ASSERT_MSG_NE(receivedAtA, nullptr, "qubit should be received at A");

    int bitAtB = nodeB->Measure(receivedAtB, Basis::Z);
    int bitAtA = nodeA->Measure(receivedAtA, Basis::Z);

    Simulator::Destroy();

    NS_TEST_ASSERT_MSG_EQ(bitAtB, 1, "A->B qubit should be flipped by X gate");
    NS_TEST_ASSERT_MSG_EQ(bitAtA, 0, "B->A qubit should remain zero (identity map)");
  }
};

/*-----------------------------------------------------------------------------
 * Test Suite: QChannel - Edge Cases
 *---------------------------------------------------------------------------*/

// NOTE: Some former edge-case tests (e.g., unattached devices, multiple Connect calls,
// and null-device wiring) were removed because QNetDevice is now intentionally an
// internal implementation detail. QChannel behavior is validated via NetController/QNode
// integration tests above.

/**
 * \brief Edge case: Negative delay values (symmetric setter)
 * \note ns-3 Time can represent negative values; tests channel behavior
 * \see QChannel::SetDelay()
 */
class QChannelNegativeDelayCase : public TestCase {
public:
  QChannelNegativeDelayCase() : TestCase("QChannel: negative delay value") {}

private:
  void DoRun() override {
    auto ch = CreateObject<QChannel>();
    ch->SetDelay(NanoSeconds(50));

    NS_TEST_ASSERT_MSG_EQ(ch->GetDelay(), NanoSeconds(50), "Initial delay should be 50ns");

    // Attempt to set negative delay (should be rejected)
    ch->SetDelay(NanoSeconds(-100));

    NS_TEST_ASSERT_MSG_EQ(ch->GetDelay(), NanoSeconds(50),
                          "Negative delay should be ignored, delay remains 50ns");
  }
};

/**
 * \brief Edge case: Negative delay in asymmetric setters
 * \note Tests that SetDelayAB/BA reject negative values
 * \see QChannel::SetDelayAB(), QChannel::SetDelayBA()
 */
class QChannelAsymmetricNegativeDelayCase : public TestCase {
public:
  QChannelAsymmetricNegativeDelayCase()
      : TestCase("QChannel: negative delay in asymmetric setters") {}

private:
  void DoRun() override {
    auto ch = CreateObject<QChannel>();

    // Set valid initial delays
    ch->SetDelayAB(MicroSeconds(10));
    ch->SetDelayBA(MicroSeconds(20));

    NS_TEST_ASSERT_MSG_EQ(ch->GetDelay(), MicroSeconds(10), "Initial A->B delay should be 10us");

    // Attempt to set negative delays (should be rejected)
    ch->SetDelayAB(NanoSeconds(-100));

    NS_TEST_ASSERT_MSG_EQ(ch->GetDelay(), MicroSeconds(10),
                          "SetDelayAB should reject negative value, delay remains 10us");
  }
};


/*-----------------------------------------------------------------------------
 * Suite Assembly
 *---------------------------------------------------------------------------*/
/**
 * \ingroup q2ns-test
 * \brief Test suite for QChannel quantum communication channel operations
 */
class Q2nsChannelTestSuite : public TestSuite {
public:
  Q2nsChannelTestSuite() : TestSuite("q2ns-qchannel", TestSuite::Type::UNIT) {
    // Basic configuration
    AddTestCase(new QChannelConnectDevicesCase, TestCase::Duration::QUICK);
    AddTestCase(new QChannelSymmetricDelayCase, TestCase::Duration::QUICK);
    AddTestCase(new QChannelAsymmetricDelayCase, TestCase::Duration::QUICK);

    // Transmission tests
    AddTestCase(new QChannelZeroDelayCase, TestCase::Duration::QUICK);
    AddTestCase(new QChannelFiniteDelayCase, TestCase::Duration::QUICK);
    AddTestCase(new QChannelBidirectionalCase, TestCase::Duration::QUICK);

    // Loss and noise
    AddTestCase(new QChannelPerfectLossCase, TestCase::Duration::QUICK);
    AddTestCase(new QChannelAsymmetricLossCase, TestCase::Duration::QUICK);
    AddTestCase(new QChannelLostQubitInaccessibleCase, TestCase::Duration::QUICK);

    // Quantum maps
    AddTestCase(new QChannelIdentityMapCase, TestCase::Duration::QUICK);
    AddTestCase(new QChannelGateMapCase, TestCase::Duration::QUICK);
    AddTestCase(new QChannelAsymmetricMapCase, TestCase::Duration::QUICK);

    // Edge cases - validation (symmetric setters)
    AddTestCase(new QChannelNegativeDelayCase, TestCase::Duration::QUICK);

    // Edge cases - validation (asymmetric setters)
    AddTestCase(new QChannelAsymmetricNegativeDelayCase, TestCase::Duration::QUICK);
  }
};

static Q2nsChannelTestSuite g_q2nsChannelTestSuite; ///< Static instance for auto-registration