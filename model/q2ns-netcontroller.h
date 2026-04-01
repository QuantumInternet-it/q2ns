/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/
/**
 * @file q2ns-netcontroller.h
 * @brief Declares q2ns::NetController, the main user-facing facade for
 * creating and configuring a quantum network in ns-3.
 */

#pragma once

#include "ns3/ptr.h"
#include "ns3/simulator.h"
#include "ns3/traced-callback.h"

#include "ns3/q2ns-qchannel.h"
#include "ns3/q2ns-qmap.h"
#include "ns3/q2ns-qstate-registry.h"

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace q2ns {

class QNode;
class Qubit;
class QState;

/**
 * @ingroup q2ns_api
 * @class NetController
 * @brief Main user-facing facade for creating and configuring a quantum network.
 *
 * NetController owns the shared QStateRegistry used by the q2ns network and
 * provides convenience helpers to create QNodes, install QChannels, and query
 * network objects by node or link.
 *
 * Qubit creation and manipulation are normally performed through QNode. In
 * contrast, NetController is primarily responsible for topology construction,
 * shared configuration, and deterministic RNG stream assignment.
 *
 * Responsibilities:
 * - Own the shared QStateRegistry for the network.
 * - Create and track QNode instances.
 * - Install quantum links, chains, and all-to-all topologies.
 * - Configure the default QState backend used for newly created states.
 * - Assign RNG streams to q2ns-owned random sources.
 *
 * @see QNode
 * @see QChannel
 * @see QStateRegistry
 */
class NetController {
public:
  /**
   * @brief Default constructor.
   */
  NetController();

  /**
   * @brief Access the shared state registry.
   * @return Reference to the internal QStateRegistry.
   */
  QStateRegistry& GetRegistry();

  /**
   * @brief Set the default backend used for newly created quantum states.
   * @param b Backend enum value.
   *
   * @see SetQStateBackend(std::string_view)
   * @see GetQStateBackend
   */
  void SetQStateBackend(QStateBackend b);

  /**
   * @brief Set the default backend by name.
   *
   * This is a convenience overload intended for CLI or configuration use.
   *
   * @param name Backend name such as "ket", "dm", or "stab".
   *
   * @see SetQStateBackend(QStateBackend)
   * @see GetQStateBackend
   */
  void SetQStateBackend(std::string_view name);

  /**
   * @brief Get the current default backend.
   * @return Current backend enum value.
   *
   * @see SetQStateBackend(QStateBackend)
   * @see SetQStateBackend(std::string_view)
   */
  QStateBackend GetQStateBackend();

  /**
   * @brief Assign RNG streams to q2ns-owned random sources.
   *
   * This method should normally be called after the topology has been built and
   * before the simulation is run. Streams are assigned deterministically from
   * the provided starting index.
   *
   * @param stream Starting stream index.
   * @return Number of streams consumed.
   */
  int64_t AssignStreams(int64_t stream);

  /**
   * @brief Create a QNode with an optional human-readable label.
   *
   * The node is registered internally and bound to this controller's shared
   * QStateRegistry.
   *
   * @param label Optional human-readable label.
   * @return Pointer to the created QNode.
   *
   * @see GetNode
   * @see GetRegistry
   */
  ns3::Ptr<QNode> CreateNode(const std::string& label = "");

  /**
   * @brief Return the QNode associated with a node id.
   * @param nodeId Desired node identifier.
   * @return Pointer to the matching QNode, or nullptr if none exists.
   *
   * @see CreateNode
   */
  ns3::Ptr<QNode> GetNode(uint32_t nodeId);

  /**
   * @brief Install a duplex quantum link between two nodes.
   *
   * This method creates two QNetDevice instances and one QChannel, attaches one
   * device to each node, and installs per-destination host routes so each node
   * can send to the other over the created link.
   *
   * @param a First endpoint node.
   * @param b Second endpoint node.
   * @return Pointer to the created QChannel, or the existing channel if the
   * link already exists.
   *
   * @see GetChannel
   * @see InstallQuantumChain
   * @see InstallQuantumAllToAll
   */
  ns3::Ptr<QChannel> InstallQuantumLink(ns3::Ptr<QNode> a, ns3::Ptr<QNode> b);

  /**
   * @brief Connect a sequence of nodes as a linear chain.
   *
   * For a sequence n0, n1, ..., n{k-1}, this installs links
   * (n0,n1), (n1,n2), ..., (n{k-2},n{k-1}).
   *
   * Existing links are left unchanged and their existing channel pointers are
   * returned in the corresponding positions.
   *
   * @param nodes Ordered node sequence.
   *
   * @see InstallQuantumLink
   * @see InstallQuantumAllToAll
   */
  std::vector<ns3::Ptr<QChannel>> InstallQuantumChain(const std::vector<ns3::Ptr<QNode>>& nodes);

  /**
   * @brief Connect a set of nodes with all-to-all quantum links.
   *
   * For N nodes, this installs one link for every unordered node pair.
   * Existing links are left unchanged and their existing channel pointers are
   * returned in the corresponding positions.
   *
   * @param nodes Node set.
   *
   * @see InstallQuantumLink
   * @see InstallQuantumChain
   */
  std::vector<ns3::Ptr<QChannel>> InstallQuantumAllToAll(const std::vector<ns3::Ptr<QNode>>& nodes);

  /**
   * @brief Return the QChannel connecting two nodes.
   * @param a One endpoint node.
   * @param b The other endpoint node.
   * @return Pointer to the connecting QChannel, or nullptr if no such link exists.
   *
   * @see InstallQuantumLink
   */
  ns3::Ptr<QChannel> GetChannel(ns3::Ptr<QNode> a, ns3::Ptr<QNode> b);

  /**
   * @brief Convenience helper to get a qubit's current backend state.
   * @param q Target qubit.
   * @return Shared pointer to the current QState, or nullptr if unknown.
   */
  std::shared_ptr<QState> GetState(const std::shared_ptr<Qubit>& q) const;

private:
  /**
   * @brief Ensure RNG streams have been assigned before simulation activity begins.
   *
   * This is used as an internal safety net so q2ns behaves deterministically
   * even if the user calls ns3::Simulator::Run() directly instead of going
   * through a controller-owned wrapper.
   */
  void EnsureStreamsAssigned_();

  bool streamsAssigned_ = false; //!< True once AssignStreams has been called.
  int64_t nextStream_ = 0;       //!< Next stream index to assign.

  QStateRegistry registry_;                             //!< Shared state registry.
  std::unordered_map<uint32_t, ns3::Ptr<QNode>> nodes_; //!< Node id to QNode map.
  std::map<std::pair<uint32_t, uint32_t>, ns3::Ptr<QChannel>>
      channels_; //!< Undirected link map keyed by sorted endpoint ids.
};

} // namespace q2ns