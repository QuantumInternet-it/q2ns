/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/
/**
 * @file q2ns-analysis.h
 * @brief Backend-agnostic analysis for q2ns quantum states.
 */

#pragma once

namespace q2ns {

class QState;

namespace analysis {

/**
 * @ingroup q2ns_api
 * @brief Compute fidelity between two QState objects of the same backend type.
 *
 * Currently supported backend pairings are:
 * - QStateKet with QStateKet
 * - QStateDM with QStateDM
 * - QStateStab with QStateStab
 *
 * Mixed backend comparisons are not yet supported and throw.
 *
 * @param a First state.
 * @param b Second state.
 * @return Fidelity in the range [0.0, 1.0].
 *
 * @throws std::invalid_argument if the states are empty.
 * @throws std::runtime_error if the states have different sizes, use unsupported
 * backends, or require unavailable backend-specific functionality.
 *
 * @see QState
 */
double Fidelity(const QState& a, const QState& b);

/**
 * @ingroup q2ns_api
 * @brief Compute the purity of a quantum state.
 *
 * Purity is defined as Tr(rho^2), where rho is the density matrix of the state.
 * It is equal to 1 for pure states and less than 1 for mixed states.
 *
 * For backend-specific reasons, this function currently operates only on a
 * single QState and therefore does not involve cross-backend comparison.
 *
 * @param s Input state.
 * @return Purity in the range [0.0, 1.0], up to numerical tolerance.
 *
 * @throws std::invalid_argument if the state is empty.
 * @throws std::runtime_error if the backend is unsupported or requires
 * unavailable backend-specific functionality.
 *
 * @see IsPure
 * @see VonNeumannEntropy
 * @see QState
 */
double Purity(const QState& s);

/**
 * @ingroup q2ns_api
 * @brief Check whether a quantum state is pure within a numerical tolerance.
 *
 * This function evaluates purity and compares it against 1.0. It is intended
 * as a convenience helper for diagnostics, tests, and analysis code.
 *
 * @param s Input state.
 * @param tol Absolute tolerance for comparing purity against 1.0.
 * @return True if the state is pure within tolerance, false otherwise.
 *
 * @throws std::invalid_argument if the state is empty or if tol is negative.
 * @throws std::runtime_error if the backend is unsupported or requires
 * unavailable backend-specific functionality.
 *
 * @see Purity
 * @see QState
 */
bool IsPure(const QState& s, double tol = 1e-12);

/**
 * @ingroup q2ns_api
 * @brief Compute the von Neumann entropy of a quantum state in bits.
 *
 * The von Neumann entropy is defined as
 * S(rho) = -Tr(rho log2 rho).
 *
 * Eigenvalues whose magnitude is below numerical tolerance are treated as zero
 * and do not contribute to the sum.
 *
 * @param s Input state.
 * @return von Neumann entropy in bits.
 *
 * @throws std::invalid_argument if the state is empty.
 * @throws std::runtime_error if the backend is unsupported, if the backend
 * requires unavailable functionality, or if the density matrix is not positive
 * semidefinite within numerical tolerance.
 *
 * @see Purity
 * @see TraceDistance
 * @see QState
 */
double VonNeumannEntropy(const QState& s);

/**
 * @ingroup q2ns_api
 * @brief Compute the trace distance between two QState objects of the same backend type.
 *
 * The trace distance is defined as
 * D(rho, sigma) = 0.5 * ||rho - sigma||_1,
 * where ||.||_1 is the trace norm.
 *
 * Currently supported backend pairings are:
 * - QStateKet with QStateKet
 * - QStateDM with QStateDM
 * - QStateStab with QStateStab
 *
 * Mixed backend comparisons are not supported and throw.
 *
 * @param a First state.
 * @param b Second state.
 * @return Trace distance in the range [0.0, 1.0], up to numerical tolerance.
 *
 * @throws std::invalid_argument if the states are empty.
 * @throws std::runtime_error if the states have different sizes, use unsupported
 * backends, or require unavailable backend-specific functionality.
 *
 * @see Fidelity
 * @see VonNeumannEntropy
 * @see QState
 */
double TraceDistance(const QState& a, const QState& b);

} // namespace analysis
} // namespace q2ns