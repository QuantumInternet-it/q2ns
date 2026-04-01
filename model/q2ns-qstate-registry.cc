/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/
/**
 * @file q2ns-qstate-registry.cc
 * @brief Defines q2ns::QStateRegistry.
 */

#include "ns3/q2ns-qstate-registry.h"

#include "ns3/q2ns-qstate-all.h"
#include "ns3/q2ns-qubit.h"

#include "ns3/log.h"

#include <algorithm>
#include <unordered_set>
#include <vector>

namespace q2ns {

NS_LOG_COMPONENT_DEFINE("QStateRegistry");

QStateBackend BackendFromString(std::string_view s) {
  if (s == "ket" || s == "Ket" || s == "KET") {
    return QStateBackend::Ket;
  }
  if (s == "dm" || s == "DM" || s == "density" || s == "rho") {
    return QStateBackend::DM;
  }
  if (s == "stab" || s == "Stab" || s == "STAB" || s == "qfe" || s == "QFE") {
    return QStateBackend::Stab;
  }

  return QStateBackend::Ket;
}

void QStateRegistry::SetDefaultBackend(QStateBackend b) {
  defaultBackend_ = b;
}

QStateBackend QStateRegistry::GetDefaultBackend() const {
  return defaultBackend_;
}

CreateStateResult QStateRegistry::CreateState(unsigned int n) {
  if (n == 0) {
    NS_LOG_WARN("CreateState rejected: n must be greater than zero.");
    return {};
  }

  const StateId sid = nextStateId_++;

  std::shared_ptr<QState> state;
  switch (defaultBackend_) {
  case QStateBackend::Ket:
    state = std::make_shared<QStateKet>(static_cast<std::size_t>(n));
    break;
  case QStateBackend::DM:
    state = std::make_shared<QStateDM>(static_cast<std::size_t>(n));
    break;
  case QStateBackend::Stab:
    state = std::make_shared<QStateStab>(static_cast<std::size_t>(n));
    break;
  }

  states_[sid] = state;
  state->SetStateId(sid);

  if (state && streamAssigner_) {
    streamAssigner_(*state);
  }

  std::vector<unsigned int> idx(n);
  for (unsigned int i = 0; i < n; ++i) {
    idx[i] = i;
  }

  members_[sid];
  return CreateStateResult{sid, std::move(idx)};
}

CreateStateResult QStateRegistry::CreateStateFromExisting(const std::shared_ptr<QState>& state) {
  if (!state) {
    NS_LOG_WARN("CreateStateFromExisting rejected: null state.");
    return {};
  }

  const StateId newId = nextStateId_++;
  states_.emplace(newId, state);
  state->SetStateId(newId);

  if (streamAssigner_) {
    streamAssigner_(*state);
  }

  const auto n = static_cast<unsigned int>(state->NumQubits());
  std::vector<unsigned int> idx(n);
  for (unsigned int i = 0; i < n; ++i) {
    idx[i] = i;
  }

  members_[newId] = {};
  return {newId, std::move(idx)};
}

std::shared_ptr<QState> QStateRegistry::MergeStates(const std::vector<std::shared_ptr<Qubit>>& qs) {
  if (qs.empty()) {
    NS_LOG_WARN("MergeStates rejected: empty qubit list.");
    return nullptr;
  }

  for (const auto& q : qs) {
    if (!q) {
      NS_LOG_WARN("MergeStates rejected: null qubit in input list.");
      return nullptr;
    }
  }

  std::vector<StateId> sids;
  sids.reserve(qs.size());
  for (const auto& q : qs) {
    sids.push_back(q->GetStateId());
  }
  std::sort(sids.begin(), sids.end());
  sids.erase(std::unique(sids.begin(), sids.end()), sids.end());

  if (sids.size() == 1) {
    return GetState(sids.front());
  }

  // Preserve each state's internal qubit order and choose a deterministic
  // cross-state merge order so post-merge indices are reproducible.
  struct StateBucket {
    StateId sid;
    std::shared_ptr<QState> state;
    QubitId leadingQubitId;
    std::vector<std::shared_ptr<Qubit>> members;
  };

  std::vector<StateBucket> buckets;
  buckets.reserve(sids.size());

  for (auto sid : sids) {
    StateBucket bucket;
    bucket.sid = sid;
    bucket.state = GetState(sid);
    if (!bucket.state) {
      NS_LOG_WARN("MergeStates failed: referenced state was not found.");
      return nullptr;
    }

    bucket.members = QubitsOf(sid);

    std::sort(bucket.members.begin(), bucket.members.end(), [](const auto& x, const auto& y) {
      return x->GetIndexInState() < y->GetIndexInState();
    });

    bucket.leadingQubitId = bucket.members.empty() ? 0 : bucket.members.front()->GetQubitId();

    buckets.push_back(std::move(bucket));
  }

  std::sort(buckets.begin(), buckets.end(), [](const StateBucket& a, const StateBucket& b) {
    if (a.leadingQubitId != b.leadingQubitId) {
      return a.leadingQubitId < b.leadingQubitId;
    }
    return a.sid < b.sid;
  });

  std::vector<std::shared_ptr<Qubit>> allQs;
  for (const auto& bucket : buckets) {
    allQs.insert(allQs.end(), bucket.members.begin(), bucket.members.end());
  }

  auto merged = buckets.front().state;
  for (std::size_t i = 1; i < buckets.size(); ++i) {
    merged = merged->MergeDisjoint(*buckets[i].state);
    if (!merged) {
      NS_LOG_WARN("MergeStates failed: backend merge returned null.");
      return nullptr;
    }
  }

  auto created = CreateStateFromExisting(merged);
  if (created.stateId == 0) {
    return nullptr;
  }

  for (std::size_t i = 0; i < allQs.size(); ++i) {
    const auto& q = allQs[i];
    auto loc = GetLocation(q);

    UnregisterEverywhere(q);
    q->SetStateId(created.stateId);
    q->SetIndexInState(created.indices[i]);
    Register(q);
    SetLocation(q, loc);
  }

  for (const auto& bucket : buckets) {
    if (QubitsOf(bucket.sid).empty()) {
      RemoveState(bucket.sid);
    }
  }

  return GetState(created.stateId);
}

void QStateRegistry::RemoveState(StateId id) {
  states_.erase(id);
  members_.erase(id);
}

std::vector<std::shared_ptr<QState>> QStateRegistry::GetStatesSortedById() const {
  std::vector<std::pair<StateId, std::shared_ptr<QState>>> items;
  items.reserve(states_.size());

  for (const auto& kv : states_) {
    items.emplace_back(kv.first, kv.second);
  }

  std::sort(items.begin(), items.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });

