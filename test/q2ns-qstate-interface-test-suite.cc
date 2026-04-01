/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/
/**
 * @file q2ns-qstate-interface-test-suite.cc
 * @brief Tests for the q2ns::QState backend interface.
 *
 * Purpose:
 *  - Provide a backend-agnostic conformance suite for QState implementations.
 *  - Catch common contract violations (index order, measure-and-split semantics,
 *    MergeDisjoint ordering, basis handling, deterministic RNG seeding via AssignStreams).
 *
 * Backend selection:
 *  - By default, runs against all backends in q2ns::QStateBackend.
 *  - Optionally filter via environment variable:
 *      Q2NS_QSTATE_BACKEND=ket|dm|stab
 *    or a comma-separated list:
 *      Q2NS_QSTATE_BACKEND=ket,stab
 */

#include "ns3/test.h"

#include "ns3/q2ns-netcontroller.h"
#include "ns3/q2ns-qgate.h"
#include "ns3/q2ns-qnode.h"
#include "ns3/q2ns-qstate-dm.h"
#include "ns3/q2ns-qstate-registry.h"
#include "ns3/q2ns-qstate.h"
#include "ns3/q2ns-types.h"

#include "ns3/rng-seed-manager.h"
#include "ns3/simulator.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace q2ns {

static std::string ToLower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
  return s;
}

static std::vector<std::string> SplitCsv(std::string s) {
  std::vector<std::string> out;
  std::string cur;
  for (char c : s) {
    if (c == ',') {
      if (!cur.empty())
        out.push_back(cur);
      cur.clear();
    } else {
      cur.push_back(c);
    }
  }
  if (!cur.empty())
    out.push_back(cur);
  for (auto& x : out) {
    // trim spaces
    x.erase(x.begin(),
            std::find_if(x.begin(), x.end(), [](unsigned char ch) { return !std::isspace(ch); }));
    x.erase(std::find_if(x.rbegin(), x.rend(), [](unsigned char ch) { return !std::isspace(ch); })
                .base(),
            x.end());
    x = ToLower(x);
  }
  out.erase(std::remove_if(out.begin(), out.end(), [](const std::string& x) { return x.empty(); }),
            out.end());
  return out;
}

static std::vector<QStateBackend> GetBackendsToTest() {
  // default: all backends
  const std::vector<QStateBackend> all = AllQStateBackends();

  const char* env = std::getenv("Q2NS_QSTATE_BACKEND");
  if (!env || std::string(env).empty()) {
    return all;
  }

  std::set<QStateBackend> selected;
  for (const auto& tok : SplitCsv(env)) {
    selected.insert(BackendFromString(tok));
  }

  std::vector<QStateBackend> out;
  for (auto b : all) {
    if (selected.count(b))
      out.push_back(b);
  }
  // If parsing produced nothing (e.g., env var garbage), fall back to all.
  if (out.empty())
    return all;
  return out;
}

static const char* BackendName(QStateBackend b) {
  switch (b) {
  case QStateBackend::Ket:
    return "ket";
  case QStateBackend::DM:
    return "dm";
  case QStateBackend::Stab:
    return "stab";
  }
  return "unknown";
}

// Create a backend state via the registry factory to keep tests backend-agnostic.
static std::shared_ptr<QState> MakeState(QStateBackend backend, std::size_t n) {
  QStateRegistry reg;
  reg.SetDefaultBackend(backend);
  auto created = reg.CreateState(static_cast<unsigned int>(n));
  return reg.GetState(created.stateId);
}

class QStateConformanceTestCase : public ns3::TestCase {
public:
  explicit QStateConformanceTestCase(QStateBackend backend)
      : ns3::TestCase(std::string("QState conformance (") + BackendName(backend) + ")"),
        m_backend(backend) {}

private:
  void DoRun() override {
    // Keep deterministic across runs.
    ns3::RngSeedManager::SetSeed(1);
    ns3::RngSeedManager::SetRun(1);

    TestBasics_();
    TestSingleQubitDeterminism_();
    TestBasisXDeterminism_();
    TestBasisYDeterminism_();
    TestMergeDisjointOrdering_();
    TestMeasureSplitIndexShift_();
    TestRandomnessReproducibilityRunMode_();
    TestRandomnessReproducibilityImmediateMode_();

    // Defensive: NetController may schedule callbacks at t=0; ensure no dangling
    // events leak into subsequent test cases.
    ns3::Simulator::Destroy();
  }

  void TestBasics_() {
    auto s = MakeState(m_backend, 2);
    NS_TEST_ASSERT_MSG_NE(s, nullptr, "state must be created");
    NS_TEST_ASSERT_MSG_EQ(s->NumQubits(), 2u, "NumQubits must match constructor");
    NS_TEST_ASSERT_MSG_NE(s->GetStateId(), 0u,
                          "state id should be nonzero after registry creation");

    // PrintState should be callable (content is backend-defined).
    std::ostringstream oss;
    oss << *s;
  }

