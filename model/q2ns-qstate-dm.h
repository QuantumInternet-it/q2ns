/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/
/**
 * @file q2ns-qstate-dm.h
 * @brief Declares q2ns::QStateDM, a density-matrix QState backend using qpp.
 */

#pragma once

#include <qpp/qpp.hpp>

#include "ns3/q2ns-qstate.h"

#include <memory>
#include <vector>

namespace q2ns {

/**
 * @ingroup q2ns_qstate
 * @class QStateDM
 * @brief Density-matrix concrete QState backend using qpp::cmat.
 *
 * QStateDM stores an N-qubit state as a 2^N by 2^N density matrix and
 * implements the common QState interface.
 *
 * Supported behavior includes:
 * - general unitary evolution
 * - single-qubit measurement via basis rotation and projective measurement
 * - disjoint merge by tensor product
 * - partial trace extraction of subsystems
 *
 * Constructors validate basic density-matrix structure such as square shape,
 * power-of-two dimension, and trace close to one.
 *
 * @see QState
 * @see QStateKet
 */
class QStateDM final : public QState {
public:
  /**
   * @brief Construct the |0...0><0...0| state on numQubits qubits.
   * @param numQubits Number of qubits.
   */
  explicit QStateDM(std::size_t numQubits);

  /**
   * @brief Construct from an existing density matrix.
   * @param rho Density matrix.
   * @throws std::invalid_argument if the matrix is not square, not Hermitian,
   *         not positive semidefinite, not of dimension 2^N, or does not have trace one.
   */
  explicit QStateDM(qpp::cmat rho);

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
   * @brief Print a human-readable representation of the state.
   * @param os Output stream.
   */
  void Print(std::ostream& os) const override;

  /**
   * @brief Return the number of logical qubits in the state.
   * @return Number of logical qubits.
   */
  std::size_t NumQubits() const override;

  /**
   * @brief Apply a gate to the given target qubits.
   * @param g Gate descriptor.
   * @param targets Target qubit indices.
   */
  void Apply(const QGate& g, const std::vector<q2ns::Index>& targets) override;

  /**
   * @brief Return the disjoint merge of this state and another density-matrix backend.
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
   * The returned measured state is a 1-qubit density matrix expressed in the
   * requested measurement basis. The survivors state contains the remaining
   * qubits in their original relative order.
   *
   * @param target Index of the qubit to measure.
   * @param basis Measurement basis. Defaults to Z.
   * @return Measurement result containing outcome, measured state, and survivor state.
   */
  MeasureResult Measure(q2ns::Index target, q2ns::Basis basis = q2ns::Basis::Z) override;

  /**
   * @brief Return the underlying density matrix.
   * @return Reference to the backend density matrix.
   *
   * @see SetRho
   */
  const qpp::cmat& GetRho() const {
    return rho_;
  }

  /**
   * @brief Replace the underlying density matrix after validation.
   * @param rho New density matrix.
   * @throws std::invalid_argument if the matrix is not square, not Hermitian,
   *         not positive semidefinite, not of dimension 2^N, or does not have trace one.
   *
   * @see GetRho
   */
  void SetRho(const qpp::cmat& rho);

  /**
   * @brief Extract a subsystem by partial trace.
   *
   * The qubits listed in subsystemA are kept in the returned state. This object
   * is mutated to become the complementary subsystem.
   *
   * @param subsystemA Indices of the subsystem to keep in the returned state.
   * @return New state representing subsystemA.
   */
  std::shared_ptr<QState> PartialTrace(const std::vector<q2ns::Index>& subsystemA);

private:
  /**
   * @brief Validate basic density-matrix structure.
   * @param rho Density matrix to validate.
   */
  static void ValidateDensityMatrix(const qpp::cmat& rho);

  qpp::cmat rho_; //!< Backend density matrix.
};

} // namespace q2ns