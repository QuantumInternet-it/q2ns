/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/
/**
 * @file q2ns-netcontroller.cc
 * @brief Defines q2ns::NetController.
 */

#include "ns3/q2ns-netcontroller.h"

#include "ns3/q2ns-qchannel.h"
#include "ns3/q2ns-qmap.h"
#include "ns3/q2ns-qnet-device.h"
#include "ns3/q2ns-qnode.h"
#include "ns3/q2ns-qstate.h"
#include "ns3/q2ns-qubit.h"
#include "q2ns-qnetworker.h"

#include "ns3/log.h"
#include "ns3/names.h"
#include "ns3/node.h"
#include "ns3/rng-seed-manager.h"
#include "ns3/simulator.h"

#include <algorithm>
#include <string>
#include <utility>

namespace q2ns {

NS_LOG_COMPONENT_DEFINE("NetController");

NetController::NetController() {
  // Schedule a one-time initializer at time 0 so RNG stream binding uses the
  // final ns-3 seed/run settings regardless of whether the user calls a
  // controller helper or ns3::Simulator::Run() directly.
  ns3::Simulator::ScheduleNow(&NetController::EnsureStreamsAssigned_, this);

  // Install a registry hook so states created after stream assignment receive
  // deterministic streams automatically.
  registry_.SetStreamAssigner([this](QState& s) {
    if (!streamsAssigned_) {
      return;
    }
    nextStream_ += s.AssignStreams(nextStream_);
  });
}

int64_t NetController::AssignStreams(int64_t stream) {
  if (streamsAssigned_) {
    NS_LOG_WARN("AssignStreams called more than once; streams will be reassigned.");
  }

  streamsAssigned_ = true;
  nextStream_ = stream;

  int64_t used = 0;

  // Channels are stored in std::map, so iteration order is deterministic.
  for (auto& kv : channels_) {
    auto& ch = kv.second;
    const int64_t n = ch->AssignStreams(nextStream_);
    nextStream_ += n;
    used += n;
  }

  // States are stored internally in an unordered container, so assign streams
  // in sorted state-id order for deterministic behavior.
  for (auto& st : registry_.GetStatesSortedById()) {
    if (st) {
      const int64_t n = st->AssignStreams(nextStream_);
      nextStream_ += n;
      used += n;
    }
  }

  return used;
}

void NetController::EnsureStreamsAssigned_() {
  if (streamsAssigned_) {
    return;
  }

  AssignStreams(0);
}

QStateRegistry& NetController::GetRegistry() {
  return registry_;
}

void NetController::SetQStateBackend(QStateBackend b) {
  registry_.SetDefaultBackend(b);
}

void NetController::SetQStateBackend(std::string_view name) {
  registry_.SetDefaultBackend(BackendFromString(name));
}

QStateBackend NetController::GetQStateBackend() {
  return registry_.GetDefaultBackend();
}

ns3::Ptr<QNode> NetController::CreateNode(const std::string& label) {
  auto qnode = ns3::CreateObject<QNode>(registry_);

  const uint32_t id = qnode->GetId();
  nodes_[id] = qnode;

  if (!label.empty()) {
    ns3::Names::Add(label, qnode);
  }

  return qnode;
}

ns3::Ptr<QNode> NetController::GetNode(uint32_t nodeId) {
  auto it = nodes_.find(nodeId);
  if (it != nodes_.end() && it->second) {
    return it->second;
  }

  return nullptr;
}

ns3::Ptr<QChannel> NetController::InstallQuantumLink(ns3::Ptr<QNode> a, ns3::Ptr<QNode> b) {
  if (!a || !b) {
    NS_LOG_WARN("InstallQuantumLink rejected: null node.");
    return nullptr;
  }

  const uint32_t ida = a->GetId();
  const uint32_t idb = b->GetId();
  const auto key = std::make_pair(std::min(ida, idb), std::max(ida, idb));

  auto it = channels_.find(key);
  if (it != channels_.end()) {
    NS_LOG_WARN("InstallQuantumLink reused existing channel for nodes " << ida << " and " << idb
                                                                        << ".");
    return it->second;
  }

  auto devA = ns3::CreateObject<QNetDevice>();
  auto devB = ns3::CreateObject<QNetDevice>();
  auto chan = ns3::CreateObject<QChannel>();

  if (streamsAssigned_) {
    nextStream_ += chan->AssignStreams(nextStream_);
  }

  chan->Connect(devA, devB);

  a->AddDevice(devA);
  b->AddDevice(devB);

  const uint32_t ifA = a->networker_->AddInterface(devA);
  const uint32_t ifB = b->networker_->AddInterface(devB);

  a->networker_->AddRoute(idb, ifA);
  b->networker_->AddRoute(ida, ifB);

  channels_[key] = chan;
  return chan;
}

std::vector<ns3::Ptr<QChannel>>
NetController::InstallQuantumChain(const std::vector<ns3::Ptr<QNode>>& nodes) {
  std::vector<ns3::Ptr<QChannel>> channels;
  if (nodes.size() < 2) {
    return channels;
  }

  channels.reserve(nodes.size() - 1);

  for (std::size_t i = 0; i + 1 < nodes.size(); ++i) {
    const auto& a = nodes[i];
    const auto& b = nodes[i + 1];

    if (!a || !b) {
      NS_LOG_WARN("InstallQuantumChain skipped link at positions " << i << " and " << (i + 1)
                                                                   << " because a node was null.");
      continue;
    }

    if (a == b) {
      NS_LOG_WARN("InstallQuantumChain skipped self-link at positions " << i << " and " << (i + 1)
                                                                        << ".");
      continue;
    }

    channels.push_back(InstallQuantumLink(a, b));
  }

  return channels;
}

std::vector<ns3::Ptr<QChannel>>
NetController::InstallQuantumAllToAll(const std::vector<ns3::Ptr<QNode>>& nodes) {
  std::vector<ns3::Ptr<QChannel>> channels;
  if (nodes.size() < 2) {
    return channels;
  }

  const std::size_t n = nodes.size();
  channels.reserve((n * (n - 1)) / 2);

  for (std::size_t i = 0; i < n; ++i) {
    const auto& a = nodes[i];
    if (!a) {
      NS_LOG_WARN("InstallQuantumAllToAll skipped all pairs involving null node at position "
                  << i << ".");
      continue;
    }

    for (std::size_t j = i + 1; j < n; ++j) {
      const auto& b = nodes[j];

      if (!b) {
        NS_LOG_WARN("InstallQuantumAllToAll skipped pair at positions "
                    << i << " and " << j << " because a node was null.");
        continue;
      }

      if (a == b) {
        NS_LOG_WARN("InstallQuantumAllToAll skipped self-link at positions " << i << " and " << j
                                                                             << ".");
        continue;
      }

      channels.push_back(InstallQuantumLink(a, b));
    }
  }

  return channels;
}

ns3::Ptr<QChannel> NetController::GetChannel(ns3::Ptr<QNode> a, ns3::Ptr<QNode> b) {
  if (!a || !b) {
    NS_LOG_WARN("GetChannel rejected: null node.");
    return nullptr;
  }

  const uint32_t ida = a->GetId();
  const uint32_t idb = b->GetId();
  const auto key = std::make_pair(std::min(ida, idb), std::max(ida, idb));

  auto it = channels_.find(key);
  if (it == channels_.end()) {
    NS_LOG_WARN("GetChannel failed: no channel connects the provided nodes.");
    return nullptr;
  }

  return it->second;
}

std::shared_ptr<QState> NetController::GetState(const std::shared_ptr<Qubit>& q) const {
  return registry_.GetState(q);
}

} // namespace q2ns