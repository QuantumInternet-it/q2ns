/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/
/**
 * @file q2ns-qmap.h
 * @brief Declares q2ns::QMap and standard channel-map models.
 */

#pragma once

#include "ns3/nstime.h"
#include "ns3/object.h"
#include "ns3/ptr.h"
#include "ns3/random-variable-stream.h"

#include "ns3/q2ns-types.h"

#include <cmath>
#include <functional>
#include <memory>
#include <vector>

namespace q2ns {

class QNode;
class QGate;
class Qubit;

/**
 * @struct QMapContext
 * @brief Optional per-sample context passed to QMaps.
 *
 * This is intentionally small today, but can be extended later with additional
 * context such as distance, wavelength, or metadata about a traversed link.
 */
struct QMapContext {
  ns3::Time elapsedTime{ns3::Seconds(0)}; //!< Elapsed time that this map is applied over
};

/**
 * @ingroup q2ns_qmap
 * @class QMap
 * @brief Abstract base class for channel map models.
 *
 * A QMap does not directly mutate a qubit when the channel is configured.
 * Instead, it samples a per-transmission QMapInstance that is carried alongside
 * the transmitted qubit and later executed by the receiving QNode after the
 * qubit becomes local there.
 *
 * Most probabilistic QMaps share two common knobs:
 * - Probability: direct per-transmission application probability
 * - Rate: Poisson event rate in 1/s, converted to a per-flight probability
 *
 * If Rate is greater than zero, it overrides Probability using the current
 * flight time in the provided QMapContext.
 *
 * @see QMapInstance
 * @see QNode
 */
class QMap : public ns3::Object {
public:
  /**
   * @brief Get the ns-3 TypeId.
   * @return TypeId for q2ns::QMap.
   */
  static ns3::TypeId GetTypeId();

  /**
   * @brief Virtual destructor.
   */
  ~QMap() override = default;

  /**
   * @brief Sample a per-transmission QMapInstance.
   *
   * The returned callable is later executed at the receiving node after the
   * qubit has become local there.
   *
   * @param u Uniform random source to use for sampling.
   * @param ctx Optional per-transmission context.
   * @return Per-transmission QMapInstance. An empty callable represents the
   * identity map.
   */
  virtual QMapInstance Sample(ns3::Ptr<ns3::UniformRandomVariable> u,
                              const QMapContext& ctx = QMapContext{}) const = 0;

  /**
   * @brief Compose two QMaps into one sequential composite QMap.
   *
   * The resulting map applies the sampled instance of a first, then the sampled
   * instance of b. If either step marks the qubit lost, later steps are not run.
   *
   * @param a First QMap in the sequence.
   * @param b Second QMap in the sequence.
   * @return Composite QMap object.
   *
   * @see Compose(const std::vector<ns3::Ptr<QMap>>&)
   */
  static ns3::Ptr<QMap> Compose(const ns3::Ptr<QMap>& a, const ns3::Ptr<QMap>& b);

  /**
   * @brief Compose a sequence of QMaps into one sequential composite QMap.
   *
   * Maps are applied left to right in the order provided.
   *
   * @param maps Sequence of maps to compose.
   * @return Composite QMap object.
   *
   * @see Compose(const ns3::Ptr<QMap>&, const ns3::Ptr<QMap>&)
   */
  static ns3::Ptr<QMap> Compose(const std::vector<ns3::Ptr<QMap>>& maps);

  /**
   * @brief Build a QMap from a simple lambda.
   *
   * The lambda is executed at the receiving node and receives only the node and
   * qubit handle.
   *
   * @param f User-provided callable.
   * @return QMap wrapping the callable.
   *
   * @see FromLambda(std::function<void(QNode&, std::shared_ptr<Qubit>&,
   *      ns3::Ptr<ns3::UniformRandomVariable>, const QMapContext&)>)
   */
  static ns3::Ptr<QMap> FromLambda(std::function<void(QNode&, std::shared_ptr<Qubit>&)> f);

  /**
   * @brief Build a QMap from an advanced lambda.
   *
   * The lambda is executed at the receiving node and also receives the random
   * source and sampled transmission context.
   *
   * @param f User-provided callable.
   * @return QMap wrapping the callable.
   *
   * @see FromLambda(std::function<void(QNode&, std::shared_ptr<Qubit>&)>)
   */
  static ns3::Ptr<QMap>
  FromLambda(std::function<void(QNode&, std::shared_ptr<Qubit>&,
                                ns3::Ptr<ns3::UniformRandomVariable>, const QMapContext&)>
                 f);

  /**
   * @brief Convert a Poisson rate and elapsed time into an event probability.
   *
   * This returns the probability of at least one event in time t under a
   * Poisson process with rate rate_per_s.
   *
   * @param rate_per_s Event rate in 1/s.
   * @param t Elapsed time.
   * @return Probability in [0, 1].
   */
  static inline double RateToProb(double rate_per_s, const ns3::Time& t) {
    const double s = t.GetSeconds();
    if (rate_per_s <= 0.0 || s <= 0.0) {
      return 0.0;
    }
    const double p = 1.0 - std::exp(-rate_per_s * s);
    return (p < 0.0) ? 0.0 : (p > 1.0 ? 1.0 : p);
  }

protected:
  /**
   * @brief Return the effective application probability for this transmission.
   * @param ctx Per-transmission context.
   * @return Probability in [0, 1].
   */
  double GetProb_(const QMapContext& ctx) const {
    return (rate_ > 0.0) ? RateToProb(rate_, ctx.elapsedTime) : p_;
  }

