/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/

#include "ns3/q2ns-swap-helper.h"
#include "ns3/q2ns-netcontroller.h"
#include "ns3/q2ns-qnode.h"
#include "ns3/q2ns-swap-app.h"

#include "ns3/log.h"
#include "ns3/names.h"
#include "ns3/random-variable-stream.h"

#include <algorithm>
#include <sstream>

namespace q2ns {

NS_LOG_COMPONENT_DEFINE("SwapAppHelper");

std::vector<SwapSessionSpec> AllAtOncePolicy::PlanSessions(const SwapTopologySpec& topo) {
  std::vector<SwapSessionSpec> out;
  out.reserve(topo.sessions.size() * std::max(1u, topo.rounds.rounds));

  // RNG for jitter (ns-3): interpret topo.rounds.seed as a stream index
  auto rvJitter = ns3::CreateObject<ns3::UniformRandomVariable>();
  rvJitter->SetStream(static_cast<int64_t>(topo.rounds.seed));

  if (!topo.rounds.Enabled()) {
    out = topo.sessions;
    return out;
  }

  // Replicate sessions across rounds, offsetting start times
  for (uint32_t r = 0; r < topo.rounds.rounds; ++r) {
    for (const auto& s : topo.sessions) {
      auto copy = s;
      const double jitter = topo.rounds.jitter_s > 0.0
                                ? rvJitter->GetValue(-topo.rounds.jitter_s, topo.rounds.jitter_s)
                                : 0.0;
      copy.start_s = s.start_s + r * topo.rounds.period_s + jitter;
      // Optionally remap session IDs per round to keep unique: sid = base + r
      // Here we keep the user-provided sessionId; user should ensure uniqueness across rounds.
      out.emplace_back(std::move(copy));
    }
  }
  return out;
}

EntanglementSwapHelper& EntanglementSwapHelper::SetNetController(NetController* nc) {
  m_nc = nc;
  return *this;
}
EntanglementSwapHelper& EntanglementSwapHelper::SetPortBase(uint16_t base) {
  m_portBase = base;
  return *this;
}
EntanglementSwapHelper& EntanglementSwapHelper::SetPolicy(std::unique_ptr<ISwapPolicy> policy) {
  m_policy = std::move(policy);
  return *this;
}


void EntanglementSwapHelper::Install(const SwapTopologySpec& spec,
                                     const ns3::ClassicalNetworkBuilder::NetworkHandle& net,
                                     bool useIpv6, uint32_t ctrlPayloadBytes) {
  NS_ASSERT_MSG(m_nc != nullptr, "EntanglementSwapHelper requires NetController");

  BindAppsOnProvidedNodes(spec);
  BuildQuantum(spec);

  std::unique_ptr<ISwapPolicy> localDefault;
  ISwapPolicy* policy = m_policy.get();
  if (!policy) {
    localDefault = std::make_unique<AllAtOncePolicy>();
    policy = localDefault.get();
  }
  auto planned = policy->PlanSessions(spec);

  InstallUnifiedApps(spec, planned, net, useIpv6, ctrlPayloadBytes);

  NS_LOG_INFO("SwapHelper: installation done (" << planned.size() << " sessions).");
}



uint16_t EntanglementSwapHelper::AllocatePort(uint64_t sessionId) const {
  uint16_t p = static_cast<uint16_t>(m_portBase + (sessionId % 10000ull));
  if (p == 0)
    p = m_portBase + 1;
  return p;
}

EntanglementSwapHelper&
EntanglementSwapHelper::SetNodes(const std::map<std::string, ns3::Ptr<q2ns::QNode>>& nodes) {
  m_name2node.clear();
  for (const auto& kv : nodes) {
    m_name2node.emplace(kv.first, kv.second);
  }
  return *this;
}

void EntanglementSwapHelper::BindAppsOnProvidedNodes(const SwapTopologySpec& spec) {
  NS_ASSERT_MSG(!m_name2node.empty(), "SetNodes(...) must be called before Install().");
  m_appByNode.clear();
  for (const auto& name : spec.nodes) {
    auto it = m_name2node.find(name);
    NS_ASSERT_MSG(it != m_name2node.end(), "Missing QNode for '" << name << "' in SetNodes.");
    ns3::Ptr<q2ns::QNode> qn = it->second;

    // Create and attach one SwapApp per node (QNode derives from ns3::Node, so this is fine)
    ns3::Ptr<SwapApp> app = ns3::CreateObject<SwapApp>();
    qn->AddApplication(app);
    app->SetNetController(m_nc);
    app->SetStartTime(ns3::Seconds(0.0));
    app->SetStopTime(ns3::Seconds(1e9));
    m_appByNode.emplace(name, app);
  }
}



void EntanglementSwapHelper::BuildQuantum(const SwapTopologySpec& spec) {
  NS_LOG_INFO("Quantum edges: " << spec.quantumEdges.size());
  for (const auto& e : spec.quantumEdges) {
    auto a = ns3::DynamicCast<q2ns::QNode>(m_name2node.at(e.u));
    auto b = ns3::DynamicCast<q2ns::QNode>(m_name2node.at(e.v));
    const double delayNs = e.delayNsPerKm * e.distanceKm;
    auto ch = m_nc->InstallQuantumLink(a, b);
    ch->SetAttribute(
        "Delay", ns3::TimeValue(ns3::NanoSeconds(static_cast<uint64_t>(std::max(0.0, delayNs)))));
  }
}


void EntanglementSwapHelper::InstallUnifiedApps(
    const SwapTopologySpec& spec, const std::vector<SwapSessionSpec>& planned,
    const ns3::ClassicalNetworkBuilder::NetworkHandle& net, bool useIpv6,
    uint32_t ctrlPayloadBytes) {
  using ns3::Seconds;

  auto getV4 = [&](const std::string& name) -> ns3::Ipv4Address {
    auto it = net.nodes.find(name);
    if (it == net.nodes.end() || it->second.v4Addrs.empty())
      return ns3::Ipv4Address("0.0.0.0");
    return it->second.v4Addrs.front();
  };
  auto getV6 = [&](const std::string& name) -> ns3::Ipv6Address {
    auto it = net.nodes.find(name);
    if (it == net.nodes.end() || it->second.v6Addrs.empty())
      return ns3::Ipv6Address::GetAny();
    return it->second.v6Addrs.front();
  };

  for (auto& kv : m_appByNode) {
    kv.second->SetPayloadBytes(ctrlPayloadBytes);
    kv.second->SetUseIpv6(useIpv6);
  }

  for (const auto& sess : planned) {
    if (sess.path.size() < 2)
      continue;

    const uint16_t port = sess.ctrlPort ? sess.ctrlPort : AllocatePort(sess.sessionId);
    const auto& Aname = sess.path.front();
    const auto& Bname = sess.path.back();

    const auto addrA_v4 = getV4(Aname);
    const auto addrB_v4 = getV4(Bname);
    const auto addrA_v6 = getV6(Aname);
    const auto addrB_v6 = getV6(Bname);

    const uint32_t kRepeaters =
        sess.path.size() >= 2 ? static_cast<uint32_t>(sess.path.size() - 2) : 0;

    // Endpoints: Source (Prev)
    {
      auto appA = m_appByNode.at(Aname);
      SwapApp::SessionConfig cfg{};
      cfg.sid = sess.sessionId;
      cfg.role = SwapApp::Role::Prev;
      cfg.applyCorrections = false;
      cfg.start = Seconds(sess.start_s);
      cfg.proto = sess.protocol;
      cfg.ctrlPort = port;
      cfg.expectedMsgs = kRepeaters;
      cfg.verifyFidelity = true;
      cfg.verifyThreshold = 0.99;

      // Orient the chain
      auto nextNode = ns3::DynamicCast<q2ns::QNode>(m_name2node.at(sess.path[1]));
      cfg.genNext = true;
      cfg.nextPeerId = nextNode->GetId();

      appA->AddSession(cfg);
    }

    // Endpoints: Sink (Next)
    {
      auto appB = m_appByNode.at(Bname);
      SwapApp::SessionConfig cfg{};
      cfg.sid = sess.sessionId;
      cfg.role = SwapApp::Role::Next;
      cfg.applyCorrections = true;
      cfg.start = Seconds(sess.start_s);
      cfg.proto = sess.protocol;
      cfg.ctrlPort = port;
      cfg.expectedMsgs = kRepeaters;
      cfg.verifyFidelity = true;
      cfg.verifyThreshold = 0.99;
      appB->AddSession(cfg);
    }

    // Repeaters along path: give them A/B endpoint addresses (v4 or v6)
    for (size_t i = 1; i + 1 < sess.path.size(); ++i) {
      auto Rname = sess.path[i];
      auto appR = m_appByNode.at(Rname);

      auto prevNode = ns3::DynamicCast<q2ns::QNode>(m_name2node.at(sess.path[i - 1]));
      auto nextNode = ns3::DynamicCast<q2ns::QNode>(m_name2node.at(sess.path[i + 1]));

      SwapApp::SessionConfig cfg{};
      cfg.sid = sess.sessionId;
      cfg.role = SwapApp::Role::Repeater;
      cfg.applyCorrections = false;
      cfg.start = Seconds(sess.start_s);
      cfg.proto = sess.protocol;
      cfg.ctrlPort = port;
      cfg.prevPeerId = prevNode->GetId();
      cfg.nextPeerId = nextNode->GetId();
      cfg.genNext = true;

      if (useIpv6) {
        cfg.prevEndAddr6 = addrA_v6;
        cfg.nextEndAddr6 = addrB_v6;
      } else {
        cfg.prevEndAddr = addrA_v4;
        cfg.nextEndAddr = addrB_v4;
      }

      appR->AddSession(cfg);
    }
  }
}

} // namespace q2ns