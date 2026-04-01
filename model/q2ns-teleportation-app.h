/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/
/**
 * @file q2ns-teleportation-app.h
 * @brief Declares q2ns::TeleportationApp, the teleportation ns-3 Application.
 */
#pragma once

#include "ns3/application.h"
#include "ns3/data-rate.h"
#include "ns3/ipv4-address.h"
#include "ns3/ptr.h"
#include "ns3/socket.h"
#include "ns3/traced-callback.h"

#include <cstdint>
#include <string>

namespace ns3 {
class Packet;
class Address;
class PacketSink;
class Application; // base class already included via application.h
} // namespace ns3

namespace q2ns {

class NetController;
class QNode;
class Qubit;
class QState;


class TeleportationApp final : public ns3::Application {
public:
  static ns3::TypeId GetTypeId();

  TeleportationApp();
  ~TeleportationApp() override = default;

  // ns3::Application
  void StartApplication() override;
  void StopApplication() override;

  // Inject central orchestrator (not an ns-3 Object, so we use a plain setter)
  void SetNetController(NetController* nc);

  // User must set the state to be teleported
  void SetTeleportState(const std::shared_ptr<QState>& state);

  // TraceSources
  // Source-side (Alice) local events
  ns3::TracedCallback<uint64_t, ns3::Time> m_traceSourceStart;    // when Alice starts session
  ns3::TracedCallback<uint64_t, ns3::Time> m_traceSourceBellDone; // Bell measurement done
  ns3::TracedCallback<uint64_t, ns3::Time> m_traceSourceCtrlSent; // 2-bit ctrl sent

  // Sink-side (Bob) local events
  ns3::TracedCallback<uint64_t, ns3::Time> m_traceSinkStart;      // when Bob starts session
  ns3::TracedCallback<uint64_t, ns3::Time> m_traceSinkQArrive;    // qubits arrived
  ns3::TracedCallback<uint64_t, ns3::Time> m_traceSinkCtrlArrive; // ctrl bits arrived
  ns3::TracedCallback<uint64_t, ns3::Time> m_traceSinkCorrection; // correction completed

private:
  std::string m_role; // "source" | "sink"
  ns3::Ptr<q2ns::QNode> m_peerQNode;
  std::shared_ptr<QState> m_teleportState;
  ns3::Ipv4Address m_peer;      // ctrl peer addr (used by source)
  uint16_t m_ctrlPort{7000};    // per-session port
  std::string m_ctrlProto;      // "udp" | "tcp"
  std::string m_backend;        // "stab" | "ket" | ...
  std::string m_targetQubitTag; // e.g., "teleport_target"
  uint64_t m_sessionId{0};
  ns3::Time m_sessionStart{ns3::Seconds(0.0)};

  q2ns::NetController* m_nc{nullptr};      // central orchestrator (non-owning)
  ns3::Ptr<ns3::Packet> m_pendingCtrlPkt;  // Pending ctrl packet for TCP connect-complete
  ns3::Ptr<ns3::PacketSink> m_ctrlSink;    // sink (on Bob)
  ns3::Ptr<ns3::Application> m_ctrlSender; // one-shot sender (on Alice)

  void DoSourceStart();
  void DoSinkStart();

  void OnTcpConnectSuccess(ns3::Ptr<ns3::Socket> s);
  void OnTcpConnectFail(ns3::Ptr<ns3::Socket> s);

  void QubitArrivalHandler(std::shared_ptr<Qubit> q);

  void OnCtrlRx(ns3::Ptr<const ns3::Packet> pkt, const ns3::Address& from);

  void SendCtrlBits(uint8_t m1, uint8_t m2);

  void TryApplyCorrection();
  bool m_haveCtrl{false};
  bool m_haveQ{false};
  uint8_t m_m1{0}, m_m2{0};
};

} // namespace q2ns