  std::vector<std::shared_ptr<QState>> out;
  out.reserve(items.size());
  for (auto& kv : items) {
    out.push_back(std::move(kv.second));
  }

  return out;
}

void QStateRegistry::Register(const std::shared_ptr<Qubit>& q) {
  if (!q) {
    NS_LOG_WARN("Register rejected: null qubit.");
    return;
  }

  const StateId sid = q->GetStateId();

  if (q->GetQubitId() == 0) {
    q->SetQubitId(nextQubitId_++);
  }

  if (location_.count(q->GetQubitId()) == 0) {
    SetLocation(q, MakeUnsetLocation());
  }

  auto& vec = members_[sid];
  const auto raw = q.get();

  for (const auto& w : vec) {
    auto sp = w.lock();
    if (sp && sp.get() == raw) {
      return;
    }
  }

  vec.emplace_back(q);
}

void QStateRegistry::Unregister(const std::shared_ptr<Qubit>& q) {
  if (!q) {
    NS_LOG_WARN("Unregister rejected: null qubit.");
    return;
  }

  const StateId sid = q->GetStateId();
  auto it = members_.find(sid);
  if (it == members_.end()) {
    return;
  }

  auto& vec = it->second;
  const auto raw = q.get();

  vec.erase(std::remove_if(vec.begin(), vec.end(),
                           [&](const std::weak_ptr<Qubit>& w) {
                             auto sp = w.lock();
                             return !sp || sp.get() == raw;
                           }),
            vec.end());
}

