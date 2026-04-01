/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/
/**
 * @file q2ns-qubit.h
 * @brief Declares q2ns::Qubit, a lightweight handle for one qubit within a
 * registry-managed quantum state.
 */

#pragma once

#include "ns3/q2ns-types.h"

#include <cstdint>
#include <memory>
#include <string>

namespace q2ns {

class QStateRegistry;
class QState;

/**
 * @ingroup q2ns_api
 * @class Qubit
 * @brief Lightweight handle for one qubit inside a registry-managed state.
 *
 * Qubit does not own the underlying QState. Instead, it stores:
 * - a stable qubit id assigned by QStateRegistry
 * - a state id identifying the current backend state
 * - a zero-based index within that state
 * - an optional human-readable label
 *
 * Resolution of the underlying backend state and tracked location is performed
 * through QStateRegistry.
 *
 * Qubit is intentionally a lightweight handle object. Most user-facing
 * operations are performed through QNode rather than directly through Qubit.
 *
 * @see QNode
 * @see QStateRegistry
 */
class Qubit : public std::enable_shared_from_this<Qubit> {
public:
  /**
   * @brief Construct a qubit handle bound to a registry, state id, and index.
   *
   * The stable qubit id is assigned later by QStateRegistry::Register().
   *
   * @param registry Backing state registry.
   * @param stateId Current backend state id.
   * @param index Index within that backend state.
   * @param label Optional human-readable qubit label.
   *
   * @see QStateRegistry::Register
   */
  Qubit(QStateRegistry& registry, StateId stateId, unsigned int index, std::string label = "");

  /**
   * @brief Get the stable qubit id.
   * @return Stable qubit id, or 0 if not yet registered.
   *
   * @see QStateRegistry::Register
   */
  QubitId GetQubitId() const;

  /**
   * @brief Get the current backend state id.
   * @return Current state id.
   */
  StateId GetStateId() const;

  /**
   * @brief Get the current index within the backend state.
   * @return Zero-based in-state index.
   */
  unsigned int GetIndexInState() const;

  /**
   * @brief Get the application-level label.
   * @return Label string, possibly empty.
   */
  const std::string& GetLabel() const;

  /**
   * @brief Set the application-level label.
   * @param label New label string.
   */
  void SetLabel(std::string label);

  /**
   * @brief Return the registry-tracked current location of this qubit.
   * @return Current tracked location, or Unset if unknown.
   *
   * @see QStateRegistry::GetLocation
   */
  Location GetLocation() const;

private:
  /**
   * @brief Assign the stable qubit id.
   * @param id Stable qubit id.
   *
   * @see QStateRegistry::Register
   */
  void SetQubitId(QubitId id);

  /**
   * @brief Update the current backend state id.
   * @param stateId New state id.
   */
  void SetStateId(StateId stateId);

  /**
   * @brief Update the current index within the backend state.
   * @param index New zero-based in-state index.
   */
  void SetIndexInState(unsigned int index);

  /**
   * @brief Rebind this handle to a different state id and index.
   *
   * This is an internal helper used during state rewrites such as merging or
   * splitting.
   *
   * @param newStateId New backend state id.
   * @param newIndex New zero-based in-state index.
   */
  void Rebind(StateId newStateId, std::size_t newIndex);

  /**
   * @brief Mark this qubit as local to a node.
   * @param nodeId Owning node id.
   */
  void SetLocationNode(uint32_t nodeId);

  /**
   * @brief Mark this qubit as in transit on a channel.
   * @param channelId Channel id.
   */
  void SetLocationChannel(uint32_t channelId);

  /**
   * @brief Mark this qubit as lost.
   */
  void SetLocationLost();

  QStateRegistry& registry_; //!< Backing state registry.

  QubitId qubitId_{0};          //!< Stable qubit id assigned by the registry.
  StateId stateId_{0};          //!< Current backend state id.
  unsigned int indexInState_{}; //!< Current zero-based index within the state.
  std::string label_;           //!< Optional human-readable label.

  friend class QStateRegistry;
  friend class QProcessor;
  friend class QNetworker;
  friend class QMap;
};

} // namespace q2ns