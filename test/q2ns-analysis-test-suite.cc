/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/
/**
 * @file q2ns-analysis-test-suite.cc
 * @brief Tests for backend-agnostic q2ns::analysis helpers.
 *
 * Purpose:
 *  - Provide a backend-agnostic conformance suite for q2ns::analysis over QState.
 *  - Verify that analysis routines behave consistently across all public QState backends.
 *  - Keep generic contract tests backend-agnostic by creating states through QStateRegistry.
 *
 * Backend selection:
 *  - By default, runs against all backends in q2ns::QStateBackend.
 *  - Optionally filter via environment variable:
 *      Q2NS_QSTATE_BACKEND=ket|dm|stab
 *    or a comma-separated list:
 *      Q2NS_QSTATE_BACKEND=ket,stab
 *
 * Notes:
 *  - Tests that truly require explicit density matrices remain in a DM-only validation
 *    case, analogous to qstate-interface's QStateDmValidationTestCase.
 *  - We do NOT directly instantiate QStateKet/QStateStab in generic analysis tests.
 */

#include "ns3/test.h"

#include "ns3/q2ns-analysis.h"
#include "ns3/q2ns-qgate.h"
#include "ns3/q2ns-qstate-dm.h"
#include "ns3/q2ns-qstate-registry.h"
#include "ns3/q2ns-qstate.h"
#include "ns3/q2ns-types.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <set>
#include <string>
#include <vector>

namespace q2ns {

/*-----------------------------------------------------------------------------
 * Helper utilities
 *---------------------------------------------------------------------------*/

static bool CheckEqualityFP(double a, double b, double epsilon = 1e-10) {
  return std::abs(a - b) < epsilon;
}

static bool ThrowsAny(const std::function<void()>& fn) {
  try {
    fn();
  } catch (const std::exception&) {
    return true;
  } catch (...) {
    return true;
  }
  return false;
}

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

static std::shared_ptr<QState> MakeState(QStateBackend backend, std::size_t n) {
  QStateRegistry reg;
  reg.SetDefaultBackend(backend);
  auto created = reg.CreateState(static_cast<unsigned int>(n));
  return reg.GetState(created.stateId);
}

static std::shared_ptr<QState> MakeZero(QStateBackend backend, std::size_t n) {
  return MakeState(backend, n);
}

static std::shared_ptr<QState> MakeOne(QStateBackend backend) {
  auto s = MakeState(backend, 1);
  s->Apply(gates::X(), {0});
  return s;
}

static std::shared_ptr<QState> MakePlus(QStateBackend backend) {
  auto s = MakeState(backend, 1);
  s->Apply(gates::H(), {0});
  return s;
}

static std::shared_ptr<QState> MakeBellPhiPlus(QStateBackend backend) {
  auto s = MakeState(backend, 2);
  s->Apply(gates::H(), {0});
  s->Apply(gates::CNOT(), {0, 1});
  return s;
}

static std::shared_ptr<QState> MakeBellPsiPlus(QStateBackend backend) {
  auto s = MakeState(backend, 2);
  s->Apply(gates::H(), {0});
  s->Apply(gates::CNOT(), {0, 1});
  s->Apply(gates::X(), {1});
  return s;
}

/*-----------------------------------------------------------------------------
 * Conformance suite: backend-agnostic analysis behavior over QState
 *---------------------------------------------------------------------------*/

class QStateAnalysisConformanceTestCase : public ns3::TestCase {
public:
  explicit QStateAnalysisConformanceTestCase(QStateBackend backend)
      : ns3::TestCase(std::string("q2ns::analysis conformance (") + BackendName(backend) + ")"),
        m_backend(backend) {}

private:
  void DoRun() override {
    TestFidelityIdentical_();
    TestFidelityOrthogonal_();
    TestFidelitySuperposition_();
    TestFidelityPartialOverlap_();
    TestFidelityBellStates_();
    TestFidelityDifferentBellStates_();
    TestFidelityDimensionMismatch_();

    TestPurityZero_();
    TestPurityPlus_();
    TestPurityBell_();

    TestIsPureTrueZero_();
    TestIsPureTruePlus_();
    TestIsPureTrueBell_();
    TestIsPureNegativeTolerance_();

    TestEntropyZero_();
    TestEntropyPlus_();
    TestEntropyBell_();

    TestTraceDistanceIdentical_();
    TestTraceDistanceOrthogonal_();
    TestTraceDistancePartialOverlap_();
    TestTraceDistanceDimensionMismatch_();

    // Only meaningful where the tested states are available through public QState ops.
    if (m_backend == QStateBackend::Stab) {
      TestTraceDistanceDifferentBellStates_();
    }

    TestCrossBackendFidelityThrows_();
    TestCrossBackendTraceDistanceThrows_();
  }

