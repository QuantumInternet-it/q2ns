/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/
/**
 * @file q2ns-qmap.cc
 * @brief Defines q2ns::QMap and standard channel-map implementations.
 */

#include "ns3/q2ns-qmap.h"

#include "ns3/q2ns-qgate.h"
#include "ns3/q2ns-qnode.h"
#include "ns3/q2ns-qubit.h"

#include "ns3/attribute.h"
#include "ns3/double.h"
#include "ns3/integer.h"
#include "ns3/log.h"
#include "ns3/string.h"

#include <cctype>
#include <utility>

namespace q2ns {

NS_LOG_COMPONENT_DEFINE("QMap");

NS_OBJECT_ENSURE_REGISTERED(QMap);

ns3::TypeId QMap::GetTypeId() {
  static ns3::TypeId tid =
      ns3::TypeId("q2ns::QMap")
          .SetParent<ns3::Object>()
          .SetGroupName("q2ns")
          .AddAttribute("Probability",
                        "Base application probability used when Rate is 0. "
                        "If Rate > 0, Probability is ignored.",
                        ns3::DoubleValue(0.0), ns3::MakeDoubleAccessor(&QMap::p_),
                        ns3::MakeDoubleChecker<double>(0.0, 1.0))
          .AddAttribute("Rate",
                        "QMap event rate in 1/s. If > 0, the per-flight "
                        "application probability is p(t)=1-exp(-Rate*elapsedTime).",
                        ns3::DoubleValue(0.0), ns3::MakeDoubleAccessor(&QMap::rate_),
                        ns3::MakeDoubleChecker<double>(0.0));
  return tid;
}



void QMap::SetLost_(Qubit& q) {
  q.SetLocationLost();
}

namespace {

/**
 * @class CompositeQMap
 * @brief Internal QMap implementation for sequential composition.
 *
 * This type is intentionally hidden from users. It samples each component map
 * once for the current transmission and later executes the sampled instances
 * left to right at the receiver.
 */
class CompositeQMap final : public QMap {
public:
  static ns3::TypeId GetTypeId() {
    static ns3::TypeId tid = ns3::TypeId("q2ns::CompositeQMap")
                                 .SetParent<QMap>()
                                 .SetGroupName("q2ns")
                                 .AddConstructor<CompositeQMap>();
    return tid;
  }

  CompositeQMap() = default;



  void SetMaps(const std::vector<ns3::Ptr<QMap>>& maps) {
    maps_ = maps;
  }



