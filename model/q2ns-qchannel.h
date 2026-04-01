/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/
/**
 * @file q2ns-qchannel.h
 * @brief Declares q2ns::QChannel, a duplex quantum channel supporting
 * per-direction delay, jitter, and transit quantum maps.
 */

#pragma once

#include "ns3/channel.h"
#include "ns3/nstime.h"
#include "ns3/ptr.h"
#include "ns3/random-variable-stream.h"
#include "ns3/type-id.h"

#include <cstdint>
#include <memory>

namespace q2ns {

class Qubit;
class QNetDevice;
class QMap;

/**
 * @ingroup q2ns_network
 * @class QChannel
 * @brief Duplex quantum channel with configurable delay, jitter, and transit maps.
 *
 * QChannel connects exactly two QNetDevice endpoints, referred to as A and B.
 * Delay, jitter, and QMap configuration may be set symmetrically for both
 * directions or independently for A->B and B->A.
 *
 * On each transmission, the channel selects the direction-specific delay,
 * applies an optional random jitter to that delay, samples the direction-
 * specific QMap to obtain a per-transmission QMapInstance, and schedules
 * delivery to the opposite endpoint.
 *
 * The sampled QMapInstance is not applied immediately in the channel. Instead,
 * it is delivered alongside the qubit and later executed at the receiving node
 * after the qubit becomes local there.
 *
 * @see QNetDevice
 * @see QMap
 */
class QChannel : public ns3::Channel {
public:
  /**
   * @brief Get the ns-3 TypeId.
   * @return TypeId for q2ns::QChannel.
   */
  static ns3::TypeId GetTypeId(void);

  /**
   * @brief Default constructor.
   */
  QChannel();

  /**
   * @brief Set the same propagation delay in both directions.
   * @param d Propagation delay.
   *
   * @see SetDelayAB
   * @see SetDelayBA
   */
  void SetDelay(ns3::Time d);

  /**
   * @brief Get the A->B propagation delay.
   *
   * If the channel is configured asymmetrically, this returns the A->B delay.
   * Use GetDelayBA() to query the opposite direction explicitly.
   *
   * @return A->B propagation delay.
   *
   * @see GetDelayAB
   * @see GetDelayBA
   */
  ns3::Time GetDelay() const;

  /**
   * @brief Set the same jitter in both directions.
   *
   * Jitter is sampled uniformly in [-jitter, +jitter] and added to the base
   * delay on each transmission, with a minimum resulting delay of zero.
   *
   * @param j Symmetric jitter magnitude.
   *
   * @see SetJitterAB
   * @see SetJitterBA
   */
  void SetJitter(ns3::Time j);

  /**
   * @brief Get the A->B jitter.
   *
   * If the channel is configured asymmetrically, this returns the A->B jitter.
   * Use GetJitterBA() to query the opposite direction explicitly.
   *
   * @return A->B jitter magnitude.
   *
   * @see GetJitterAB
   * @see GetJitterBA
   */
  ns3::Time GetJitter() const;

  /**
   * @brief Set the same transit QMap in both directions.
   *
   * A null map is treated as the identity map.
   *
   * @param m Symmetric transit QMap.
   *
   * @see SetQMapAB
   * @see SetQMapBA
   */
  void SetQMap(ns3::Ptr<QMap> m);

  /**
   * @brief Get the A->B transit QMap.
   *
   * If the channel is configured asymmetrically, this returns the A->B map.
   * Use GetQMapBA() to query the opposite direction explicitly.
   *
   * @return A->B transit QMap, or nullptr for identity behavior.
   *
   * @see GetQMapAB
   * @see GetQMapBA
   */
  ns3::Ptr<QMap> GetQMap() const;

  /**
   * @brief Set the one-way delay from A to B.
   * @param d Propagation delay from A to B.
   *
   * @see SetDelay
   * @see GetDelayAB
   */
  void SetDelayAB(ns3::Time d);

  /**
   * @brief Get the one-way delay from A to B.
   * @return Propagation delay from A to B.
   */
  ns3::Time GetDelayAB() const;

  /**
   * @brief Set the one-way delay from B to A.
   * @param d Propagation delay from B to A.
   *
   * @see SetDelay
   * @see GetDelayBA
   */
  void SetDelayBA(ns3::Time d);

  /**
   * @brief Get the one-way delay from B to A.
   * @return Propagation delay from B to A.
   */
  ns3::Time GetDelayBA() const;