  void TestFidelityIdentical_() {
    auto a = MakeZero(m_backend, 1);
    auto b = MakeZero(m_backend, 1);

    double result = q2ns::analysis::Fidelity(*a, *b);
    NS_TEST_ASSERT_MSG_EQ(CheckEqualityFP(result, 1.0), true,
                          "Fidelity between identical |0> states should be 1.0");
  }

  void TestFidelityOrthogonal_() {
    auto zero = MakeZero(m_backend, 1);
    auto one = MakeOne(m_backend);

    double result = q2ns::analysis::Fidelity(*zero, *one);
    NS_TEST_ASSERT_MSG_EQ(CheckEqualityFP(result, 0.0), true,
                          "Fidelity between |0> and |1> should be 0.0");
  }

  void TestFidelitySuperposition_() {
    auto zero = MakeZero(m_backend, 1);
    auto plus = MakePlus(m_backend);

    double result = q2ns::analysis::Fidelity(*zero, *plus);
    NS_TEST_ASSERT_MSG_EQ(CheckEqualityFP(result, 0.5), true,
                          "Fidelity between |0> and |+> should be 0.5");
  }

  void TestFidelityPartialOverlap_() {
    auto plus = MakePlus(m_backend);
    auto one = MakeOne(m_backend);

    double result = q2ns::analysis::Fidelity(*plus, *one);
    NS_TEST_ASSERT_MSG_EQ(CheckEqualityFP(result, 0.5), true,
                          "Fidelity between |+> and |1> should be 0.5");
  }

  void TestFidelityBellStates_() {
    auto a = MakeBellPhiPlus(m_backend);
    auto b = MakeBellPhiPlus(m_backend);

    double result = q2ns::analysis::Fidelity(*a, *b);
    NS_TEST_ASSERT_MSG_EQ(CheckEqualityFP(result, 1.0), true,
                          "Fidelity between identical Bell states should be 1.0");
  }

  void TestFidelityDifferentBellStates_() {
    auto phiPlus = MakeBellPhiPlus(m_backend);
    auto psiPlus = MakeBellPsiPlus(m_backend);

    double result = q2ns::analysis::Fidelity(*phiPlus, *psiPlus);
    NS_TEST_ASSERT_MSG_EQ(CheckEqualityFP(result, 0.0), true,
                          "Fidelity between orthogonal Bell states should be 0.0");
  }

  void TestFidelityDimensionMismatch_() {
    auto a = MakeZero(m_backend, 1);
    auto b = MakeZero(m_backend, 2);

    NS_TEST_ASSERT_MSG_EQ(ThrowsAny([&]() { (void) q2ns::analysis::Fidelity(*a, *b); }), true,
                          "Fidelity with mismatched dimensions should throw");
  }

  void TestPurityZero_() {
    auto s = MakeZero(m_backend, 1);
    double result = q2ns::analysis::Purity(*s);

    NS_TEST_ASSERT_MSG_EQ(CheckEqualityFP(result, 1.0), true, "Purity of |0> should be 1.0");
  }

