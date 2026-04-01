/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/

#ifndef Q2NS_CONGESTION_HELPER_H
#define Q2NS_CONGESTION_HELPER_H

#include "ns3/data-rate.h"
#include "ns3/ipv4-address.h"
#include "ns3/node.h"
#include "ns3/object.h"
#include "ns3/ptr.h"

#include <set>
#include <string>
#include <utility>
#include <vector>

namespace q2ns {

// Describe one background flow from src -> dst
struct TrafficFlow {
  std::string src;             // node name (Names::Add)
  std::string dst;             // node name (Names::Add)
  std::string protocol{"udp"}; // "udp" | "tcp"
  std::string source{"onoff"}; // "onoff" | "bulk"   (Bulk only valid with TCP)
  uint16_t dstPort{9000};
  double rateMbps{1.0};      // used by OnOff (UDP or TCP). Bulk ignores this.
  uint32_t packetSize{1000}; // bytes (SendSize for Bulk; PacketSize for OnOff)
  double start_s{0.0};       // nominal start time
  double stop_s{0.0};        // 0 => never stop (until Simulator::Stop)
  std::string id;            // optional tag for your bookkeeping
};

struct TrafficSpec {
  std::vector<TrafficFlow> flows;
  bool autoCreateSinks{true}; // install PacketSink on dst if true
};

class CongestionHelper {
public:
  CongestionHelper() = default;

  // Install all flows described by 'spec'
  void Install(const TrafficSpec& spec);

  // Utility: compute a TCP warm-up time so cwnd can sustain app rate.
  // rttSec ~ 2 * one-way link delay; mssBytes = PacketSize; initCwndPkts ~10.
  static double ComputeTcpWarmup(double rateMbps, double rttSec, uint32_t mssBytes = 1200,
                                 double initCwndPkts = 10.0);

private:
  // utility
  static ns3::Ptr<ns3::Node> FindNodeByName(const std::string& name);
  static ns3::Ipv4Address FirstNonLoopback(ns3::Ptr<ns3::Node> node);

  // prevent duplicate sink installs: track (dstNode, port, proto)
  std::set<std::tuple<ns3::Ptr<ns3::Node>, uint16_t, std::string>> m_installedSinks;
};

} // namespace q2ns

#endif // Q2NS_CONGESTION_HELPER_H