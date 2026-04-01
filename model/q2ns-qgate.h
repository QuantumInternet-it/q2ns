/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/
/**
 * @file q2ns-qgate.h
 * @brief Gate helpers, gate descriptors, and predefined matrices for the
 * standard q2ns gate set.
 */

#pragma once

#include "ns3/q2ns-types.h"

#include "ns3/assert.h"
#include "ns3/nstime.h"

#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <memory>
#include <utility>

namespace q2ns {

/**
 * @ingroup q2ns_core
 * @brief Enumerates built-in gate kinds.
 *
 * Custom represents a caller-supplied unitary matrix. All other values refer
 * to predefined gates with built-in matrix definitions.
 */
enum class QGateKind {
  Custom, //!< Caller-supplied matrix.

  I,
  X,
  Y,
  Z,
  H,
  S,
  SDG,

  CNOT,
  CZ,
  SWAP
};

/**
 * @ingroup q2ns_core
 * @class QGate
 * @brief Lightweight gate descriptor used by QState backends.
 *
 * A QGate stores either:
 * - a built-in gate kind, or
 * - a custom unitary matrix
 *
 * It may also carry an optional duration value for higher-level scheduling or
 * interpretation. QGate itself does not enforce timing behavior.
 *
 * @see QGateKind
 * @see gates
 */
class QGate {
public:
  /**
   * @brief Default constructor.
   */
  QGate() = default;

  /**
   * @brief Construct a built-in gate descriptor.
   * @param k Built-in gate kind.
   * @param d Optional duration metadata.
   */
  explicit QGate(QGateKind k, ns3::Time d = ns3::Seconds(0)) : kind_(k), duration_(d) {}

  /**
   * @brief Return the gate kind.
   * @return Stored gate kind.
   */
  QGateKind Kind() const {
    return kind_;
  }

  /**
   * @brief Return the optional duration metadata.
   * @return Stored duration.
   */
  ns3::Time Duration() const {
    return duration_;
  }

  /**
   * @brief Return the custom unitary matrix.
   *
   * This is only valid when Kind() returns QGateKind::Custom.
   *
   * @return Stored custom unitary matrix.
   *
   * @see Kind
   */
  const Matrix& Unitary() const {
    NS_ASSERT_MSG(U_, "QGate::Unitary() called for a non-custom gate");
    return *U_;
  }

  /**
   * @brief Construct a custom gate by copying a matrix.
   * @param U Unitary matrix to copy.
   * @param d Optional duration metadata.
   * @return Custom gate descriptor.
   *
   * @see Custom(Matrix&&, ns3::Time)
   */
  static QGate Custom(const Matrix& U, ns3::Time d = ns3::Seconds(0)) {
    QGate g;
    g.kind_ = QGateKind::Custom;
    g.U_ = std::make_shared<const Matrix>(U);
    g.duration_ = d;
    return g;
  }

