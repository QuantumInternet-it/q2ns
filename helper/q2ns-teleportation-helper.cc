/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/

#include "ns3/q2ns-teleportation-helper.h"
#include "ns3/q2ns-teleportation-app.h"

#include "ns3/q2ns-netcontroller.h"
#include "ns3/q2ns-qchannel.h"
#include "ns3/q2ns-qnode.h"

#include "ns3/data-rate.h"
#include "ns3/double.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv4.h"
#include "ns3/log.h"
#include "ns3/names.h"
#include "ns3/node-container.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/pointer.h"
#include "ns3/queue-size.h"
#include "ns3/simulator.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"

#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace q2ns {

NS_LOG_COMPONENT_DEFINE("TeleportationHelper");

TeleportationHelper& TeleportationHelper::UseBackend(std::string backend) {
  m_backend = std::move(backend);
  return *this;
}

TeleportationHelper& TeleportationHelper::SetDefaultQueue(std::string max) {
  m_defaultQueue = std::move(max);
  return *this;
}

TeleportationHelper& TeleportationHelper::SetPortBase(uint16_t base) {
  m_portBase = base;
  return *this;
}

TeleportationHelper& TeleportationHelper::SetNetController(NetController* nc) {
  m_nc = nc;
  return *this;
}

TeleportationHelper&
TeleportationHelper::SetTeleportState(const std::shared_ptr<q2ns::QState>& tpl) {
  m_teleportState = tpl;
  return *this;
}

void TeleportationHelper::Install(const TopologySpec& spec) {
  NS_LOG_INFO("TeleportationHelper::Install start");

  if (!m_nc) {
    NS_LOG_ERROR("TeleportationHelper: NetController not set; call SetNetController(...) first.\n");
    throw std::runtime_error("TeleportationHelper requires NetController");
  }

  if (!m_teleportState) {
    NS_LOG_ERROR(
        "TeleportationHelper: Teleport state not set; call SetTeleportState(...) first.\n");
    throw std::runtime_error("TeleportationHelper requires TeleportState");
  }


  // 1) Nodes + Names
  BuildNodes(spec);

  // 2) Internet stack on all nodes
  ns3::InternetStackHelper internet;
  for (auto& kv : m_name2node) {
    internet.Install(kv.second);
  }

  // 3) Classical links (p2p), addressing, queues, background traffic
  BuildClassical(spec);

  // 4) Quantum links
  BuildQuantum(spec);

  // 5) Populate routing for multi-hop classical control & background
  ns3::Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // 6) Install per-session TeleportationApp instances
  InstallSessionApps(spec);

  NS_LOG_INFO("TeleportationHelper::Install done");
}

uint16_t TeleportationHelper::AllocatePort(uint64_t sessionId) const {
  uint16_t p = static_cast<uint16_t>(m_portBase + (sessionId % 10000ull));
  if (p == 0)
    p = m_portBase + 1;
  return p;
}

void TeleportationHelper::BuildNodes(const TopologySpec& spec) {
  NS_LOG_INFO("Building " << spec.nodes.size() << " nodes");

  for (const auto& n : spec.nodes) {
    if (m_name2node.count(n.name)) {
      NS_LOG_WARN("Duplicate node id '" << n.name << "'; skipping duplicate");
      continue;
    }

    // Create QNode through NetController (wires it to the shared registry)
    auto qnode = m_nc->CreateNode();               // Ptr<QNode>
    auto node = ns3::StaticCast<ns3::Node>(qnode); // Store as Ptr<Node> for Internet helpers
    m_name2node.emplace(n.name, node);
    ns3::Names::Add(n.name, node);
  }
}

static ns3::Ptr<ns3::Ipv4> GetIpv4(ns3::Ptr<ns3::Node> node) {
  return node->GetObject<ns3::Ipv4>();
}

static ns3::Ipv4Address GetFirstNonLoopback(ns3::Ptr<ns3::Node> node) {
  auto ipv4 = GetIpv4(node);
  if (!ipv4)
    return ns3::Ipv4Address::GetAny();

  for (uint32_t i = 0; i < ipv4->GetNInterfaces(); ++i) {
    for (uint32_t j = 0; j < ipv4->GetNAddresses(i); ++j) {
      auto ifaddr = ipv4->GetAddress(i, j);
      auto addr = ifaddr.GetLocal();
      if (addr != ns3::Ipv4Address("127.0.0.1") && addr != ns3::Ipv4Address::GetZero()) {
        return addr;
      }
    }
  }
  return ns3::Ipv4Address::GetAny(); // fallback; routing should still work by dest
}