  QMapInstance Sample(ns3::Ptr<ns3::UniformRandomVariable> u,
                      const QMapContext& ctx = QMapContext{}) const override {
    std::vector<QMapInstance> inst;
    inst.reserve(maps_.size());
    for (const auto& m : maps_) {
      inst.push_back(m ? m->Sample(u, ctx) : QMapInstance{});
    }

    return [inst = std::move(inst)](QNode& node, std::shared_ptr<Qubit>& q) mutable {
      for (auto& f : inst) {
        if (f) {
          f(node, q);
        }

        // Stop immediately if one stage marks the qubit lost.
        auto loc = q->GetLocation();
        if (loc.type == LocationType::Lost) {
          return;
        }
      }
    };
  }

private:
  std::vector<ns3::Ptr<QMap>> maps_; //!< Component maps in left-to-right order.
};

NS_OBJECT_ENSURE_REGISTERED(CompositeQMap);

} // namespace



ns3::Ptr<QMap> QMap::Compose(const ns3::Ptr<QMap>& a, const ns3::Ptr<QMap>& b) {
  std::vector<ns3::Ptr<QMap>> v;
  v.reserve(2);
  v.push_back(a);
  v.push_back(b);
  return Compose(v);
}



ns3::Ptr<QMap> QMap::Compose(const std::vector<ns3::Ptr<QMap>>& maps) {
  auto out = ns3::CreateObject<CompositeQMap>();
  out->SetMaps(maps);
  return out;
}



ns3::Ptr<QMap> QMap::FromLambda(std::function<void(QNode&, std::shared_ptr<Qubit>&)> f) {
  return ns3::CreateObject<LambdaQMap>(std::move(f));
}



ns3::Ptr<QMap>
QMap::FromLambda(std::function<void(QNode&, std::shared_ptr<Qubit>&,
                                    ns3::Ptr<ns3::UniformRandomVariable>, const QMapContext&)>
                     f) {
  return ns3::CreateObject<LambdaQMap>(std::move(f));
}



NS_OBJECT_ENSURE_REGISTERED(LambdaQMap);

ns3::TypeId LambdaQMap::GetTypeId() {
  static ns3::TypeId tid = ns3::TypeId("q2ns::LambdaQMap")
                               .SetParent<QMap>()
                               .SetGroupName("q2ns")
                               .AddConstructor<LambdaQMap>();
  return tid;
}



LambdaQMap::LambdaQMap(SimpleFn f) : simple_(std::move(f)) {}



LambdaQMap::LambdaQMap(AdvancedFn f) : advanced_(std::move(f)) {}



void LambdaQMap::Set(SimpleFn f) {
  simple_ = std::move(f);
  advanced_ = AdvancedFn{};
}



void LambdaQMap::Set(AdvancedFn f) {
  advanced_ = std::move(f);
  simple_ = SimpleFn{};
}



QMapInstance LambdaQMap::Sample(ns3::Ptr<ns3::UniformRandomVariable> u,
                                const QMapContext& ctx) const {
  // Prefer the advanced callable when present so the caller can use the sampled
  // context and RNG source directly.
  if (advanced_) {
    auto adv = advanced_;
    return [adv, u, ctx](QNode& node, std::shared_ptr<Qubit>& q) mutable { adv(node, q, u, ctx); };
  }

  if (simple_) {
    auto simp = simple_;
    return [simp](QNode& node, std::shared_ptr<Qubit>& q) mutable { simp(node, q); };
  }

  return QMapInstance{};
}



NS_OBJECT_ENSURE_REGISTERED(ConditionalQMap);

ns3::TypeId ConditionalQMap::GetTypeId() {
  static ns3::TypeId tid = ns3::TypeId("q2ns::ConditionalQMap")
                               .SetParent<QMap>()
                               .SetGroupName("q2ns")
                               .AddConstructor<ConditionalQMap>();
  return tid;
}



void ConditionalQMap::SetQMap(ns3::Ptr<QMap> qmap) {
  qmap_ = qmap;
}



void ConditionalQMap::SetCondition(Condition pred) {
  cond_ = std::move(pred);
}



QMapInstance ConditionalQMap::Sample(ns3::Ptr<ns3::UniformRandomVariable> u,
                                     const QMapContext& ctx) const {
  const auto pred = cond_;
  QMapInstance qmapInst = qmap_ ? qmap_->Sample(u, ctx) : QMapInstance{};

  return [pred, ctx, qmapInst](QNode& node, std::shared_ptr<Qubit>& q) mutable {
    if (!qmapInst) {
      return;
    }
    if (!pred || pred(q, ctx)) {
      qmapInst(node, q);
    }
  };
}



NS_OBJECT_ENSURE_REGISTERED(DephasingQMap);

ns3::TypeId DephasingQMap::GetTypeId() {
  static ns3::TypeId tid = ns3::TypeId("q2ns::DephasingQMap")
                               .SetParent<QMap>()
                               .SetGroupName("q2ns")
                               .AddConstructor<DephasingQMap>();
  return tid;
}



QMapInstance DephasingQMap::Sample(ns3::Ptr<ns3::UniformRandomVariable> u,
                                   const QMapContext& ctx) const {
  const bool flip = Bernoulli_(u, ctx);
  return [flip](QNode& node, std::shared_ptr<Qubit>& q) {
    if (flip) {
      node.Apply(q2ns::gates::Z(), {q});
    }
  };
}



NS_OBJECT_ENSURE_REGISTERED(DepolarizingQMap);

ns3::TypeId DepolarizingQMap::GetTypeId() {
  static ns3::TypeId tid = ns3::TypeId("q2ns::DepolarizingQMap")
                               .SetParent<QMap>()
                               .SetGroupName("q2ns")
                               .AddConstructor<DepolarizingQMap>();
  return tid;
}



QMapInstance DepolarizingQMap::Sample(ns3::Ptr<ns3::UniformRandomVariable> u,
                                      const QMapContext& ctx) const {
  const bool apply = Bernoulli_(u, ctx);

  uint32_t r = 0;
  if (apply) {
    r = u->GetInteger(0, 2);
  }

  return [apply, r](QNode& node, std::shared_ptr<Qubit>& q) {
    if (!apply) {
      return;
    }
    if (r == 0) {
      node.Apply(q2ns::gates::X(), {q});
    } else if (r == 1) {
      node.Apply(q2ns::gates::Y(), {q});
    } else {
      node.Apply(q2ns::gates::Z(), {q});
    }
  };
}



NS_OBJECT_ENSURE_REGISTERED(LossQMap);

ns3::TypeId LossQMap::GetTypeId() {
  static ns3::TypeId tid = ns3::TypeId("q2ns::LossQMap")
                               .SetParent<QMap>()
                               .SetGroupName("q2ns")
                               .AddConstructor<LossQMap>();
  return tid;
}



QMapInstance LossQMap::Sample(ns3::Ptr<ns3::UniformRandomVariable> u,
                              const QMapContext& ctx) const {
  const bool lose = Bernoulli_(u, ctx);
  return [lose](QNode&, std::shared_ptr<Qubit>& q) {
    if (lose) {
      SetLost_(*q);
    }
  };
}



NS_OBJECT_ENSURE_REGISTERED(RandomGateQMap);

ns3::TypeId RandomGateQMap::GetTypeId() {
  static ns3::TypeId tid = ns3::TypeId("q2ns::RandomGateQMap")
                               .SetParent<QMap>()
                               .SetGroupName("q2ns")
                               .AddConstructor<RandomGateQMap>();
  return tid;
}



void RandomGateQMap::Clear() {
  gates_.clear();
  weights_.clear();
  totalWeight_ = 0.0;
}



void RandomGateQMap::AddGate(const QGate& gate, double weight) {
  NS_ABORT_MSG_IF(weight < 0.0, "RandomGateQMap: weight must be >= 0");
  gates_.push_back(gate);
  weights_.push_back(weight);
  totalWeight_ += weight;
}



void RandomGateQMap::SetDistribution(std::vector<QGate> gates, std::vector<double> weights) {
  NS_ABORT_MSG_IF(gates.size() != weights.size(),
                  "RandomGateQMap: gates and weights must have the same length");

  gates_ = std::move(gates);
  weights_ = std::move(weights);

  totalWeight_ = 0.0;
  for (double w : weights_) {
    NS_ABORT_MSG_IF(w < 0.0, "RandomGateQMap: weight must be >= 0");
    totalWeight_ += w;
  }
}



std::size_t RandomGateQMap::PickIndex_(ns3::Ptr<ns3::UniformRandomVariable> u) const {
  NS_ABORT_MSG_IF(gates_.empty(), "RandomGateQMap: no gates configured");
  NS_ABORT_MSG_IF(totalWeight_ <= 0.0, "RandomGateQMap: total weight must be > 0");

  const double r = u->GetValue(0.0, totalWeight_);
  double acc = 0.0;
  for (std::size_t i = 0; i < weights_.size(); ++i) {
    acc += weights_[i];
    if (r <= acc) {
      return i;
    }
  }
  return weights_.size() - 1;
}



QMapInstance RandomGateQMap::Sample(ns3::Ptr<ns3::UniformRandomVariable> u,
                                    const QMapContext& ctx) const {
  const bool apply = Bernoulli_(u, ctx);

  std::size_t idx = 0;
  QGate gate;
  if (apply) {
    idx = PickIndex_(u);
    gate = gates_[idx];
  }

  return [apply, gate = std::move(gate)](QNode& node, std::shared_ptr<Qubit>& q) mutable {
    if (apply) {
      node.Apply(gate, {q});
    }
  };
}



NS_OBJECT_ENSURE_REGISTERED(RandomUnitaryQMap);

ns3::TypeId RandomUnitaryQMap::GetTypeId() {
  static ns3::TypeId tid = ns3::TypeId("q2ns::RandomUnitaryQMap")
                               .SetParent<QMap>()
                               .SetGroupName("q2ns")
                               .AddConstructor<RandomUnitaryQMap>();
  return tid;
}



// Haar-random SU(2) from a random unit quaternion on S^3.
// Converted to the 2x2 unitary
// [ u0 + i u3     u2 + i u1 ]
// [ -u2 + i u1    u0 - i u3 ].
Matrix RandomUnitaryQMap::SampleHaarSU2_(ns3::Ptr<ns3::UniformRandomVariable> u) {
  const double a = u->GetValue(0.0, 1.0);
  const double b = u->GetValue(0.0, 1.0);
  const double c = u->GetValue(0.0, 1.0);

  const double s1 = std::sqrt(1.0 - a);
  const double s2 = std::sqrt(a);
  constexpr double kTwoPi = 6.28318530717958647692;
  const double t1 = kTwoPi * b;
  const double t2 = kTwoPi * c;

  const double u0 = s2 * std::cos(t2);
  const double u3 = s2 * std::sin(t2);
  const double u2 = s1 * std::cos(t1);
  const double u1 = s1 * std::sin(t1);

  const Complex A(u0, u3);
  const Complex B(u2, u1);
  const Complex C(-u2, u1);
  const Complex D(u0, -u3);

  return MakeMatrix({{A, B}, {C, D}});
}



QMapInstance RandomUnitaryQMap::Sample(ns3::Ptr<ns3::UniformRandomVariable> u,
                                       const QMapContext& ctx) const {
  const bool apply = Bernoulli_(u, ctx);

  Matrix U;
  if (apply) {
    U = SampleHaarSU2_(u);
  }

  return [apply, U = std::move(U)](QNode& node, std::shared_ptr<Qubit>& q) mutable {
    if (apply) {
      node.Apply(U, {q});
    }
  };
}

} // namespace q2ns