/**
 * This file is derived from the STAB library:
 *   https://github.com/softwareQinc/stab
 *
 * Original author: softwareQ
 * Original license: MIT
 *
 * Modifications for Q2NS:
 * - Added explicit Seed(uint64_t) API
 * - Added stricter argument validation
 * - Extended includes to support the revised RNG implementation
 * - Minor maintenance and formatting edits
 *
 * These modifications are part of the Q2NS project and are distributed under
 * the licensing terms of this repository in addition to the original MIT-licensed
 * third-party notice preserved for STAB.
 */

#ifndef STAB_RANDOM_H_
#define STAB_RANDOM_H_

#include <limits>
#include <random>
#include <stdexcept>
#include <type_traits>

namespace stab {

void Seed(uint64_t seed);
std::mt19937& get_prng_engine();

int random_bit(double p = 0.5);

// template functions need to be defined in the header

template <class T = int, std::enable_if_t<std::is_integral_v<T>>* = nullptr>
T random_integer(T a = std::numeric_limits<T>::min(), T b = std::numeric_limits<T>::max()) {
  std::uniform_int_distribution<T> dist(a, b);
  if (a > b) {
    throw std::out_of_range("a > b");
  }
  return dist(get_prng_engine());
}

template <class T = double, std::enable_if_t<std::is_floating_point_v<T>>* = nullptr>
T random_real(T a = std::numeric_limits<T>::min(), T b = std::numeric_limits<T>::max()) {
  if (a >= b) {
    throw std::out_of_range("a >= b");
  }
  std::uniform_real_distribution<T> dist(a, b);
  return dist(get_prng_engine());
}
} /* namespace stab */

#endif /* STAB_RANDOM_H_ */