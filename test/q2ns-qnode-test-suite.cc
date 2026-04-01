/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/

#include "ns3/q2ns-netcontroller.h"
#include "ns3/q2ns-qgate.h"
#include "ns3/q2ns-qmap.h"
#include "ns3/q2ns-qnode.h"
#include "ns3/q2ns-qstate.h"
#include "ns3/q2ns-qubit.h"

#include "ns3/simulator.h"
#include "ns3/test.h"

#include <memory>


using namespace ns3;
using namespace q2ns;

namespace {

// Small helper: run + destroy the simulator for each test case.
inline void RunAndDestroy(Time stopAt = MilliSeconds(1)) {
  Simulator::Stop(stopAt);
  Simulator::Run();
  Simulator::Destroy();
}

inline void RunOnly(Time stopAt = MilliSeconds(1)) {
  Simulator::Stop(stopAt);
  Simulator::Run();
}

inline bool IsBit(int x) {
  return x == 0 || x == 1;
}

} // namespace

/*-----------------------------------------------------------------------------
 * Test cases
 *---------------------------------------------------------------------------*/

class QNodeCreateQubitCase : public TestCase {
public:
  QNodeCreateQubitCase() : TestCase("QNode: CreateQubit(label) creates a qubit in the 0 state") {}

private:
  void DoRun() override {
    NetController net;
    net.SetQStateBackend(QStateBackend::Ket);

    auto A = net.CreateNode();

    auto q = A->CreateQubit("test");
    NS_TEST_ASSERT_MSG_NE(q, nullptr, "CreateQubit should return a non-null qubit");

    NS_TEST_ASSERT_MSG_EQ(q->GetLabel(), std::string("test"), "Label should be preserved");
    NS_TEST_ASSERT_MSG_EQ(q->GetIndexInState(), 0u,
                          "First qubit in a fresh 1-qubit state should have index 0");
    NS_TEST_ASSERT_MSG_NE(q->GetStateId(), 0u, "StateId should be assigned");

    auto st = A->GetState(q);
    NS_TEST_ASSERT_MSG_NE(st, nullptr, "QNode::GetState should resolve state");
    NS_TEST_ASSERT_MSG_EQ(st->NumQubits(), 1u,
                          "Freshly created qubit should live in a 1-qubit state");

    RunAndDestroy();
  }
};

class QNodeApplyMeasureCase : public TestCase {
public:
  QNodeApplyMeasureCase() : TestCase("QNode: Apply(X) flips 0 to 1 and Measure(Z)=1") {}

private:
  void DoRun() override {
    NetController net;
    net.SetQStateBackend(QStateBackend::Ket);

    auto A = net.CreateNode();
    auto q = A->CreateQubit();

    const bool ok = A->Apply(gates::X(), {q});
    NS_TEST_ASSERT_MSG_EQ(ok, true, "Apply should succeed on a local qubit");

    const int m = A->Measure(q, q2ns::Basis::Z);
    NS_TEST_ASSERT_MSG_EQ(m, 1, "X|0> should measure to 1 in Z basis");

    RunAndDestroy();
  }
};

class QNodeGetQubitCase : public TestCase {
public:
  QNodeGetQubitCase() : TestCase("QNode: GetQubit by label and by id") {}

private:
  void DoRun() override {
    NetController net;
    auto A = net.CreateNode();

    auto q = A->CreateQubit("alice");
    NS_TEST_ASSERT_MSG_NE(q, nullptr, "CreateQubit should succeed");

    auto byLabel = A->GetQubit("alice");
    NS_TEST_ASSERT_MSG_NE(byLabel, nullptr, "GetQubit(label) should find an existing local qubit");
    NS_TEST_ASSERT_MSG_EQ(byLabel.get(), q.get(), "GetQubit(label) should return the same handle");

    auto byId = A->GetQubit(q->GetQubitId());
    NS_TEST_ASSERT_MSG_NE(byId, nullptr, "GetQubit(id) should find an existing local qubit");
    NS_TEST_ASSERT_MSG_EQ(byId.get(), q.get(), "GetQubit(id) should return the same handle");

    auto missing = A->GetQubit("does-not-exist");
    NS_TEST_ASSERT_MSG_EQ(missing, nullptr,
                          "GetQubit(label) should return nullptr for missing label");

    RunAndDestroy();
  }
};

class QNodeBellPairCorrelationCase : public TestCase {
public:
  QNodeBellPairCorrelationCase()
      : TestCase("QNode: CreateBellPair yields Z-basis correlated outcomes") {}

private:
  void DoRun() override {
    NetController net;
    net.SetQStateBackend(QStateBackend::Ket);

    auto A = net.CreateNode();
    auto [q0, q1] = A->CreateBellPair();

    NS_TEST_ASSERT_MSG_NE(q0, nullptr, "Bell qubit 0 should be non-null");
    NS_TEST_ASSERT_MSG_NE(q1, nullptr, "Bell qubit 1 should be non-null");
    NS_TEST_ASSERT_MSG_EQ(q0->GetStateId(), q1->GetStateId(),
                          "Bell pair should start in the same state");

    const int m0 = A->Measure(q0, q2ns::Basis::Z);
    const int m1 = A->Measure(q1, q2ns::Basis::Z);

    NS_TEST_ASSERT_MSG_EQ(IsBit(m0), true, "Measurement must be a bit");
    NS_TEST_ASSERT_MSG_EQ(IsBit(m1), true, "Measurement must be a bit");
    NS_TEST_ASSERT_MSG_EQ(m0, m1, "|Phi+> should give perfectly correlated Z outcomes");

    RunAndDestroy();
  }
};

