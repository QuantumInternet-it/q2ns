/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/
/**
 * @file q2ns-qnet-device.h
 * @brief Declares q2ns::QNetDevice, a minimal quantum net device that bridges
 * a QChannel and a QNetworker.
 */

#pragma once

#include "ns3/net-device.h"
#include "ns3/ptr.h"
#include "ns3/traced-callback.h"
#include "ns3/type-id.h"

#include "ns3/q2ns-types.h"

#include <cstdint>
#include <memory>

namespace q2ns {

class Qubit;
class QNetworker;
class QChannel;

/**
 * @ingroup q2ns_network
 * @class QNetDevice
 * @brief Minimal quantum net device that bridges a QChannel and a QNetworker.
 *
 * QNetDevice is an internal plumbing component used by QNode and QNetworker.
 * It is intentionally small: it forwards outgoing qubits to an attached
 * QChannel and forwards incoming qubits from that channel to the bound
 * QNetworker.
 *
 * The inherited ns-3 NetDevice interface is implemented only to the extent
 * needed for integration with ns-3 nodes and channels.
 *
 * @see QChannel
 * @see QNetworker
 */
class QNetDevice : public ns3::NetDevice {
public:
  /**
   * @brief Get the ns-3 TypeId.
   * @return TypeId for q2ns::QNetDevice.
   */
  static ns3::TypeId GetTypeId(void);

  /**
   * @brief Default constructor.
   */
  QNetDevice();

  /**
   * @brief Bind the owning networker.
   *
   * The bound networker is notified when a qubit is delivered from the attached
   * channel.
   *
   * @param networker Owning QNetworker.
   *
   * @see ReceiveFromChannel
   */
  void BindNetworker(QNetworker& networker);

  /**
   * @brief Attach this device to a quantum channel.
   *
   * This is normally called by QChannel when wiring the link endpoints.
   *
   * @param ch Attached channel.
   *
   * @see QChannel::Connect
   */
  void AttachChannel(ns3::Ptr<QChannel> ch);

  /**
   * @brief Send a qubit through the attached channel.
   *
   * This method forwards the transmission request to the attached channel. It
   * does not perform routing.
   *
   * @param q Qubit to send.
   * @return True if the attached channel accepted the transmission request,
   * false otherwise.
   *
   * @see QChannel::SendFrom
   */
  bool Send(std::shared_ptr<Qubit> q);

  /**
   * @brief Return the attached channel.
   * @return Attached channel as an ns-3 Channel pointer, or nullptr if none is
   * attached.
   */
  ns3::Ptr<ns3::Channel> GetChannel() const override;

  /**
   * @brief Set the owning ns-3 node.
   * @param node Owning node.
   */
  void SetNode(ns3::Ptr<ns3::Node> node) override;

  /**
   * @brief Get the owning ns-3 node.
   * @return Owning node.
   */
  ns3::Ptr<ns3::Node> GetNode() const override;

  /**
   * @brief Set the interface index assigned by the owning node.
   * @param i Interface index.
   */
  void SetIfIndex(std::uint32_t i) override;

  /**
   * @brief Get the interface index assigned by the owning node.
   * @return Interface index.
   */
  std::uint32_t GetIfIndex() const override;

  void SetAddress(ns3::Address) override {}
  ns3::Address GetAddress() const override {
    return ns3::Address();
  }
  bool SetMtu(std::uint16_t) override {
    return true;
  }
  std::uint16_t GetMtu() const override {
    return 1500;
  }
  bool IsLinkUp() const override {
    return true;
  }
  void AddLinkChangeCallback(ns3::Callback<void>) override {}
  bool IsBroadcast() const override {
    return false;
  }
  ns3::Address GetBroadcast() const override {
    return ns3::Address();
  }
  bool IsMulticast() const override {
    return false;
  }
  ns3::Address GetMulticast(ns3::Ipv4Address) const override {
    return ns3::Address();
  }
  ns3::Address GetMulticast(ns3::Ipv6Address) const override {
    return ns3::Address();
  }
  bool IsBridge() const override {
    return false;
  }
  bool IsPointToPoint() const override {
    return true;
  }
  bool NeedsArp() const override {
    return false;
  }
  void SetReceiveCallback(ReceiveCallback) override {}
  void SetPromiscReceiveCallback(PromiscReceiveCallback) override {}
  bool SupportsSendFrom() const override {
    return false;
  }
  bool Send(ns3::Ptr<ns3::Packet>, const ns3::Address&, std::uint16_t) override {
    return false;
  }
  bool SendFrom(ns3::Ptr<ns3::Packet>, const ns3::Address&, const ns3::Address&,
                std::uint16_t) override {
    return false;
  }

private:
  /**
   * @brief Receive a qubit from the attached channel and forward it upward.
   *
   * The optional sampled QMapInstance represents per-transmission channel
   * effects to be applied by the receiving networker.
   *
   * @param q Qubit received from the channel.
   * @param map Sampled per-transmission channel map.
   *
   * @see QNetworker::ReceiveFromDevice
   */
  void ReceiveFromChannel(std::shared_ptr<Qubit> q, const QMapInstance& map = {});

  std::uint32_t ifIndex_ = 0;       //!< Interface index.
  ns3::Ptr<ns3::Node> node_;        //!< Owning ns-3 node.
  ns3::Ptr<QChannel> channel_;      //!< Attached quantum channel.
  QNetworker* networker_ = nullptr; //!< Bound networker, not owned.

  friend QChannel;
};

} // namespace q2ns