  /**
   * @brief Perform one Bernoulli trial using the effective probability.
   * @param u Uniform random source.
   * @param ctx Per-transmission context.
   * @return True if the event occurs, false otherwise.
   */
  bool Bernoulli_(ns3::Ptr<ns3::UniformRandomVariable> u, const QMapContext& ctx) const {
    const double p = GetProb_(ctx);
    if (p <= 0.0) {
      return false;
    }
    if (p >= 1.0) {
      return true;
    }
    return u->GetValue(0.0, 1.0) < p;
  }

  /**
   * @brief Mark a qubit lost through the standard registry-backed location path.
   * @param q Qubit handle.
   */
  static void SetLost_(Qubit& q);

  double p_ = 0.0;    //!< Direct per-transmission probability.
  double rate_ = 0.0; //!< Poisson event rate in 1/s. Overrides p_ when positive.
};

/**
 * @ingroup q2ns_qmap
 * @class LambdaQMap
 * @brief QMap implementation that wraps a user-provided lambda.
 *
 * LambdaQMap can store either:
 * - a simple callable that receives only node and qubit
 * - an advanced callable that also receives the RNG source and QMapContext
 *
 * If both are unset, sampling returns the identity map.
 *
 * @see QMap::FromLambda
 */
class LambdaQMap final : public QMap {
public:
  /**
   * @brief Simple callable type.
   */
  using SimpleFn = std::function<void(QNode&, std::shared_ptr<Qubit>&)>;

  /**
   * @brief Advanced callable type.
   */
  using AdvancedFn = std::function<void(QNode&, std::shared_ptr<Qubit>&,
                                        ns3::Ptr<ns3::UniformRandomVariable>, const QMapContext&)>;

  /**
   * @brief Get the ns-3 TypeId.
   * @return TypeId for q2ns::LambdaQMap.
   */
  static ns3::TypeId GetTypeId();

  /**
   * @brief Default constructor.
   */
  LambdaQMap() = default;

  /**
   * @brief Construct from a simple callable.
   * @param f Simple callable.
   */
  explicit LambdaQMap(SimpleFn f);

  /**
   * @brief Construct from an advanced callable.
   * @param f Advanced callable.
   */
  explicit LambdaQMap(AdvancedFn f);

  /**
   * @brief Sample a per-transmission QMapInstance.
   * @param u Uniform random source.
   * @param ctx Per-transmission context.
   * @return Sampled instance, or identity if no callable is configured.
   */
  QMapInstance Sample(ns3::Ptr<ns3::UniformRandomVariable> u,
                      const QMapContext& ctx = QMapContext{}) const override;

  /**
   * @brief Replace the stored callable with a simple callable.
   * @param f Simple callable.
   */
  void Set(SimpleFn f);

  /**
   * @brief Replace the stored callable with an advanced callable.
   * @param f Advanced callable.
   */
  void Set(AdvancedFn f);

private:
  SimpleFn simple_{};     //!< Simple callable, if configured.
  AdvancedFn advanced_{}; //!< Advanced callable, if configured.
};

/**
 * @ingroup q2ns_qmap
 * @class ConditionalQMap
 * @brief QMap wrapper that conditionally applies another QMap.
 *
 * The wrapped QMap is sampled normally, but its sampled instance is only
 * executed if the configured predicate returns true.
 */
class ConditionalQMap final : public QMap {
public:
  /**
   * @brief Predicate type controlling whether the wrapped map is applied.
   */
  using Condition = std::function<bool(const std::shared_ptr<Qubit>&, const QMapContext&)>;

  /**
   * @brief Get the ns-3 TypeId.
   * @return TypeId for q2ns::ConditionalQMap.
   */
  static ns3::TypeId GetTypeId();

  /**
   * @brief Set the wrapped QMap.
   * @param qmap Wrapped QMap.
   */
  void SetQMap(ns3::Ptr<QMap> qmap);

  /**
   * @brief Set the condition predicate.
   * @param pred Predicate controlling application.
   */
  void SetCondition(Condition pred);

  /**
   * @brief Sample a per-transmission QMapInstance.
   * @param u Uniform random source.
   * @param ctx Per-transmission context.
   * @return Sampled conditional instance.
   */
  QMapInstance Sample(ns3::Ptr<ns3::UniformRandomVariable> u,
                      const QMapContext& ctx = QMapContext{}) const override;

private:
  ns3::Ptr<QMap> qmap_; //!< Wrapped QMap.
  Condition cond_;      //!< Application predicate.
};

/**
 * @ingroup q2ns_qmap
 * @class DephasingQMap
 * @brief Dephasing noise model that applies Z with probability p.
 */
class DephasingQMap final : public QMap {
public:
  /**
   * @brief Get the ns-3 TypeId.
   * @return TypeId for q2ns::DephasingQMap.
   */
  static ns3::TypeId GetTypeId();

