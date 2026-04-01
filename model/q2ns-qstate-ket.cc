/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/
/**
 * @file q2ns-qstate-ket.cc
 * @brief Defines q2ns::QStateKet.
 */

#include "ns3/q2ns-qstate-ket.h"

#include "ns3/assert.h"

#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace q2ns {

void QStateKet::ValidateKet(const qpp::ket& k) {
  constexpr double dimTol = 1e-9;
  constexpr double normTol = 1e-9;

  if (k.rows() <= 0) {
    throw std::invalid_argument("QStateKet: ket must be non-empty");
  }

  if (k.cols() != 1) {
    throw std::invalid_argument("QStateKet: ket must be a column vector");
  }

  const double log2dim = std::log2(static_cast<double>(k.rows()));
  if (std::fabs(std::round(log2dim) - log2dim) > dimTol) {
    throw std::invalid_argument("QStateKet: dim(ket) must be 2^N for some integer N");
  }

  const double norm2 = std::real((k.adjoint() * k)(0, 0));
  if (std::fabs(norm2 - 1.0) > normTol) {
    throw std::invalid_argument("QStateKet: ket must be normalized");
  }
}



QStateKet::QStateKet(std::size_t numQubits) {
  std::vector<qpp::idx> zeros(numQubits, 0);
  state_ = qpp::mket(zeros);
}



QStateKet::QStateKet(qpp::ket state) : state_(std::move(state)) {
  ValidateKet(state_);
}



int64_t QStateKet::AssignStreams(int64_t stream) {
  constexpr uint64_t QPPK_SALT = 0x5150504BULL; // "QPPK"

  return QState::AssignStreamsGlobal<QPPK_SALT>(stream, [&](uint64_t s64) {
    auto seq = QState::MakeSeedSeq(s64);
    qpp::RandomDevices::get_instance().get_prng().seed(seq);
  });
}



std::size_t QStateKet::NumQubits() const {
  return static_cast<std::size_t>(std::llround(std::log2(state_.rows())));
}



void QStateKet::Print(std::ostream& os) const {
  PrintHeader(os, "Ket");
  os << state_ << "\n";
}



void QStateKet::Apply(const QGate& g, const std::vector<q2ns::Index>& targets) {
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
                  "QStateKet::Apply: wrong gate dimension");

  state_ = qpp::apply(state_, U, t);
}



std::shared_ptr<QState> QStateKet::MergeDisjoint(const QState& other) const {
  const auto* otherKet = dynamic_cast<const QStateKet*>(&other);
  if (!otherKet) {
    throw std::runtime_error("QStateKet::MergeDisjoint: incompatible backend");
  }

  const qpp::ket combined = qpp::kron(state_, otherKet->state_);
  return std::make_shared<QStateKet>(combined);
}



QState::MeasureResult QStateKet::Measure(q2ns::Index target, q2ns::Basis basis) {
  if (target >= NumQubits()) {
    throw std::out_of_range("QStateKet::Measure: target index out of range");
  }

  switch (basis) {
  case Basis::Z:
    break;
  case Basis::X:
    state_ = qpp::apply(state_, qpp::gt.H, {static_cast<qpp::idx>(target)});
    break;
  case Basis::Y:
    state_ = qpp::apply(state_, qpp::adjoint(qpp::gt.S), {static_cast<qpp::idx>(target)});
    state_ = qpp::apply(state_, qpp::gt.H, {static_cast<qpp::idx>(target)});
    break;
  }

  auto [res, probs, states] = qpp::measure(state_, qpp::gt.Z, {static_cast<qpp::idx>(target)});
  (void) probs;

  // Construct the measured 1-qubit state in the requested basis rather than
  // leaving it in the intermediate rotated Z-basis representation.
  qpp::ket oneQubit = (res == 0 ? qpp::mket({0}) : qpp::mket({1}));
  switch (basis) {
  case Basis::Z:
    break;
  case Basis::X:
    oneQubit = qpp::apply(oneQubit, qpp::gt.H, {0});
    break;
  case Basis::Y:
    oneQubit = qpp::apply(oneQubit, qpp::adjoint(qpp::gt.S), {0});
    oneQubit = qpp::apply(oneQubit, qpp::gt.H, {0});
    break;
  }

  auto measured1q = std::make_shared<QStateKet>(std::move(oneQubit));
  auto survivors = std::make_shared<QStateKet>(std::move(states[res]));
  return MeasureResult{static_cast<int>(res), measured1q, survivors};
}



const qpp::ket& QStateKet::GetKet() const {
  return state_;
}



void QStateKet::SetKet(const qpp::ket& k) {
  ValidateKet(k);
  state_ = k;
}



qpp::cmat QStateKet::GetDensityMatrix() const {
  return qpp::prj(state_);
}

} // namespace q2ns