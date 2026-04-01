/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/
/**
 * @file q2ns-qstate-registry.h
 * @brief Declares q2ns::QStateRegistry and related types for tracking quantum
 * states, qubit membership, and qubit locations.
 */

#pragma once

#include "ns3/q2ns-types.h"

#include <functional>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace q2ns {

class Qubit;
class QState;

/**
 * @struct CreateStateResult
 * @brief Result of creating a new backend state.
 *
 * The returned indices are the initial in-state qubit indices for that state,
 * typically 0 through N-1.
 */
struct CreateStateResult {
  StateId stateId = 0;               //!< Assigned state id.
  std::vector<unsigned int> indices; //!< Initial in-state qubit indices.
};

/**
 * @ingroup q2ns_qstate
 * @class QStateRegistry
 * @brief Internal registry that owns backend states and tracks qubit membership
 * and location.
 *
 * QStateRegistry is not intended to be a primary user-facing API. It acts as
 * the central source of truth for:
 * - backend state ownership
 * - qubit membership within each state
 * - current qubit location
 * - qubit-id to handle resolution
 *
 * QNode, QProcessor, QNetworker, and NetController rely on this registry to
 * coordinate state evolution, movement, and lookup.
 *
 * @see QNode
 * @see QProcessor
 * @see QNetworker
 * @see NetController
 */
class QStateRegistry {
public:
  /**
   * @brief Callback type used to assign RNG streams to newly registered states.
   */
  using StreamAssigner = std::function<void(QState&)>;

  /**
   * @brief Set the callback used to assign streams to newly created states.
   *
   * This is typically installed by NetController so states created after stream
   * assignment receive deterministic streams automatically.
   *
   * @param fn Stream-assignment callback.
   *
   * @see GetStatesSortedById
   */
  void SetStreamAssigner(StreamAssigner fn) {
    streamAssigner_ = std::move(fn);
  }

  /**
   * @brief Set the default backend used for newly created states.
   * @param b Backend enum value.
   *
   * @see GetDefaultBackend
   * @see BackendFromString
   */
  void SetDefaultBackend(QStateBackend b);

  /**
   * @brief Get the default backend used for newly created states.
   * @return Current backend enum value.
   *
   * @see SetDefaultBackend
   */
  QStateBackend GetDefaultBackend() const;

  /**
   * @brief Create a new state with n qubits initialized in the |0...0> state.
   * @param n Number of qubits in the new state.
   * @return Result containing the assigned state id and initial qubit indices.
   *
   * @see CreateStateFromExisting
   */
  CreateStateResult CreateState(unsigned int n);

  /**
   * @brief Register an already-constructed backend state object.
   *
   * The caller is responsible for binding qubits to the returned state id and
   * indices and then calling Register() for each qubit handle.
   *
   * @param state Backend state to register.
   * @return Result containing the assigned state id and initial qubit indices.
   *
   * @see CreateState
   * @see Register
   */
  CreateStateResult CreateStateFromExisting(const std::shared_ptr<QState>& state);

  /**
   * @brief Ensure the provided qubits belong to a single backend state.
   *
   * If all qubits already belong to one state, that state is returned
   * unchanged. Otherwise, the involved states are merged, qubits are rebound to
   * the merged state with compacted indices, and empty predecessor states are
   * removed.
   *
   * @param qs Qubits that must share one backend state.
   * @return Shared pointer to the resulting backend state, or nullptr if the
   * merge is rejected.
   *
   * @see GetState
   * @see RemoveState
   */
  std::shared_ptr<QState> MergeStates(const std::vector<std::shared_ptr<Qubit>>& qs);

  /**
   * @brief Remove a backend state and its membership tracking.
   * @param id State id to remove.
   *
   * @see MergeStates
   */
  void RemoveState(StateId id);

  /**
   * @brief Get a backend state by state id.
   * @param stateId State identifier.
   * @return Shared pointer to the state, or nullptr if unknown.
   *
   * @see GetState(const std::shared_ptr<Qubit>&)
   */
  std::shared_ptr<QState> GetState(StateId stateId) const;

  /**
   * @brief Get the backend state associated with a qubit handle.
   * @param q Qubit handle.
   * @return Shared pointer to the state, or nullptr if the qubit is null or unknown.
   *
   * @see GetState(StateId)
   */
  std::shared_ptr<QState> GetState(const std::shared_ptr<Qubit>& q) const;

  /**
   * @brief Register a qubit handle as a member of its current state.
   *
   * If the qubit does not yet have a stable qubit id, one is assigned here.
   * If the qubit does not yet have a tracked location, it is marked Unset.
   *
   * @param q Qubit handle.
   *
   * @see Unregister
   * @see SetLocation
   */
  void Register(const std::shared_ptr<Qubit>& q);

