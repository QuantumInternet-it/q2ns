/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/
/**
 * @file q2ns-qubit.cc
 * @brief Defines q2ns::Qubit.
 */

#include "ns3/q2ns-qubit.h"

#include "ns3/q2ns-qstate-registry.h"
#include "ns3/q2ns-qstate.h"

#include "ns3/log.h"

#include <utility>

namespace q2ns {

NS_LOG_COMPONENT_DEFINE("Qubit");



Qubit::Qubit(QStateRegistry& registry, StateId stateId, unsigned int indexInState,
             std::string label)
    : registry_(registry), stateId_(stateId), indexInState_(indexInState),
      label_(std::move(label)) {}



QubitId Qubit::GetQubitId() const {
  return qubitId_;
}



void Qubit::SetQubitId(QubitId id) {
  qubitId_ = id;
}



StateId Qubit::GetStateId() const {
  return stateId_;
}



void Qubit::SetStateId(StateId stateId) {
  stateId_ = stateId;
}



unsigned int Qubit::GetIndexInState() const {
  return indexInState_;
}



void Qubit::SetIndexInState(unsigned int index) {
  indexInState_ = index;
}



const std::string& Qubit::GetLabel() const {
  return label_;
}



void Qubit::SetLabel(std::string label) {
  label_ = std::move(label);
}



Location Qubit::GetLocation() const {
  return registry_.GetLocation(qubitId_);
}



void Qubit::SetLocationNode(uint32_t nodeId) {
  const auto loc = registry_.GetLocation(qubitId_);
  if (loc.type == LocationType::Lost) {
    NS_LOG_WARN("SetLocationNode rejected: lost qubits cannot be moved back to a node.");
    return;
  }

  registry_.SetLocation(shared_from_this(), Location{LocationType::Node, nodeId});
}



void Qubit::SetLocationChannel(uint32_t channelId) {
  const auto loc = registry_.GetLocation(qubitId_);
  if (loc.type == LocationType::Lost) {
    NS_LOG_WARN("SetLocationChannel rejected: lost qubits cannot be moved onto a channel.");
    return;
  }

  registry_.SetLocation(shared_from_this(), Location{LocationType::Channel, channelId});
}



void Qubit::SetLocationLost() {
  registry_.SetLocation(shared_from_this(), Location{LocationType::Lost, 0});
}



void Qubit::Rebind(StateId newStateId, std::size_t newIndex) {
  stateId_ = newStateId;
  indexInState_ = static_cast<unsigned int>(newIndex);
}

} // namespace q2ns