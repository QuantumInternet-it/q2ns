/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/
/**
 * @file q2ns-qnetworker.h
 * @brief Declares q2ns::QNetworker, an internal per-node networking component
 * for quantum transmission and reception.
 */

#pragma once

#include "ns3/callback.h"
#include "ns3/ptr.h"

#include "ns3/q2ns-types.h"

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace ns3 {
class NetDevice;
} // namespace ns3

namespace q2ns {

class Qubit;
class QNode;
class QNetDevice;

/**
 * @ingroup q2ns_network
 * @class QNetworker
 * @brief Internal helper owned by QNode for node-local quantum networking.
 *
 * QNetworker is not intended to be the main user-facing API. Instead, users should send qubits and
 * register receive callbacks through QNode, which delegates network-facing behavior to its internal
 * QNetworker.
 *
 * Responsibilities:
 * - Maintain a list of node-local quantum interfaces backed by QNetDevice.
 * - Maintain a minimal host-route table mapping destination node id to
 *   outgoing interface index.
 * - Send local qubits toward a destination node using the configured route.
 * - Accept qubits delivered from a channel, adopt them into the owning node,
 *   apply any sampled channel map, and notify the registered receive callback.
 */
class QNetworker {
public:
  /**
   * @brief Construct a networker bound to an owning node.
   * @param [in] owner Owning node.
   */
  explicit QNetworker(QNode& owner_);

  /**
   * @brief Register a quantum device as an outgoing interface. Analogue of
   * Ipv4L3Protocol::AddInterface(dev).
   *
   * The supplied device must be a QNetDevice.
   *
   * @param dev NetDevice to register.
   * @return Interface index assigned to the registered device.
   */
  uint32_t AddInterface(ns3::Ptr<ns3::NetDevice> dev);

  /**
   * @brief Add or replace a host route for a destination node.
   *
   * The route maps a destination node id to an outgoing interface index.
   *
   * @param dstNodeId Destination node identifier.
   * @param oif Outgoing interface index.
   */
  void AddRoute(uint32_t dstNodeId, uint32_t oif);

  /**
   * @brief Send a local qubit toward a destination node.
   *
   * The qubit must be local to the owning node and not lost. The destination
   * must have a configured host route that resolves to a valid outgoing
   * interface and attached channel.
   *
   * On successful acceptance by the outgoing device, the qubit location is
   * updated to the channel before transmission proceeds.
   *
   * @param q Qubit to send.
   * @param dstNodeId Destination node identifier.
   * @return True if the outgoing device accepted the transmission request,
   * false otherwise.
   */
  bool Send(std::shared_ptr<Qubit> q, uint32_t dstNodeId);

  /**
   * @brief Handle a qubit delivered from a channel.
   *
   * The qubit is first adopted into the owning node. If a sampled QMapInstance
   * is provided, it is then applied. If the qubit is not lost after that
   * processing, the registered receive callback is invoked.
   *
   * @param q Delivered qubit.
   * @param map Sampled per-transmission channel map to apply on receipt.
   */
  void ReceiveFromDevice(std::shared_ptr<Qubit> q, const QMapInstance& map);

  /**
   * @brief Set the application-level receive callback.
   * @param cb Callback invoked for qubits successfully delivered to this node.
   */
  void SetRecvCallback(RecvCallback cb);

  /**
   * @brief Get the registered outgoing interfaces.
   * @return Read-only reference to the interface table.
   */
  const std::vector<ns3::Ptr<QNetDevice>>& GetInterfaces() const {
    return m_ifaces;
  }

private:
  QNode& owner_; //!< Owning node.

  std::vector<ns3::Ptr<QNetDevice>> m_ifaces;          //!< Outgoing interfaces by index.
  std::unordered_map<uint32_t, uint32_t> m_hostRoutes; //!< Host routes: dst node id to oif.
  RecvCallback recvCallback_;                          //!< Application-level receive callback.
};
} // namespace q2ns