  void TestPurityPlus_() {
    auto s = MakePlus(m_backend);
    double result = q2ns::analysis::Purity(*s);

    NS_TEST_ASSERT_MSG_EQ(CheckEqualityFP(result, 1.0), true, "Purity of |+> should be 1.0");
  }

  void TestPurityBell_() {
    auto s = MakeBellPhiPlus(m_backend);
    double result = q2ns::analysis::Purity(*s);

    NS_TEST_ASSERT_MSG_EQ(CheckEqualityFP(result, 1.0), true,
                          "Purity of pure Bell state should be 1.0");
  }

  void TestIsPureTrueZero_() {
    auto s = MakeZero(m_backend, 1);
    bool result = q2ns::analysis::IsPure(*s);

    NS_TEST_ASSERT_MSG_EQ(result, true, "|0> should be pure");
  }

  void TestIsPureTruePlus_() {
    auto s = MakePlus(m_backend);
    bool result = q2ns::analysis::IsPure(*s);

    NS_TEST_ASSERT_MSG_EQ(result, true, "|+> should be pure");
  }

  void TestIsPureTrueBell_() {
    auto s = MakeBellPhiPlus(m_backend);
    bool result = q2ns::analysis::IsPure(*s);

    NS_TEST_ASSERT_MSG_EQ(result, true, "Bell state should be pure");
  }

  void TestIsPureNegativeTolerance_() {
    auto s = MakeZero(m_backend, 1);

    NS_TEST_ASSERT_MSG_EQ(ThrowsAny([&]() { (void) q2ns::analysis::IsPure(*s, -1e-12); }), true,
                          "IsPure with negative tolerance should throw");
  }

  void TestEntropyZero_() {
    auto s = MakeZero(m_backend, 1);
    double result = q2ns::analysis::VonNeumannEntropy(*s);

    NS_TEST_ASSERT_MSG_EQ(CheckEqualityFP(result, 0.0), true,
                          "VonNeumannEntropy of pure |0> should be 0.0");
  }

  void TestEntropyPlus_() {
    auto s = MakePlus(m_backend);
    double result = q2ns::analysis::VonNeumannEntropy(*s);

    NS_TEST_ASSERT_MSG_EQ(CheckEqualityFP(result, 0.0), true,
                          "VonNeumannEntropy of pure |+> should be 0.0");
  }

  void TestEntropyBell_() {
    auto s = MakeBellPhiPlus(m_backend);
    double result = q2ns::analysis::VonNeumannEntropy(*s);

    NS_TEST_ASSERT_MSG_EQ(CheckEqualityFP(result, 0.0), true,
                          "VonNeumannEntropy of pure Bell state should be 0.0");
  }

  void TestTraceDistanceIdentical_() {
    auto a = MakeZero(m_backend, 1);
    auto b = MakeZero(m_backend, 1);

    double result = q2ns::analysis::TraceDistance(*a, *b);
    NS_TEST_ASSERT_MSG_EQ(CheckEqualityFP(result, 0.0), true,
                          "TraceDistance between identical states should be 0.0");
  }

  void TestTraceDistanceOrthogonal_() {
    auto zero = MakeZero(m_backend, 1);
    auto one = MakeOne(m_backend);

    double result = q2ns::analysis::TraceDistance(*zero, *one);
    NS_TEST_ASSERT_MSG_EQ(CheckEqualityFP(result, 1.0), true,
                          "TraceDistance between |0> and |1> should be 1.0");
  }

  void TestTraceDistancePartialOverlap_() {
    auto zero = MakeZero(m_backend, 1);
    auto plus = MakePlus(m_backend);

    const double expected = std::sqrt(0.5);
    double result = q2ns::analysis::TraceDistance(*zero, *plus);

    NS_TEST_ASSERT_MSG_EQ(CheckEqualityFP(result, expected), true,
                          "TraceDistance between |0> and |+> should be sqrt(0.5)");
  }