  /**
   * @brief Construct a custom gate by moving a matrix.
   * @param U Unitary matrix to move.
   * @param d Optional duration metadata.
   * @return Custom gate descriptor.
   *
   * @see Custom(const Matrix&, ns3::Time)
   */
  static QGate Custom(Matrix&& U, ns3::Time d = ns3::Seconds(0)) {
    QGate g;
    g.kind_ = QGateKind::Custom;
    g.U_ = std::make_shared<const Matrix>(std::move(U));
    g.duration_ = d;
    return g;
  }

private:
  QGateKind kind_{QGateKind::I};        //!< Gate kind.
  std::shared_ptr<const Matrix> U_{};   //!< Custom unitary for Custom gates.
  ns3::Time duration_{ns3::Seconds(0)}; //!< Optional duration metadata.
};

/**
 * @brief Build a Matrix from nested initializer lists.
 * @param rows Matrix rows.
 * @return Constructed matrix.
 */
inline Matrix MakeMatrix(std::initializer_list<std::initializer_list<Complex>> rows) {
  const std::size_t r = rows.size();
  const std::size_t c = (r > 0) ? rows.begin()->size() : 0;

  Matrix m(static_cast<int>(r), static_cast<int>(c));
  std::size_t i = 0;
  for (const auto& row : rows) {
    NS_ASSERT_MSG(row.size() == c, "MakeMatrix: ragged initializer_list");
    std::size_t j = 0;
    for (const auto& v : row) {
      m(static_cast<int>(i), static_cast<int>(j)) = v;
      ++j;
    }
    ++i;
  }
  return m;
}

/**
 * @brief Return the 1-qubit identity matrix.
 * @return Reference to the cached matrix.
 */
inline const Matrix& MatrixI() {
  static const Matrix I = [] {
    return MakeMatrix({{Complex{1, 0}, Complex{0, 0}}, {Complex{0, 0}, Complex{1, 0}}});
  }();
  return I;
}

/**
 * @brief Return the Pauli-X matrix.
 * @return Reference to the cached matrix.
 */
inline const Matrix& MatrixX() {
  static const Matrix X = [] {
    return MakeMatrix({{Complex{0, 0}, Complex{1, 0}}, {Complex{1, 0}, Complex{0, 0}}});
  }();
  return X;
}

/**
 * @brief Return the Pauli-Y matrix.
 * @return Reference to the cached matrix.
 */
inline const Matrix& MatrixY() {
  static const Matrix Y = [] {
    return MakeMatrix({{Complex{0, 0}, Complex{0, -1}}, {Complex{0, 1}, Complex{0, 0}}});
  }();
  return Y;
}

/**
 * @brief Return the Pauli-Z matrix.
 * @return Reference to the cached matrix.
 */
inline const Matrix& MatrixZ() {
  static const Matrix Z = [] {
    return MakeMatrix({{Complex{1, 0}, Complex{0, 0}}, {Complex{0, 0}, Complex{-1, 0}}});
  }();
  return Z;
}

/**
 * @brief Return the Hadamard matrix.
 * @return Reference to the cached matrix.
 */
inline const Matrix& MatrixH() {
  static const Matrix H = [] {
    const double s = 1.0 / std::sqrt(2.0);
    return MakeMatrix({{Complex{s, 0}, Complex{s, 0}}, {Complex{s, 0}, Complex{-s, 0}}});
  }();
  return H;
}

/**
 * @brief Return the phase gate matrix S = diag(1, i).
 * @return Reference to the cached matrix.
 */
inline const Matrix& MatrixS() {
  static const Matrix S = [] {
    return MakeMatrix({{Complex{1, 0}, Complex{0, 0}}, {Complex{0, 0}, Complex{0, 1}}});
  }();
  return S;
}

/**
 * @brief Return the inverse phase gate matrix SDG = diag(1, -i).
 * @return Reference to the cached matrix.
 */
inline const Matrix& MatrixSDG() {
  static const Matrix SDG = [] {
    return MakeMatrix({
        {Complex{1, 0}, Complex{0, 0}},
        {Complex{0, 0}, Complex{0, -1}},
    });
  }();
  return SDG;
}

/**
 * @brief Return the CNOT matrix with control qubit 0 and target qubit 1.
 *
 * Basis ordering is |00>, |01>, |10>, |11>.
 *
 * @return Reference to the cached matrix.
 */
inline const Matrix& MatrixCNOT() {
  static const Matrix CNOT = [] {
    return MakeMatrix({
        {Complex{1, 0}, Complex{0, 0}, Complex{0, 0}, Complex{0, 0}},
        {Complex{0, 0}, Complex{1, 0}, Complex{0, 0}, Complex{0, 0}},
        {Complex{0, 0}, Complex{0, 0}, Complex{0, 0}, Complex{1, 0}},
        {Complex{0, 0}, Complex{0, 0}, Complex{1, 0}, Complex{0, 0}},
    });
  }();
  return CNOT;
}

/**
 * @brief Return the CZ matrix.
 * @return Reference to the cached matrix.
 */
inline const Matrix& MatrixCZ() {
  static const Matrix CZ = [] {
    return MakeMatrix({
        {Complex{1, 0}, Complex{0, 0}, Complex{0, 0}, Complex{0, 0}},
        {Complex{0, 0}, Complex{1, 0}, Complex{0, 0}, Complex{0, 0}},
        {Complex{0, 0}, Complex{0, 0}, Complex{1, 0}, Complex{0, 0}},
        {Complex{0, 0}, Complex{0, 0}, Complex{0, 0}, Complex{-1, 0}},
    });
  }();
  return CZ;
}

/**
 * @brief Return the SWAP matrix.
 *
 * Basis ordering is |00>, |01>, |10>, |11>.
 *
 * @return Reference to the cached matrix.
 */
inline const Matrix& MatrixSWAP() {
  static const Matrix SWAP = [] {
    return MakeMatrix({
        {Complex{1, 0}, Complex{0, 0}, Complex{0, 0}, Complex{0, 0}},
        {Complex{0, 0}, Complex{0, 0}, Complex{1, 0}, Complex{0, 0}},
        {Complex{0, 0}, Complex{1, 0}, Complex{0, 0}, Complex{0, 0}},
        {Complex{0, 0}, Complex{0, 0}, Complex{0, 0}, Complex{1, 0}},
    });
  }();
  return SWAP;
}

/**
 * @brief Return the built-in matrix for a non-custom gate kind.
 * @param k Built-in gate kind.
 * @return Reference to the corresponding cached matrix.
 */
inline const Matrix& MatrixOf(QGateKind k) {
  switch (k) {
  case QGateKind::I:
    return MatrixI();
  case QGateKind::X:
    return MatrixX();
  case QGateKind::Y:
    return MatrixY();
  case QGateKind::Z:
    return MatrixZ();
  case QGateKind::H:
    return MatrixH();
  case QGateKind::S:
    return MatrixS();
  case QGateKind::SDG:
    return MatrixSDG();
  case QGateKind::CNOT:
    return MatrixCNOT();
  case QGateKind::CZ:
    return MatrixCZ();
  case QGateKind::SWAP:
    return MatrixSWAP();
  default:
    NS_ABORT_MSG("MatrixOf: custom gates do not have a built-in matrix");
  }
}

namespace gates {

/**
 * @brief Return the identity gate descriptor.
 * @param d Optional duration metadata.
 * @return Gate descriptor.
 */
inline QGate I(ns3::Time d = ns3::Seconds(0)) {
  return QGate(QGateKind::I, d);
}

/**
 * @brief Return the Pauli-X gate descriptor.
 * @param d Optional duration metadata.
 * @return Gate descriptor.
 */
inline QGate X(ns3::Time d = ns3::Seconds(0)) {
  return QGate(QGateKind::X, d);
}

/**
 * @brief Return the Pauli-Y gate descriptor.
 * @param d Optional duration metadata.
 * @return Gate descriptor.
 */
inline QGate Y(ns3::Time d = ns3::Seconds(0)) {
  return QGate(QGateKind::Y, d);
}

/**
 * @brief Return the Pauli-Z gate descriptor.
 * @param d Optional duration metadata.
 * @return Gate descriptor.
 */
inline QGate Z(ns3::Time d = ns3::Seconds(0)) {
  return QGate(QGateKind::Z, d);
}

/**
 * @brief Return the Hadamard gate descriptor.
 * @param d Optional duration metadata.
 * @return Gate descriptor.
 */
inline QGate H(ns3::Time d = ns3::Seconds(0)) {
  return QGate(QGateKind::H, d);
}

/**
 * @brief Return the phase gate descriptor.
 * @param d Optional duration metadata.
 * @return Gate descriptor.
 */
inline QGate S(ns3::Time d = ns3::Seconds(0)) {
  return QGate(QGateKind::S, d);
}

/**
 * @brief Return the inverse phase gate descriptor.
 * @param d Optional duration metadata.
 * @return Gate descriptor.
 */
inline QGate SDG(ns3::Time d = ns3::Seconds(0)) {
  return QGate(QGateKind::SDG, d);
}

/**
 * @brief Return the CNOT gate descriptor.
 * @param d Optional duration metadata.
 * @return Gate descriptor.
 */
inline QGate CNOT(ns3::Time d = ns3::Seconds(0)) {
  return QGate(QGateKind::CNOT, d);
}

/**
 * @brief Return the CZ gate descriptor.
 * @param d Optional duration metadata.
 * @return Gate descriptor.
 */
inline QGate CZ(ns3::Time d = ns3::Seconds(0)) {
  return QGate(QGateKind::CZ, d);
}

/**
 * @brief Return the SWAP gate descriptor.
 * @param d Optional duration metadata.
 * @return Gate descriptor.
 */
inline QGate SWAP(ns3::Time d = ns3::Seconds(0)) {
  return QGate(QGateKind::SWAP, d);
}

/**
 * @brief Return a custom gate descriptor by copying a matrix.
 * @param U Unitary matrix to copy.
 * @param d Optional duration metadata.
 * @return Gate descriptor.
 */
inline QGate Custom(const Matrix& U, ns3::Time d = ns3::Seconds(0)) {
  return QGate::Custom(U, d);
}

/**
 * @brief Return a custom gate descriptor by moving a matrix.
 * @param U Unitary matrix to move.
 * @param d Optional duration metadata.
 * @return Gate descriptor.
 */
inline QGate Custom(Matrix&& U, ns3::Time d = ns3::Seconds(0)) {
  return QGate::Custom(std::move(U), d);
}

} // namespace gates

} // namespace q2ns