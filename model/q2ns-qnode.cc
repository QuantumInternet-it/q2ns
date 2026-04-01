/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/
/**
 * @file q2ns-qnode.cc
 * @brief Defines q2ns::QNode.
 */

#include "ns3/q2ns-qnode.h"

#include "ns3/q2ns-qnet-device.h"
#include "ns3/q2ns-qstate-registry.h"
#include "ns3/q2ns-qstate.h"
#include "ns3/q2ns-qubit.h"
#include "q2ns-qnetworker.h"
#include "q2ns-qprocessor.h"

namespace q2ns {

NS_OBJECT_ENSURE_REGISTERED(QNode);

ns3::TypeId QNode::GetTypeId(void) {
  static ns3::TypeId tid = ns3::TypeId("q2ns::QNode").SetParent<ns3::Node>().SetGroupName("q2ns");
  return tid;
}



QNode::QNode(QStateRegistry& registry)
    : registry_(registry), processor_(std::make_unique<QProcessor>(registry_, *this)),
      networker_(std::make_unique<QNetworker>(*this)) {}



QNode::~QNode() = default;



void QNode::AddRoute(uint32_t dstNodeId, uint32_t oif) {
  networker_->AddRoute(dstNodeId, oif);
}



void QNode::AddDevice(ns3::Ptr<ns3::NetDevice> device) {
  // Register with the underlying ns-3 Node first so the device participates in
  // the normal node device inventory regardless of whether it is quantum-aware.
  ns3::Node::AddDevice(device);

  // If the device is quantum-aware, also bind it to the internal QNetworker so
  // arrivals from the channel are forwarded into the node's quantum stack.
  if (auto qdev = ns3::DynamicCast<q2ns::QNetDevice>(device)) {
    qdev->SetNode(this);
    qdev->BindNetworker(*networker_);
  }
}



std::shared_ptr<Qubit> QNode::CreateQubit(const std::string& label) {
  return processor_->CreateQubit(label);
}



std::shared_ptr<Qubit> QNode::CreateQubit(const std::shared_ptr<QState>& state,
                                          const std::string& label) {
  return processor_->CreateQubit(state, label);
}



std::shared_ptr<Qubit> QNode::GetQubit(const std::string& label) const {
  return processor_->GetQubit(label);
}



std::shared_ptr<Qubit> QNode::GetQubit(QubitId id) const {
  return processor_->GetQubit(id);
}



std::vector<std::shared_ptr<Qubit>> QNode::GetLocalQubits() const {
  return processor_->GetLocalQubits();
}



std::pair<std::shared_ptr<Qubit>, std::shared_ptr<Qubit>> QNode::CreateBellPair() {
  return processor_->CreateBellPair();
}



std::shared_ptr<QState> QNode::GetState(const std::shared_ptr<Qubit>& q) {
  return processor_->GetState(q);
}



bool QNode::Apply(const QGate& gate, const std::vector<std::shared_ptr<Qubit>>& qs) {
  return processor_->Apply(gate, qs);
}



bool QNode::Apply(const q2ns::Matrix& gate, const std::vector<std::shared_ptr<Qubit>>& qs) {
  return Apply(q2ns::gates::Custom(gate), qs);
}



int QNode::Measure(const std::shared_ptr<Qubit>& q, q2ns::Basis basis) {
  return processor_->Measure(q, basis);
}



std::pair<int, int> QNode::MeasureBell(const std::shared_ptr<Qubit>& a,
                                       const std::shared_ptr<Qubit>& b) {
  return processor_->MeasureBell(a, b);
}



bool QNode::Send(std::shared_ptr<Qubit> q, uint32_t dstNodeId) {
  return networker_->Send(std::move(q), dstNodeId);
}



void QNode::SetRecvCallback(RecvCallback cb) {
  networker_->SetRecvCallback(std::move(cb));
}



void QNode::AdoptQubit(const std::shared_ptr<Qubit>& q) {
  processor_->AdoptQubit(q);
}



} // namespace q2ns