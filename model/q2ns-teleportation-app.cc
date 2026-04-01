/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/
/**
 * @file q2ns-teleportation-app.cc
 * @brief Defines q2ns::TeleportationApp.
 */
#include "ns3/q2ns-teleportation-app.h"
#include "ns3/q2ns-analysis.h"
#include "ns3/q2ns-netcontroller.h"
#include "ns3/q2ns-qgate.h"
#include "ns3/q2ns-qnode.h"
#include "ns3/q2ns-qstate-ket.h"
#include "ns3/q2ns-qubit.h"


#include "ns3/boolean.h"
#include "ns3/bulk-send-helper.h"
#include "ns3/data-rate.h"
#include "ns3/double.h"
#include "ns3/inet-socket-address.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4.h"
#include "ns3/log.h"
#include "ns3/names.h"
#include "ns3/node.h"
#include "ns3/on-off-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/packet-sink.h"
#include "ns3/packet.h"
#include "ns3/pointer.h"
#include "ns3/simulator.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"


#include <cmath>
#include <limits>
#include <sstream>
#include <string>

namespace q2ns {

NS_LOG_COMPONENT_DEFINE("TeleportationApp");
NS_OBJECT_ENSURE_REGISTERED(TeleportationApp);

ns3::TypeId TeleportationApp::GetTypeId() {
  static ns3::TypeId tid =
      ns3::TypeId("q2ns::TeleportationApp")
          .SetParent<ns3::Application>()
          .SetGroupName("q2ns")

          .AddAttribute("Role", "Session role for this app: 'source' (Alice) or 'sink' (Bob).",
                        ns3::StringValue("source"),
                        ns3::MakeStringAccessor(&TeleportationApp::m_role),
                        ns3::MakeStringChecker())

          .AddAttribute("PeerQNode", "Quantum peer (destination) as a direct object pointer.",
                        ns3::PointerValue(), // default null
                        ns3::MakePointerAccessor(&TeleportationApp::m_peerQNode),
                        ns3::MakePointerChecker<q2ns::QNode>())

          .AddAttribute("Peer",
                        "Classical control-plane peer address (destination for ctrl packets, used "
                        "by source).",
                        ns3::Ipv4AddressValue(ns3::Ipv4Address("0.0.0.0")),
                        ns3::MakeIpv4AddressAccessor(&TeleportationApp::m_peer),
                        ns3::MakeIpv4AddressChecker())

          .AddAttribute(
              "CtrlPort", "UDP/TCP port used for teleportation control packets (per-session).",
              ns3::UintegerValue(7000), ns3::MakeUintegerAccessor(&TeleportationApp::m_ctrlPort),
              ns3::MakeUintegerChecker<uint16_t>())

          .AddAttribute("ClassicalProtocol", "Control-plane transport protocol: 'udp' or 'tcp'.",
                        ns3::StringValue("udp"),
                        ns3::MakeStringAccessor(&TeleportationApp::m_ctrlProto),
                        ns3::MakeStringChecker())

          .AddAttribute(
              "Backend", "Quantum state backend to use (e.g., 'ket').", ns3::StringValue("ket"),
              ns3::MakeStringAccessor(&TeleportationApp::m_backend), ns3::MakeStringChecker())

          .AddAttribute("SessionId", "Unique identifier for the logical teleportation session.",
                        ns3::UintegerValue(0),
                        ns3::MakeUintegerAccessor(&TeleportationApp::m_sessionId),
                        ns3::MakeUintegerChecker<uint64_t>())

          .AddAttribute("TargetQubitTag",
                        "Label used to fetch Bob's target qubit from the NetController.",
                        ns3::StringValue("teleport_target_s"),
                        ns3::MakeStringAccessor(&TeleportationApp::m_targetQubitTag),
                        ns3::MakeStringChecker())

          .AddAttribute("SessionStartTime", "Simulation time at which this app begins its session.",
                        ns3::TimeValue(ns3::Seconds(0.0)),
                        ns3::MakeTimeAccessor(&TeleportationApp::m_sessionStart),
                        ns3::MakeTimeChecker())

          .AddTraceSource("SourceStart", "Alice session start (sid,time)",
                          ns3::MakeTraceSourceAccessor(&TeleportationApp::m_traceSourceStart),
                          "ns3::TracedCallback::Uint64Time")

          .AddTraceSource("SourceBellDone", "Alice Bell measurement done (sid,time)",
                          ns3::MakeTraceSourceAccessor(&TeleportationApp::m_traceSourceBellDone),
                          "ns3::TracedCallback::Uint64Time")

          .AddTraceSource("SourceCtrlSent", "Alice sent ctrl bits (sid,time)",
                          ns3::MakeTraceSourceAccessor(&TeleportationApp::m_traceSourceCtrlSent),
                          "ns3::TracedCallback::Uint64Time")

          .AddTraceSource("SinkStart", "Bob session start (sid,time)",
                          ns3::MakeTraceSourceAccessor(&TeleportationApp::m_traceSinkStart),
                          "ns3::TracedCallback::Uint64Time")

          .AddTraceSource("SinkQArrive", "Bob received his qubit (sid,time)",
                          ns3::MakeTraceSourceAccessor(&TeleportationApp::m_traceSinkQArrive),
                          "ns3::TracedCallback::Uint64Time")

          .AddTraceSource("SinkCtrlArrive", "Bob received ctrl bits (sid,time)",
                          ns3::MakeTraceSourceAccessor(&TeleportationApp::m_traceSinkCtrlArrive),
                          "ns3::TracedCallback::Uint64Time")

          .AddTraceSource("SinkCorrection", "Bob applied correction (sid,time)",
                          ns3::MakeTraceSourceAccessor(&TeleportationApp::m_traceSinkCorrection),
                          "ns3::TracedCallback::Uint64Time");

  return tid;
}

void TeleportationApp::SetNetController(NetController* nc) {
  m_nc = nc;
}

void TeleportationApp::SetTeleportState(const std::shared_ptr<QState>& st) {
  m_teleportState = st;
}


TeleportationApp::TeleportationApp() = default;

void TeleportationApp::StartApplication() {

  // Ensure the transmitted qubit has a unique id (append once)
  {
    const std::string suffix = "_s" + std::to_string(m_sessionId);
    if (m_targetQubitTag.size() < suffix.size() ||
        m_targetQubitTag.substr(m_targetQubitTag.size() - suffix.size()) != suffix) {
      m_targetQubitTag += suffix;
    }
  }

  // Defer work to SessionStart to allow staggered starts via Attribute.
  ns3::Simulator::Schedule(m_sessionStart, [this]() {
    const auto now = ns3::Simulator::Now().GetSeconds();
    NS_LOG_INFO("[" << now << "s] " << ns3::Names::FindName(GetNode()) << " TeleportationApp("
                    << m_role << ") start");
    if (m_role == "source") {
      DoSourceStart();
    } else if (m_role == "sink") {
      DoSinkStart();
    } else {
      NS_LOG_ERROR("TeleportationApp role must be 'source' or 'sink', got: " << m_role);
    }
  });
}

void TeleportationApp::StopApplication() {
  if (m_ctrlSender) {
    m_ctrlSender = nullptr;
  }
  if (m_ctrlSink) {
    m_ctrlSink = nullptr;
  }
}

void TeleportationApp::DoSourceStart() {

  m_traceSourceStart(m_sessionId, ns3::Simulator::Now());

  auto qnode = ns3::DynamicCast<q2ns::QNode>(GetNode());
  if (!qnode) {
    NS_LOG_WARN("TeleportationApp(source): no QNode bound to this ns-3 node.");
    return;
  }

  if (!m_teleportState) {
    NS_LOG_ERROR(
        "TeleportationApp(source): no source QState to be teleported provided. Aborting session.");
    return;
  }

  auto psi = qnode->CreateQubit(m_teleportState, "teleport_src_s" + std::to_string(m_sessionId));

  auto [aHalf, bHalf] = qnode->CreateBellPair();
  aHalf->SetLabel("epr_A_s" + std::to_string(m_sessionId));
  bHalf->SetLabel(m_targetQubitTag);

  bool sent = false;
  if (m_peerQNode) {
    sent = qnode->Send(bHalf, m_peerQNode->GetId());
  } else {
    NS_LOG_INFO("[" << ns3::Simulator::Now() << "s] " << ns3::Names::FindName(GetNode())
                    << " TeleportationApp(source): no PeerQNode set; qubit cannot be sent.\n");
    sent = false;
  }
  if (!sent) {
    NS_LOG_WARN("TeleportationApp(source): failed to send EPR half to Bob; proceeding anyway");
  }

  auto [m1, m2] = qnode->MeasureBell(psi, aHalf);
  m_traceSourceBellDone(m_sessionId, ns3::Simulator::Now());

  SendCtrlBits(m1, m2);
}

void TeleportationApp::DoSinkStart() {

  m_traceSinkStart(m_sessionId, ns3::Simulator::Now());

  auto qnode = ns3::DynamicCast<q2ns::QNode>(GetNode());
  qnode->SetRecvCallback(MakeCallback(&TeleportationApp::QubitArrivalHandler, this));

  using namespace ns3;

  Address bindAddr = InetSocketAddress(Ipv4Address::GetAny(), m_ctrlPort);
  if (m_ctrlProto == "udp") {
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", bindAddr);
    auto apps = sinkHelper.Install(GetNode());
    m_ctrlSink = DynamicCast<PacketSink>(apps.Get(0));
  } else {
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", bindAddr);
    auto apps = sinkHelper.Install(GetNode());
    m_ctrlSink = DynamicCast<PacketSink>(apps.Get(0));
  }

  if (m_ctrlSink) {
    m_ctrlSink->TraceConnectWithoutContext("Rx",
                                           ns3::MakeCallback(&TeleportationApp::OnCtrlRx, this));
  } else {
    NS_LOG_ERROR("Failed to install PacketSink for ctrl on port " << m_ctrlPort);
  }
}

void TeleportationApp::OnTcpConnectSuccess(ns3::Ptr<ns3::Socket> s) {
  if (m_pendingCtrlPkt) {
    s->Send(m_pendingCtrlPkt);
    m_pendingCtrlPkt = nullptr;
  }
  s->Close();
}

void TeleportationApp::OnTcpConnectFail(ns3::Ptr<ns3::Socket> s) {
  NS_LOG_WARN("TeleportationApp(source): TCP connect failed\n");
  s->Close();
}


void TeleportationApp::SendCtrlBits(uint8_t m1, uint8_t m2) {
  using namespace ns3;


  m_traceSourceCtrlSent(m_sessionId, ns3::Simulator::Now());
  NS_LOG_INFO("[" << ns3::Simulator::Now() << "s] " << ns3::Names::FindName(GetNode())
                  << " TeleportationApp(source): Sending classical packet.");

  uint8_t buf[2] = {m1, m2};
  Ptr<Packet> pkt = Create<Packet>(buf, 2);

  if (m_ctrlProto == "udp") {
    Ptr<Socket> sock = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    InetSocketAddress dst(m_peer, m_ctrlPort);
    if (sock->Connect(dst) == 0) {
      sock->Send(pkt);
    } else {
      NS_LOG_WARN("TeleportationApp(source): UDP connect failed to " << m_peer << ":" << m_ctrlPort
                                                                     << "\n");
    }
    sock->Close();
    return;
  }

  Ptr<Socket> sock = Socket::CreateSocket(GetNode(), TcpSocketFactory::GetTypeId());
  InetSocketAddress dst(m_peer, m_ctrlPort);

  // Save the packet; send it when the TCP handshake completes.
  m_pendingCtrlPkt = pkt;

  sock->SetConnectCallback(ns3::MakeCallback(&TeleportationApp::OnTcpConnectSuccess, this),
                           ns3::MakeCallback(&TeleportationApp::OnTcpConnectFail, this));

  int rc = sock->Connect(dst);
  if (rc != 0) {
    NS_LOG_WARN("TeleportationApp(source): TCP connect() immediate failure to "
                << m_peer << ":" << m_ctrlPort << "\n");
    sock->Close();
    m_pendingCtrlPkt = nullptr;
  }
}

void TeleportationApp::QubitArrivalHandler(std::shared_ptr<Qubit> q) {

  m_traceSinkQArrive(m_sessionId, ns3::Simulator::Now());
  NS_LOG_INFO("[" << ns3::Simulator::Now() << "s] " << ns3::Names::FindName(GetNode())
                  << " TeleportationApp(sink): Received qubit " + q->GetLabel() + ".");

  m_haveQ = true;
  TryApplyCorrection();
}

void TeleportationApp::OnCtrlRx(ns3::Ptr<const ns3::Packet> pkt, const ns3::Address& /*from*/) {
  using namespace ns3;

  m_traceSinkCtrlArrive(m_sessionId, ns3::Simulator::Now());
  NS_LOG_INFO("[" << ns3::Simulator::Now() << "s] " << ns3::Names::FindName(GetNode())
                  << " TeleportationApp(sink): Received classical packet.");

  uint8_t bits[2] = {0, 0};
  if (!pkt || pkt->GetSize() < 2) {
    NS_LOG_WARN("TeleportationApp(sink): control packet too small (" << (pkt ? pkt->GetSize() : 0)
                                                                     << " bytes)\n");
    return;
  }
  pkt->CopyData(bits, 2);
  m_m1 = bits[0];
  m_m2 = bits[1];
  m_haveCtrl = true;


  TryApplyCorrection();
}

void TeleportationApp::TryApplyCorrection() {
  if (!(m_haveCtrl && m_haveQ))
    return;

  if (auto qnode = ns3::DynamicCast<q2ns::QNode>(GetNode())) {
    auto qB = qnode->GetQubit(m_targetQubitTag);
    if (qB) {
      if (m_m2)
        qnode->Apply(q2ns::gates::X(), {qB});
      if (m_m1)
        qnode->Apply(q2ns::gates::Z(), {qB});

      m_traceSinkCorrection(m_sessionId, ns3::Simulator::Now());
    } else {
      // We thought we had Q, but label not found -- keep waiting.
      return;
    }
  }
}


} // namespace q2ns