  /**
   * @brief Sample a per-transmission QMapInstance.
   * @param u Uniform random source.
   * @param ctx Per-transmission context.
   * @return Sampled dephasing instance.
   */
  QMapInstance Sample(ns3::Ptr<ns3::UniformRandomVariable> u,
                      const QMapContext& ctx = QMapContext{}) const override;
};

/**
 * @ingroup q2ns_qmap
 * @class DepolarizingQMap
 * @brief Trajectory-style depolarizing model that applies a random Pauli from
 * {X, Y, Z} with probability p.
 */
class DepolarizingQMap final : public QMap {
public:
  /**
   * @brief Get the ns-3 TypeId.
   * @return TypeId for q2ns::DepolarizingQMap.
   */
  static ns3::TypeId GetTypeId();

  /**
   * @brief Sample a per-transmission QMapInstance.
   * @param u Uniform random source.
   * @param ctx Per-transmission context.
   * @return Sampled depolarizing instance.
   */
  QMapInstance Sample(ns3::Ptr<ns3::UniformRandomVariable> u,
                      const QMapContext& ctx = QMapContext{}) const override;
};

/**
 * @ingroup q2ns_qmap
 * @class LossQMap
 * @brief Erasure model that marks the qubit lost with probability p.
 */
class LossQMap final : public QMap {
public:
  /**
   * @brief Get the ns-3 TypeId.
   * @return TypeId for q2ns::LossQMap.
   */
  static ns3::TypeId GetTypeId();

  /**
   * @brief Sample a per-transmission QMapInstance.
   * @param u Uniform random source.
   * @param ctx Per-transmission context.
   * @return Sampled loss instance.
   */
  QMapInstance Sample(ns3::Ptr<ns3::UniformRandomVariable> u,
                      const QMapContext& ctx = QMapContext{}) const override;
};

/**
 * @ingroup q2ns_qmap
 * @class RandomGateQMap
 * @brief QMap that samples one gate from a weighted distribution and applies it.
 *
 * The sampled gate is chosen once per transmission. As with other probabilistic
 * QMaps, the overall application event may itself be gated by Probability or Rate.
 */
class RandomGateQMap final : public QMap {
public:
  /**
   * @brief Get the ns-3 TypeId.
   * @return TypeId for q2ns::RandomGateQMap.
   */
  static ns3::TypeId GetTypeId();

  /**
   * @brief Replace the weighted gate distribution.
   * @param gates Candidate gates.
   * @param weights Non-negative weights associated with the gates.
   */
  void SetDistribution(std::vector<QGate> gates, std::vector<double> weights);

  /**
   * @brief Append one weighted gate to the distribution.
   * @param gate Gate to add.
   * @param weight Non-negative selection weight.
   */
  void AddGate(const QGate& gate, double weight);

  /**
   * @brief Clear the weighted gate distribution.
   */
  void Clear();

  /**
   * @brief Sample a per-transmission QMapInstance.
   * @param u Uniform random source.
   * @param ctx Per-transmission context.
   * @return Sampled random-gate instance.
   */
  QMapInstance Sample(ns3::Ptr<ns3::UniformRandomVariable> u,
                      const QMapContext& ctx = QMapContext{}) const override;

private:
  /**
   * @brief Pick one gate index from the configured weighted distribution.
   * @param u Uniform random source.
   * @return Selected gate index.
   */
  std::size_t PickIndex_(ns3::Ptr<ns3::UniformRandomVariable> u) const;

  std::vector<QGate> gates_;    //!< Candidate gates.
  std::vector<double> weights_; //!< Selection weights.
  double totalWeight_ = 0.0;    //!< Sum of all selection weights.
};

/**
 * @ingroup q2ns_qmap
 * @class RandomUnitaryQMap
 * @brief QMap that applies one Haar-random single-qubit SU(2) unitary.
 *
 * The unitary is sampled once per transmission if the overall application event
 * occurs.
 */
class RandomUnitaryQMap final : public QMap {
public:
  /**
   * @brief Get the ns-3 TypeId.
   * @return TypeId for q2ns::RandomUnitaryQMap.
   */
  static ns3::TypeId GetTypeId();

  /**
   * @brief Sample a per-transmission QMapInstance.
   * @param u Uniform random source.
   * @param ctx Per-transmission context.
   * @return Sampled random-unitary instance.
   */
  QMapInstance Sample(ns3::Ptr<ns3::UniformRandomVariable> u,
                      const QMapContext& ctx = QMapContext{}) const override;

private:
  /**
   * @brief Sample a Haar-random SU(2) matrix.
   * @param u Uniform random source.
   * @return Sampled 2x2 unitary matrix.
   */
  static Matrix SampleHaarSU2_(ns3::Ptr<ns3::UniformRandomVariable> u);
};

} // namespace q2ns