  /**
   * @brief Unregister a qubit handle from its current state membership list.
   * @param q Qubit handle.
   *
   * @see Register
   * @see UnregisterEverywhere
   */
  void Unregister(const std::shared_ptr<Qubit>& q);

  /**
   * @brief Remove a qubit handle from membership lists across all states.
   * @param q Qubit handle.
   *
   * @see Unregister
   */
  void UnregisterEverywhere(const std::shared_ptr<Qubit>& q);

  /**
   * @brief Set the tracked location for a qubit handle.
   *
   * This also ensures the global qubit-id directory can resolve the qubit id
   * back to a handle while the handle remains alive.
   *
   * @param q Qubit handle.
   * @param loc Location descriptor.
   *
   * @see GetLocation
   */
  void SetLocation(const std::shared_ptr<Qubit>& q, Location loc);

  /**
   * @brief Set the tracked location for a qubit id.
   * @param id Qubit id.
   * @param loc Location descriptor.
   *
   * @see GetLocation
   */
  void SetLocation(QubitId id, Location loc);

  /**
   * @brief Get the tracked location for a qubit handle.
   * @param q Qubit handle.
   * @return Current location if tracked, otherwise an Unset location.
   *
   * @see GetLocation(QubitId)
   */
  Location GetLocation(const std::shared_ptr<Qubit>& q) const;

  /**
   * @brief Get the tracked location for a qubit id.
   * @param id Qubit id.
   * @return Current location if tracked, otherwise an Unset location.
   *
   * @see GetLocation(const std::shared_ptr<Qubit>&)
   */
  Location GetLocation(QubitId id) const;

  /**
   * @brief Return qubit ids currently located at a given node.
   *
   * Only qubits whose tracked location is LocationType::Node with matching
   * ownerId are returned.
   *
   * @param nodeId Node identifier.
   * @return Snapshot of qubit ids currently local to that node.
   *
   * @see GetLocalQubits
   */
  std::vector<QubitId> GetQubitsAtNode(uint32_t nodeId) const;

  /**
   * @brief Return a tracked qubit handle by qubit id.
   * @param id Qubit identifier.
   * @return Shared pointer to the qubit handle, or nullptr if unknown or expired.
   *
   * @see GetQubitsAtNode
   * @see GetLocalQubits
   */
  std::shared_ptr<Qubit> GetQubitHandle(QubitId id) const;

  /**
   * @brief Return qubit handles currently located at a given node.
   *
   * The returned vector is a snapshot taken at call time. Expired handles are
   * filtered out. Element order is not guaranteed.
   *
   * @param nodeId Node identifier.
   * @return Snapshot of qubit handles currently local to that node.
   *
   * @see GetQubitsAtNode
   */
  std::vector<std::shared_ptr<Qubit>> GetLocalQubits(uint32_t nodeId) const;

  /**
   * @brief Return qubit handles that currently belong to a given state.
   * @param stateId State identifier.
   * @return Snapshot of qubit handles in that state with expired entries filtered out.
   *
   * @see GetState
   */
  std::vector<std::shared_ptr<Qubit>> QubitsOf(StateId stateId) const;

  /**
   * @brief Return all registered states sorted by state id.
   *
   * This provides deterministic iteration order for operations such as delayed
   * RNG stream assignment.
   *
   * @return Vector of states sorted by state id.
   *
   * @see SetStreamAssigner
   */
  std::vector<std::shared_ptr<QState>> GetStatesSortedById() const;

private:
  StreamAssigner streamAssigner_{}; //!< Callback used when a new state is registered.

  QStateBackend defaultBackend_ = QStateBackend::Ket; //!< Default backend for CreateState().

  StateId nextStateId_ = 1; //!< Next state id to assign.
  QubitId nextQubitId_ = 1; //!< Next qubit id to assign.

  std::unordered_map<StateId, std::shared_ptr<QState>> states_; //!< State id to backend state.
  std::unordered_map<StateId, std::vector<std::weak_ptr<Qubit>>>
      members_;                                    //!< State id to member qubit handles.
  std::unordered_map<QubitId, Location> location_; //!< Qubit id to current location.
  std::unordered_map<uint32_t, std::unordered_set<QubitId>>
      qubitsAtNode_; //!< Node id to currently local qubit ids.
  std::unordered_map<QubitId, std::weak_ptr<Qubit>> qubitById_; //!< Qubit id to weak handle.
};

/**
 * @brief Convert a backend name string to a QStateBackend enum value.
 *
 * Unrecognized names fall back to QStateBackend::Ket.
 *
 * @param s Backend name string.
 * @return Corresponding backend enum value.
 */
QStateBackend BackendFromString(std::string_view s);

} // namespace q2ns