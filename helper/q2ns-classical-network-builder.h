/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/

#pragma once

#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"


namespace ns3 {

/**
 * ClassicalNetworkBuilder:
 *   A high-level utility that builds arbitrary classical networks
 *   from user-specified topology data (nodes, links, LANs, etc.).
 *   Supports IPv4, IPv6, or dual stack; routing can be global,
 *   static (shortest-path), or none (single-LAN).
 */
class ClassicalNetworkBuilder : public Object {
public:
  enum class IpVersion { V4, V6, Dual };
  enum class Routing { None, Global, StaticShortestPaths, Ripng };

  struct Link {
    std::string id;
    std::string nodeA;
    std::string nodeB;
    std::string rate{"10Gbps"};
    std::string delay{"1us"};
  };

  struct Lan {
    std::string id;
    std::vector<std::string> members;
    std::string rate{"1Gbps"};
    std::string delay{"1us"};
  };

  struct NetworkHandle {
    struct NodeEntry {
      Ptr<Node> node;
      std::vector<Ptr<NetDevice>> devices;
      std::vector<Ipv4Address> v4Addrs;
      std::vector<Ipv6Address> v6Addrs;
      std::vector<uint32_t> ifaceIndex;
      std::map<std::string, Ptr<NetDevice>> devBySegment;

      struct IfaceInfo {
        uint32_t ifIndexV4 = UINT32_MAX;
        uint32_t ifIndexV6 = UINT32_MAX;
        ns3::Ipv4Address v4{ns3::Ipv4Address("0.0.0.0")};
        ns3::Ipv6Address v6{ns3::Ipv6Address::GetAny()};
        ns3::Ipv6Address v6ll{ns3::Ipv6Address::GetLoopback()};
      };
      std::map<std::string, IfaceInfo> segIf; // segId -> iface info
    };
    std::map<std::string, NodeEntry> nodes;
    bool hasIpv4{true};
    bool hasIpv6{true};
  };


  ClassicalNetworkBuilder();

  // User-facing configuration
  void SetIpVersion(IpVersion v);
  void SetRouting(Routing r);
  void AddLink(const Link& link);
  void AddLan(const Lan& lan);
  void SetDefaultDataRate(std::string rate);
  void SetDefaultDelay(std::string delay);

  // Attach physical ns-3 nodes by name before calling Build()
  void AttachNode(std::string name, Ptr<Node> node);

  // Build and return a ready-to-use handle
  NetworkHandle Build();

  // Routing synthesizer
  void InstallStaticEndpointRoutes(const std::vector<std::string>& orderedPath,
                                   NetworkHandle& handle, bool forIpv4, bool forIpv6);
  void InstallStaticShortestPaths(NetworkHandle& handle);

private:
  // Internal build steps
  void InstallInternetStacks();
  void BuildLinks(NetworkHandle& handle);
  void BuildLans(NetworkHandle& handle);
  void AssignAddresses(NetworkHandle& handle);

  std::map<std::string, Ptr<Node>> m_nodes;
  std::vector<Link> m_links;
  std::vector<Lan> m_lans;

  IpVersion m_ipVersion{IpVersion::Dual};
  Routing m_routing{Routing::Global};
  std::string m_defRate{"10Gbps"};
  std::string m_defDelay{"1us"};
};

} // namespace ns3