/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/
/**
 * @file q2ns-qstate-stab.h
 * @brief Declares q2ns::QStateStab, a stabilizer QState backend using
 * softwareQinc/stab.
 *
 * This backend targets correct implementation of the q2ns::QState interface
 * for Clifford-state simulation.
 */

#pragma once

#include "AffineState.h"

#include "ns3/q2ns-qstate.h"

#include <memory>
#include <vector>

namespace q2ns {

/**
 * @ingroup q2ns_qstate
 * @class QStateStab
 * @brief Stabilizer concrete QState backend using stab::AffineState.
 *
 * QStateStab supports Clifford-state simulation using a vendored
 * stab::AffineState implementation.
 *
 * Supported operations include:
 * - 1-qubit Clifford gates I, H, S, SDG, X, Y, Z
 * - 2-qubit Clifford gates CNOT, CZ, SWAP
 * - single-qubit projective measurement in X, Y, and Z bases
 * - disjoint merge by tensor product
 *
 * This backend does not support arbitrary custom unitary gates.
 *
 * @see QState
 * @see QGateKind
 */
class QStateStab final : public QState {
public:
  /**
   * @brief Construct the |0...0> stabilizer state on numQubits qubits.
   * @param numQubits Number of qubits.
   */
  explicit QStateStab(std::size_t numQubits);

  /**
   * @brief Assign RNG streams for deterministic randomness.
   *
   * This seeds the underlying stab random source used by measurement and other
   * stochastic backend operations.
   *
   * @param stream Starting stream index.
   * @return Number of streams consumed.
   */
  int64_t AssignStreams(int64_t stream) override;

  /**
   * @brief Print a human-readable representation of the state.
   * @param os Output stream.
   */
  void Print(std::ostream& os) const override;

  /**
   * @brief Apply a supported Clifford gate to the given target qubits.
   * @param g Gate descriptor.
   * @param t Target qubit indices.
   */
  void Apply(const QGate& g, const std::vector<Index>& t) override;

  /**
   * @brief Return the number of logical qubits in the state.
   * @return Number of logical qubits.
   */
  std::size_t NumQubits() const override;

  /**
   * @brief Measure one qubit in the requested basis and split the result.
   *
   * The returned measured state is a 1-qubit stabilizer eigenstate in the
   * requested basis. The survivors state contains the remaining qubits in
   * their original relative order.
   *
   * @param target Index of the qubit to measure.
   * @param basis Measurement basis. Defaults to Z.
   * @return Measurement result containing outcome, measured state, and survivor state.
   */
  MeasureResult Measure(Index target, Basis basis = Basis::Z) override;

  /**
   * @brief Return the disjoint merge of this state and another stabilizer backend.
   *
   * The merged qubit order is [this-qubits..., other-qubits...].
   *
   * @param other Other state to merge with.
   * @return Newly allocated merged state.
   */
  std::shared_ptr<QState> MergeDisjoint(const QState& other) const override;

  /**
   * @brief Return a clone of this stabilizer state.
   * @return Newly allocated copy of this state.
   */
  std::shared_ptr<QState> Clone() const {
    return std::make_shared<QStateStab>(*this);
  }

  /**
   * @brief Return the underlying AffineState.
   * @return Reference to the backend stabilizer state.
   */
  const stab::AffineState& GetAffineState() const {
    return psi_;
  }

private:
  /**
   * @brief Rotate a local basis so that measuring Z implements the requested basis.
   *
   * X measurement is implemented as H then Z.
   * Y measurement is implemented as SDG then H then Z.
   *
   * @param q Target qubit.
   * @param basis Requested measurement basis.
   */
  void RotateIntoZBasis_(Index q, Basis basis);

  /**
   * @brief Build a 1-qubit basis eigenstate for a measurement outcome.
   * @param basis Measurement basis.
   * @param bit Outcome bit.
   * @return Newly allocated 1-qubit stabilizer state.
   */
  static std::shared_ptr<QStateStab> Synth1QEigenstate_(Basis basis, int bit);

  /**
   * @brief Remove one qubit while preserving survivor order.
   *
   * The target is swapped to the end and then dropped from the underlying
   * AffineState.
   *
   * @param target Qubit to remove.
   */
  void RemoveQubit_(Index target);

  std::size_t numQubits_{0}; //!< Number of logical qubits.
  stab::AffineState psi_;    //!< Underlying stabilizer state.
};

} // namespace q2ns