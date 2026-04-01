/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/
/**
 * @file q2ns-analysis.cc
 * @brief Defines q2ns::analysis.
 */

#include "ns3/q2ns-analysis.h"

#include "ns3/q2ns-qstate-all.h"
#include "ns3/q2ns-types.h"

#include <Eigen/Dense>

#include <cmath>
#include <complex>
#include <stdexcept>

namespace q2ns::analysis {

namespace {


/**
 * @brief Convert a supported QState backend to a density matrix.
 *
 * This helper provides a common density-matrix representation for analysis
 * routines that are naturally expressed in terms of rho, such as purity,
 * entropy, and trace distance.
 *
 * For ket and stabilizer backends, this may require constructing a dense
 * matrix representation and can therefore be substantially more expensive than
 * backend-native calculations.
 *
 * @param s Input state.
 * @return Dense density-matrix representation of s.
 *
 * @throws std::runtime_error if the backend is unsupported or if conversion of
 * a stabilizer state requires unavailable backend-specific functionality.
 */
qpp::cmat ToDensityMatrix(const QState& s) {
  if (const auto* k = dynamic_cast<const QStateKet*>(&s)) {
    return k->GetDensityMatrix();
  }

  if (const auto* d = dynamic_cast<const QStateDM*>(&s)) {
    return d->GetRho();
  }

  if (const auto* st = dynamic_cast<const QStateStab*>(&s)) {
#ifdef USE_QPP
    return qpp::prj(st->GetAffineState().to_ket());
#else
    throw std::runtime_error("ToDensityMatrix(QStateStab): unavailable without USE_QPP support");
#endif
  }

  throw std::runtime_error("ToDensityMatrix: unsupported QState backend");
}



/**
 * @brief Clamp a scalar metric to a closed interval within numerical tolerance.
 *
 * Values slightly outside the target interval due to floating-point roundoff are
 * clamped back to the nearest endpoint. Values outside the tolerance window are
 * returned unchanged.
 *
 * @param x Input value.
 * @param lo Lower bound.
 * @param hi Upper bound.
 * @param tol Numerical tolerance for endpoint clamping.
 * @return Clamped value when within tolerance of the interval, otherwise x.
 */
double ClampToInterval(double x, double lo, double hi, double tol = 1e-10) {
  if (x < lo && x > lo - tol) {
    return lo;
  }
  if (x > hi && x < hi + tol) {
    return hi;
  }
  return x;
}



/**
 * @brief Compute the Hermitian square root of a positive semidefinite matrix.
 *
 * Small negative eigenvalues caused by numerical roundoff are clamped to zero.
 *
 * @param A Input matrix.
 * @param negEigTol Tolerance for small negative eigenvalues.
 * @return Hermitian square root of A.
 */
Eigen::MatrixXcd HermitianSqrtPSD(const Eigen::MatrixXcd& A, double negEigTol = 1e-12) {
  if (A.rows() != A.cols()) {
    throw std::runtime_error("HermitianSqrtPSD: matrix must be square");
  }

  // Symmetrize first to suppress tiny anti-Hermitian numerical noise.
  Eigen::MatrixXcd H = 0.5 * (A + A.adjoint());

  Eigen::SelfAdjointEigenSolver<Eigen::MatrixXcd> es(H);
  if (es.info() != Eigen::Success) {
    throw std::runtime_error("HermitianSqrtPSD: eigensolver failed");
  }

  Eigen::VectorXd evals = es.eigenvalues();
  Eigen::MatrixXcd evecs = es.eigenvectors();

  for (int i = 0; i < evals.size(); ++i) {
    if (evals[i] < 0.0) {
      if (evals[i] > -negEigTol) {
        evals[i] = 0.0;
      } else {
        throw std::runtime_error("HermitianSqrtPSD: matrix is not positive semidefinite");
      }
    }
  }

  Eigen::VectorXd sqrtEvals = evals.array().sqrt();
  return evecs * sqrtEvals.asDiagonal() * evecs.adjoint();
}



/**
 * @brief Compute Uhlmann fidelity between two density matrices.
 *
 * The returned value is F(rho, sigma) = (Tr sqrt(sqrt(rho) sigma sqrt(rho)))^2.
 *
 * @param rho First density matrix.
 * @param sigma Second density matrix.
 * @param negEigTol Tolerance for small negative eigenvalues in intermediate PSD checks.
 * @return Fidelity in the range [0.0, 1.0], up to numerical tolerance.
 */
double UhlmannFidelity(const Eigen::MatrixXcd& rho, const Eigen::MatrixXcd& sigma,
                       double negEigTol = 1e-12) {
  if (rho.rows() != rho.cols() || sigma.rows() != sigma.cols() || rho.rows() != sigma.rows()) {
    throw std::runtime_error("UhlmannFidelity: dimension mismatch");
  }

  Eigen::MatrixXcd sqrtRho = HermitianSqrtPSD(rho, negEigTol);
  Eigen::MatrixXcd X = sqrtRho * sigma * sqrtRho;

  Eigen::MatrixXcd sqrtX = HermitianSqrtPSD(X, negEigTol);
  std::complex<double> tr = sqrtX.trace();

  double f = std::real(tr);
  f = f * f;

  // Clamp tiny numerical drift outside [0, 1].
  if (f < 0.0 && f > -1e-10) {
    f = 0.0;
  }
  if (f > 1.0 && f < 1.0 + 1e-10) {
    f = 1.0;
  }

  return f;
}



/**
 * @brief Compute the trace norm of a Hermitian matrix.
 *
 * For a Hermitian matrix H, the trace norm is the sum of the absolute values of
 * its eigenvalues.
 *
 * The input is symmetrized first to suppress tiny anti-Hermitian numerical
 * noise before diagonalization.
 *
 * @param A Input matrix.
 * @return Trace norm of A.
 *
 * @throws std::runtime_error if the matrix is not square or if eigenvalue
 * decomposition fails.
 */
double HermitianTraceNorm(const Eigen::MatrixXcd& A) {
  if (A.rows() != A.cols()) {
    throw std::runtime_error("HermitianTraceNorm: matrix must be square");
  }

  Eigen::MatrixXcd H = 0.5 * (A + A.adjoint());

  Eigen::SelfAdjointEigenSolver<Eigen::MatrixXcd> es(H);
  if (es.info() != Eigen::Success) {
    throw std::runtime_error("HermitianTraceNorm: eigensolver failed");
  }

  return es.eigenvalues().cwiseAbs().sum();
}

} // namespace



double Fidelity(const QState& a, const QState& b) {
  if (a.NumQubits() != b.NumQubits()) {
    throw std::runtime_error("Fidelity: states must have the same number of qubits");
  }

  const std::size_t n = a.NumQubits();
  if (n == 0) {
    throw std::invalid_argument("Fidelity: empty states are not supported");
  }

  // Ket-backend fidelity is the squared overlap |<psi|phi>|^2.
  if (auto ak = dynamic_cast<const QStateKet*>(&a)) {
    if (auto bk = dynamic_cast<const QStateKet*>(&b)) {
      const auto& psi = ak->GetKet();
      const auto& phi = bk->GetKet();

      const auto amp = (psi.adjoint() * phi)(0, 0);
      double f = std::norm(amp);
      return ClampToInterval(f, 0.0, 1.0);
    }
  }

  // Density-matrix fidelity uses the standard Uhlmann expression.
  if (auto ad = dynamic_cast<const QStateDM*>(&a)) {
    if (auto bd = dynamic_cast<const QStateDM*>(&b)) {
      const auto& rho = ad->GetRho();
      const auto& sigma = bd->GetRho();
      return UhlmannFidelity(rho, sigma);
    }
  }

  // Stabilizer fidelity is currently evaluated by converting to ket form when
  // that backend functionality is available.
  if (auto as = dynamic_cast<const QStateStab*>(&a)) {
    if (auto bs = dynamic_cast<const QStateStab*>(&b)) {
#ifdef USE_QPP
      const auto& psiS = as->GetAffineState();
      const auto& phiS = bs->GetAffineState();

      const auto psi = psiS.to_ket();
      const auto phi = phiS.to_ket();

      const auto amp = (psi.adjoint() * phi)(0, 0);
      double f = std::norm(amp);
      return ClampToInterval(f, 0.0, 1.0);
#else
      throw std::runtime_error("Fidelity(Stab, Stab) is unavailable without USE_QPP support");
#endif
    }
  }

  throw std::runtime_error("Fidelity: mixed backends are not supported. "
                           "Supported pairs are Ket-Ket, DM-DM, and Stab-Stab.");
}



double Purity(const QState& s) {
  const std::size_t n = s.NumQubits();
  if (n == 0) {
    throw std::invalid_argument("Purity: empty states are not supported");
  }

  const auto rho = ToDensityMatrix(s);
  const std::complex<double> tr = qpp::trace(rho * rho);

  double p = std::real(tr);
  p = ClampToInterval(p, 0.0, 1.0);
  return p;
}



bool IsPure(const QState& s, double tol) {
  if (tol < 0.0) {
    throw std::invalid_argument("IsPure: tolerance must be non-negative");
  }

  return std::fabs(Purity(s) - 1.0) <= tol;
}



double VonNeumannEntropy(const QState& s) {
  constexpr double negEigTol = 1e-12;
  constexpr double zeroTol = 1e-15;

  const std::size_t n = s.NumQubits();
  if (n == 0) {
    throw std::invalid_argument("VonNeumannEntropy: empty states are not supported");
  }

  const auto rho = ToDensityMatrix(s);

  // Symmetrize first to suppress tiny anti-Hermitian numerical noise.
  Eigen::MatrixXcd H = 0.5 * (rho + rho.adjoint());

  Eigen::SelfAdjointEigenSolver<Eigen::MatrixXcd> es(H);
  if (es.info() != Eigen::Success) {
    throw std::runtime_error("VonNeumannEntropy: eigensolver failed");
  }

  const auto evals = es.eigenvalues();

  double entropy = 0.0;
  for (int i = 0; i < evals.size(); ++i) {
    double lambda = evals[i];

    if (lambda < 0.0) {
      if (lambda > -negEigTol) {
        lambda = 0.0;
      } else {
        throw std::runtime_error("VonNeumannEntropy: density matrix is not positive semidefinite");
      }
    }

    if (lambda > zeroTol) {
      entropy -= lambda * std::log2(lambda);
    }
  }

  if (entropy < 0.0 && entropy > -1e-10) {
    entropy = 0.0;
  }

  return entropy;
}



double TraceDistance(const QState& a, const QState& b) {
  if (a.NumQubits() != b.NumQubits()) {
    throw std::runtime_error("TraceDistance: states must have the same number of qubits");
  }

  const std::size_t n = a.NumQubits();
  if (n == 0) {
    throw std::invalid_argument("TraceDistance: empty states are not supported");
  }

  // Restrict to same-backend comparisons only.
  const bool bothKet = dynamic_cast<const QStateKet*>(&a) && dynamic_cast<const QStateKet*>(&b);
  const bool bothDm = dynamic_cast<const QStateDM*>(&a) && dynamic_cast<const QStateDM*>(&b);
  const bool bothStab = dynamic_cast<const QStateStab*>(&a) && dynamic_cast<const QStateStab*>(&b);

  if (!(bothKet || bothDm || bothStab)) {
    throw std::runtime_error("TraceDistance: mixed backends are not supported. "
                             "Supported pairs are Ket-Ket, DM-DM, and Stab-Stab.");
  }

  const auto rho = ToDensityMatrix(a);
  const auto sigma = ToDensityMatrix(b);

  double d = 0.5 * HermitianTraceNorm(rho - sigma);
  d = ClampToInterval(d, 0.0, 1.0);
  return d;
}

} // namespace q2ns::analysis