void TeleportationHelper::BuildClassical(const TopologySpec& spec) {
  NS_LOG_INFO("Building " << spec.classicalEdges.size() << " classical links");

  // We'll hand out /30 subnets per link: 10.0.X.Y/30
  ns3::Ipv4AddressHelper ip;
  uint32_t netId = 0; // increment per link

  for (const auto& e : spec.classicalEdges) {
    auto itU = m_name2node.find(e.u);
    auto itV = m_name2node.find(e.v);
    if (itU == m_name2node.end() || itV == m_name2node.end()) {
      NS_LOG_ERROR("Classical edge refers to unknown nodes: " << e.u << " - " << e.v);
      continue;
    }

    ns3::NodeContainer pair(itU->second, itV->second);

    // Point-to-point with per-edge DataRate and Delay (distance * delayNsPerKm)
    ns3::PointToPointHelper p2p;
    std::ostringstream dr;
    dr << e.mbps << "Mbps";
    p2p.SetDeviceAttribute("DataRate", ns3::StringValue(dr.str()));

    double delayNs = e.delayNsPerKm * e.distanceKm;
    std::ostringstream dl;
    dl << delayNs << "ns";
    p2p.SetChannelAttribute("Delay", ns3::StringValue(dl.str()));

    // TxQueue MaxSize (DropTail) -- per link; falls back to helper default
    std::string qmax = e.queueMax.empty() ? m_defaultQueue : e.queueMax;
    p2p.SetQueue("ns3::DropTailQueue<Packet>", "MaxSize",
                 ns3::QueueSizeValue(ns3::QueueSize(qmax)));

    auto devices = p2p.Install(pair);

    // Address this link with its own /30
    // 10.(netId/256).(netId%256).0/30
    uint8_t a = static_cast<uint8_t>((netId >> 8) & 0xFF);
    uint8_t b = static_cast<uint8_t>(netId & 0xFF);
    std::ostringstream base;
    base << "10." << unsigned(a) << "." << unsigned(b) << ".0";
    ip.SetBase(ns3::Ipv4Address(base.str().c_str()), "255.255.255.252");
    ip.Assign(devices);

    ++netId;
  }
}



void TeleportationHelper::BuildQuantum(const TopologySpec& spec) {
  NS_LOG_INFO("Quantum edges: " << spec.quantumEdges.size());
  for (const auto& e : spec.quantumEdges) {
    auto itU = m_name2node.find(e.u);
    auto itV = m_name2node.find(e.v);
    if (itU == m_name2node.end() || itV == m_name2node.end()) {
      NS_LOG_WARN("Quantum edge refers to unknown nodes: " << e.u << " - " << e.v << "\n");
      continue;
    }
    auto a = ns3::DynamicCast<q2ns::QNode>(itU->second);
    auto b = ns3::DynamicCast<q2ns::QNode>(itV->second);
    if (!a || !b) {
      NS_LOG_ERROR("Quantum edge endpoints are not QNodes: " << e.u << " - " << e.v << "\n");
      continue;
    }
    const double delayNs = e.delayNsPerKm * e.distanceKm;
    ns3::Time oneWay = ns3::NanoSeconds(static_cast<uint64_t>(std::max(0.0, delayNs)));
    auto ch = m_nc->InstallQuantumLink(a, b);
    ch->SetAttribute("Delay", ns3::TimeValue(oneWay));
  }
}


