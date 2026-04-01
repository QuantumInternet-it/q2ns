/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/
/**
 * @file q2ns-swap-app.cc
 * @brief Defines q2ns::SwapApp.
 */
#include "ns3/q2ns-swap-app.h"

#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/ipv4-address.h"
#include "ns3/log.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/socket.h"
#include "ns3/string.h"
#include "ns3/tcp-socket-factory.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/uinteger.h"

#include "ns3/names.h"

#include "ns3/q2ns-analysis.h"
#include "ns3/q2ns-netcontroller.h"
#include "ns3/q2ns-qgate.h"
#include "ns3/q2ns-qnode.h"
#include "ns3/q2ns-qstate.h"
#include "ns3/q2ns-qubit.h"

#include <algorithm>
#include <cstring>


namespace q2ns {

NS_LOG_COMPONENT_DEFINE("SwapApp");
NS_OBJECT_ENSURE_REGISTERED(SwapApp);

// Small ctrl header for demux at endpoints
struct CtrlHeader {
  uint64_t sid; // session id
  uint8_t m1;   // Z bit
  uint8_t m2;   // X bit
} __attribute__((packed));

ns3::TypeId SwapApp::GetTypeId() {
  using namespace ns3;
  static TypeId tid =
      TypeId("q2ns::SwapApp")
          .SetParent<Application>()
          .SetGroupName("q2ns")
          .AddTraceSource("RoundStart", "Session round begins (sid,time).",
                          MakeTraceSourceAccessor(&SwapApp::m_traceRoundStart),
                          "ns3::TracedCallback::Uint64Time")
          .AddTraceSource("BSMDone", "Repeater BSM complete (sid,time,m1,m2).",
                          MakeTraceSourceAccessor(&SwapApp::m_traceBSMDone),
                          "ns3::TracedCallback::Uint64TimeTimeUint8Uint8")
          .AddTraceSource("CtrlSent", "Repeater sent ctrl bits (sid,time,m1,m2).",
                          MakeTraceSourceAccessor(&SwapApp::m_traceCtrlSent),
                          "ns3::TracedCallback::Uint64TimeTimeUint8Uint8")
          .AddTraceSource("FrameResolved", "Endpoint accumulated all BSMs (sid,time,M1,M2).",
                          MakeTraceSourceAccessor(&SwapApp::m_traceFrameResolved),
                          "ns3::TracedCallback::Uint64TimeTimeUint8Uint8")
          .AddTraceSource("CorrectionApplied",
                          "Emitted when the sink applies final Pauli corrections for a session.",
                          ns3::MakeTraceSourceAccessor(&SwapApp::m_traceCorrectionApplied),
                          "ns3::TracedCallback<uint64_t, ns3::Time>")
          .AddTraceSource("VerifyFidelity", "Endpoint Bell fidelity check (sid,time,value,stderr).",
                          ns3::MakeTraceSourceAccessor(&SwapApp::m_traceVerifyFidelity),
                          "ns3::TracedCallback::Uint64TimeDoubleDouble");

  return tid;
}

void SwapApp::StartApplication() {
  // Subscribe once to qubit arrivals for all sessions on this node
  if (auto qnode = ns3::DynamicCast<q2ns::QNode>(GetNode())) {
    qnode->SetRecvCallback([this](std::shared_ptr<q2ns::Qubit> q) {
      if (!q)
        return;
      const std::string& L = q->GetLabel();
      constexpr const char* PFX_LIN = "swap_in_from_prev_s"; // goes into qPrev
      constexpr const char* PFX_RIN = "swap_in_from_next_s"; // goes into qNext

      if (L.rfind(PFX_LIN, 0) == 0) {
        uint64_t sid = std::strtoull(L.c_str() + std::strlen(PFX_LIN), nullptr, 10);
        auto it = m_sessions.find(sid);
        if (it != m_sessions.end()) {
          auto& s = it->second;
          if (!s.qPrev)
            s.qPrev = std::move(q);

          // If repeater and s.qNext exists (local), trigger BSM now:
          if ((s.cfg.role == Role::Repeater) && s.qNext && !s.haveBSM) {
            MaybeDoBsmAndAnnounce(s);
          }

          // If this is an endpoint role, this is its final qubit (Prev endpoint keeps qPrev)
          if (s.cfg.role != Role::Repeater && !s.haveQ) {
            s.targetQubit = s.qPrev; // endpoint keeps its arrived qubit
            s.haveQ = true;
            if (s.haveF)
              TryApply(s); // apply if frame already complete
          }
        }
      } else if (L.rfind(PFX_RIN, 0) == 0) {
        uint64_t sid = std::strtoull(L.c_str() + std::strlen(PFX_RIN), nullptr, 10);
        auto it = m_sessions.find(sid);
        if (it != m_sessions.end()) {
          auto& s = it->second;
          if (!s.qNext)
            s.qNext = std::move(q);

          // If repeater and s.qPrev exists (local), trigger BSM now:
          if ((s.cfg.role == Role::Repeater) && s.qPrev && !s.haveBSM) {
            MaybeDoBsmAndAnnounce(s);
          }

          // If this is an endpoint role, this is its final qubit (Next endpoint keeps qNext)
          if (s.cfg.role != Role::Repeater && !s.haveQ) {
            s.targetQubit = s.qNext; // endpoint keeps its arrived qubit
            s.haveQ = true;
            if (s.haveF)
              TryApply(s);
          }
        }
      }
    });
  }

  for (auto& kv : m_sessions) {
    ScheduleSession(kv.second);
  }
}

void SwapApp::StopApplication() {
  for (auto& kv : m_udp4)
    if (kv.second)
      kv.second->Close();
  for (auto& kv : m_udp6)
    if (kv.second)
      kv.second->Close();
  for (auto& kv : m_tcp)
    if (kv.second)
      kv.second->Close();
  m_udp4.clear();
  m_udp6.clear();
  m_tcp.clear();
  m_tcpPendingOnce.clear();
  m_tcpPending.clear();
  m_sinksByPort.clear();
}

void SwapApp::AddSession(const SessionConfig& cfg) {
  SessionState s;
  s.cfg = cfg;
  m_sessions.emplace(cfg.sid, std::move(s));

  // If endpoint, ensure sink for its ctrlPort so late-added sessions still work
  if (cfg.role != Role::Repeater) {
    EnsureSink(cfg.ctrlPort, cfg.proto);
  }
}

void SwapApp::ScheduleSession(const SessionState& s) {
  ns3::Simulator::Schedule(s.cfg.start, [this, sid = s.cfg.sid] {
    auto it = m_sessions.find(sid);
    if (it == m_sessions.end())
      return;
    m_traceRoundStart(sid, ns3::Simulator::Now());

    auto& st = it->second;

    switch (st.cfg.role) {
    case Role::Repeater:
      DoRepeaterRound(st);
      break;
    case Role::Prev:
    case Role::Next:
      DoEndpointStart(st);
      break;
    }
  });
}

void SwapApp::DoRepeaterRound(SessionState& s) {
  using namespace ns3;
  auto qnode = ns3::DynamicCast<q2ns::QNode>(GetNode());
  NS_ASSERT_MSG(qnode, "SwapApp(repeater) must run on a QNode");

  // Generate RIGHT link if requested
  if (s.cfg.genNext) {
    auto bellR = qnode->CreateBellPair();
    s.qNext = bellR.first; // keep local for BSM
    auto rRemote = bellR.second;
    rRemote->SetLabel(std::string("swap_in_from_prev_s") + std::to_string(s.cfg.sid));
    bool okR = qnode->Send(rRemote, s.cfg.nextPeerId);
    NS_ASSERT_MSG(okR, "Send(next) failed");
  }

  // Generate LEFT link if requested
  if (s.cfg.genPrev) {
    auto bellL = qnode->CreateBellPair();
    s.qPrev = bellL.first; // keep local for BSM
    auto lRemote = bellL.second;
    lRemote->SetLabel(std::string("swap_in_from_next_s") + std::to_string(s.cfg.sid));
    bool okL = qnode->Send(lRemote, s.cfg.prevPeerId);
    NS_ASSERT_MSG(okL, "Send(prev) failed");
  }

  if (s.qPrev && s.qNext && !s.haveBSM) {
    MaybeDoBsmAndAnnounce(s);
  }
}

void SwapApp::DoEndpointStart(SessionState& s) {
  auto qnode = ns3::DynamicCast<q2ns::QNode>(GetNode());
  if (!qnode)
    return;

  if (s.cfg.role == Role::Prev && s.cfg.genNext) {
    auto bell = qnode->CreateBellPair();
    s.targetQubit = bell.first; // local half: this is Alice's final qubit
    s.haveQ = true;

    auto remote = bell.second; // remote goes to the first repeater
    remote->SetLabel(std::string("swap_in_from_prev_s") + std::to_string(s.cfg.sid));
    bool ok = qnode->Send(remote, s.cfg.nextPeerId);
    NS_ASSERT_MSG(ok, "Source send(remote) failed");
  }

  // Sink (Next) does nothing here; it will receive its qubit later.
}


void SwapApp::EnsureSink(uint16_t port, const std::string& proto) {
  if (m_sinksByPort.count(port))
    return;
  using namespace ns3;

  const std::string factory =
      (proto == "tcp" || proto == "TCP") ? "ns3::TcpSocketFactory" : "ns3::UdpSocketFactory";
  ApplicationContainer apps;
  if (m_useIpv6) {
    PacketSinkHelper sinkHelper(factory, Inet6SocketAddress(Ipv6Address::GetAny(), port));
    apps = sinkHelper.Install(GetNode());
  } else {
    PacketSinkHelper sinkHelper(factory, InetSocketAddress(Ipv4Address::GetAny(), port));
    apps = sinkHelper.Install(GetNode());
  }

  auto sink = ns3::DynamicCast<ns3::PacketSink>(apps.Get(0));

  // Bind Rx callback with the port we're listening on
  sink->TraceConnectWithoutContext("Rx", ns3::MakeCallback(&q2ns::SwapApp::OnCtrlRx, this));

  m_sinksByPort.emplace(port, sink);
}


void SwapApp::OnCtrlRx(ns3::Ptr<const ns3::Packet> pkt, const ns3::Address& from) {

  if (!pkt) {
    NS_LOG_WARN("In OnCtrlRx. pkt is nullptr.");
    return;
  }
  CtrlHeader hdr{};
  if (pkt->GetSize() < sizeof(hdr)) {
    NS_LOG_WARN("In OnCtrlRx. pkt is too small.");
    return;
  }
  pkt->CopyData(reinterpret_cast<uint8_t*>(&hdr), sizeof(hdr));

  auto it = m_sessions.find(hdr.sid);
  if (it == m_sessions.end())
    return; // not our session
  auto& s = it->second;

  s.accumM1 ^= hdr.m1;
  s.accumM2 ^= hdr.m2;
  s.recv++;

  NS_LOG_INFO("EP sid=" << s.cfg.sid << " recv=" << s.recv << " expect=" << s.cfg.expectedMsgs
                        << " M=(" << int(s.accumM1) << "," << int(s.accumM2) << ")");
  if (s.recv >= s.cfg.expectedMsgs) {

    s.haveF = true;
    m_traceFrameResolved(s.cfg.sid, ns3::Simulator::Now(), s.accumM1, s.accumM2);
    TryApply(s);
  }
}

void SwapApp::MaybeDoBsmAndAnnounce(SessionState& s) {
  if (s.haveBSM || !s.qPrev || !s.qNext)
    return;
  auto qnode = ns3::DynamicCast<q2ns::QNode>(GetNode());
  if (!qnode)
    return;

  auto bits = qnode->MeasureBell(s.qPrev, s.qNext);
  s.haveBSM = true;

  uint8_t m1 = static_cast<uint8_t>(bits.first);
  uint8_t m2 = static_cast<uint8_t>(bits.second);
  m_traceBSMDone(s.cfg.sid, ns3::Simulator::Now(), m1, m2);

  CtrlHeader hdr{s.cfg.sid, m1, m2};

  std::vector<uint8_t> buf(std::max<uint32_t>(sizeof(CtrlHeader), m_payloadBytes), 0);
  std::memcpy(buf.data(), &hdr, sizeof(CtrlHeader));
  ns3::Ptr<ns3::Packet> pkt = ns3::Create<ns3::Packet>(buf.data(), buf.size());


  auto sendV4 = [&](const ns3::Ipv4Address& dst) {
    if (dst == ns3::Ipv4Address("0.0.0.0"))
      return;
    if (s.cfg.proto == "udp" || s.cfg.proto == "UDP") {
      auto sock = GetOrCreateUdpSender(s.cfg.ctrlPort, /*v6=*/false);
      sock->SendTo(pkt->Copy(), 0, ns3::InetSocketAddress(dst, s.cfg.ctrlPort));
    } else {
      ns3::Address a = ns3::InetSocketAddress(dst, s.cfg.ctrlPort);
      auto sock = GetOrCreateTcpSender(a, /*v6=*/false, s.cfg.ctrlPort);
      auto id = reinterpret_cast<uintptr_t>(PeekPointer(sock));
      // Try immediate send; if not connected yet, queue once and let OnTcpConnected handle it.
      int sent = sock->Send(pkt->Copy());
      if (sent < 0 && sock->GetErrno() == ns3::Socket::ERROR_NOTCONN) {
        m_tcpPendingOnce[id] = pkt->Copy();
      }
    }
  };

  auto sendV6 = [&](const ns3::Ipv6Address& dst6) {
    if (dst6 == ns3::Ipv6Address::GetAny())
      return;
    if (s.cfg.proto == "udp" || s.cfg.proto == "UDP") {
      ns3::Ptr<ns3::Socket> sock =
          ns3::Socket::CreateSocket(GetNode(), ns3::UdpSocketFactory::GetTypeId());
      // Do NOT Bind() for v6. Let Connect choose a proper global source & egress.
      sock->Connect(ns3::Inet6SocketAddress(dst6, s.cfg.ctrlPort));
      sock->Send(pkt->Copy());
      sock->Close();
    } else {
      ns3::Address a = ns3::Inet6SocketAddress(dst6, s.cfg.ctrlPort);
      auto sock = GetOrCreateTcpSender(a, /*v6=*/true, s.cfg.ctrlPort);
      auto id = reinterpret_cast<uintptr_t>(PeekPointer(sock));
      int sent = sock->Send(pkt->Copy());
      if (sent < 0 && sock->GetErrno() == ns3::Socket::ERROR_NOTCONN) {
        m_tcpPendingOnce[id] = pkt->Copy();
      }
    }
  };



  if (m_useIpv6) {
    NS_LOG_INFO("Prev: " << s.cfg.prevEndAddr6 << " | Next: " << s.cfg.nextEndAddr6);
    // sendV6(s.cfg.prevEndAddr6);
    sendV6(s.cfg.nextEndAddr6);
  } else {
    // sendV4(s.cfg.prevEndAddr);
    sendV4(s.cfg.nextEndAddr);
  }

  m_traceCtrlSent(s.cfg.sid, ns3::Simulator::Now(), m1, m2);
}


void SwapApp::TryApply(SessionState& s) {

  if (!s.cfg.applyCorrections) {
    s.done = true;
    return;
  } else {
    if (s.done || !s.haveQ || !s.haveF)
      return;
    auto qnode = ns3::DynamicCast<q2ns::QNode>(GetNode());
    if (!qnode || !s.targetQubit)
      return;

    // Apply X then Z (consistent with Teleportation)
    if (s.accumM2)
      qnode->Apply(q2ns::gates::X(), {s.targetQubit});
    if (s.accumM1)
      qnode->Apply(q2ns::gates::Z(), {s.targetQubit});

    s.done = true;

    // Fidelity verification (endpoint roles only; opt-in)
    if ((s.cfg.role == Role::Prev || s.cfg.role == Role::Next) && s.cfg.verifyFidelity) {
      auto qnode = ns3::DynamicCast<q2ns::QNode>(GetNode());
      if (qnode && s.targetQubit) {
        // Actual post-swap 2-qubit state containing this qubit
        auto actual = m_nc->GetState(s.targetQubit); // const QState* (shared_ptr)
        if (actual && actual->NumQubits() == 2) {    // sanity; typical for clean swap chains
          // Build reference |Phi+> in the same backend by making a fresh Bell pair locally
          auto refBell = qnode->CreateBellPair();
          auto ref = m_nc->GetState(refBell.first); // same backend as current default

          if (ref) {
            auto f = q2ns::analysis::Fidelity(*actual, *ref); // value + stderr
            m_traceVerifyFidelity(s.cfg.sid, ns3::Simulator::Now(), f);

            NS_LOG_INFO("Final State:\n" << actual);

            if (f < s.cfg.verifyThreshold) {
              NS_LOG_WARN("Swap fidelity check FAILED for sid=" << s.cfg.sid << " value=" << f
                                                                << " < threshold="
                                                                << s.cfg.verifyThreshold);
              // Optional hard fail for CI:
              // NS_ASSERT_MSG(false, "Entanglement swap fidelity below threshold");
            }
          }
        }
      }
    }

    m_traceCorrectionApplied(s.cfg.sid, ns3::Simulator::Now());
  }
}

void SwapApp::OnTcpConnected(ns3::Ptr<ns3::Socket> sock) {
  auto id = reinterpret_cast<uintptr_t>(PeekPointer(sock));
  auto it = m_tcpPendingOnce.find(id);
  if (it != m_tcpPendingOnce.end()) {
    sock->Send(it->second);
    m_tcpPendingOnce.erase(it);
  }
}


void SwapApp::OnTcpConnectFail(ns3::Ptr<ns3::Socket> s) {
  auto key = reinterpret_cast<uintptr_t>(PeekPointer(s));
  m_tcpPending.erase(key);
  NS_LOG_WARN("SwapApp: TCP connect failed");
  s->Close();
}


ns3::Ptr<ns3::Socket> SwapApp::GetOrCreateUdpSender(uint16_t port, bool v6) {
  using namespace ns3;
  auto& map = v6 ? m_udp6 : m_udp4;
  auto it = map.find(port);
  if (it != map.end())
    return it->second;

  Ptr<Socket> s = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
  // UDP senders don't have to bind, but binding to a local port is fine.
  if (v6)
    s->Bind(Inet6SocketAddress(Ipv6Address::GetAny(), 0));
  else
    s->Bind(InetSocketAddress(Ipv4Address::GetAny(), 0));
  map.emplace(port, s);
  return s;
}

ns3::Ptr<ns3::Socket> SwapApp::GetOrCreateTcpSender(const ns3::Address& dst, bool v6,
                                                    uint16_t port) {
  using namespace ns3;
  // stringify address to key
  std::ostringstream oss;
  if (v6) {
    Inet6SocketAddress a = Inet6SocketAddress::ConvertFrom(dst);
    oss << a.GetIpv6();
  } else {
    InetSocketAddress a = InetSocketAddress::ConvertFrom(dst);
    oss << a.GetIpv4();
  }
  TcpKey key{v6, port, oss.str()};
  auto it = m_tcp.find(key);
  if (it != m_tcp.end())
    return it->second;

  Ptr<Socket> sock = Socket::CreateSocket(GetNode(), TcpSocketFactory::GetTypeId());
  sock->SetConnectCallback(MakeCallback(&SwapApp::OnTcpConnected, this),
                           MakeCallback(&SwapApp::OnTcpConnectFail, this));

  // kick off connect once; send will happen in OnTcpConnected if needed
  sock->Connect(dst);
  m_tcp.emplace(key, sock);
  return sock;
}


ns3::Ptr<ns3::Socket> SwapApp::GetOrCreateUdpV6Connected(const ns3::Ipv6Address& dst,
                                                         uint16_t port) {
  using namespace ns3;
  // key by textual dest + port (same scheme as TCP)
  std::ostringstream oss;
  oss << dst;
  TcpKey key{/*v6=*/true, port, oss.str()};
  auto it = m_udp6_conn.find(key);
  if (it != m_udp6_conn.end())
    return it->second;

  Ptr<Socket> s = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
  // IMPORTANT: do NOT bind to :: here. Let Connect() choose a proper global source.
  s->Connect(Inet6SocketAddress(dst, port));
  m_udp6_conn.emplace(key, s);
  return s;
}



} // namespace q2ns