/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/
/**
 * @file q2ns-qchannel.cc
 * @brief Defines q2ns::QChannel.
 */

#include "ns3/q2ns-qchannel.h"

#include "ns3/q2ns-qmap.h"
#include "ns3/q2ns-qnet-device.h"
#include "ns3/q2ns-qubit.h"

#include "ns3/attribute.h"
#include "ns3/log.h"
#include "ns3/net-device.h"
#include "ns3/nstime.h"
#include "ns3/pointer.h"
#include "ns3/simulator.h"

namespace q2ns {

NS_LOG_COMPONENT_DEFINE("QChannel");
NS_OBJECT_ENSURE_REGISTERED(QChannel);

ns3::TypeId QChannel::GetTypeId(void) {
  static ns3::TypeId tid =
      ns3::TypeId("q2ns::QChannel")
          .SetParent<ns3::Channel>()
          .SetGroupName("q2ns")
          .AddConstructor<QChannel>()
          .AddAttribute("Delay", "Symmetric propagation delay", ns3::TimeValue(ns3::Seconds(0)),
                        MakeTimeAccessor(&QChannel::SetDelay, &QChannel::GetDelay),
                        ns3::MakeTimeChecker())
          .AddAttribute("DelayAB", "One-way propagation delay A->B",
                        ns3::TimeValue(ns3::Seconds(0)),
                        MakeTimeAccessor(&QChannel::SetDelayAB, &QChannel::GetDelayAB),
                        ns3::MakeTimeChecker())
          .AddAttribute("DelayBA", "One-way propagation delay B->A",
                        ns3::TimeValue(ns3::Seconds(0)),
                        MakeTimeAccessor(&QChannel::SetDelayBA, &QChannel::GetDelayBA),
                        ns3::MakeTimeChecker())
          .AddAttribute("Jitter",
                        "Symmetric jitter. A uniform random jitter in [-jitter, +jitter] is added "
                        "to the propagation delay on each send, with a minimum value of 0.",
                        ns3::TimeValue(ns3::Seconds(0)),
                        MakeTimeAccessor(&QChannel::SetJitter, &QChannel::GetJitter),
                        ns3::MakeTimeChecker())
          .AddAttribute(
              "JitterAB",
              "One-way jitter A->B. A uniform random jitter in [-jitter, +jitter] is added "
              "to the propagation delay on each send, with a minimum value of 0.",
              ns3::TimeValue(ns3::Seconds(0)),
              MakeTimeAccessor(&QChannel::SetJitterAB, &QChannel::GetJitterAB),
              ns3::MakeTimeChecker())
          .AddAttribute(
              "JitterBA",
              "One-way jitter B->A. A uniform random jitter in [-jitter, +jitter] is added "
              "to the propagation delay on each send, with a minimum value of 0.",
              ns3::TimeValue(ns3::Seconds(0)),
              MakeTimeAccessor(&QChannel::SetJitterBA, &QChannel::GetJitterBA),
              ns3::MakeTimeChecker())
          .AddAttribute(
              "QMap", "Symmetric channel map applied to both directions unless overridden.",
              ns3::PointerValue(), ns3::MakePointerAccessor(&QChannel::SetQMap, &QChannel::GetQMap),
              ns3::MakePointerChecker<QMap>())
          .AddAttribute("QMapAB", "One-way channel map for A->B.", ns3::PointerValue(),
                        ns3::MakePointerAccessor(&QChannel::SetQMapAB, &QChannel::GetQMapAB),
                        ns3::MakePointerChecker<QMap>())
          .AddAttribute("QMapBA", "One-way channel map for B->A.", ns3::PointerValue(),
                        ns3::MakePointerAccessor(&QChannel::SetQMapBA, &QChannel::GetQMapBA),
                        ns3::MakePointerChecker<QMap>());
  return tid;
}



QChannel::QChannel() {
  jitterRv_ = ns3::CreateObject<ns3::UniformRandomVariable>();
  mapRv_ = ns3::CreateObject<ns3::UniformRandomVariable>();
}



void QChannel::SetDelay(ns3::Time d) {
  if (d < ns3::Seconds(0)) {
    NS_LOG_WARN("SetDelay rejected: negative delay.");
    return;
  }

  delayAB_ = d;
  delayBA_ = d;
}



ns3::Time QChannel::GetDelay() const {
  if (delayAB_ != delayBA_) {
    NS_LOG_WARN("GetDelay returned A->B delay because the channel is asymmetric.");
  }

  return delayAB_;
}



void QChannel::SetJitter(ns3::Time j) {
  if (j < ns3::Seconds(0)) {
    NS_LOG_WARN("SetJitter rejected: negative jitter.");
    return;
  }

  jitterAB_ = j;
  jitterBA_ = j;
}



ns3::Time QChannel::GetJitter() const {
  if (jitterAB_ != jitterBA_) {
    NS_LOG_WARN("GetJitter returned A->B jitter because the channel is asymmetric.");
  }

  return jitterAB_;
}



void QChannel::SetQMap(ns3::Ptr<QMap> m) {
  if (!m) {
    NS_LOG_WARN("SetQMap received a null map; transmissions will use the identity map.");
  }

  qmapAB_ = m;
  qmapBA_ = m;
}



ns3::Ptr<QMap> QChannel::GetQMap() const {
  if (qmapAB_ != qmapBA_) {
    NS_LOG_WARN("GetQMap returned A->B map because the channel is asymmetric.");
  }

  return qmapAB_;
}



void QChannel::SetDelayAB(ns3::Time d) {
  if (d < ns3::Seconds(0)) {
    NS_LOG_WARN("SetDelayAB rejected: negative delay.");
    return;
  }

  delayAB_ = d;
}



ns3::Time QChannel::GetDelayAB() const {
  return delayAB_;
}



void QChannel::SetDelayBA(ns3::Time d) {
  if (d < ns3::Seconds(0)) {
    NS_LOG_WARN("SetDelayBA rejected: negative delay.");
    return;
  }

  delayBA_ = d;
}



ns3::Time QChannel::GetDelayBA() const {
  return delayBA_;
}



void QChannel::SetJitterAB(ns3::Time j) {
  if (j < ns3::Seconds(0)) {
    NS_LOG_WARN("SetJitterAB rejected: negative jitter.");
    return;
  }

  jitterAB_ = j;
}



ns3::Time QChannel::GetJitterAB() const {
  return jitterAB_;
}



void QChannel::SetJitterBA(ns3::Time j) {
  if (j < ns3::Seconds(0)) {
    NS_LOG_WARN("SetJitterBA rejected: negative jitter.");
    return;
  }

  jitterBA_ = j;
}



ns3::Time QChannel::GetJitterBA() const {
  return jitterBA_;
}



void QChannel::SetQMapAB(ns3::Ptr<QMap> m) {
  if (!m) {
    NS_LOG_WARN("SetQMapAB received a null map; transmissions will use the identity map.");
  }

  qmapAB_ = m;
}



ns3::Ptr<QMap> QChannel::GetQMapAB() const {
  return qmapAB_;
}



void QChannel::SetQMapBA(ns3::Ptr<QMap> m) {
  if (!m) {
    NS_LOG_WARN("SetQMapBA received a null map; transmissions will use the identity map.");
  }

  qmapBA_ = m;
}



ns3::Ptr<QMap> QChannel::GetQMapBA() const {
  return qmapBA_;
}



void QChannel::Connect(ns3::Ptr<QNetDevice> a, ns3::Ptr<QNetDevice> b) {
  if (!a || !b) {
    NS_LOG_WARN("Connect received a null endpoint device.");
  }

  if (a == b && a != nullptr) {
    NS_LOG_WARN("Connect received the same device for both endpoints.");
  }

  aDev_ = a;
  bDev_ = b;

  if (aDev_) {
    aDev_->AttachChannel(ns3::Ptr<QChannel>(this));
  }
  if (bDev_) {
    bDev_->AttachChannel(ns3::Ptr<QChannel>(this));
  }
}



bool QChannel::SendFrom(QNetDevice* src, std::shared_ptr<Qubit> q) {
  if (!src) {
    NS_LOG_WARN("SendFrom rejected: null source device.");
    return false;
  }

  if (!q) {
    NS_LOG_WARN("SendFrom rejected: null qubit.");
    return false;
  }

  const auto loc = q->GetLocation();
  if (loc.type == LocationType::Lost) {
    NS_LOG_WARN("SendFrom rejected: qubit " << q->GetQubitId() << " is lost.");
    return false;
  }

  ns3::Ptr<QNetDevice> dst;
  ns3::Time dly;
  ns3::Time jtr;
  ns3::Ptr<QMap> qmap;

  if (aDev_ && src == ns3::PeekPointer(aDev_)) {
    dst = bDev_;
    dly = delayAB_;
    jtr = jitterAB_;
    qmap = qmapAB_;
  } else if (bDev_ && src == ns3::PeekPointer(bDev_)) {
    dst = aDev_;
    dly = delayBA_;
    jtr = jitterBA_;
    qmap = qmapBA_;
  } else {
    NS_LOG_WARN("SendFrom rejected: source device is not attached to this channel.");
    return false;
  }

  if (!dst) {
    NS_LOG_WARN("SendFrom rejected: destination device is null.");
    return false;
  }

  // Sample per-send jitter and clamp the resulting propagation delay to zero.
  if (jtr > ns3::Time(0)) {
    const double j = jitterRv_->GetValue(-jtr.GetSeconds(), jtr.GetSeconds());
    dly += ns3::Seconds(j);
    if (dly < ns3::Time(0)) {
      dly = ns3::Time(0);
    }
  }

  // Sample the configured direction-specific QMap once for this transmission.
  QMapInstance mapInstance;
  if (qmap) {
    QMapContext ctx;
    ctx.elapsedTime = dly;
    mapInstance = qmap->Sample(mapRv_, ctx);
  }

  ns3::Simulator::Schedule(
      dly, [dst, q, mapInstance]() mutable { dst->ReceiveFromChannel(q, mapInstance); });

  return true;
}



std::size_t QChannel::GetNDevices() const {
  std::size_t n = 0;
  if (aDev_) {
    ++n;
  }
  if (bDev_) {
    ++n;
  }
  return n;
}



ns3::Ptr<ns3::NetDevice> QChannel::GetDevice(std::size_t i) const {
  if (i == 0) {
    return ns3::Ptr<ns3::NetDevice>(aDev_);
  }
  if (i == 1) {
    return ns3::Ptr<ns3::NetDevice>(bDev_);
  }
  return nullptr;
}



int64_t QChannel::AssignStreams(int64_t stream) {
  int64_t used = 0;

  if (jitterRv_) {
    jitterRv_->SetStream(stream + used++);
  }
  if (mapRv_) {
    mapRv_->SetStream(stream + used++);
  }

  return used;
}



} // namespace q2ns