/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/

#ifndef Q2NS_SWAP_HELPER_H
#define Q2NS_SWAP_HELPER_H

#include "ns3/node.h"
#include "ns3/ptr.h"

#include "ns3/q2ns-classical-network-builder.h"

#include <cstdint>
#include <memory>

#include <string>
#include <unordered_map>
#include <vector>

namespace q2ns {

class NetController;
class QNode;
class SwapApp;

struct SwapSessionSpec {
  std::vector<std::string> path; // [A, R1, ..., Rk, B]
  double start_s{0.0};
  uint64_t sessionId{0};
  uint16_t ctrlPort{0};        // 0 => helper auto-assigns
  std::string protocol{"udp"}; // "udp" | "tcp"
};

// Classical link spec (unchanged)
struct LinkSpec {
  std::string u, v; // node names
  double mbps{1.0};
  std::string protocol{"udp"};
  double delayNsPerKm{5000};
  double distanceKm{0};
  std::string queueMax{"128kB"};
};

// Quantum link spec (unchanged)
struct QLinkSpec {
  std::string u, v;
  double distanceKm{0.0};
  double delayNsPerKm{5000.0};
};

struct RoundsConfig {
  uint32_t rounds{0};   // 0 => disabled; use explicit session start_s
  double period_s{0.0}; // spacing between round starts
  double jitter_s{0.0}; // +/- uniform jitter per session
  uint64_t seed{0};     // RNG seed for reproducibility
  bool Enabled() const {
    return rounds > 0 && period_s > 0.0;
  }
};

struct SwapTopologySpec {
  std::vector<std::string> nodes;
  std::vector<LinkSpec> classicalEdges;
  std::vector<QLinkSpec> quantumEdges;
  std::vector<SwapSessionSpec> sessions; // optional if a policy will generate them

  // Optional global scheduling
  RoundsConfig rounds{};                 // if Enabled(), helper/policy assign round start times
  std::string policyName{"all-at-once"}; // name looked up by the helper
};

class ISwapPolicy {
public:
  virtual ~ISwapPolicy() = default;
  // Return a fully-specified list of sessions with start times (helper will install them)
  virtual std::vector<SwapSessionSpec> PlanSessions(const SwapTopologySpec& topo) = 0;
};

// Built-in default policy: keep provided sessions; if rounds.Enabled(), replicate them across
// rounds with start times T0 + r*period (+/- jitter).
class AllAtOncePolicy final : public ISwapPolicy {
public:
  std::vector<SwapSessionSpec> PlanSessions(const SwapTopologySpec& topo) override;
};

class EntanglementSwapHelper {
public:
  EntanglementSwapHelper& SetNetController(NetController* nc);
  EntanglementSwapHelper& SetPortBase(uint16_t base);
  EntanglementSwapHelper& SetPolicy(std::unique_ptr<ISwapPolicy> policy);
  EntanglementSwapHelper& SetNodes(const std::map<std::string, ns3::Ptr<q2ns::QNode>>& nodes);

  void Install(const SwapTopologySpec& spec, const ns3::ClassicalNetworkBuilder::NetworkHandle& net,
               bool useIpv6, uint32_t ctrlPayloadBytes = 16);

private:
  uint16_t m_portBase{7000};
  NetController* m_nc{nullptr};
  std::unique_ptr<ISwapPolicy> m_policy; // if null, use AllAtOncePolicy
  std::unordered_map<std::string, ns3::Ptr<q2ns::QNode>> m_name2node;
  std::unordered_map<std::string, ns3::Ptr<SwapApp>> m_appByNode; // one app per node

  uint16_t AllocatePort(uint64_t sessionId) const;
  void BindAppsOnProvidedNodes(const SwapTopologySpec& spec);
  void BuildQuantum(const SwapTopologySpec& spec);
  void InstallUnifiedApps(const SwapTopologySpec& spec, const std::vector<SwapSessionSpec>& planned,
                          const ns3::ClassicalNetworkBuilder::NetworkHandle& net, bool useIpv6,
                          uint32_t ctrlPayloadBytes);
};

} // namespace q2ns

#endif // Q2NS_SWAP_HELPER_H
