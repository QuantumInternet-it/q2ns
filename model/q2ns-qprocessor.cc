/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/
/**
 * @file q2ns-qprocessor.cc
 * @brief Defines q2ns::QProcessor.
 */

#include "q2ns-qprocessor.h"

#include "ns3/q2ns-qnode.h"
#include "ns3/q2ns-qstate-registry.h"
#include "ns3/q2ns-qstate.h"
#include "ns3/q2ns-qubit.h"

#include "ns3/log.h"

#include <algorithm>

namespace q2ns {

NS_LOG_COMPONENT_DEFINE("QProcessor");

QProcessor::QProcessor(QStateRegistry& registry, QNode& owner)
    : registry_(registry), owner_(owner) {}



const QNode& QProcessor::GetOwnerNode() const {
  return owner_;
}



std::shared_ptr<Qubit> QProcessor::CreateQubit(const std::string& label) {

  if (!label.empty() && GetQubit(label)) {
    NS_LOG_WARN(
        "CreateQubit received a duplicate local label '"
        << label
        << "'. A new qubit will still be created, but label-based lookup may be ambiguous.");
  }

  auto res = registry_.CreateState(1);
  auto q = std::make_shared<Qubit>(registry_, res.stateId, res.indices[0], label);

  registry_.Register(q);
  q->SetLocationNode(owner_.GetId());

  return q;
}



std::shared_ptr<Qubit> QProcessor::CreateQubit(const std::shared_ptr<QState>& state,
                                               const std::string& label) {
  if (!state) {
    NS_LOG_WARN("CreateQubit rejected: null state.");
    return nullptr;
  }

  if (!label.empty() && GetQubit(label)) {
    NS_LOG_WARN("CreateQubit received a duplicate local label '"
                << label << "'. Label-based lookup may be ambiguous.");
  }

  auto res = registry_.CreateStateFromExisting(state);
  auto q = std::make_shared<Qubit>(registry_, res.stateId, res.indices[0], label);

  registry_.Register(q);
  q->SetLocationNode(owner_.GetId());

  return q;
}



std::pair<std::shared_ptr<Qubit>, std::shared_ptr<Qubit>> QProcessor::CreateBellPair() {
  auto created = registry_.CreateState(2);
  auto q0 = std::make_shared<Qubit>(registry_, created.stateId, created.indices[0], "bell0");
  auto q1 = std::make_shared<Qubit>(registry_, created.stateId, created.indices[1], "bell1");

  registry_.Register(q0);
  registry_.Register(q1);
  q0->SetLocationNode(owner_.GetId());
  q1->SetLocationNode(owner_.GetId());

  Apply(q2ns::gates::H(), {q0});
  Apply(q2ns::gates::CNOT(), {q0, q1});

  return {q0, q1};
}



std::shared_ptr<Qubit> QProcessor::GetQubit(const std::string& label) const {
  if (label.empty()) {
    NS_LOG_WARN("GetQubit failed: label is empty.");
    return nullptr;
  }

  for (const auto& q : registry_.GetLocalQubits(owner_.GetId())) {
    if (q && q->GetLabel() == label) {
      return q;
    }
  }

  NS_LOG_WARN("GetQubit failed: no qubit found in local qubits with label=" << label << ".");
  return nullptr;
}



std::shared_ptr<Qubit> QProcessor::GetQubit(QubitId id) const {
  auto q = registry_.GetQubitHandle(id);
  if (!q) {
    NS_LOG_WARN("GetQubit failed: no matching qubit.");
    return nullptr;
  }

  const auto loc = registry_.GetLocation(id);
  if (loc.type == LocationType::Node && loc.ownerId == owner_.GetId()) {
    return q;
  }

  NS_LOG_WARN("GetQubit failed: no qubit found in local qubits with id=" << id << ".");
  return nullptr;
}



std::vector<std::shared_ptr<Qubit>> QProcessor::GetLocalQubits() const {
  return registry_.GetLocalQubits(owner_.GetId());
}



void QProcessor::AdoptQubit(const std::shared_ptr<Qubit>& q) {
  if (!q) {
    NS_LOG_WARN("AdoptQubit rejected: null qubit.");
    return;
  }

  const auto loc = q->GetLocation();
  if (loc.type == LocationType::Lost) {
    NS_LOG_WARN("AdoptQubit rejected: qubit is lost.");
    return;
  }

  q->SetLocationNode(owner_.GetId());
}



std::shared_ptr<QState> QProcessor::GetState(const std::shared_ptr<Qubit>& q) const {
  if (!q) {
    NS_LOG_WARN("GetState rejected: null qubit.");
    return nullptr;
  }
  return registry_.GetState(q);
}



bool QProcessor::Apply(const QGate& gate, const std::vector<std::shared_ptr<Qubit>>& qs) {
  if (qs.empty()) {
    NS_LOG_WARN("Apply rejected: empty qubit list.");
    return false;
  }

  for (const auto& q : qs) {
    if (!q) {
      NS_LOG_WARN("Apply rejected: null qubit in target list.");
      return false;
    }

    const auto locQ = registry_.GetLocation(q);
    if (!(locQ.type == LocationType::Node && locQ.ownerId == owner_.GetId())) {
      NS_LOG_WARN("Apply rejected: target qubit is not local to this node.");
      return false;
    }
  }

  // Merge target states if needed so the backend gate can be applied using a
  // single state object and the current post-merge qubit indices.
  auto st = registry_.MergeStates(qs);
  if (!st) {
    NS_LOG_WARN("Apply failed: state merge did not produce a valid backend state.");
    return false;
  }

  std::vector<q2ns::Index> targets;
  targets.reserve(qs.size());
  for (const auto& q : qs) {
    targets.push_back(q->GetIndexInState());
  }

  st->Apply(gate, targets);
  return true;
}



int QProcessor::Measure(const std::shared_ptr<Qubit>& q, q2ns::Basis basis) {
  if (!q) {
    NS_LOG_WARN("Measure rejected: null qubit.");
    return -1;
  }

  const auto locQ = registry_.GetLocation(q);
  if (!(locQ.type == LocationType::Node && locQ.ownerId == owner_.GetId())) {
    NS_LOG_WARN("Measure rejected: qubit is not local to this node.");
    return -1;
  }

  auto currentState = registry_.GetState(q->GetStateId());
  if (!currentState) {
    NS_LOG_WARN("Measure failed: could not retrieve the qubit's current state.");
    return -1;
  }

  auto related = registry_.QubitsOf(currentState->GetStateId());

  // Sort by current state index before rebinding. After measurement removes
  // one qubit, survivor indices are compacted and must be reassigned in order.
  std::sort(related.begin(), related.end(), [](const auto& a, const auto& b) {
    return a->GetIndexInState() < b->GetIndexInState();
  });

  const auto removedIndex = q->GetIndexInState();
  auto split = currentState->Measure(removedIndex, basis);
  const int result = static_cast<int>(split.outcome);


  // Unregister all related qubits from the old state before rebinding them to the measured and
  // survivor states created below.
  for (const auto& r : related) {
    registry_.UnregisterEverywhere(r);
  }

  // 1) Register the measured 1-qubit state
  auto newMeasured = registry_.CreateStateFromExisting(split.measured);

  // Rebind 'q' to the measured state (index 0) and restore location (value already retrieved above
  // during initial checks)
  q->SetStateId(newMeasured.stateId);
  q->SetIndexInState(newMeasured.indices[0]);
  registry_.Register(q);
  registry_.SetLocation(q, locQ);


  // 2) If survivors exist, register them and rebind remaining qubits
  if (split.survivors && split.survivors->NumQubits() > 0) {
    auto survivorsRes = registry_.CreateStateFromExisting(split.survivors);

    // Rebind: all original related except 'q' (whose index was removed)
    unsigned int nextIdx = 0;
    for (const auto& r : related) {
      if (r.get() == q.get()) {
        continue; // skip the measured qubit; already bound above
      }

      // Preserve location if available
      auto l = registry_.GetLocation(r);

      r->SetStateId(survivorsRes.stateId);
      r->SetIndexInState(survivorsRes.indices[nextIdx++]);
      registry_.Register(r);
      registry_.SetLocation(r, l);
    }
  }

  // Cleanup the old state if it's empty
  auto remaining = registry_.QubitsOf(currentState->GetStateId());
  if (remaining.empty()) {
    registry_.RemoveState(currentState->GetStateId());
  }

  return result;
}



std::pair<int, int> QProcessor::MeasureBell(const std::shared_ptr<Qubit>& a,
                                            const std::shared_ptr<Qubit>& b) {
  if (!a || !b) {
    NS_LOG_WARN("MeasureBell rejected: null qubit.");
    return {-1, -1};
  }

  const auto locA = a->GetLocation();
  if (locA.type == LocationType::Lost) {
    NS_LOG_WARN("MeasureBell rejected: first qubit " << a->GetQubitId() << " is lost.");
    return {-1, -1};
  }

  const auto locB = b->GetLocation();
  if (locB.type == LocationType::Lost) {
    NS_LOG_WARN("MeasureBell rejected: second qubit " << b->GetQubitId() << " is lost.");
    return {-1, -1};
  }

  const auto locA2 = registry_.GetLocation(a);
  const auto locB2 = registry_.GetLocation(b);
  if (!(locA2.type == LocationType::Node && locA2.ownerId == owner_.GetId()) ||
      !(locB2.type == LocationType::Node && locB2.ownerId == owner_.GetId())) {
    NS_LOG_WARN("MeasureBell rejected: qubits are not both local to this node.");
    return {-1, -1};
  }

  // Bell-basis measurement implemented as a basis rotation followed by two
  // single-qubit measurements in the computational basis.
  if (!Apply(q2ns::gates::CNOT(), {a, b})) {
    return {-1, -1};
  }
  if (!Apply(q2ns::gates::H(), {a})) {
    return {-1, -1};
  }

  auto resultA = Measure(a);
  auto resultB = Measure(b);

  return {resultA, resultB};
}
} // namespace q2ns