  /**
   * @brief Set the one-way jitter from A to B.
   * @param j Jitter magnitude from A to B.
   *
   * @see SetJitter
   * @see GetJitterAB
   */
  void SetJitterAB(ns3::Time j);

  /**
   * @brief Get the one-way jitter from A to B.
   * @return Jitter magnitude from A to B.
   */
  ns3::Time GetJitterAB() const;

  /**
   * @brief Set the one-way jitter from B to A.
   * @param j Jitter magnitude from B to A.
   *
   * @see SetJitter
   * @see GetJitterBA
   */
  void SetJitterBA(ns3::Time j);

  /**
   * @brief Get the one-way jitter from B to A.
   * @return Jitter magnitude from B to A.
   */
  ns3::Time GetJitterBA() const;

  /**
   * @brief Set the one-way transit QMap from A to B.
   * @param m Transit QMap from A to B.
   *
   * @see SetQMap
   * @see GetQMapAB
   */
  void SetQMapAB(ns3::Ptr<QMap> m);

  /**
   * @brief Get the one-way transit QMap from A to B.
   * @return Transit QMap from A to B, or nullptr for identity behavior.
   */
  ns3::Ptr<QMap> GetQMapAB() const;

  /**
   * @brief Set the one-way transit QMap from B to A.
   * @param m Transit QMap from B to A.
   *
   * @see SetQMap
   * @see GetQMapBA
   */
  void SetQMapBA(ns3::Ptr<QMap> m);

  /**
   * @brief Get the one-way transit QMap from B to A.
   * @return Transit QMap from B to A, or nullptr for identity behavior.
   */
  ns3::Ptr<QMap> GetQMapBA() const;

  /**
   * @brief Connect two QNetDevice endpoints to this channel.
   *
   * The provided devices become endpoints A and B. Each device is also updated
   * to reference this channel.
   *
   * @param a Endpoint A device.
   * @param b Endpoint B device.
   *
   * @see QNetDevice::AttachChannel
   */
  void Connect(ns3::Ptr<QNetDevice> a, ns3::Ptr<QNetDevice> b);

  /**
   * @brief Return the number of attached devices.
   * @return Number of attached devices in the range [0, 2].
   */
  std::size_t GetNDevices() const override;

  /**
   * @brief Return an attached device by endpoint index.
   * @param i Endpoint index, where 0 selects A and 1 selects B.
   * @return Attached device pointer, or nullptr if the index is invalid or the
   * endpoint is not attached.
   */
  ns3::Ptr<ns3::NetDevice> GetDevice(std::size_t i) const override;

  /**
   * @brief Assign RNG streams used by this channel.
   *
   * Streams are assigned to the jitter and QMap sampling random variables.
   *
   * @param stream Starting stream index.
   * @return Number of streams consumed.
   */
  int64_t AssignStreams(int64_t stream);

protected:
  /**
   * @brief Protected destructor to prevent stack allocation.
   */
  ~QChannel() override = default;

private:
  /**
   * @brief Send a qubit from one endpoint to the opposite endpoint.
   *
   * The source device must be one of the two attached endpoints. The direction-
   * specific delay, jitter, and QMap are selected from that source endpoint.
   * Delivery is scheduled after the resulting propagation delay.
   *
   * @param src Source device. Must be either endpoint A or endpoint B.
   * @param q Qubit to transmit.
   * @return True if the transmission request was accepted, false otherwise.
   */
  bool SendFrom(QNetDevice* src, std::shared_ptr<Qubit> q);

  ns3::Ptr<QNetDevice> aDev_; //!< Device attached to endpoint A.
  ns3::Ptr<QNetDevice> bDev_; //!< Device attached to endpoint B.

  ns3::Time delayAB_{}; //!< Propagation delay from A to B.
  ns3::Time delayBA_{}; //!< Propagation delay from B to A.

  ns3::Time jitterAB_{ns3::Time(0.0)};            //!< Jitter magnitude from A to B.
  ns3::Time jitterBA_{ns3::Time(0.0)};            //!< Jitter magnitude from B to A.
  ns3::Ptr<ns3::UniformRandomVariable> jitterRv_; //!< Jitter sampler.

  ns3::Ptr<QMap> qmapAB_;                      //!< Transit QMap from A to B. Null means identity.
  ns3::Ptr<QMap> qmapBA_;                      //!< Transit QMap from B to A. Null means identity.
  ns3::Ptr<ns3::UniformRandomVariable> mapRv_; //!< QMap sampler.

  friend QNetDevice;
};

} // namespace q2ns