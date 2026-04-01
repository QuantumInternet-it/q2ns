/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/
/**
 * @file q2ns-qstate.h
 * @brief Declares q2ns::QState, the backend-agnostic quantum state interface.
 */

#pragma once

#include "ns3/q2ns-qgate.h"
#include "ns3/q2ns-types.h"

#include "ns3/abort.h"
#include "ns3/rng-seed-manager.h"

#include <cstdint>
#include <iosfwd>
#include <memory>
#include <mutex>
#include <random>
#include <vector>

namespace q2ns {

/**
 * @ingroup q2ns_qstate
 * @class QState
 * @brief Backend-agnostic interface for a quantum state object.
 *
 * QState defines the minimal common interface implemented by concrete backend
 * state classes such as ket, density-matrix, and stabilizer backends.
 *
 * Design goals:
 * - Keep high-level code backend-agnostic.
 * - Define deterministic index-order semantics for merge and measurement.
 * - Centralize backend stream-assignment helpers shared across implementations.
 *
 * Concrete implementations may expose backend-specific views on their own
 * concrete classes, but QState provides the common operational interface used
 * by QProcessor and QStateRegistry.
 *
 * @see QProcessor
 * @see QStateRegistry
 */
class QState {
public:
  /**
   * @brief Virtual destructor.
   */
  virtual ~QState();

  /**
   * @brief Assign RNG streams for deterministic randomness.
   *
   * Backends that use random sampling should deterministically seed or bind
   * their random sources here using the provided stream index together with the
   * current ns-3 seed and run values.
   *
   * Backends that consume no random streams may keep the default behavior.
   *
   * @param stream Starting stream index.
   * @return Number of streams consumed.
   */
  virtual int64_t AssignStreams(int64_t stream) {
    (void) stream;
    return 0;
  }

  /**
   * @brief Get the registry-assigned state id.
   * @return Current state id.
   */
  StateId GetStateId() const {
    return stateId_;
  }

  /**
   * @brief Set the registry-assigned state id.
   * @param id New state id.
   */
  void SetStateId(StateId id) {
    stateId_ = id;
  }

  /**
   * @brief Print a human-readable representation of the state.
   * @param os Output stream.
   */
  virtual void Print(std::ostream& os) const = 0;

  /**
   * @brief Return the number of logical qubits in the state.
   * @return Number of logical qubits.
   */
  virtual std::size_t NumQubits() const = 0;

  /**
   * @brief Apply a gate to the given target indices.
   *
   * Concrete backends may support different gate subsets. For example,
   * matrix-based backends may accept arbitrary custom unitaries while other
   * backends may reject non-native operations.
   *
   * @param g Gate descriptor.
   * @param targets Target qubit indices.
   */
  virtual void Apply(const QGate& g, const std::vector<Index>& targets) = 0;

  /**
   * @brief Return a new state that is the disjoint merge of this state and another.
   *
   * Index-order invariant:
   * - The result first contains this state's qubits in their current index
   *   order.
   * - It then contains the other state's qubits in their current index order.
   *
   * @param other Other state to merge with.
   * @return Newly allocated merged state object.
   */
  virtual std::shared_ptr<QState> MergeDisjoint(const QState& other) const = 0;

  /**
   * @struct MeasureResult
   * @brief Result of measuring one qubit and splitting the state.
   *
   * The measured qubit is returned as a new 1-qubit state in measured. The
   * remaining qubits are returned as a separate survivors state.
   */
  struct MeasureResult {
    int outcome{0};                    //!< Classical outcome bit.
    std::shared_ptr<QState> measured;  //!< Measured 1-qubit state.
    std::shared_ptr<QState> survivors; //!< Remaining qubits after measurement.
  };

  /**
   * @brief Measure one qubit and split the result into new states.
   *
   * Survivors preserve their original relative order. Any original indices
   * greater than the measured target shift down by one in the survivors state.
   *
   * @param target Index of the qubit to measure.
   * @param basis Measurement basis. Defaults to Z.
   * @return Measurement result containing outcome, measured state, and survivor state.
   */
  virtual MeasureResult Measure(q2ns::Index target, q2ns::Basis basis = q2ns::Basis::Z) = 0;

protected:
  /**
   * @brief Print a standard backend header.
   * @param os Output stream.
   * @param backendName Human-readable backend name.
   */
  void PrintHeader(std::ostream& os, const char* backendName) const {
    os << "QState{backend=" << backendName << ", id=" << GetStateId() << ", n=" << NumQubits()
       << "}\n";
  }