  void TestSingleQubitDeterminism_() {
    // X then Z-measure => 1 deterministically.
    auto s = MakeState(m_backend, 1);
    s->Apply(gates::X(), {0});
    auto r = s->Measure(0, Basis::Z);
    NS_TEST_ASSERT_MSG_EQ(r.outcome, 1, "X|0> measured in Z should be 1");
    NS_TEST_ASSERT_MSG_NE(r.measured, nullptr, "measured state must exist");
    NS_TEST_ASSERT_MSG_EQ(r.measured->NumQubits(), 1u, "measured state must be 1 qubit");
    if (r.survivors) {
      NS_TEST_ASSERT_MSG_EQ(r.survivors->NumQubits(), 0u,
                            "survivors should be empty for 1-qubit input");
    }
  }

  void TestBasisXDeterminism_() {
    // H then X-measure => 0 deterministically (|+> is +1 eigenstate of X).
    auto s = MakeState(m_backend, 1);
    s->Apply(gates::H(), {0});
    auto r = s->Measure(0, Basis::X);
    NS_TEST_ASSERT_MSG_EQ(r.outcome, 0, "H|0> measured in X should yield bit 0");
  }

  void TestBasisYDeterminism_() {
    // |y+> = S H |0>, measured in Y => 0
    {
      auto s = MakeState(m_backend, 1);
      s->Apply(gates::H(), {0});
      s->Apply(gates::S(), {0});
      auto r = s->Measure(0, Basis::Y);
      NS_TEST_ASSERT_MSG_EQ(r.outcome, 0, "|y+> measured in Y should yield bit 0");
    }
    // |y-> = S^\dagger H |0>, measured in Y => 1
    {
      auto s = MakeState(m_backend, 1);
      s->Apply(gates::H(), {0});
      s->Apply(gates::SDG(), {0});
      auto r = s->Measure(0, Basis::Y);
      NS_TEST_ASSERT_MSG_EQ(r.outcome, 1, "|y-> measured in Y should yield bit 1");
    }
  }

  void TestMergeDisjointOrdering_() {
    // Create two 1-qubit states, set first to |1>, merge, measure indices 0 and 1.
    auto a = MakeState(m_backend, 1);
    auto b = MakeState(m_backend, 1);

    a->Apply(gates::X(), {0});

    auto merged = a->MergeDisjoint(*b);
    NS_TEST_ASSERT_MSG_NE(merged, nullptr, "MergeDisjoint must return a state");
    NS_TEST_ASSERT_MSG_EQ(merged->NumQubits(), 2u, "Merged state must have 2 qubits");

    // Measure qubit 0 (should be 1) and ensure qubit 1 remains 0.
    auto r0 = merged->Measure(0, Basis::Z);
    NS_TEST_ASSERT_MSG_EQ(r0.outcome, 1, "Merged qubit[0] should come from first state");
    NS_TEST_ASSERT_MSG_NE(r0.survivors, nullptr,
                          "Survivors should exist after measuring from 2-qubit state");
    NS_TEST_ASSERT_MSG_EQ(r0.survivors->NumQubits(), 1u, "Survivors must have 1 qubit");

    auto r1 = r0.survivors->Measure(0, Basis::Z);
    NS_TEST_ASSERT_MSG_EQ(r1.outcome, 0,
                          "Merged qubit[1] should come from second state and be |0>");
  }

  void TestMeasureSplitIndexShift_() {
    // Build 3 qubits as disjoint merge of three singles:
    //  q0=|0>, q1=|0>, q2=|1>
    // Measure q1 (middle). Survivors should preserve order [q0,q2] => q2 shifts to index 1.
    auto s0 = MakeState(m_backend, 1);
    auto s1 = MakeState(m_backend, 1);
    auto s2 = MakeState(m_backend, 1);
    s2->Apply(gates::X(), {0});

    auto s01 = s0->MergeDisjoint(*s1);
    auto s012 = s01->MergeDisjoint(*s2);
    NS_TEST_ASSERT_MSG_EQ(s012->NumQubits(), 3u, "Merged state must have 3 qubits");

    auto mid = s012->Measure(1, Basis::Z);
    NS_TEST_ASSERT_MSG_NE(mid.survivors, nullptr,
                          "Survivors must exist after measuring from 3-qubit state");
    NS_TEST_ASSERT_MSG_EQ(mid.survivors->NumQubits(), 2u, "Survivors must have 2 qubits");

    // The former q2 should now be at index 1 and still be |1>.
    auto r = mid.survivors->Measure(1, Basis::Z);
    NS_TEST_ASSERT_MSG_EQ(r.outcome, 1,
                          "Index-shift invariant violated: last qubit did not shift to index 1");
  }

