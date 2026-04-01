/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/
#pragma once

#include "ns3/nstime.h"
#include "ns3/q2nsviz-trace-writer.h"
#include <initializer_list>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

/*
  v1 NDJSON schema (viewer.html expects these):
    - createNode     {t_ns, label, x, y}
    - createChannel  {t_ns, from, to, kind}
    - createBit      {t_ns, node, label, kind, color}
    - setBitColor    {t_ns, bit, color}
    - entangle       {t_ns, bits:[...]}
    - measure        {t_ns, bit, base, b0}        // generic measurement
    - graphMeasure   {t_ns, bit, base, supportNode}  // graph state measurement
    - sendBit        {t0_ns, t1_ns, bit, from, to, kind}
    - sendPacket     {t0_ns, t1_ns, from, to, label}
    - traceText      {t_ns, text}                 // trace panel (persistent)
    - traceText      {t_ns, node, text}           // node-local (one step)
*/

/*-----------------------------------------------------------------------------
 * StrCat: concatenate any streamable parts into a std::string
 *---------------------------------------------------------------------------*/
template <typename... Ts> inline std::string StrCat(Ts&&... parts) {
  std::ostringstream _oss;
  (void) std::initializer_list<int>{((_oss << std::forward<Ts>(parts)), 0)...};
  return _oss.str();
}

inline uint64_t NowNs() {
  return ns3::Simulator::Now().GetNanoSeconds();
}


/*-----------------------------------------------------------------------------
 * Nodes & Channels
 *---------------------------------------------------------------------------*/
inline void TraceCreateNode(const std::string& label, int xPct, int yPct) {
  std::ostringstream o;
  o << "{"
    << "\"type\":\"createNode\",\"t_ns\":0,"
    << "\"label\":" << J(label) << ",\"x\":" << xPct << ",\"y\":" << yPct << "}";
  TraceWriter::Instance().Write(o.str());
}

inline void TraceCreateChannel(const std::string& from, const std::string& to,
                               const std::string& kind) {
  std::ostringstream o;
  o << "{"
    << "\"type\":\"createChannel\",\"t_ns\":0,"
    << "\"from\":" << J(from) << ",\"to\":" << J(to) << ",\"kind\":" << J(kind) << "}";
  TraceWriter::Instance().Write(o.str());
}

/*-----------------------------------------------------------------------------
 * Bits: create / color / entangle / measure
 *---------------------------------------------------------------------------*/
inline void TraceCreateBit(const std::string& node, const std::string& bitLabel,
                           const std::string& kind, const std::string& color) {
  std::ostringstream o;
  o << "{"
    << "\"type\":\"createBit\",\"t_ns\":" << JTns() << ",\"node\":" << J(node)
    << ",\"label\":" << J(bitLabel) << ",\"kind\":" << J(kind) << ",\"color\":" << J(color) << "}";
  TraceWriter::Instance().Write(o.str());
}

inline void TraceSetBitColor(const std::string& bitLabel, const std::string& color) {
  std::ostringstream o;
  o << "{"
    << "\"type\":\"setBitColor\",\"t_ns\":" << JTns() << ",\"bit\":" << J(bitLabel)
    << ",\"color\":" << J(color) << "}";
  TraceWriter::Instance().Write(o.str());
}

inline void TraceEntangle(const std::vector<std::string>& bits) {
  std::ostringstream o;
  o << "{"
    << "\"type\":\"entangle\",\"t_ns\":" << JTns() << ",\"bits\":[";
  for (size_t i = 0; i < bits.size(); ++i) {
    if (i)
      o << ",";
    o << J(bits[i]);
  }
  o << "]}";
  TraceWriter::Instance().Write(o.str());
}

/* Measure/collapse a bit: removes the bit from any entanglement going forward.
   Optional measurement basis (default "Z"). */
inline void TraceMeasure(const std::string& bitLabel, const std::string& base = "Z") {
  std::ostringstream o;
  o << "{"
    << "\"type\":\"measure\",\"t_ns\":" << JTns() << ",\"bit\":" << J(bitLabel)
    << ",\"base\":" << J(base) << "}";
  TraceWriter::Instance().Write(o.str());
}

