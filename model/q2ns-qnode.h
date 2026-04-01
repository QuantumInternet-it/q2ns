/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/
/**
 * @file q2ns-qnode.h
 * @brief Declares q2ns::QNode, the main user-facing per-node API for quantum
 * operations and transmission.
 */

#pragma once

#include "ns3/node.h"
#include "ns3/type-id.h"

#include "ns3/q2ns-qgate.h"
#include "ns3/q2ns-qstate.h"
#include "ns3/q2ns-types.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace ns3 {
class NetDevice;
} // namespace ns3

namespace q2ns {

class Qubit;
class QStateRegistry;
class QProcessor;
class QNetworker;

/**
 * @ingroup q2ns_api
 * @class QNode
 * @brief Main user-facing per-node API for quantum operations and transmission.
 *
 * QNode is the primary interface through which users create qubits, apply
 * local operations, perform measurements, and send qubits to other nodes.
 *
 * Internally, QNode delegates local quantum state manipulation to QProcessor
 * and network-facing transmission and reception to QNetworker. These internal
 * components operate against a shared QStateRegistry.
 *
 * Responsibilities:
 * - Provide user-facing helpers for qubit creation, lookup, measurement,
 *   entanglement, and gate application.
 * - Own an internal QProcessor for local quantum operations.
 * - Own an internal QNetworker for transmission and reception.
 * - Integrate quantum devices with the underlying ns-3 Node device model.
 *
 * @see QProcessor
 * @see QNetworker
 */
class QNode : public ns3::Node {
public:
  /**
   * @brief Construct a node bound to a shared state registry.
   * @param registry Registry used for state and location tracking.
   */
  explicit QNode(QStateRegistry& registry);

  /**
   * @brief Destructor.
   */
  ~QNode() override;

  /**
   * @brief Get the ns-3 TypeId.
   * @return TypeId for q2ns::QNode.
   */
  static ns3::TypeId GetTypeId(void);

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
   * @brief Return the qubits currently located at this node.
   *
   * The returned vector is a snapshot taken at call time. No stable internal
   * container is exposed, and element order is not guaranteed.
   *
   * @return Snapshot of qubit handles currently local to this node.
   */
  std::vector<std::shared_ptr<Qubit>> GetLocalQubits() const;

  /**
   * @brief Register a device with this node.
   *
   * The device is added to the underlying ns-3 Node. If the device is a
   * QNetDevice, it is also bound to this node's internal QNetworker so that
   * quantum arrivals are forwarded upward correctly.
   *
   * @param device Device to add.
   *
   * @see QNetDevice
   * @see AddRoute
   */
  void AddDevice(ns3::Ptr<ns3::NetDevice> device);

  /**
   * @brief Add or replace a simple host route for quantum transmission.
   *
   * The route maps a destination node id to an outgoing interface index in the
   * internal QNetworker.
   *
   * @param dstNodeId Destination node identifier.
   * @param oif Outgoing interface index.
   *
   * @see Send
   */
  void AddRoute(uint32_t dstNodeId, uint32_t oif);

  /**
   * @brief Send a qubit toward a destination node.
   *
   * The qubit must be local to this node and a valid route must exist to the
   * destination node.
   *
   * @param q Qubit to send.
   * @param dstNodeId Destination node identifier.
   * @return True if the send request was accepted, false otherwise.
   *
   * @see AddRoute
   * @see SetRecvCallback
   */
  bool Send(std::shared_ptr<Qubit> q, uint32_t dstNodeId);

  /**
   * @brief Set the application-level receive callback.
   *
   * The callback is invoked for qubits successfully delivered to this node
   * after destination-side adoption and channel-map processing.
   *
   * @param cb Callback invoked with the arriving qubit.
   *
   * @see Send
   */
  void SetRecvCallback(RecvCallback cb);

  /**
   * @brief Create a new local qubit initialized in the |0> state.
   * @param label Optional application-level qubit label.
   * @return Shared pointer to the new qubit.
   */
  std::shared_ptr<Qubit> CreateQubit(const std::string& label = "");

  /**
   * @brief Create a new local qubit from an existing single-qubit backend state.
   * @param state Backend state to use for the created qubit.
   * @param label Optional application-level qubit label.
   * @return Shared pointer to the new qubit, or nullptr if state is null.
   *
   * @see CreateQubit(const std::string&)
   */
  std::shared_ptr<Qubit> CreateQubit(const std::shared_ptr<QState>& state,
                                     const std::string& label = "");

  /**
   * @brief Create a local Bell pair in the |Phi+> state.
   * @return Pair of local qubits {q0, q1}.
   */
  std::pair<std::shared_ptr<Qubit>, std::shared_ptr<Qubit>> CreateBellPair();

  /**
   * @brief Get the current backend state of a qubit.
   * @param q Target qubit.
   * @return Shared pointer to the current backend state, or nullptr if the
   * qubit is null or unknown to the registry.
   */
  std::shared_ptr<QState> GetState(const std::shared_ptr<Qubit>& q);

  /**
   * @brief Apply a gate to one or more local qubits.
   * @param gate Gate to apply.
   * @param qs Target qubits.
   * @return True if the gate was applied successfully, false otherwise.
   *
   * @see Apply(const q2ns::Matrix&, const std::vector<std::shared_ptr<Qubit>>&)
   */
  bool Apply(const QGate& gate, const std::vector<std::shared_ptr<Qubit>>& qs);

  /**
   * @brief Apply a custom matrix gate to one or more local qubits.
   * @param gate Gate matrix to wrap as a custom gate.
   * @param qs Target qubits.
   * @return True if the gate was applied successfully, false otherwise.
   *
   * @see Apply(const QGate&, const std::vector<std::shared_ptr<Qubit>>&)
   */
  bool Apply(const q2ns::Matrix& gate, const std::vector<std::shared_ptr<Qubit>>& qs);

  /**
   * @brief Measure a local qubit in the given basis.
   * @param q Target qubit.
   * @param basis Measurement basis. Defaults to q2ns::Basis::Z.
   * @return Classical outcome bit (0 or 1), or -1 if the measurement is
   * rejected.
   *
   * @see MeasureBell
   */
  int Measure(const std::shared_ptr<Qubit>& q, q2ns::Basis basis = q2ns::Basis::Z);

  /**
   * @brief Perform a Bell-state measurement on two local qubits.
   * @param a First qubit.
   * @param b Second qubit.
   * @return Pair of classical outcomes {mZZ, mXX}, or {-1, -1} if the
   * operation is rejected.
   *
   * @see Measure
   * @see QProcessor::MeasureBell
   */
  std::pair<int, int> MeasureBell(const std::shared_ptr<Qubit>& a, const std::shared_ptr<Qubit>& b);

private:
  /**
   * @brief Mark a qubit as local to this node.
   *
   * This is an internal helper used by the receive path. Locality is tracked
   * centrally by QStateRegistry through QProcessor.
   *
   * @param q Qubit handle.
   */
  void AdoptQubit(const std::shared_ptr<Qubit>& q);

  QStateRegistry& registry_;              //!< Shared state registry.
  std::unique_ptr<QProcessor> processor_; //!< Internal local quantum processor.
  std::unique_ptr<QNetworker> networker_; //!< Internal networking component.

  friend class NetController;
  friend class QNetworker;
};

} // namespace q2ns