  void TestTraceDistanceDifferentBellStates_() {
    auto phiPlus = MakeBellPhiPlus(m_backend);
    auto psiPlus = MakeBellPsiPlus(m_backend);

    double result = q2ns::analysis::TraceDistance(*phiPlus, *psiPlus);
    NS_TEST_ASSERT_MSG_EQ(CheckEqualityFP(result, 1.0), true,
                          "TraceDistance between orthogonal Bell states should be 1.0");
  }

  void TestTraceDistanceDimensionMismatch_() {
    auto a = MakeZero(m_backend, 1);
    auto b = MakeZero(m_backend, 2);

    NS_TEST_ASSERT_MSG_EQ(ThrowsAny([&]() { (void) q2ns::analysis::TraceDistance(*a, *b); }), true,
                          "TraceDistance with mismatched dimensions should throw");
  }

  void TestCrossBackendFidelityThrows_() {
    auto a = MakeState(QStateBackend::Ket, 1);
    auto b = MakeState(QStateBackend::DM, 1);

    NS_TEST_ASSERT_MSG_EQ(ThrowsAny([&]() { (void) q2ns::analysis::Fidelity(*a, *b); }), true,
                          "Cross-backend Fidelity should throw");
  }

  void TestCrossBackendTraceDistanceThrows_() {
    auto a = MakeState(QStateBackend::Ket, 1);
    auto b = MakeState(QStateBackend::DM, 1);

    NS_TEST_ASSERT_MSG_EQ(ThrowsAny([&]() { (void) q2ns::analysis::TraceDistance(*a, *b); }), true,
                          "Cross-backend TraceDistance should throw");
  }

private:
  QStateBackend m_backend;
};

/*-----------------------------------------------------------------------------
 * DM-only validation suite: explicit density matrices
 *---------------------------------------------------------------------------*/

class QStateAnalysisDmValidationTestCase : public ns3::TestCase {
public:
  QStateAnalysisDmValidationTestCase()
      : ns3::TestCase("q2ns::analysis DM mixed-state validation") {}

private:
  static qpp::cmat MakeMaximallyMixedOneQubit_() {
    qpp::cmat rho(2, 2);
    rho << 0.5, 0.0, 0.0, 0.5;
    return rho;
  }

  static qpp::cmat MakeDiagonalOneQubit_(double p0, double p1) {
    qpp::cmat rho(2, 2);
    rho << p0, 0.0, 0.0, p1;
    return rho;
  }

  static qpp::cmat MakePlusDensityMatrix_() {
    qpp::cmat rho(2, 2);
    rho << 0.5, 0.5, 0.5, 0.5;
    return rho;
  }

  static qpp::cmat MakeBellDensityMatrix_() {
    qpp::cmat rho(4, 4);
    rho << 0.5, 0.0, 0.0, 0.5, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.5, 0.0, 0.0, 0.5;
    return rho;
  }

  void DoRun() override {
    TestFidelityIdentical_();
    TestFidelityOrthogonal_();
    TestFidelityBellStates_();
    TestFidelityMixedIdentical_();
    TestFidelityMixedDiagonal_();
    TestFidelityDimensionMismatch_();

    TestPurityPure_();
    TestPurityMaximallyMixed_();
    TestPurityDiagonalMixed_();

    TestIsPureTrue_();
    TestIsPureFalse_();

    TestEntropyPure_();
    TestEntropyMaximallyMixed_();
    TestEntropyDiagonalMixed_();
    TestEntropyBellPure_();

    TestTraceDistanceIdentical_();
    TestTraceDistanceOrthogonal_();
    TestTraceDistanceDiagonalMixed_();
  }

  void TestFidelityIdentical_() {
    QStateDM a(1);
    QStateDM b(1);

    double result = q2ns::analysis::Fidelity(a, b);
    NS_TEST_ASSERT_MSG_EQ(CheckEqualityFP(result, 1.0), true,
                          "DM fidelity of identical pure states should be 1.0");
  }

