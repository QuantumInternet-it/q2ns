/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/

#include "ns3/q2ns-classical-network-builder.h"

#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ripng-helper.h"

#include <sstream>

using namespace ns3;


ClassicalNetworkBuilder::ClassicalNetworkBuilder() {}

void ClassicalNetworkBuilder::SetIpVersion(IpVersion v) {
  m_ipVersion = v;
}
void ClassicalNetworkBuilder::SetRouting(Routing r) {
  m_routing = r;
}
void ClassicalNetworkBuilder::AddLink(const Link& link) {
  m_links.push_back(link);
}
void ClassicalNetworkBuilder::AddLan(const Lan& lan) {
  m_lans.push_back(lan);
}
void ClassicalNetworkBuilder::SetDefaultDataRate(std::string r) {
  m_defRate = r;
}
void ClassicalNetworkBuilder::SetDefaultDelay(std::string d) {
  m_defDelay = d;
}
void ClassicalNetworkBuilder::AttachNode(std::string name, Ptr<Node> node) {
  m_nodes[name] = node;
}

void ClassicalNetworkBuilder::InstallInternetStacks() {
  bool v4 = (m_ipVersion == IpVersion::V4 || m_ipVersion == IpVersion::Dual);
  bool v6 = (m_ipVersion == IpVersion::V6 || m_ipVersion == IpVersion::Dual);

  NodeContainer all;
  for (auto& kv : m_nodes)
    all.Add(kv.second);

  if (m_routing == Routing::Ripng && v6) {
    InternetStackHelper stack;
    stack.SetIpv4StackInstall(v4);
    stack.SetIpv6StackInstall(v6);

    Ipv6ListRoutingHelper list6;
    RipNgHelper rip6;
    // Optional: faster convergence, lower periodic updates if you want:
    // rip6.Set("LinkDown", BooleanValue(false));

    // priority 0 = highest
    list6.Add(rip6, 0);
    // We can still add static/v6 as a backup with lower priority if desired:
    // Ipv6StaticRoutingHelper v6static; list6.Add(v6static, 10);

    // Build a combined InternetStack with the custom v6 list routing
    InternetStackHelper custom;
    custom.SetIpv4StackInstall(v4);
    custom.SetIpv6StackInstall(v6);
    custom.SetRoutingHelper(list6);
    custom.Install(all);
    return;
  }

  // Default (Global/StaticShortestPaths/None): vanilla stack
  InternetStackHelper stack;
  stack.SetIpv4StackInstall(v4);
  stack.SetIpv6StackInstall(v6);
  stack.Install(all);
}


ClassicalNetworkBuilder::NetworkHandle ClassicalNetworkBuilder::Build() {
  NS_ASSERT(!m_nodes.empty());
  InstallInternetStacks();

  NetworkHandle handle;
  handle.hasIpv4 = (m_ipVersion == IpVersion::V4 || m_ipVersion == IpVersion::Dual);
  handle.hasIpv6 = (m_ipVersion == IpVersion::V6 || m_ipVersion == IpVersion::Dual);

  // Initialize node entries
  for (auto& kv : m_nodes)
    handle.nodes[kv.first].node = kv.second;

  BuildLinks(handle);
  BuildLans(handle);
  AssignAddresses(handle);

  // Optional routing
  if (m_routing == Routing::Global) {
    if (handle.hasIpv4)
      Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    // v6: on-link ND only; multi-hop v6 won't work in this mode
  } else if (m_routing == Routing::StaticShortestPaths) {
    InstallStaticShortestPaths(handle); // v4+v6 static (kept)
  } else if (m_routing == Routing::Ripng) {
    // nothing to call: RIPng learns routes during sim time
  }


  return handle;
}

