/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/
/**
 * @file q2ns-qstate-ket.h
 * @brief Declares q2ns::QStateKet, a ket-based QState backend using qpp.
 */

#pragma once

#include <qpp/qpp.hpp>

#include "ns3/q2ns-qstate.h"

#include <memory>
#include <vector>

namespace q2ns {

/**
 * @ingroup q2ns_qstate
 * @class QStateKet
 * @brief Ket-based concrete QState backend using qpp.
 *
 * QStateKet stores an N-qubit pure state as a qpp::ket and implements the
 * common QState interface.
 *
 * This is the default backend and supports:
 * - general unitary gate application
 * - disjoint merge by tensor product
 * - single-qubit measurement with split return semantics
 *
 * It also exposes backend-specific helpers for direct ket access and
 * conversion to a density matrix.
 *
 * Constructors validate basic ket structure such as power-of-two dimension and norm close to one.
 *
 * @see QState
 * @see QStateDM
 */
class QStateKet final : public QState {
public:
  /**
   * @brief Construct the |0...0> state on numQubits qubits.
   * @param numQubits Number of qubits.
   */
  explicit QStateKet(std::size_t numQubits);

  /**
   * @brief Construct from an existing ket.
   * @param state Pure state ket.
   * @throws std::invalid_argument if the ket is empty, not a column vector,
   *         not of dimension 2^N, or not normalized.
   */
  explicit QStateKet(qpp::ket state);

  /**
   * @brief Assign RNG streams for deterministic randomness.
   *
   * This seeds qpp's random source for measurement and other stochastic qpp
   * operations used by this backend.
   *
   * @param stream Starting stream index.
   * @return Number of streams consumed.
   */
  int64_t AssignStreams(int64_t stream) override;

  /**
   * @brief Return the number of logical qubits in the state.
   * @return Number of logical qubits.
   */
  std::size_t NumQubits() const override;

  /**
   * @brief Print a human-readable representation of the state.
   * @param os Output stream.
   */
  void Print(std::ostream& os) const override;

  /**
   * @brief Apply a gate to the given target qubits.
   * @param g Gate descriptor.
   * @param targets Target qubit indices.
   */
  void Apply(const QGate& g, const std::vector<q2ns::Index>& targets) override;

  /**
   * @brief Return the disjoint merge of this state and another ket backend.
   *
   * The merged qubit order is [this-qubits..., other-qubits...].
   *
   * @param other Other state to merge with.
   * @return Newly allocated merged state.
   */
  std::shared_ptr<QState> MergeDisjoint(const QState& other) const override;

  /**
   * @brief Measure one qubit in the requested basis and split the result.
   *
   * The returned measured state is a 1-qubit collapsed state expressed in the
   * requested measurement basis. The survivors state contains the remaining
   * qubits in their original relative order.
   *
   * @param target Index of the qubit to measure.
   * @param basis Measurement basis. Defaults to Z.
   * @return Measurement result containing outcome, measured state, and survivor state.
   */
  MeasureResult Measure(q2ns::Index target, q2ns::Basis basis = q2ns::Basis::Z) override;

  /**
   * @brief Return the underlying ket.
   * @return Reference to the backend ket.
   *
   * @see SetKet
   * @see GetDensityMatrix
   */
  const qpp::ket& GetKet() const;

  /**
   * @brief Replace the underlying ket.
   * @param k New backend ket.
   * @throws std::invalid_argument if the ket is empty, not a column vector,
   *         not of dimension 2^N, or not normalized.
   *
   * @see GetKet
   */
  void SetKet(const qpp::ket& k);

  /**
   * @brief Return the density matrix |psi><psi| of the current ket.
   * @return Density matrix corresponding to the current pure state.
   *
   * @see GetKet
   */
  qpp::cmat GetDensityMatrix() const;

private:
  /**
   * @brief Validate basic ket properties.
   * @param k Ket to validate.
   */
  static void ValidateKet(const qpp::ket& k);

  qpp::ket state_; //!< Pure state ket.
};

} // namespace q2ns