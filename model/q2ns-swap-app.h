/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/
/**
 * @file q2ns-swap-app.h
 * @brief Declares q2ns::SwapApp, the entanglement-swapping ns-3 Application.
 */
#pragma once

#include "ns3/application.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv6-address.h"
#include "ns3/packet-sink.h"
#include "ns3/ptr.h"
#include "ns3/traced-callback.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace ns3 {
class Packet;
class PacketSink;
class Socket;
} // namespace ns3

namespace q2ns {

class QNode;
class NetController;
class Qubit;

class SwapApp final : public ns3::Application {
public:
  enum class Role : uint8_t { Prev = 0, Repeater = 1, Next = 2 };

  struct SessionConfig {
    uint64_t sid{0};
    Role role{Role::Repeater};
    ns3::Time start{ns3::Seconds(0.0)};
    std::string proto{"udp"}; // "udp" | "tcp"
    uint16_t ctrlPort{7000};

    // Repeater-specific
    uint64_t prevPeerId{0};
    uint64_t nextPeerId{0};
    ns3::Ipv4Address prevEndAddr{ns3::Ipv4Address("0.0.0.0")};
    ns3::Ipv4Address nextEndAddr{ns3::Ipv4Address("0.0.0.0")};
    ns3::Ipv6Address prevEndAddr6{ns3::Ipv6Address::GetAny()};
    ns3::Ipv6Address nextEndAddr6{ns3::Ipv6Address::GetAny()};

    // Endpoint-specific
    uint64_t expectedMsgs{1};     // number of repeater BSM packets to XOR
    bool genPrev{false};          // generate the prev link and send remote to prev neighbor
    bool genNext{false};          // generate the next link and send remote to next neighbor
    bool applyCorrections{false}; // only true on Sink

    bool verifyFidelity{false};
    double verifyThreshold{0.90};
  };

  // Classical network info
  void SetPayloadBytes(uint32_t n) {
    m_payloadBytes = n;
  }
  void SetUseIpv6(bool v6) {
    m_useIpv6 = v6;
  }
  void SetPeerAddress(ns3::Ipv4Address v4) {
    m_peerV4 = v4;
  }
  void SetPeerAddress6(ns3::Ipv6Address v6) {
    m_peerV6 = v6;
  }

  static ns3::TypeId GetTypeId();

  SwapApp() = default;
  ~SwapApp() override = default;

  // ns3::Application
  void StartApplication() override;
  void StopApplication() override;

  // External wiring
  void SetNetController(NetController* nc) {
    m_nc = nc;
  }

  // Add one session (may be called multiple times before StartApplication or during sim setup)
  void AddSession(const SessionConfig& cfg);

  // Traces (include sid to avoid per-session trace creation)
  ns3::TracedCallback<uint64_t, ns3::Time> m_traceRoundStart; // repeater/endpoint sees its start
  ns3::TracedCallback<uint64_t, ns3::Time, uint8_t, uint8_t> m_traceBSMDone;  // repeater BSM done
  ns3::TracedCallback<uint64_t, ns3::Time, uint8_t, uint8_t> m_traceCtrlSent; // repeater sends bits
  ns3::TracedCallback<uint64_t, ns3::Time, uint8_t, uint8_t>
      m_traceFrameResolved;                                          // endpoint frame ready
  ns3::TracedCallback<uint64_t, ns3::Time> m_traceCorrectionApplied; // endpoint applied Pauli
  ns3::TracedCallback<uint64_t, ns3::Time, double> m_traceVerifyFidelity;


private:
  struct SessionState {
    SessionConfig cfg;

    // Endpoint state
    std::shared_ptr<q2ns::Qubit> targetQubit;
    bool haveQ{false};
    bool haveF{false};
    bool done{false};
    uint8_t accumM1{0}, accumM2{0};
    uint32_t recv{0};

    // Repeater state
    std::shared_ptr<Qubit> qPrev; // the qubit tied to the prev link at this repeater
    std::shared_ptr<Qubit> qNext; // the qubit tied to the next link at this repeater
    bool haveBSM{false};
  };

  NetController* m_nc{nullptr};

  // All sessions on this node
  std::unordered_map<uint64_t, SessionState> m_sessions; // keyed by sid

  // Control-plane sinks per port
  std::unordered_map<uint16_t, ns3::Ptr<ns3::PacketSink>> m_sinksByPort;

  uint32_t m_payloadBytes{16}; // defaults to small
  bool m_useIpv6{false};
  ns3::Ipv4Address m_peerV4;
  ns3::Ipv6Address m_peerV6;

  // Internal helpers
  void ScheduleSession(const SessionState& s);
  void DoRepeaterRound(SessionState& s);
  void DoEndpointStart(SessionState& s);

  void OnCtrlRx(ns3::Ptr<const ns3::Packet> pkt, const ns3::Address& from);
  void EnsureSink(uint16_t port, const std::string& proto);

  // Repeater BSM attempt
  void MaybeDoBsmAndAnnounce(SessionState& s);

  void TryApply(SessionState& s);

  // TCP helpers (UDP is default)
  void OnTcpConnected(ns3::Ptr<ns3::Socket> s);
  void OnTcpConnectFail(ns3::Ptr<ns3::Socket> s);


  std::unordered_map<uint16_t, ns3::Ptr<ns3::Socket>> m_udp4; // keyed by local send port (optional)
  std::unordered_map<uint16_t, ns3::Ptr<ns3::Socket>> m_udp6;
  struct TcpKey {
    bool v6;
    uint16_t port;

    std::string dst;
    bool operator==(const TcpKey& o) const {
      return v6 == o.v6 && port == o.port && dst == o.dst;
    }
  };
  struct TcpKeyHash {
    size_t operator()(const TcpKey& k) const {
      return std::hash<std::string>()(k.dst) ^ (k.port << 1) ^ (k.v6 ? 0x9e37 : 0);
    }
  };
  std::unordered_map<TcpKey, ns3::Ptr<ns3::Socket>, TcpKeyHash> m_tcp;

  std::unordered_map<uintptr_t, ns3::Ptr<ns3::Packet>>
      m_tcpPendingOnce; // existing is fine to re-use

  std::unordered_map<uintptr_t, ns3::Ptr<ns3::Packet>>
      m_tcpPending; // Pending ctrl packets keyed by socket pointer (unique per socket)

  std::unordered_map<TcpKey, ns3::Ptr<ns3::Socket>, TcpKeyHash> m_udp6_conn;
  ns3::Ptr<ns3::Socket> GetOrCreateUdpV6Connected(const ns3::Ipv6Address& dst, uint16_t port);

  ns3::Ptr<ns3::Socket> GetOrCreateUdpSender(uint16_t port, bool v6);
  ns3::Ptr<ns3::Socket> GetOrCreateTcpSender(const ns3::Address& dst, bool v6, uint16_t port);
};

} // namespace q2ns