void ClassicalNetworkBuilder::BuildLinks(NetworkHandle& handle) {
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue(m_defRate));
  p2p.SetChannelAttribute("Delay", StringValue(m_defDelay));

  Ipv4AddressHelper v4h;
  v4h.SetBase("10.1.0.0", "255.255.0.0"); // /16: ~65k addresses
  Ipv6AddressHelper v6h;
  v6h.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));

  for (const auto& l : m_links) {
    Ptr<Node> A = m_nodes.at(l.nodeA);
    Ptr<Node> B = m_nodes.at(l.nodeB);
    NetDeviceContainer devs = p2p.Install(A, B);

    auto& aEntry = handle.nodes[l.nodeA];
    auto& bEntry = handle.nodes[l.nodeB];
    aEntry.devices.push_back(devs.Get(0));
    bEntry.devices.push_back(devs.Get(1));
    aEntry.devBySegment[l.id] = devs.Get(0);
    bEntry.devBySegment[l.id] = devs.Get(1);

    Ipv4InterfaceContainer if4;
    Ipv6InterfaceContainer if6;

    if (handle.hasIpv4) {
      v4h.NewNetwork();
      if4 = v4h.Assign(devs);
      aEntry.v4Addrs.push_back(if4.GetAddress(0));
      bEntry.v4Addrs.push_back(if4.GetAddress(1));
    }
    if (handle.hasIpv6) {
      v6h.NewNetwork();
      if6 = v6h.Assign(devs);
      if6.SetForwarding(0, true);
      if6.SetForwarding(1, true);
      if (m_routing == Routing::Global) {
        if6.SetDefaultRouteInAllNodes(0);
        if6.SetDefaultRouteInAllNodes(1);
      }
      aEntry.v6Addrs.push_back(if6.GetAddress(0, 1));
      bEntry.v6Addrs.push_back(if6.GetAddress(1, 1));
    }

    // segIf cache
    auto& ai = aEntry.segIf[l.id];
    auto& bi = bEntry.segIf[l.id];
    if (handle.hasIpv4) {
      ai.ifIndexV4 = A->GetObject<Ipv4>()->GetInterfaceForDevice(devs.Get(0));
      bi.ifIndexV4 = B->GetObject<Ipv4>()->GetInterfaceForDevice(devs.Get(1));
      ai.v4 = if4.GetAddress(0);
      bi.v4 = if4.GetAddress(1);
    }
    if (handle.hasIpv6) {
      ai.ifIndexV6 = A->GetObject<Ipv6>()->GetInterfaceForDevice(devs.Get(0));
      bi.ifIndexV6 = B->GetObject<Ipv6>()->GetInterfaceForDevice(devs.Get(1));
      ai.v6 = if6.GetAddress(0, 1);   // global
      bi.v6 = if6.GetAddress(1, 1);   // global
      ai.v6ll = if6.GetAddress(0, 0); // link-local
      bi.v6ll = if6.GetAddress(1, 0); // link-local
    }
  }
}

void ClassicalNetworkBuilder::BuildLans(NetworkHandle& handle) {
  for (const auto& lan : m_lans) {
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue(lan.rate.empty() ? m_defRate : lan.rate));
    csma.SetChannelAttribute("Delay", StringValue(lan.delay.empty() ? m_defDelay : lan.delay));

    NodeContainer members;
    for (auto& name : lan.members)
      members.Add(m_nodes.at(name));

    NetDeviceContainer devs = csma.Install(members);

    Ipv4AddressHelper v4h;
    v4h.SetBase("10.1.0.0", "255.255.0.0"); // /16: ~65k addresses
    Ipv6AddressHelper v6h;
    v6h.SetBase(Ipv6Address("2001:db8:1::"), Ipv6Prefix(64));

    Ipv4InterfaceContainer if4;
    Ipv6InterfaceContainer if6;

    if (handle.hasIpv4) {
      if4 = v4h.Assign(devs);
      for (uint32_t i = 0; i < members.GetN(); ++i) {
        auto name = lan.members[i];
        handle.nodes[name].devices.push_back(devs.Get(i));
        handle.nodes[name].v4Addrs.push_back(if4.GetAddress(i));
        handle.nodes[name].ifaceIndex.push_back(
            members.Get(i)->GetObject<Ipv4>()->GetInterfaceForDevice(devs.Get(i)));
        handle.nodes[name].devBySegment[lan.id] = devs.Get(i);

        auto& info4 = handle.nodes[name].segIf[lan.id];
        info4.ifIndexV4 = members.Get(i)->GetObject<Ipv4>()->GetInterfaceForDevice(devs.Get(i));
        info4.v4 = if4.GetAddress(i);
      }
    }
    if (handle.hasIpv6) {
      if6 = v6h.Assign(devs);
      for (uint32_t i = 0; i < members.GetN(); ++i) {
        auto name = lan.members[i];
        if6.SetForwarding(i, true);
        if (m_routing == Routing::Global) {
          if6.SetDefaultRouteInAllNodes(i);
        }
        handle.nodes[name].devices.push_back(devs.Get(i));
        handle.nodes[name].v6Addrs.push_back(if6.GetAddress(i, 1));
        handle.nodes[name].ifaceIndex.push_back(
            members.Get(i)->GetObject<Ipv6>()->GetInterfaceForDevice(devs.Get(i)));
        handle.nodes[name].devBySegment[lan.id] = devs.Get(i);

        auto& info6 = handle.nodes[name].segIf[lan.id];
        info6.ifIndexV6 = members.Get(i)->GetObject<Ipv6>()->GetInterfaceForDevice(devs.Get(i));
        info6.v6 = if6.GetAddress(i, 1);   // global
        info6.v6ll = if6.GetAddress(i, 0); // link-local
      }
    }
  }
}

