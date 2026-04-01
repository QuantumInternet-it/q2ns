/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/
#pragma once

#include "ns3/simulator.h"
#include <cstdio>
#include <fstream>
#include <mutex>
#include <string>

class TraceWriter {
public:
  static TraceWriter& Instance() {
    static TraceWriter inst;
    return inst;
  }
  void Open(const std::string& path) {
    std::lock_guard<std::mutex> g(mu_);
    if (ofs_.is_open())
      ofs_.close();
    ofs_.open(path, std::ios::out | std::ios::trunc);
  }
  void Write(const std::string& line) {
    std::lock_guard<std::mutex> g(mu_);
    if (ofs_.is_open()) {
      ofs_ << line << "\n";
      ofs_.flush();
    }
  }
  void Close() {
    std::lock_guard<std::mutex> g(mu_);
    if (ofs_.is_open())
      ofs_.close();
  }

private:
  std::mutex mu_;
  std::ofstream ofs_;
  TraceWriter() = default;
};

inline std::string J(const std::string& s) {
  std::string out;
  out.reserve(s.size() + 8);
  out.push_back('"');
  for (unsigned char uc : s) {
    switch (uc) {
    case '\"':
      out += "\\\"";
      break;
    case '\\':
      out += "\\\\";
      break;
    case '\b':
      out += "\\b";
      break;
    case '\f':
      out += "\\f";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\r':
      out += "\\r";
      break;
    case '\t':
      out += "\\t";
      break;
    default:
      if (uc < 0x20) {
        char buf[7];
        std::snprintf(buf, sizeof(buf), "\\u%04X", (unsigned) uc);
        out += buf;
      } else {
        out.push_back(static_cast<char>(uc)); // keep UTF-8 bytes as-is
      }
    }
  }
  out.push_back('"');
  return out;
}


inline uint64_t JTns() {
  return ns3::Simulator::Now().GetNanoSeconds();
}