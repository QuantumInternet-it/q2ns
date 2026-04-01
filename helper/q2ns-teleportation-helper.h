/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/

#ifndef Q2NS_TELEPORTATION_HELPER_H
#define Q2NS_TELEPORTATION_HELPER_H

#include "ns3/q2ns-netcontroller.h"
#include "ns3/q2ns-qnode.h"

#include "ns3/node.h"
#include "ns3/ptr.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace q2ns {

struct SessionSpec {
  std::string src;
  std::string dst;
  double start_s{0.0};
  uint64_t sessionId{0};
  uint16_t ctrlPort{0}; // 0 => auto-assign
  std::string protocol; // optional per-session override ("" => use link/default)
};

struct LinkSpec {
  std::string u, v; // node names
  double mbps{1.0};
  std::string protocol{"udp"}; // "udp" | "tcp" (used for ctrl + bg defaults)
  double delayNsPerKm{5000};
  double distanceKm{0};
  std::string queueMax{"128kB"};
};

struct NodeSpec {
  std::string name;
};

struct TopologySpec {
  std::vector<NodeSpec> nodes;
  std::vector<LinkSpec> classicalEdges;
  std::vector<LinkSpec> quantumEdges; // delay only (ideal noise for now)
  std::vector<SessionSpec> sessions;
  std::string backend{"ket"};
};

class TeleportationHelper {
public:
  TeleportationHelper& UseBackend(std::string backend);
  TeleportationHelper& SetDefaultQueue(std::string max);
  TeleportationHelper& SetPortBase(uint16_t base);

  // Provide the central orchestrator (non-owning pointer).
  TeleportationHelper& SetNetController(NetController* nc);

  // Provide the 1-qubit state to be teleported (used by both Alice & Bob apps)
  TeleportationHelper& SetTeleportState(const std::shared_ptr<q2ns::QState>& tpl);

  // Main entry: build nodes/links, install apps/background, set addresses/queues
  void Install(const TopologySpec& spec);

private:
  uint16_t m_portBase{7000};
  std::string m_defaultQueue{"128kB"};
  std::string m_backend{"ket"};

  // Central orchestrator (required for node creation & quantum wiring)
  q2ns::NetController* m_nc{nullptr};


  std::unordered_map<std::string, ns3::Ptr<ns3::Node>> m_name2node;

  std::shared_ptr<q2ns::QState> m_teleportState{nullptr};

  uint16_t AllocatePort(uint64_t sessionId) const;

  void BuildNodes(const TopologySpec& spec);
  void BuildClassical(const TopologySpec& spec); // p2p devices + IP + queues
  void BuildQuantum(const TopologySpec& spec);   // delay only, ideal channel
  void InstallSessionApps(const TopologySpec& spec);
};

} // namespace q2ns

#endif // Q2NS_TELEPORTATION_HELPER_H