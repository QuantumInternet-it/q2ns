/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/

#include "ns3/q2ns-congestion-helper.h"

#include "ns3/inet-socket-address.h"
#include "ns3/ipv4.h"
#include "ns3/log.h"
#include "ns3/names.h"
#include "ns3/node-list.h"
#include "ns3/node.h"

#include "ns3/application-container.h"
#include "ns3/bulk-send-helper.h"
#include "ns3/data-rate.h"
#include "ns3/on-off-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/simulator.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>

namespace q2ns {

NS_LOG_COMPONENT_DEFINE("CongestionHelper");

ns3::Ptr<ns3::Node> CongestionHelper::FindNodeByName(const std::string& name) {
  if (name.empty())
    return nullptr;
  return ns3::Names::Find<ns3::Node>(name);
}

ns3::Ipv4Address CongestionHelper::FirstNonLoopback(ns3::Ptr<ns3::Node> node) {
  auto ipv4 = node->GetObject<ns3::Ipv4>();
  if (!ipv4)
    return ns3::Ipv4Address::GetAny();
  for (uint32_t i = 0; i < ipv4->GetNInterfaces(); ++i) {
    for (uint32_t j = 0; j < ipv4->GetNAddresses(i); ++j) {
      auto ifaddr = ipv4->GetAddress(i, j).GetLocal();
      if (ifaddr != ns3::Ipv4Address("127.0.0.1") && ifaddr != ns3::Ipv4Address::GetZero()) {
        return ifaddr;
      }
    }
  }
  return ns3::Ipv4Address::GetAny();
}

double CongestionHelper::ComputeTcpWarmup(double rateMbps, double rttSec, uint32_t mssBytes,
                                          double initCwndPkts) {
  // cwnd_needed (pkts) = (rate * RTT) / MSS
  double cwndNeeded = (rateMbps * 1e6 * rttSec) / (double(mssBytes) * 8.0);
  if (cwndNeeded <= 0.0)
    return 0.0;
  double nDoublings = std::ceil(std::log2(std::max(1.0, cwndNeeded / initCwndPkts)));
  // Cushion of 3 RTTs for ACK-clock settling
  return (nDoublings + 3.0) * rttSec;
}

void CongestionHelper::Install(const TrafficSpec& spec) {
  using namespace ns3;
  NS_LOG_INFO("Installing " << spec.flows.size() << " congestion flows");

  for (const auto& fraw : spec.flows) {
    // Lowercase normalized copies
    TrafficFlow f = fraw;
    std::string proto = f.protocol;
    std::string srcKind = f.source;
    std::transform(proto.begin(), proto.end(), proto.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    std::transform(srcKind.begin(), srcKind.end(), srcKind.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    // Validate knobs
    if (proto != "udp" && proto != "tcp") {
      NS_LOG_WARN("Unknown protocol '" << f.protocol << "'; defaulting to UDP");
      proto = "udp";
    }
    if (srcKind != "onoff" && srcKind != "bulk") {
      NS_LOG_WARN("Unknown source '" << f.source << "'; defaulting to onoff");
      srcKind = "onoff";
    }
    if (proto == "udp" && srcKind == "bulk") {
      NS_LOG_WARN("UDP does not support BulkSend; falling back to OnOff for flow to port "
                  << f.dstPort);
      srcKind = "onoff";
    }

    auto srcNode = FindNodeByName(f.src);
    auto dstNode = FindNodeByName(f.dst);
    if (!srcNode || !dstNode) {
      NS_LOG_ERROR("TrafficFlow refers to unknown nodes: " << f.src << " -> " << f.dst);
      continue;
    }

    // Install sink once per (dst,port,proto)
    if (spec.autoCreateSinks) {
      auto key = std::make_tuple(dstNode, f.dstPort, proto);
      if (!m_installedSinks.count(key)) {
        Address bind = InetSocketAddress(Ipv4Address::GetAny(), f.dstPort);
        if (proto == "tcp") {
          PacketSinkHelper sink("ns3::TcpSocketFactory", bind);
          sink.Install(dstNode);
        } else {
          PacketSinkHelper sink("ns3::UdpSocketFactory", bind);
          sink.Install(dstNode);
        }
        m_installedSinks.insert(key);
      }
    }

    // Sender address/target
    Ipv4Address dstAddr = FirstNonLoopback(dstNode);
    InetSocketAddress dst(dstAddr, f.dstPort);

    // Compute start/stop (start includes warm-up offset)
    Time tStart = Seconds(std::max(0.0, f.start_s));
    Time tStop = (f.stop_s > 0.0) ? Seconds(f.stop_s) : Time(); // 0 => run until sim stop

    if (srcKind == "onoff") {
      // App-paced traffic for either UDP or TCP
      std::ostringstream rate;
      rate << f.rateMbps << "Mbps";
      const char* factory = (proto == "tcp") ? "ns3::TcpSocketFactory" : "ns3::UdpSocketFactory";
      OnOffHelper onoff(factory, dst);
      onoff.SetAttribute("DataRate", StringValue(rate.str()));
      onoff.SetAttribute("PacketSize", UintegerValue(f.packetSize));
      onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
      onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

      auto apps = onoff.Install(srcNode);
      apps.Start(tStart);
      if (f.stop_s > 0.0)
        apps.Stop(tStop);

      NS_LOG_INFO("Installed OnOff "
                  << proto << " " << f.src << " -> " << f.dst << ":" << f.dstPort
                  << " rate=" << f.rateMbps << "Mbps pkt=" << f.packetSize
                  << " start=" << tStart.GetSeconds() << "s"
                  << (f.stop_s > 0.0 ? (" stop=" + std::to_string(tStop.GetSeconds()) + "s") : ""));

    } else { // "bulk" (TCP only)
      BulkSendHelper bulk("ns3::TcpSocketFactory", dst);
      bulk.SetAttribute("MaxBytes", UintegerValue(0));            // sustained firehose
      bulk.SetAttribute("SendSize", UintegerValue(f.packetSize)); // segment size hint

      auto apps = bulk.Install(srcNode);
      apps.Start(tStart);
      if (f.stop_s > 0.0)
        apps.Stop(tStop);

      NS_LOG_INFO("Installed BulkSend TCP "
                  << f.src << " -> " << f.dst << ":" << f.dstPort << " sendSize=" << f.packetSize
                  << " start=" << tStart.GetSeconds() << "s"
                  << (f.stop_s > 0.0 ? (" stop=" + std::to_string(tStop.GetSeconds()) + "s") : "")
                  << " (ignores rateMbps by design)");
    }
  }
}

} // namespace q2ns