void TeleportationHelper::InstallSessionApps(const TopologySpec& spec) {
  NS_LOG_INFO("Installing " << spec.sessions.size() << " teleportation sessions");

  // Track used (dstNode, port) to warn on user-specified conflicts
  std::unordered_set<uint64_t> usedPortKey;
  auto keyOf = [&](ns3::Ptr<ns3::Node> n, uint16_t p) -> uint64_t {
    return (static_cast<uint64_t>(n->GetId()) << 32) | static_cast<uint64_t>(p);
  };

  for (const auto& s : spec.sessions) {
    if (!m_name2node.count(s.src) || !m_name2node.count(s.dst)) {
      NS_LOG_ERROR("Session refers to unknown nodes: " << s.src << " -> " << s.dst);
      continue;
    }
    auto srcNode = m_name2node.at(s.src);
    auto dstNode = m_name2node.at(s.dst);
    auto dstQ = ns3::DynamicCast<q2ns::QNode>(dstNode);

    // Classical address of Bob (dst) -- choose first non-loopback; routing handles path
    ns3::Ipv4Address dstAddr = GetFirstNonLoopback(dstNode);
    if (dstAddr == ns3::Ipv4Address::GetAny()) {
      NS_LOG_WARN("Destination node " << s.dst << " has no IPv4 address yet");
    }

    // Control port per session (auto if 0)
    uint16_t ctrlPort = s.ctrlPort ? s.ctrlPort : AllocatePort(s.sessionId);
    if (s.ctrlPort) {
      uint64_t k = keyOf(dstNode, ctrlPort);
      if (usedPortKey.count(k)) {
        NS_LOG_WARN("Two sessions target dst=" << s.dst << " with the same CtrlPort=" << ctrlPort
                                               << " -- PacketSink bind conflict likely.");
      } else {
        usedPortKey.insert(k);
      }
    }

    // Protocol preference: per-session override if provided, else try to infer from first edge
    std::string proto = s.protocol;
    if (proto.empty()) {
      // Fallback: use "udp" unless a matching classical edge was declared "tcp"
      proto = "udp";
      for (const auto& e : spec.classicalEdges) {
        if ((e.u == s.src && e.v == s.dst) || (e.u == s.dst && e.v == s.src)) {
          proto = e.protocol;
          break;
        }
      }
    }

    // Backend for both apps
    std::string backend = spec.backend.empty() ? m_backend : spec.backend;

    // Session-unique qubit tag on Bob
    std::ostringstream tag;
    tag << "teleport_target_s" << s.sessionId;

    // Source (Alice)
    {
      auto app = ns3::CreateObject<TeleportationApp>();
      srcNode->AddApplication(app);
      app->SetAttribute("Role", ns3::StringValue("source"));
      app->SetAttribute("Peer", ns3::Ipv4AddressValue(dstAddr));
      app->SetAttribute("CtrlPort", ns3::UintegerValue(ctrlPort));
      app->SetAttribute("ClassicalProtocol", ns3::StringValue(proto));
      app->SetAttribute("Backend", ns3::StringValue(backend));
      app->SetAttribute("TargetQubitTag", ns3::StringValue(tag.str()));
      app->SetAttribute("SessionId", ns3::UintegerValue(s.sessionId));
      app->SetAttribute("SessionStartTime", ns3::TimeValue(ns3::Seconds(s.start_s)));
      if (dstQ) {
        app->SetAttribute("PeerQNode", ns3::PointerValue(dstQ));
      }
      app->SetNetController(m_nc); // NetController so the app can schedule sends / resolve states
      if (m_teleportState) {
        app->SetTeleportState(m_teleportState);
      }
      app->SetStartTime(ns3::Seconds(0.0)); // app schedules its own SessionStart
      app->SetStopTime(ns3::Seconds(1e9));  // effectively forever unless user stops earlier
    }

    // Sink (Bob)
    {
      auto app = ns3::CreateObject<TeleportationApp>();
      dstNode->AddApplication(app);
      app->SetAttribute("Role", ns3::StringValue("sink"));
      app->SetAttribute("Peer",
                        ns3::Ipv4AddressValue(ns3::Ipv4Address::GetAny())); // unused on sink
      app->SetAttribute("CtrlPort", ns3::UintegerValue(ctrlPort));
      app->SetAttribute("ClassicalProtocol", ns3::StringValue(proto));
      app->SetAttribute("Backend", ns3::StringValue(backend));
      app->SetAttribute("TargetQubitTag", ns3::StringValue(tag.str()));
      app->SetAttribute("SessionId", ns3::UintegerValue(s.sessionId));
      app->SetAttribute("SessionStartTime", ns3::TimeValue(ns3::Seconds(s.start_s)));
      app->SetNetController(m_nc);
      if (m_teleportState) {
        app->SetTeleportState(m_teleportState);
      }
      app->SetStartTime(ns3::Seconds(0.0));
      app->SetStopTime(ns3::Seconds(1e9));
    }
  }
}

} // namespace q2ns