  void TestFidelityOrthogonal_() {
    QStateDM zero(1);
    QStateDM one(1);
    one.Apply(gates::X(), {0});

    double result = q2ns::analysis::Fidelity(zero, one);
    NS_TEST_ASSERT_MSG_EQ(CheckEqualityFP(result, 0.0), true,
                          "DM fidelity of orthogonal pure states should be 0.0");
  }

  void TestFidelityBellStates_() {
    QStateDM a(MakeBellDensityMatrix_());
    QStateDM b(MakeBellDensityMatrix_());

    double result = q2ns::analysis::Fidelity(a, b);
    NS_TEST_ASSERT_MSG_EQ(CheckEqualityFP(result, 1.0), true,
                          "DM fidelity of identical Bell density matrices should be 1.0");
  }

  void TestFidelityMixedIdentical_() {
    QStateDM rho(MakeDiagonalOneQubit_(0.25, 0.75));
    QStateDM sigma(MakeDiagonalOneQubit_(0.25, 0.75));

    double result = q2ns::analysis::Fidelity(rho, sigma);
    NS_TEST_ASSERT_MSG_EQ(CheckEqualityFP(result, 1.0), true,
                          "DM fidelity of identical mixed diagonal states should be 1.0");
  }

  void TestFidelityMixedDiagonal_() {
    QStateDM rho(MakeDiagonalOneQubit_(0.25, 0.75));
    QStateDM sigma(MakeDiagonalOneQubit_(0.5, 0.5));

    const double expected = std::pow(std::sqrt(0.25 * 0.5) + std::sqrt(0.75 * 0.5), 2.0);
    double result = q2ns::analysis::Fidelity(rho, sigma);

    NS_TEST_ASSERT_MSG_EQ(CheckEqualityFP(result, expected), true,
                          "DM fidelity for diagonal mixed states should match analytic value");
  }

  void TestFidelityDimensionMismatch_() {
    QStateDM a(1);
    QStateDM b(2);

    NS_TEST_ASSERT_MSG_EQ(ThrowsAny([&]() { (void) q2ns::analysis::Fidelity(a, b); }), true,
                          "DM fidelity with mismatched dimensions should throw");
  }

  void TestPurityPure_() {
    QStateDM state(MakePlusDensityMatrix_());
    double result = q2ns::analysis::Purity(state);

    NS_TEST_ASSERT_MSG_EQ(CheckEqualityFP(result, 1.0), true,
                          "Purity of pure density matrix should be 1.0");
  }

  void TestPurityMaximallyMixed_() {
    QStateDM state(MakeMaximallyMixedOneQubit_());
    double result = q2ns::analysis::Purity(state);

    NS_TEST_ASSERT_MSG_EQ(CheckEqualityFP(result, 0.5), true,
                          "Purity of maximally mixed qubit should be 0.5");
  }

  void TestPurityDiagonalMixed_() {
    const double p0 = 0.3;
    const double p1 = 0.7;
    QStateDM state(MakeDiagonalOneQubit_(p0, p1));

    const double expected = p0 * p0 + p1 * p1;
    double result = q2ns::analysis::Purity(state);

    NS_TEST_ASSERT_MSG_EQ(CheckEqualityFP(result, expected), true,
                          "Purity of diagonal mixed state should match p^2+(1-p)^2");
  }

  void TestIsPureTrue_() {
    QStateDM state(MakePlusDensityMatrix_());
    bool result = q2ns::analysis::IsPure(state);

    NS_TEST_ASSERT_MSG_EQ(result, true, "Pure density matrix should be pure");
  }

  void TestIsPureFalse_() {
    QStateDM state(MakeMaximallyMixedOneQubit_());
    bool result = q2ns::analysis::IsPure(state);

    NS_TEST_ASSERT_MSG_EQ(result, false, "Maximally mixed density matrix should not be pure");
  }