void ClassicalNetworkBuilder::InstallStaticShortestPaths(NetworkHandle& h) {
  // Build segment -> members map
  std::map<std::string, std::vector<std::string>> segMembers;
  for (const auto& [name, entry] : h.nodes)
    for (const auto& kv : entry.segIf)
      segMembers[kv.first].push_back(name);

  // Build adjacency (node-name graph)
  std::map<std::string, std::vector<std::string>> adj;
  for (const auto& [seg, members] : segMembers) {
    if (members.size() < 2)
      continue;
    if (members.size() == 2) {
      adj[members[0]].push_back(members[1]);
      adj[members[1]].push_back(members[0]);
    } else {
      for (size_t i = 0; i < members.size(); ++i)
        for (size_t j = i + 1; j < members.size(); ++j) {
          adj[members[i]].push_back(members[j]);
          adj[members[j]].push_back(members[i]);
        }
    }
  }

  Ipv4StaticRoutingHelper v4h;
  Ipv6StaticRoutingHelper v6h;

  for (const auto& [srcName, srcEntry] : h.nodes) {
    // BFS from src
    std::unordered_map<std::string, std::string> parent;
    std::queue<std::string> q;
    parent[srcName] = "";
    q.push(srcName);

    while (!q.empty()) {
      std::string u = q.front();
      q.pop();
      for (const auto& v : adj[u]) {
        if (!parent.count(v)) {
          parent[v] = u;
          q.push(v);
        }
      }
    }

    auto firstHop = [&](const std::string& dst) -> std::string {
      if (dst == srcName)
        return srcName;
      if (!parent.count(dst))
        return {};
      std::string cur = dst, prev = parent.at(cur);
      while (!prev.empty() && prev != srcName) {
        cur = prev;
        prev = parent.at(cur);
      }
      return prev.empty() ? std::string() : cur;
    };

    // Program host routes to every other node
    for (const auto& [dstName, dstEntry] : h.nodes) {
      if (dstName == srcName)
        continue;
      std::string nhName = firstHop(dstName);
      if (nhName.empty())
        continue;

      // Find shared segment between src and next-hop
      std::string segId;
      for (const auto& kv : srcEntry.segIf) {
        if (h.nodes.at(nhName).segIf.count(kv.first)) {
          segId = kv.first;
          break;
        }
      }
      if (segId.empty())
        continue;

      const auto& meSeg = srcEntry.segIf.at(segId);
      const auto& nhSeg = h.nodes.at(nhName).segIf.at(segId);

      if (h.hasIpv4 && !dstEntry.v4Addrs.empty() && meSeg.ifIndexV4 != UINT32_MAX &&
          nhSeg.ifIndexV4 != UINT32_MAX) {
        auto me4 = srcEntry.node->GetObject<Ipv4>();
        auto rt = v4h.GetStaticRouting(me4);
        rt->AddHostRouteTo(dstEntry.v4Addrs.front(), nhSeg.v4, meSeg.ifIndexV4);
      }

      if (h.hasIpv6 && !dstEntry.v6Addrs.empty() && meSeg.ifIndexV6 != UINT32_MAX &&
          nhSeg.ifIndexV6 != UINT32_MAX) {
        auto me6 = srcEntry.node->GetObject<Ipv6>();
        auto rt6 = v6h.GetStaticRouting(me6);
        rt6->AddHostRouteTo(dstEntry.v6Addrs.front(), nhSeg.v6ll, meSeg.ifIndexV6);
      }
    }
  }
}



void ClassicalNetworkBuilder::AssignAddresses(NetworkHandle& handle) {
  // Placeholder for per-node address book, if you want to flatten prefixes later.
}