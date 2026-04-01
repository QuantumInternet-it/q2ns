/**
 * This file is derived from the STAB library:
 *   https://github.com/softwareQinc/stab
 *
 * Original author: softwareQ
 * Original license: MIT
 *
 * Modifications for Q2NS:
 * - Replaced the original RNG implementation with explicit seeding support
 * - Added deterministic reseeding behavior via seed epochs
 * - Added nondeterministic fallback only when no seed is provided
 * - Added ns-3 logging integration
 * - Added argument validation for random_bit()
 * - Minor maintenance and formatting edits
 *
 * These modifications are part of the Q2NS project and are distributed under
 * the licensing terms of this repository in addition to the original MIT-licensed
 * third-party notice preserved for STAB.
 */

#include "random.h"

#include <atomic>
#include <random>

// If you want ns-3 logging here (recommended in Q2NS builds):
#include "ns3/log.h"

namespace stab {

NS_LOG_COMPONENT_DEFINE("StabRandom");

// 0 => "not set"
static std::atomic<uint64_t> g_seed{0};

// increments each time Seed() is called; threads reseed lazily when epoch changes
static std::atomic<uint64_t> g_seed_epoch{0};

// true if any thread has ever obtained/used the engine
static std::atomic<bool> g_any_used{false};

void Seed(uint64_t seed) {
  const uint64_t oldSeed = g_seed.load(std::memory_order_relaxed);
  const bool alreadyUsed = g_any_used.load(std::memory_order_relaxed);

  // Normalize 0 so it doesn't mean "unset"
  const uint64_t newSeed = (seed == 0) ? 1ULL : seed;

  // Warn on "odd" reseed: it changes future draws, but cannot undo past draws.
  if (alreadyUsed) {
    if (oldSeed == 0) {
      NS_LOG_WARN("stab::Seed called after RNG was already used while unseeded. "
                  "Earlier draws were nondeterministic; reseeding only affects future draws.");
    } else if (oldSeed != newSeed) {
      NS_LOG_INFO("stab::Seed called after RNG was already used. "
                  "Future draws will follow the new seed; earlier draws remain unchanged.");
    } else {
      // Same seed reseed: harmless, but atypical—keep it quiet or INFO if you prefer.
      NS_LOG_INFO("stab::Seed called again with the same seed after RNG was already used.");
    }
  }

  g_seed.store(newSeed, std::memory_order_relaxed);
  g_seed_epoch.fetch_add(1, std::memory_order_relaxed);
}

static void SeedEngine_(std::mt19937& eng, uint64_t s) {
  // Deterministic seeding path.
  const uint32_t a = static_cast<uint32_t>(s);
  const uint32_t b = static_cast<uint32_t>(s >> 32);
  std::seed_seq seq{
      a, b, 0x13579bdu, 0x2468ace0u // extra fixed words to improve diffusion for small seeds
  };
  eng.seed(seq);
}

static void SeedEngineNondet_(std::mt19937& eng) {
  // Nondeterministic fallback (only happens if never seeded before first use).
  std::random_device rd;
  std::seed_seq seq{rd(), rd(), rd(), rd(), rd(), rd(), rd(), rd()};
  eng.seed(seq);
}

std::mt19937& get_prng_engine() {
  static thread_local std::mt19937 eng;
  static thread_local uint64_t lastEpoch = UINT64_MAX; // force init on first use

  const uint64_t epoch = g_seed_epoch.load(std::memory_order_relaxed);
  if (lastEpoch != epoch) {
    const uint64_t s = g_seed.load(std::memory_order_relaxed);
    if (s == 0) {
      SeedEngineNondet_(eng);
    } else {
      SeedEngine_(eng, s);
    }
    lastEpoch = epoch;
  }

  g_any_used.store(true, std::memory_order_relaxed);
  return eng;
}

int random_bit(double p) {
  if (p < 0.0 || p > 1.0) {
    throw std::out_of_range("random_bit: p must be in [0,1]");
  }
  std::bernoulli_distribution dist(p);
  return dist(get_prng_engine()) ? 1 : 0;
}

} // namespace stab