  void TestRandomnessReproducibilityRunMode_() {
    auto oneShot = [this]() -> int {
      ns3::RngSeedManager::SetSeed(1);
      ns3::RngSeedManager::SetRun(2);

      int outcome = -1;

      q2ns::NetController nc;
      nc.SetQStateBackend(m_backend);

      // Build minimal topology (not strictly needed for H+measure, but fine)
      auto n = nc.CreateNode();
      auto q = n->CreateQubit();
      n->Apply(gates::H(), {q});

      // Schedule the probabilistic operation inside the sim.
      ns3::Simulator::Schedule(ns3::NanoSeconds(1),
                               [&]() { outcome = n->Measure(q, q2ns::Basis::Z); });

      ns3::Simulator::Stop(ns3::MilliSeconds(1));
      ns3::Simulator::Run();
      ns3::Simulator::Destroy();

      return outcome;
    };

    const int a = oneShot();
    const int b = oneShot();

    NS_TEST_ASSERT_MSG_NE(a, -1, "Outcome a was not set; scheduled event did not run.");
    NS_TEST_ASSERT_MSG_NE(b, -1, "Outcome b was not set; scheduled event did not run.");

    NS_TEST_ASSERT_MSG_EQ(a, b, "Run-mode randomness should be reproducible for fixed Seed/Run");
  }

  void TestRandomnessReproducibilityImmediateMode_() {
    // Randomness model per README-randomness.md:
    //  - Deterministic-by-default controlled by ns-3 Seed/Run
    //  - In "immediate mode" (no Simulator::Run), user must call NetController::AssignStreams()
    //    before the first probabilistic operation.
    //
    // Conformance requirement here: repeating the *same* programmatic sequence with the
    // same Seed/Run and the same stream binding should yield the same outcome.

    auto oneShot = [this]() -> int {
      ns3::RngSeedManager::SetSeed(7);
      ns3::RngSeedManager::SetRun(3);

      q2ns::NetController nc;
      nc.SetQStateBackend(m_backend);
      nc.AssignStreams(0); // required in immediate mode

      auto n = nc.CreateNode();
      auto q = n->CreateQubit();
      n->Apply(gates::H(), {q});
      return n->Measure(q, Basis::Z);
    };

    const int a = oneShot();
    // Clear any scheduled events / global simulator state between shots.
    ns3::Simulator::Destroy();
    const int b = oneShot();

    NS_TEST_ASSERT_MSG_EQ(
        a, b,
        "Immediate-mode randomness should be reproducible for fixed Seed/Run and AssignStreams");
  }

private:
  QStateBackend m_backend;
};

class QStateDmValidationTestCase : public ns3::TestCase {
public:
  QStateDmValidationTestCase() : ns3::TestCase("QStateDM ValidateDensityMatrix") {}

private:
  void DoRun() override {
    TestRejectsNonHermitian_();
    TestRejectsNonPsd_();
    TestAcceptsValidDensityMatrix_();
  }

  void TestRejectsNonHermitian_() {
    // Trace 1, power-of-two dimension, but not Hermitian.
    qpp::cmat rho(2, 2);
    rho << 0.5, 0.25, 0.0, 0.5;

    bool threw = false;
    try {
      QStateDM s(rho);
      (void) s;
    } catch (const std::invalid_argument&) {
      threw = true;
    } catch (...) {
      NS_TEST_ASSERT_MSG_EQ(false, true, "Non-Hermitian rho should throw std::invalid_argument");
    }

    NS_TEST_ASSERT_MSG_EQ(threw, true, "QStateDM should reject non-Hermitian density matrices");
  }

  void TestRejectsNonPsd_() {
    // Hermitian, trace 1, but not PSD because one eigenvalue is negative.
    // Eigenvalues are 1.1 and -0.1.
    qpp::cmat rho(2, 2);
    rho << 1.1, 0.0, 0.0, -0.1;

    bool threw = false;
    try {
      QStateDM s(rho);
      (void) s;
    } catch (const std::invalid_argument&) {
      threw = true;
    } catch (...) {
      NS_TEST_ASSERT_MSG_EQ(false, true, "Non-PSD rho should throw std::invalid_argument");
    }

    NS_TEST_ASSERT_MSG_EQ(threw, true,
                          "QStateDM should reject density matrices with negative eigenvalues");
  }

  void TestAcceptsValidDensityMatrix_() {
    // Simple valid mixed state.
    qpp::cmat rho(2, 2);
    rho << 0.3, 0.0, 0.0, 0.7;

    bool threw = false;
    try {
      QStateDM s(rho);
      NS_TEST_ASSERT_MSG_EQ(s.NumQubits(), 1u, "Valid 2x2 density matrix should represent 1 qubit");
    } catch (...) {
      threw = true;
    }

    NS_TEST_ASSERT_MSG_EQ(threw, false,
                          "QStateDM should accept a valid Hermitian PSD trace-1 density matrix");
  }
};

class QStateInterfaceTestSuite : public ns3::TestSuite {
public:
  QStateInterfaceTestSuite() : ns3::TestSuite("q2ns-qstate-interface", TestSuite::Type::UNIT) {
    for (auto b : GetBackendsToTest()) {
      AddTestCase(new QStateConformanceTestCase(b), TestCase::Duration::QUICK);

      if (b == QStateBackend::DM)
        AddTestCase(new QStateDmValidationTestCase(), TestCase::Duration::QUICK);
    }
  }
};

static QStateInterfaceTestSuite g_qstateInterfaceTestSuite;

} // namespace q2ns