  void TestEntropyPure_() {
    QStateDM state(MakePlusDensityMatrix_());
    double result = q2ns::analysis::VonNeumannEntropy(state);

    NS_TEST_ASSERT_MSG_EQ(CheckEqualityFP(result, 0.0), true,
                          "VonNeumannEntropy of pure density matrix should be 0.0");
  }

  void TestEntropyMaximallyMixed_() {
    QStateDM state(MakeMaximallyMixedOneQubit_());
    double result = q2ns::analysis::VonNeumannEntropy(state);

    NS_TEST_ASSERT_MSG_EQ(CheckEqualityFP(result, 1.0), true,
                          "VonNeumannEntropy of maximally mixed qubit should be 1.0");
  }

  void TestEntropyDiagonalMixed_() {
    const double p0 = 0.25;
    const double p1 = 0.75;
    QStateDM state(MakeDiagonalOneQubit_(p0, p1));

    const double expected = -(p0 * std::log2(p0) + p1 * std::log2(p1));
    double result = q2ns::analysis::VonNeumannEntropy(state);

    NS_TEST_ASSERT_MSG_EQ(CheckEqualityFP(result, expected), true,
                          "VonNeumannEntropy of diagonal mixed state should match Shannon entropy");
  }

  void TestEntropyBellPure_() {
    QStateDM state(MakeBellDensityMatrix_());
    double result = q2ns::analysis::VonNeumannEntropy(state);

    NS_TEST_ASSERT_MSG_EQ(CheckEqualityFP(result, 0.0), true,
                          "VonNeumannEntropy of pure Bell density matrix should be 0.0");
  }

  void TestTraceDistanceIdentical_() {
    QStateDM rho(MakeDiagonalOneQubit_(0.3, 0.7));
    QStateDM sigma(MakeDiagonalOneQubit_(0.3, 0.7));

    double result = q2ns::analysis::TraceDistance(rho, sigma);
    NS_TEST_ASSERT_MSG_EQ(CheckEqualityFP(result, 0.0), true,
                          "TraceDistance of identical mixed states should be 0.0");
  }

  void TestTraceDistanceOrthogonal_() {
    QStateDM rho(1);
    QStateDM sigma(1);
    sigma.Apply(gates::X(), {0});

    double result = q2ns::analysis::TraceDistance(rho, sigma);
    NS_TEST_ASSERT_MSG_EQ(CheckEqualityFP(result, 1.0), true,
                          "TraceDistance of orthogonal one-qubit density matrices should be 1.0");
  }

  void TestTraceDistanceDiagonalMixed_() {
    QStateDM rho(MakeDiagonalOneQubit_(0.2, 0.8));
    QStateDM sigma(MakeDiagonalOneQubit_(0.7, 0.3));

    const double expected = 0.5;
    double result = q2ns::analysis::TraceDistance(rho, sigma);

    NS_TEST_ASSERT_MSG_EQ(CheckEqualityFP(result, expected), true,
                          "TraceDistance of diagonal mixed states should match analytic value");
  }
};

/*-----------------------------------------------------------------------------
 * Assembly
 *---------------------------------------------------------------------------*/

class Q2nsAnalysisTestSuite : public ns3::TestSuite {
public:
  Q2nsAnalysisTestSuite() : ns3::TestSuite("q2ns-analysis", ns3::TestSuite::Type::UNIT) {
    for (auto b : GetBackendsToTest()) {
      AddTestCase(new QStateAnalysisConformanceTestCase(b), TestCase::Duration::QUICK);

      if (b == QStateBackend::DM) {
        AddTestCase(new QStateAnalysisDmValidationTestCase(), TestCase::Duration::QUICK);
      }
    }
  }
};

static Q2nsAnalysisTestSuite g_q2nsAnalysisTestSuite;

} // namespace q2ns