void QStateRegistry::UnregisterEverywhere(const std::shared_ptr<Qubit>& q) {
  if (!q) {
    NS_LOG_WARN("UnregisterEverywhere rejected: null qubit.");
    return;
  }

  const auto raw = q.get();

  for (auto& [sid, vec] : members_) {
    (void) sid;
    vec.erase(std::remove_if(vec.begin(), vec.end(),
                             [&](const std::weak_ptr<Qubit>& w) {
                               auto sp = w.lock();
                               return !sp || sp.get() == raw;
                             }),
              vec.end());
  }
}

void QStateRegistry::SetLocation(const std::shared_ptr<Qubit>& q, Location loc) {
  if (!q) {
    NS_LOG_WARN("SetLocation rejected: null qubit.");
    return;
  }

  qubitById_[q->GetQubitId()] = q;
  SetLocation(q->GetQubitId(), loc);
}

void QStateRegistry::SetLocation(QubitId id, Location loc) {
  auto itOld = location_.find(id);
  if (itOld != location_.end()) {
    const auto& oldLoc = itOld->second;
    if (oldLoc.type == LocationType::Node) {
      auto itSet = qubitsAtNode_.find(oldLoc.ownerId);
      if (itSet != qubitsAtNode_.end()) {
        itSet->second.erase(id);
        if (itSet->second.empty()) {
          qubitsAtNode_.erase(itSet);
        }
      }
    }
  }

  location_[id] = loc;

  if (loc.type == LocationType::Node) {
    qubitsAtNode_[loc.ownerId].insert(id);
  }
}

Location QStateRegistry::GetLocation(const std::shared_ptr<Qubit>& q) const {
  if (!q) {
    NS_LOG_WARN("GetLocation rejected: null qubit; returning Unset location.");
    return MakeUnsetLocation();
  }

  auto it = location_.find(q->GetQubitId());
  if (it == location_.end()) {
    return MakeUnsetLocation();
  }

  return it->second;
}

Location QStateRegistry::GetLocation(QubitId id) const {
  auto it = location_.find(id);
  if (it == location_.end()) {
    return MakeUnsetLocation();
  }

  return it->second;
}

std::vector<QubitId> QStateRegistry::GetQubitsAtNode(uint32_t nodeId) const {
  std::vector<QubitId> out;

  auto it = qubitsAtNode_.find(nodeId);
  if (it == qubitsAtNode_.end()) {
    return out;
  }

  out.reserve(it->second.size());
  for (const auto id : it->second) {
    out.push_back(id);
  }

  return out;
}

std::shared_ptr<Qubit> QStateRegistry::GetQubitHandle(QubitId id) const {
  auto it = qubitById_.find(id);
  if (it == qubitById_.end()) {
    return nullptr;
  }

  return it->second.lock();
}

std::vector<std::shared_ptr<Qubit>> QStateRegistry::GetLocalQubits(uint32_t nodeId) const {
  std::vector<std::shared_ptr<Qubit>> out;

  auto it = qubitsAtNode_.find(nodeId);
  if (it == qubitsAtNode_.end()) {
    return out;
  }

  out.reserve(it->second.size());
  for (const auto id : it->second) {
    if (auto q = GetQubitHandle(id)) {
      out.push_back(q);
    }
  }

  return out;
}

std::vector<std::shared_ptr<Qubit>> QStateRegistry::QubitsOf(StateId stateId) const {
  std::vector<std::shared_ptr<Qubit>> out;

  auto it = members_.find(stateId);
  if (it == members_.end()) {
    return out;
  }

  out.reserve(it->second.size());
  for (const auto& w : it->second) {
    if (auto sp = w.lock()) {
      out.push_back(sp);
    }
  }

  return out;
}

std::shared_ptr<QState> QStateRegistry::GetState(const std::shared_ptr<Qubit>& q) const {
  if (!q) {
    return nullptr;
  }

  return GetState(q->GetStateId());
}

std::shared_ptr<QState> QStateRegistry::GetState(StateId stateId) const {
  auto it = states_.find(stateId);
  if (it == states_.end()) {
    NS_LOG_WARN("GetState failed: state id " << stateId << " was not found.");
    return nullptr;
  }

  return it->second;
}

} // namespace q2ns