/* Graph state measurement: visualization for graph-state operations. */
inline void TraceGraphMeasure(const std::string& bitLabel, const std::string& base = "Z",
                              const std::string& supportNode = "") {
  std::ostringstream o;
  o << "{"
    << "\"type\":\"graphMeasure\",\"t_ns\":" << JTns() << ",\"bit\":" << J(bitLabel)
    << ",\"base\":" << J(base);
  if (!supportNode.empty()) {
    o << ",\"supportNode\":" << J(supportNode);
  }
  o << "}";
  TraceWriter::Instance().Write(o.str());
}

inline void TraceRemoveBit(const std::string& bitLabel, uint64_t t_ns = NowNs()) {
  std::ostringstream o;
  o << "{"
    << "\"type\":\"removeBit\","
    << "\"t_ns\":" << t_ns << ",\"bit\":" << J(bitLabel) << "}";
  TraceWriter::Instance().Write(o.str());
}


/*-----------------------------------------------------------------------------
 * Movement: sendBit & sendPacket
 *---------------------------------------------------------------------------*/
inline void TraceSendBit(const std::string& bitLabel, const std::string& from,
                         const std::string& to, const std::string& kind, uint64_t t0_ns,
                         uint64_t t1_ns) {
  std::ostringstream o;
  o << "{"
    << "\"type\":\"sendBit\",\"t0_ns\":" << t0_ns << ",\"t1_ns\":" << t1_ns
    << ",\"bit\":" << J(bitLabel) << ",\"from\":" << J(from) << ",\"to\":" << J(to)
    << ",\"kind\":" << J(kind) << "}";
  TraceWriter::Instance().Write(o.str());
}
inline void TraceSendBit(const std::string& bitLabel, const std::string& from,
                         const std::string& to, const std::string& kind, ns3::Time t0,
                         ns3::Time t1) {
  TraceSendBit(bitLabel, from, to, kind, t0.GetNanoSeconds(), t1.GetNanoSeconds());
}

inline void TraceSendPacket(const std::string& from, const std::string& to, uint64_t t0_ns,
                            uint64_t t1_ns, const std::string& label) {
  std::ostringstream o;
  o << "{"
    << "\"type\":\"sendPacket\",\"t0_ns\":" << t0_ns << ",\"t1_ns\":" << t1_ns
    << ",\"from\":" << J(from) << ",\"to\":" << J(to) << ",\"label\":" << J(label) << "}";
  TraceWriter::Instance().Write(o.str());
}
inline void TraceSendPacket(const std::string& from, const std::string& to, ns3::Time t0,
                            ns3::Time t1, const std::string& label) {
  TraceSendPacket(from, to, t0.GetNanoSeconds(), t1.GetNanoSeconds(), label);
}

/*-----------------------------------------------------------------------------
 * Traces: HUD (persistent) and node-local (one-step)
 *---------------------------------------------------------------------------*/
inline void _TraceString(const std::string& text) {
  std::ostringstream o;
  o << "{"
    << "\"type\":\"traceText\",\"t_ns\":" << JTns() << ",\"text\":" << J(text) << "}";
  TraceWriter::Instance().Write(o.str());
}

inline void Trace(const std::string& text) {
  _TraceString(text);
}

template <typename... Ts> inline void Trace(Ts&&... parts) {
  _TraceString(StrCat(std::forward<Ts>(parts)...));
}

inline void _TraceNodeTextString(const std::string& node, const std::string& text) {
  std::ostringstream o;
  o << "{"
    << "\"type\":\"traceText\",\"t_ns\":" << JTns() << ",\"node\":" << J(node)
    << ",\"text\":" << J(text) << "}";
  TraceWriter::Instance().Write(o.str());
}

inline void TraceNodeText(const std::string& node, const std::string& text) {
  _TraceNodeTextString(node, text);
}

template <typename... Ts> inline void TraceNodeText(const std::string& node, Ts&&... parts) {
  _TraceNodeTextString(node, StrCat(std::forward<Ts>(parts)...));
}