class QNodeMeasureBellSplitsStateCase : public TestCase {
public:
  QNodeMeasureBellSplitsStateCase()
      : TestCase("QNode: MeasureBell returns bits and leaves qubits in 1-qubit states") {}

private:
  void DoRun() override {
    NetController net;
    net.SetQStateBackend(QStateBackend::Ket);

    auto A = net.CreateNode();
    auto [a, b] = A->CreateBellPair();

    auto [mZZ, mXX] = A->MeasureBell(a, b);
    NS_TEST_ASSERT_MSG_EQ(IsBit(mZZ), true, "mZZ must be a bit");
    NS_TEST_ASSERT_MSG_EQ(IsBit(mXX), true, "mXX must be a bit");

    auto sa = A->GetState(a);
    auto sb = A->GetState(b);
    NS_TEST_ASSERT_MSG_NE(sa, nullptr, "a should resolve to a state after MeasureBell");
    NS_TEST_ASSERT_MSG_NE(sb, nullptr, "b should resolve to a state after MeasureBell");
    NS_TEST_ASSERT_MSG_EQ(sa->NumQubits(), 1u, "a should be in a 1-qubit state after MeasureBell");
    NS_TEST_ASSERT_MSG_EQ(sb->NumQubits(), 1u, "b should be in a 1-qubit state after MeasureBell");

    RunAndDestroy();
  }
};

class QNodeSendRecvCallbackCase : public TestCase {
public:
  QNodeSendRecvCallbackCase()
      : TestCase("QNode: Send triggers receive callback and preserves Bell Z-correlation") {}

private:
  void DoRun() override {
    NetController net;
    net.SetQStateBackend(QStateBackend::Ket);

    auto A = net.CreateNode();
    auto B = net.CreateNode();

    auto ch = net.InstallQuantumLink(A, B);
    ch->SetAttribute("Delay", TimeValue(NanoSeconds(10)));

    auto [qA, qB] = A->CreateBellPair();

    bool recv = false;
    B->SetRecvCallback([&](std::shared_ptr<Qubit> q) {
      // Minimal sanity: callback qubit handle should be non-null.
      recv = (q != nullptr);
    });

    const bool ok = A->Send(qB, B->GetId());
    NS_TEST_ASSERT_MSG_EQ(ok, true, "Send should accept a routable destination");

    RunOnly(MilliSeconds(5));

    NS_TEST_ASSERT_MSG_EQ(recv, true, "Receive callback should have fired at B");

    // Post-delivery: verify expected Bell correlation in Z.
    const int mA = A->Measure(qA, q2ns::Basis::Z);
    const int mB = B->Measure(qB, q2ns::Basis::Z);

    NS_TEST_ASSERT_MSG_EQ(IsBit(mA), true, "mA must be a bit");
    NS_TEST_ASSERT_MSG_EQ(IsBit(mB), true, "mB must be a bit");
    NS_TEST_ASSERT_MSG_EQ(mA, mB, "Bell Z-measurements across nodes should be correlated");

    Simulator::Destroy();
  }
};

class QNodeNonLocalGateFailsCase : public TestCase {
public:
  QNodeNonLocalGateFailsCase() : TestCase("QNode: Apply on a non-local qubit should fail") {}

private:
  void DoRun() override {
    NetController net;
    net.SetQStateBackend(QStateBackend::Ket);

    auto A = net.CreateNode();
    auto B = net.CreateNode();

    auto ch = net.InstallQuantumLink(A, B);
    ch->SetAttribute("Delay", TimeValue(NanoSeconds(10)));

    auto q = A->CreateQubit();

    // Deliver q to B.
    B->SetRecvCallback([](std::shared_ptr<Qubit>) {});
    const bool ok = A->Send(q, B->GetId());
    NS_TEST_ASSERT_MSG_EQ(ok, true, "Send should accept");

    RunOnly(MilliSeconds(5));

    // Now q is (should be) non-local to A.
    const bool gateOk = A->Apply(gates::X(), {q});
    NS_TEST_ASSERT_MSG_EQ(gateOk, false, "Applying a gate from the wrong node should fail");

    Simulator::Destroy();
  }
};

/*-----------------------------------------------------------------------------
 * Suite registration
 *---------------------------------------------------------------------------*/

class QNodeTestSuite : public TestSuite {
public:
  QNodeTestSuite() : TestSuite("q2ns-qnode", TestSuite::Type::UNIT) {
    AddTestCase(new QNodeCreateQubitCase, TestCase::Duration::QUICK);
    AddTestCase(new QNodeApplyMeasureCase, TestCase::Duration::QUICK);
    AddTestCase(new QNodeGetQubitCase, TestCase::Duration::QUICK);
    AddTestCase(new QNodeBellPairCorrelationCase, TestCase::Duration::QUICK);
    AddTestCase(new QNodeMeasureBellSplitsStateCase, TestCase::Duration::QUICK);
    AddTestCase(new QNodeSendRecvCallbackCase, TestCase::Duration::QUICK);
    AddTestCase(new QNodeNonLocalGateFailsCase, TestCase::Duration::QUICK);
  }
};

static QNodeTestSuite g_qNodeTestSuite;
