/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/
/**
 * @file q2ns-types.h
 * @brief Common types, enums, and small helpers shared across q2ns.
 */

#pragma once

#include <Eigen/Dense>

#include <complex>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>
#include <vector>

#include "ns3/callback.h"

namespace q2ns {

class QNode;
class Qubit;

/**
 * @ingroup q2ns_core
 * @brief Complex scalar type used by matrix-based backends.
 */
using Complex = std::complex<double>;

/**
 * @ingroup q2ns_core
 * @brief Dynamic complex matrix type used for custom gates and matrix-based states.
 */
using Matrix = Eigen::MatrixXcd;

/**
 * @ingroup q2ns_core
 * @brief Generic qubit index type within a backend state.
 */
using Index = std::size_t;

/**
 * @ingroup q2ns_core
 * @brief Stable identifier for a registered backend state.
 */
using StateId = std::uint64_t;

/**
 * @ingroup q2ns_core
 * @brief Stable identifier for a registered qubit handle.
 */
using QubitId = std::uint64_t;

/**
 * @ingroup q2ns_core
 * @brief Per-transmission quantum map callable applied to a received qubit.
 *
 * QMapInstance objects are typically produced by QMap::Sample() during channel
 * transmission and later executed at the receiving node after the qubit becomes
 * local there.
 */
using QMapInstance = std::function<void(QNode&, std::shared_ptr<Qubit>&)>;

/**
 * @ingroup q2ns_core
 * @brief Callback invoked when a qubit is successfully received at a node.
 *
 * The callback is executed after the qubit has been adopted by the receiving
 * node and any QMapInstance associated with the transmission has been applied.
 *
 * @param qubit Received qubit handle.
 */
using RecvCallback = ns3::Callback<void, std::shared_ptr<Qubit>>;

/**
 * @ingroup q2ns_core
 * @brief Measurement basis for single-qubit projective measurement.
 */
enum class Basis { Z, X, Y };

/**
 * @ingroup q2ns_core
 * @brief Backend family used when creating new quantum states.
 */
enum class QStateBackend {
  Ket, //!< State-vector backend.
  DM,  //!< Density-matrix backend.
  Stab //!< Stabilizer backend.
};

/**
 * @ingroup q2ns_core
 * @brief Parse a backend name string.
 *
 * Unrecognized names fall back to QStateBackend::Ket.
 *
 * @param s Backend name string.
 * @return Corresponding backend enum value.
 */
QStateBackend BackendFromString(std::string_view s);

/**
 * @ingroup q2ns_core
 * @brief Return all supported backend enum values.
 * @return Vector containing all supported backend types.
 */
inline std::vector<QStateBackend> AllQStateBackends() {
  return {QStateBackend::Ket, QStateBackend::DM, QStateBackend::Stab};
}

/**
 * @ingroup q2ns_core
 * @brief Kind of simulated qubit location.
 */
enum class LocationType {
  Node,    //!< Qubit is local to a node identified by node id.
  Channel, //!< Qubit is in transit on a channel identified by channel id.
  Lost,    //!< Qubit was lost and is no longer accessible.
  Unset    //!< Qubit location has not yet been assigned.
};

/**
 * @ingroup q2ns_core
 * @struct Location
 * @brief Current tracked location of a qubit.
 */
struct Location {
  /**
   * @brief Kind of location.
   */
  LocationType type = LocationType::Unset;

  /**
   * @brief Owning object identifier.
   *
   * This is a node id when type is Node, a channel id when type is Channel,
   * and otherwise has no required meaning.
   */
  uint32_t ownerId = 0;
};

/**
 * @ingroup q2ns_core
 * @brief Construct an Unset location value.
 * @return Location with type LocationType::Unset.
 */
inline Location MakeUnsetLocation() {
  Location loc;
  loc.type = LocationType::Unset;
  return loc;
}

} // namespace q2ns