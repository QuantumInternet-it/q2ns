/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/
/**
 * @file q2ns-qstate-dm.cc
 * @brief Defines q2ns::QStateDM.
 */

#include "ns3/q2ns-qstate-dm.h"

#include "ns3/assert.h"

#include <cmath>
#include <cstdint>
#include <random>
#include <stdexcept>

namespace q2ns {

void QStateDM::ValidateDensityMatrix(const qpp::cmat& rho) {
  constexpr double dimTol = 1e-9;
  constexpr double hermTol = 1e-12;
  constexpr double traceTol = 1e-9;
  constexpr double psdTol = 1e-12;

  const auto rows = rho.rows();
  const auto cols = rho.cols();

  if (rows != cols) {
    throw std::invalid_argument("QStateDM: density matrix must be square");
  }

  if (rows <= 0) {
    throw std::invalid_argument("QStateDM: density matrix must be non-empty");
  }

  const double log2dim = std::log2(static_cast<double>(rows));
  if (std::fabs(std::round(log2dim) - log2dim) > dimTol) {
    throw std::invalid_argument("QStateDM: dim(rho) must be 2^N for some integer N");
  }

  // Density matrices must be Hermitian.
  if (!rho.isApprox(rho.adjoint(), hermTol)) {
    throw std::invalid_argument("QStateDM: density matrix must be Hermitian");
  }

  // Trace must be real and equal to 1 up to tolerance.
  const std::complex<double> tr = qpp::trace(rho);
  if (std::fabs(std::imag(tr)) > traceTol) {
    throw std::invalid_argument("QStateDM: Tr(rho) must be real");
  }
  if (std::fabs(std::real(tr) - 1.0) > traceTol) {
    throw std::invalid_argument("QStateDM: Tr(rho) must be 1");
  }

  // Positive semidefinite check. Symmetrize first to suppress tiny
  // anti-Hermitian numerical noise before the self-adjoint eigensolve.
  const qpp::cmat H = 0.5 * (rho + rho.adjoint());

  Eigen::SelfAdjointEigenSolver<qpp::cmat> es(H);
  if (es.info() != Eigen::Success) {
    throw std::runtime_error("QStateDM: eigensolver failed while validating density matrix");
  }

  const auto& evals = es.eigenvalues();
  for (int i = 0; i < evals.size(); ++i) {
    if (evals[i] < -psdTol) {
      throw std::invalid_argument("QStateDM: density matrix must be positive semidefinite");
    }
  }
}



QStateDM::QStateDM(std::size_t numQubits) {
  std::vector<qpp::idx> zeros(numQubits, 0);
  qpp::ket k = qpp::mket(zeros);
  rho_ = qpp::prj(k);
  ValidateDensityMatrix(rho_);
}



QStateDM::QStateDM(qpp::cmat rho) : rho_(std::move(rho)) {
  ValidateDensityMatrix(rho_);
}



int64_t QStateDM::AssignStreams(int64_t stream) {
  constexpr uint64_t QPPD_SALT = 0x51505044ULL; // "QPPD"

  return QState::AssignStreamsGlobal<QPPD_SALT>(stream, [&](uint64_t s64) {
    auto seq = QState::MakeSeedSeq(s64);
    qpp::RandomDevices::get_instance().get_prng().seed(seq);
  });
}



std::size_t QStateDM::NumQubits() const {
  const auto dim = static_cast<double>(rho_.rows());
  return static_cast<std::size_t>(std::llround(std::log2(dim)));
}



void QStateDM::Print(std::ostream& os) const {
  PrintHeader(os, "DM");
  os << rho_ << "\n";
}



void QStateDM::Apply(const QGate& g, const std::vector<q2ns::Index>& targets) {
  std::vector<qpp::idx> t;
  t.reserve(targets.size());
  for (auto i : targets) {
    t.push_back(static_cast<qpp::idx>(i));
  }

  qpp::cmat U;
  if (g.Kind() == QGateKind::Custom) {
    U = g.Unitary();
  } else {
    U = MatrixOf(g.Kind());
  }

  const std::size_t k = targets.size();
  const std::size_t dim = 1ull << k;
  NS_ABORT_MSG_IF(U.rows() != static_cast<int>(dim) || U.cols() != static_cast<int>(dim),
                  "QStateDM::Apply: wrong gate dimension");

  rho_ = qpp::apply(rho_, U, t);
}



std::shared_ptr<QState> QStateDM::MergeDisjoint(const QState& other) const {
  const auto* dm = dynamic_cast<const QStateDM*>(&other);
  if (!dm) {
    throw std::runtime_error("QStateDM::MergeDisjoint: incompatible backend");
  }

  qpp::cmat combined = qpp::kron(rho_, dm->rho_);
  return std::make_shared<QStateDM>(combined);
}



QState::MeasureResult QStateDM::Measure(q2ns::Index target, q2ns::Basis basis) {
  const auto n = NumQubits();
  if (target >= n) {
    throw std::out_of_range("QStateDM::Measure: target out of range");
  }

  switch (basis) {
  case Basis::Z:
    break;
  case Basis::X:
    rho_ = qpp::apply(rho_, qpp::gt.H, {static_cast<qpp::idx>(target)});
    break;
  case Basis::Y:
    rho_ = qpp::apply(rho_, qpp::adjoint(qpp::gt.S), {static_cast<qpp::idx>(target)});
    rho_ = qpp::apply(rho_, qpp::gt.H, {static_cast<qpp::idx>(target)});
    break;
  }

  std::vector<qpp::idx> dims(n, 2);
  std::vector<qpp::idx> subsys{static_cast<qpp::idx>(target)};
  auto meas_res = qpp::measure(rho_, qpp::gt.Z, subsys, dims);

  const qpp::idx outcome = std::get<0>(meas_res);
  auto states = std::get<2>(meas_res);

  // qpp returns post-measurement survivor states in the rotated Z-basis
  // frame, so reconstruct the measured 1-qubit state in the requested basis.
  qpp::ket oneQ = (outcome == 0 ? qpp::mket({0}) : qpp::mket({1}));
  switch (basis) {
  case Basis::Z:
    break;
  case Basis::X:
    oneQ = qpp::apply(oneQ, qpp::gt.H, {0});
    break;
  case Basis::Y:
    oneQ = qpp::apply(oneQ, qpp::adjoint(qpp::gt.S), {0});
    oneQ = qpp::apply(oneQ, qpp::gt.H, {0});
    break;
  }

  auto measured1q = std::make_shared<QStateDM>(qpp::prj(oneQ));
  auto survivors = std::make_shared<QStateDM>(std::move(states[outcome]));

  return MeasureResult{static_cast<int>(outcome), measured1q, survivors};
}



std::shared_ptr<QState> QStateDM::PartialTrace(const std::vector<q2ns::Index>& subsystemA) {
  const auto n = NumQubits();
  if (n == 0) {
    throw std::runtime_error("QStateDM::PartialTrace: empty state");
  }
  if (subsystemA.empty() || subsystemA.size() >= n) {
    throw std::invalid_argument(
        "QStateDM::PartialTrace: subsystemA must be a non-empty proper subset");
  }

  std::vector<bool> used(n, false);
  std::vector<qpp::idx> A;
  A.reserve(subsystemA.size());

  for (auto idxUser : subsystemA) {
    if (idxUser >= n) {
      throw std::out_of_range("QStateDM::PartialTrace: index out of range");
    }
    if (used[idxUser]) {
      throw std::invalid_argument("QStateDM::PartialTrace: duplicate index");
    }
    used[idxUser] = true;
    A.push_back(static_cast<qpp::idx>(idxUser));
  }

  std::vector<qpp::idx> B;
  B.reserve(n - A.size());
  for (qpp::idx i = 0; i < static_cast<qpp::idx>(n); ++i) {
    if (!used[i]) {
      B.push_back(i);
    }
  }

  std::vector<qpp::idx> dims(n, 2);

  // qpp::ptrace(rho, subsys, dims) traces out the listed subsystems.
  // rhoA = Tr_B[rho] and rhoB = Tr_A[rho].
  qpp::cmat rhoA = qpp::ptrace(rho_, B, dims);
  qpp::cmat rhoB = qpp::ptrace(rho_, A, dims);

  rho_ = std::move(rhoB);
  ValidateDensityMatrix(rho_);

  return std::make_shared<QStateDM>(rhoA);
}



void QStateDM::SetRho(const qpp::cmat& rho) {
  ValidateDensityMatrix(rho);
  rho_ = rho;
}

} // namespace q2ns