  /**
   * @brief Mix a 64-bit value deterministically.
   * @param x Input value.
   * @return Mixed value.
   */
  static inline uint64_t SplitMix64(uint64_t x) {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
  }

  /**
   * @brief Derive a deterministic 64-bit seed from ns-3 seed/run and a stream.
   * @param seed ns-3 global seed.
   * @param run ns-3 global run.
   * @param stream Stream index.
   * @param salt Backend-specific salt.
   * @return Derived 64-bit seed.
   */
  static inline uint64_t DeriveSeed64(uint32_t seed, uint32_t run, int64_t stream, uint64_t salt) {
    uint64_t x = (uint64_t(seed) << 32) ^ uint64_t(run) ^ uint64_t(stream) ^ salt;
    return SplitMix64(x);
  }

  /**
   * @brief Build a std::seed_seq from a 64-bit seed.
   * @param s64 Derived 64-bit seed.
   * @return Seed sequence suitable for standard engines.
   */
  static inline std::seed_seq MakeSeedSeq(uint64_t s64) {
    const uint32_t a = static_cast<uint32_t>(s64);
    const uint32_t b = static_cast<uint32_t>(s64 >> 32);
    return std::seed_seq{a, b, 0x13579bdu, 0x2468ace0u};
  }

  /**
   * @brief Abort if ns-3 seed or run is zero.
   * @param seed ns-3 global seed.
   * @param run ns-3 global run.
   */
  static inline void CheckSeedRunNonZero(uint32_t seed, uint32_t run) {
    NS_ABORT_MSG_IF(seed == 0, "RngSeedManager::GetSeed() returned 0; Seed must be > 0");
    NS_ABORT_MSG_IF(run == 0, "RngSeedManager::GetRun() returned 0; Run must be > 0");
  }

  /**
   * @brief Return whether a backend-global RNG should be reseeded.
   *
   * This caches the most recent seed, run, and stream tuple for a given SALT.
   *
   * @tparam SALT Backend-specific salt.
   * @param seed ns-3 global seed.
   * @param run ns-3 global run.
   * @param stream Stream index.
   * @return True if reseeding should occur.
   */
  template <uint64_t SALT> static bool ShouldReseed(uint32_t seed, uint32_t run, int64_t stream) {
    struct Key {
      uint32_t seed;
      uint32_t run;
      int64_t stream;
    };

    static std::mutex m;
    static bool hasLast = false;
    static Key last{};

    std::lock_guard<std::mutex> lock(m);
    if (hasLast && last.seed == seed && last.run == run && last.stream == stream) {
      return false;
    }
    last = Key{seed, run, stream};
    hasLast = true;
    return true;
  }

  /**
   * @brief Helper for backends that reseed a global RNG source.
   *
   * This helper reads ns-3 seed and run values, checks they are valid, skips
   * redundant reseeding when the configuration is unchanged, derives a 64-bit
   * seed, and invokes the provided reseed callback.
   *
   * @tparam SALT Backend-specific salt.
   * @tparam ReseedFn Callback type used to perform backend reseeding.
   * @param stream Stream index.
   * @param reseed_fn Callback that accepts the derived 64-bit seed.
   * @return Number of streams conceptually consumed.
   */
  template <uint64_t SALT, typename ReseedFn>
  static int64_t AssignStreamsGlobal(int64_t stream, ReseedFn reseed_fn) {
    const uint32_t seed = ns3::RngSeedManager::GetSeed();
    const uint32_t run = ns3::RngSeedManager::GetRun();
    CheckSeedRunNonZero(seed, run);

    if (!ShouldReseed<SALT>(seed, run, stream)) {
      return 1;
    }

    const uint64_t s64 = DeriveSeed64(seed, run, stream, SALT);
    reseed_fn(s64);
    return 1;
  }

private:
  StateId stateId_{0}; //!< Registry-assigned state id.
};

/**
 * @brief Stream insertion for a QState reference.
 * @param os Output stream.
 * @param s State reference.
 * @return Output stream.
 */
inline std::ostream& operator<<(std::ostream& os, const QState& s) {
  s.Print(os);
  return os;
}

/**
 * @brief Stream insertion for a shared QState pointer.
 * @param os Output stream.
 * @param s Shared state pointer.
 * @return Output stream.
 */
inline std::ostream& operator<<(std::ostream& os, const std::shared_ptr<QState>& s) {
  if (s) {
    s->Print(os);
  } else {
    os << "QState{null}";
  }
  return os;
}

} // namespace q2ns