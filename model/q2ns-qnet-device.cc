/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/
/**
 * @file q2ns-qnet-device.cc
 * @brief Defines q2ns::QNetDevice.
 */

#include "ns3/q2ns-qnet-device.h"

#include "ns3/q2ns-qchannel.h"
#include "q2ns-qnetworker.h"

#include "ns3/log.h"
#include "ns3/node.h"

namespace q2ns {

NS_LOG_COMPONENT_DEFINE("QNetDevice");
NS_OBJECT_ENSURE_REGISTERED(QNetDevice);



ns3::TypeId QNetDevice::GetTypeId(void) {
  static ns3::TypeId tid = ns3::TypeId("q2ns::QNetDevice")
                               .SetParent<ns3::NetDevice>()
                               .SetGroupName("q2ns")
                               .AddConstructor<QNetDevice>();
  return tid;
}



QNetDevice::QNetDevice() = default;



void QNetDevice::BindNetworker(QNetworker& networker) {
  networker_ = &networker;
}



void QNetDevice::AttachChannel(ns3::Ptr<QChannel> ch) {
  channel_ = ch;
}



bool QNetDevice::Send(std::shared_ptr<Qubit> q) {
  if (channel_) {
    return channel_->SendFrom(this, std::move(q));
  }

  return false;
}



void QNetDevice::ReceiveFromChannel(std::shared_ptr<Qubit> q, const QMapInstance& map) {
  NS_ASSERT_MSG(networker_, "ReceiveFromChannel called without a bound networker");
  networker_->ReceiveFromDevice(std::move(q), map);
}



ns3::Ptr<ns3::Channel> QNetDevice::GetChannel() const {
  return channel_;
}



void QNetDevice::SetNode(ns3::Ptr<ns3::Node> node) {
  node_ = node;
}



ns3::Ptr<ns3::Node> QNetDevice::GetNode() const {
  return node_;
}



void QNetDevice::SetIfIndex(std::uint32_t i) {
  ifIndex_ = i;
}



std::uint32_t QNetDevice::GetIfIndex() const {
  return ifIndex_;
}

} // namespace q2ns