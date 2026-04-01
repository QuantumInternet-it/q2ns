/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/
/**
 * @file q2ns-qstate-stab.cc
 * @brief Defines q2ns::QStateStab.
 */

#include "ns3/q2ns-qstate-stab.h"

#include "random.h"

#include <cstdint>
#include <stdexcept>

namespace q2ns {

QStateStab::QStateStab(std::size_t numQubits)
    : numQubits_(numQubits), psi_(static_cast<int>(numQubits)) {}



int64_t QStateStab::AssignStreams(int64_t stream) {
  constexpr uint64_t STAB_SALT = 0x53544142ULL; // "STAB"

  return QState::AssignStreamsGlobal<STAB_SALT>(stream, [&](uint64_t s64) { stab::Seed(s64); });
}



std::size_t QStateStab::NumQubits() const {
  return numQubits_;
}



void QStateStab::Print(std::ostream& os) const {
  PrintHeader(os, "Stab");
  os << psi_ << "\n";
}



void QStateStab::Apply(const QGate& g, const std::vector<Index>& t) {
  const auto arity = t.size();

  if (g.Kind() == QGateKind::Custom) {
    throw std::runtime_error("QStateStab does not support custom gates");
  }

  if (arity == 1) {
    const int q = static_cast<int>(t[0]);
    switch (g.Kind()) {
    case QGateKind::I:
      return;
    case QGateKind::H:
      psi_.H(q);
      return;
    case QGateKind::S:
      psi_.S(q);
      return;
    case QGateKind::SDG:
      psi_.SDG(q);
      return;
    case QGateKind::X:
      psi_.X(q);
      return;
    case QGateKind::Y:
      psi_.Y(q);
      return;
    case QGateKind::Z:
      psi_.Z(q);
      return;
    default:
      break;
    }
    throw std::runtime_error("QStateStab supports only 1-qubit Clifford gates");
  }

  if (arity == 2) {
    const int a = static_cast<int>(t[0]);
    const int b = static_cast<int>(t[1]);
    switch (g.Kind()) {
    case QGateKind::CNOT:
      psi_.CX(a, b);
      return;
    case QGateKind::CZ:
      psi_.CZ(a, b);
      return;
    case QGateKind::SWAP:
      psi_.SWAP(a, b);
      return;
    default:
      break;
    }
    throw std::runtime_error("QStateStab supports only 2-qubit Clifford gates");
  }

  throw std::runtime_error("QStateStab supports only 1-qubit and 2-qubit gates");
}

void QStateStab::RotateIntoZBasis_(Index q, Basis basis) {
  const int j = static_cast<int>(q);

  if (basis == Basis::Z) {
    return;
  }
  if (basis == Basis::X) {
    psi_.H(j);
    return;
  }
  if (basis == Basis::Y) {
    psi_.SDG(j);
    psi_.H(j);
    return;
  }

  throw std::runtime_error("QStateStab: unsupported measurement basis");
}

std::shared_ptr<QStateStab> QStateStab::Synth1QEigenstate_(Basis basis, int bit) {
  auto st = std::make_shared<QStateStab>(1);

  // Start in |0>, prepare the +1 eigenstate for the requested basis, then
  // apply Z when bit=1 to obtain the -1 eigenstate.
  if (basis == Basis::Z) {
    if (bit) {
      st->psi_.X(0);
    }
    return st;
  }
  if (basis == Basis::X) {
    st->psi_.H(0);
    if (bit) {
      st->psi_.Z(0);
    }
    return st;
  }
  if (basis == Basis::Y) {
    st->psi_.H(0);
    st->psi_.S(0);
    if (bit) {
      st->psi_.Z(0);
    }
    return st;
  }

  throw std::runtime_error("QStateStab: unsupported basis in Synth1QEigenstate_");
}



void QStateStab::RemoveQubit_(Index target) {
  if (numQubits_ == 0) {
    throw std::runtime_error("QStateStab::RemoveQubit_: empty state");
  }
  if (target >= numQubits_) {
    throw std::runtime_error("QStateStab::RemoveQubit_: target out of range");
  }

  // Swap the target to the end so dropping the last qubit preserves the
  // original relative order of survivors.
  for (Index q = target; q + 1 < numQubits_; ++q) {
    psi_.SWAP(static_cast<int>(q), static_cast<int>(q + 1));
  }

  psi_.DropLastQubit();
  --numQubits_;
}



QState::MeasureResult QStateStab::Measure(Index target, Basis basis) {
  if (target >= numQubits_) {
    throw std::runtime_error("QStateStab::Measure: target out of range");
  }

  // Standard pattern:
  // 1) rotate so Z measurement implements the requested basis
  // 2) measure Z
  // 3) build the measured 1-qubit eigenstate and the survivor state
  RotateIntoZBasis_(target, basis);

  const int bit = psi_.MeasureZ(static_cast<int>(target));

  auto survivors = std::make_shared<QStateStab>(*this);
  survivors->RemoveQubit_(target);

  auto measured = Synth1QEigenstate_(basis, bit);

  MeasureResult r;
  r.outcome = bit;
  r.measured = std::move(measured);
  r.survivors = std::move(survivors);
  return r;
}



std::shared_ptr<QState> QStateStab::MergeDisjoint(const QState& other) const {
  const auto* rhs = dynamic_cast<const QStateStab*>(&other);
  if (!rhs) {
    throw std::runtime_error("QStateStab::MergeDisjoint: incompatible backend");
  }

  stab::AffineState mergedPsi = stab::AffineState::TensorProduct(this->psi_, rhs->psi_);

  auto out = std::make_shared<QStateStab>(0);
  out->numQubits_ = this->numQubits_ + rhs->numQubits_;
  out->psi_ = std::move(mergedPsi);
  return out;
}

} // namespace q2ns