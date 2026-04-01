/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/
/**
 * @file q2ns-qprocessor.h
 * @brief Declares q2ns::QProcessor, a per-node processor that handles local quantum state
 * operations.
 */

#pragma once

#include "ns3/q2ns-qgate.h"
#include "ns3/q2ns-types.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace q2ns {

class QStateRegistry;
class QNode;
class QState;
class Qubit;


/**
 * @ingroup q2ns_qstate
 * @class QProcessor
 * @brief Internal helper owned by a QNode to handle local quantum operations.
 *
 * QProcessor is not intended to be the main user-facing API. Instead, users should use QNode, which
 * will internally delegate to QProcessor where necessary.
 *
 * Responsibilities:
 *  - Create and adopt qubit handles associated with the owning node (via registry location).
 *  - Enforce locality checks for operations intended to be local to the owning node.
 *  - Delegate gates, measurement, and state merging/splitting to QStateRegistry and QState.
 *
 * QProcessor does not own a persistent local-qubit container; locality is tracked centrally and
 * only by QStateRegistry.
 */
class QProcessor {
public:
  /**
   * @brief Construct a processor bound to a registry and owning node.
   * @param registry Reference to the global quantum state registry.
   * @param owner Reference to the owning node.
   */
  QProcessor(QStateRegistry& registry, QNode& owner);

  /**
   * @brief Get owning node.
   * @return Reference to the owning node.
   */
  const QNode& GetOwnerNode() const;

  /**
   * @brief Create a new local qubit in |0>.
   * @param label Optional human-readable qubit label.
   * @return Shared pointer to the new qubit.
   */
  std::shared_ptr<Qubit> CreateQubit(const std::string& label = "");

  /**
   * @brief Create a new local qubit handle in the given state.
   * @param state State to prepare the qubit in.
   * @param label Optional human-readable qubit label.
   * @return Shared pointer to the new qubit, or nullptr if @p state is null.
   */
  std::shared_ptr<Qubit> CreateQubit(const std::shared_ptr<QState>& state,
                                     const std::string& label = "");

  /**
   * @brief Create a local Bell pair in |Phi+>
   * @return Pair of local qubits {q0, q1}.
   */
  std::pair<std::shared_ptr<Qubit>, std::shared_ptr<Qubit>> CreateBellPair();

  /**
   * @brief Lookup a local qubit by application label.
   *
   * Labels are optional and need not be unique. Prefer GetQubit(QubitId) when a
   * stable unique identifier is available.
   *
   * @param label Application-level qubit label.
   * @return Matching local qubit, or nullptr if no local match is found.
   *
   * @see GetQubit(QubitId)
   */
  std::shared_ptr<Qubit> GetQubit(const std::string& label) const;

  /**
   * @brief Lookup a local qubit by unique identifier.
   * @param id Qubit identifier.
   * @return Matching local qubit, or nullptr if the qubit is not local or does
   * not exist.
   *
   * @see GetQubit(const std::string&)
   */
  std::shared_ptr<Qubit> GetQubit(QubitId id) const;

  /**
   * @brief Return the qubits currently located at the owning node.
   *
   * The returned vector is a snapshot taken at call time. No stable internal
   * container is exposed, and element order is not guaranteed.
   *
   * @return Snapshot of qubit handles currently local to the owning node.
   */
  std::vector<std::shared_ptr<Qubit>> GetLocalQubits() const;

  /**
   * @brief Adopt a qubit into this processor's owning node.
   *
   * Adoption updates the qubit's registry-authoritative location to the owning
   * node.
   *
   * @param q Qubit handle.
   */
  void AdoptQubit(const std::shared_ptr<Qubit>& q);

  /**
   * @brief Get the current state of the qubit.
   * @param q Target qubit.
   * @return Shared pointer to the QState, or nullptr if the
   * qubit is null or unknown to the registry.
   */
  std::shared_ptr<QState> GetState(const std::shared_ptr<Qubit>& q) const;

  /**
   * @brief Apply a gate to one or more local qubits.
   * @param gate Gate to apply.
   * @param qs   Target qubits. All must be local and not lost.
   * @return True if the application was successful, false otherwise.
   */
  bool Apply(const QGate& gate, const std::vector<std::shared_ptr<Qubit>>& qs);

  /**
   * @brief Measure a single, local qubit in the given basis (default Z).
   *
   * The measured qubit is rebound to a new one-qubit state. Any surviving
   * qubits from the original backend state are rebound to a new survivor state.
   *
   * @param [in] qubit Target qubit.
   * @param [in] basis Measurement basis.
   * @return Classical outcome bit (0 or 1), or -1 if the measurement is
   * rejected.
   */
  int Measure(const std::shared_ptr<Qubit>& qubit, q2ns::Basis basis = q2ns::Basis::Z);

  /**
   * @brief Perform a Bell-state measurement on two local qubits.
   *
   * This implementation performs a Bell-basis rotation using CNOT(a,b)
   * followed by H(a), then measures both qubits in the computational basis.
   *
   * @param a First qubit.
   * @param b Second qubit.
   * @return Pair of classical outcomes {mZZ, mXX}, or {-1, -1} if the
   * operation is rejected.
   *
   * @see Measure
   */
  std::pair<int, int> MeasureBell(const std::shared_ptr<Qubit>& a, const std::shared_ptr<Qubit>& b);

private:
  QStateRegistry& registry_; //!< Quantum state registry.
  QNode& owner_;             //!< Owning QNode.
};

} // namespace q2ns