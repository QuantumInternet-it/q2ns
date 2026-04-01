/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/
/**
 * @file q2ns-qnetworker.cc
 * @brief Defines q2ns::QNetworker.
 */

#include "q2ns-qnetworker.h"

#include "ns3/q2ns-qnet-device.h"
#include "ns3/q2ns-qnode.h"
#include "ns3/q2ns-qubit.h"

#include "ns3/channel.h"
#include "ns3/log.h"

#include <cstdint>

namespace q2ns {

NS_LOG_COMPONENT_DEFINE("QNetworker");



QNetworker::QNetworker(QNode& owner) : owner_(owner) {}



uint32_t QNetworker::AddInterface(ns3::Ptr<ns3::NetDevice> dev) {

  ns3::Ptr<QNetDevice> qdev = DynamicCast<QNetDevice>(dev);
  NS_ABORT_MSG_IF(qdev == nullptr, "QNetworker requires QNetDevice interfaces");
  m_ifaces.push_back(qdev);

  return static_cast<uint32_t>(m_ifaces.size() - 1);
}



void QNetworker::AddRoute(uint32_t dstNodeId, uint32_t oif) {
  m_hostRoutes[dstNodeId] = oif;
}



bool QNetworker::Send(std::shared_ptr<Qubit> q, uint32_t dstNodeId) {
  if (!q) {
    NS_LOG_WARN("Send rejected: null qubit.");
    return false;
  }

  const auto loc = q->GetLocation();
  if (loc.type == LocationType::Lost) {
    NS_LOG_WARN("Send rejected: qubit " << q->GetQubitId() << " is lost.");
    return false;
  }

  if (!(loc.type == LocationType::Node && loc.ownerId == owner_.GetId())) {
    NS_LOG_WARN("Send rejected: qubit " << q->GetQubitId() << " is not local to this node.");
    return false;
  }

  const auto itRoutes = m_hostRoutes.find(dstNodeId);
  if (itRoutes == m_hostRoutes.end()) {
    NS_LOG_WARN("Send rejected: no route to destination node " << dstNodeId << ".");
    return false;
  }

  const uint32_t oif = itRoutes->second;
  if (oif >= m_ifaces.size()) {
    NS_LOG_WARN("Send rejected: route references invalid interface index.");
    return false;
  }

  auto dev = m_ifaces[oif];
  if (!dev) {
    NS_LOG_WARN("Send rejected: outgoing interface has no device.");
    return false;
  }

  if (!dev->GetChannel()) {
    NS_LOG_WARN("Send rejected: outgoing device is not attached to a channel.");
    return false;
  }

  q->SetLocationChannel(dev->GetChannel()->GetId());
  return dev->Send(std::move(q));
}



void QNetworker::ReceiveFromDevice(std::shared_ptr<Qubit> q, const QMapInstance& map) {
  owner_.AdoptQubit(q);

  // Apply the sampled channel map only after the qubit has become local to the
  // destination node. This gives the map access to the destination-side node
  // context and ensures any resulting loss is visible before callback delivery.
  if (map) {
    map(owner_, q);
  }

  if (!q) {
    NS_LOG_WARN("ReceiveFromDevice terminated early with no adopted qubit: QMapInstance appears to "
                "have caused "
                "the qubit pointer to become null.");
    return;
  }

  const auto loc = q->GetLocation();
  if (loc.type != LocationType::Lost && !recvCallback_.IsNull()) {
    recvCallback_(q);
  }
}



void QNetworker::SetRecvCallback(RecvCallback cb) {
  recvCallback_ = std::move(cb);
}

} // namespace q2ns