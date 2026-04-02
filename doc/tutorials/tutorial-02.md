# Q2NS Tutorial 2: More on Quantum Operations {#q2ns_tutorial_qops}

This tutorial builds on [Tutorial 1](tutorial-01.md) and covers the quantum operations and noise models you will reach for in real protocol simulations.
By the end of this tutorial you will know how to:

- Measure qubits in different Pauli bases and build custom circuits
- Model noisy quantum channels using QMaps
- Compute the fidelity of a received state against an ideal reference

This tutorial only uses quantum channels -- no classical TCP/UDP yet. For hybrid quantum-classical protocols see [Tutorial 3](tutorial-03.md).

## 1. Custom Quantum Circuit

Quantum computation and quantum communication protocols often require measurements in bases other than the computational (Z) basis. Here we build a small circuit that:

1. Prepares the state |1⟩ using an X gate
2. Applies additional gates (H, S) to reach specific target states
3. Measures each result in the Pauli basis where the outcome is deterministic

**Measurement bases in Q2NS:**

| Basis      | $$r=0 \textbf{ eigenstate}$$                                   | $$r=1 \textbf{ eigenstate}$$                                   | What it distinguishes                |
| ---------- | -------------------------------------------------------------- | -------------------------------------------------------------- | ------------------------------------ |
| `Basis::Z` | $$\|0\rangle$$                                                 | $$\|1\rangle$$                                                 | Computational amplitude -- bit value |
| `Basis::X` | $$\|{+}\rangle = \frac{1}{\sqrt{2}}(\|0\rangle+\|1\rangle)$$   | $$\|{-}\rangle = \frac{1}{\sqrt{2}}(\|0\rangle-\|1\rangle)$$   | Relative sign of the superposition   |
| `Basis::Y` | $$\|{+i}\rangle = \frac{1}{\sqrt{2}}(\|0\rangle+i\|1\rangle)$$ | $$\|{-i}\rangle = \frac{1}{\sqrt{2}}(\|0\rangle-i\|1\rangle)$$ | Imaginary relative phase             |

`Measure` returns 0 for the r = 0 eigenstate and 1 for the r = 1 eigenstate.
The measurement probability follows the Born rule. For a pure state |ψ⟩ and basis eigenstates {|e₀⟩, |e₁⟩}:

$$P(r) = |\langle e_r | \psi \rangle|^2$$

For the general case of a mixed state ρ (used by the `DM` backend), this generalises to:

$$P(r) = \mathrm{Tr}(\Pi_r\,\rho), \qquad \Pi_r = |e_r\rangle\langle e_r|$$

This reduces to the pure-state formula when

$$\rho = |\psi\rangle\langle\psi|$$

```cpp
int r = node->Measure(q, Basis::Z);    // Z-basis
int r = node->Measure(q, Basis::X);    // X-basis
int r = node->Measure(q, Basis::Y);    // Y-basis
```

A qubit in an eigenstate of the measurement basis always yields the matching deterministic outcome. For example, |+⟩ measured in the X-basis always returns 0. That is the idea behind the three tests below:

| Test | Initial state  | Gate(s)  | Basis | Expected | Why                                                                     |
| ---- | -------------- | -------- | ----- | -------- | ----------------------------------------------------------------------- |
| 1    | $$\|0\rangle$$ | H        | X     | 0        | $$\|0\rangle\to\|{+}\rangle \text{ the } + \text{eigenstate of X}$$     |
| 2    | $$\|1\rangle$$ | H        | X     | 1        | $$\|1\rangle\to\|{-}\rangle \text{, the } - \text{ eigenstate of X}$$   |
| 3    | $$\|0\rangle$$ | H then S | Y     | 0        | $$\|0\rangle\to\|{+i}\rangle \text{, the } +i \text{ eigenstate of Y}$$ |

Note that `Measure` collapses the qubit, so a separate qubit is needed for each test. In order to correctly simulate randomness, every measurement needs to be scheduled, and the seeds set before the run.

The key API used in this section:

```cpp
// Allocate a qubit (always |0>)
auto q = node->CreateQubit();

Simulator::Schedule(MicroSeconds(10), [node, q]() {
    node->Apply(gates::X(), {q});           // bit-flip
    node->Apply(gates::H(), {q});           // Hadamard
    int r = node->Measure(q, Basis::X);     // X-basis measurement
    std::cout << r << "\n";                 // prints 1 (with p=1)
});

Simulator::Stop(MicroSeconds(100));
Simulator::Run();
Simulator::Destroy();
```

Each test uses its own qubit because `Measure` collapses the state. Allocating
all qubits before `Run()` and scheduling the gates + measurements as separate
events mirrors the event-driven structure used throughout Q2NS.

The S gate (phase gate) multiplies the |1⟩ amplitude by i, turning |+⟩ into |+i⟩. Explicitly,

$$
|+\rangle = \frac{|0\rangle + |1\rangle}{\sqrt{2}}, \qquad
|+i\rangle = \frac{|0\rangle + i|1\rangle}{\sqrt{2}}.
$$

The Ket and Stab backends both support all three of these Clifford gates.

**Custom gates** in Q2NS are arbitrary unitaries passed as a matrix. To compose H and S into a single gate object, multiply their matrices -- in standard matrix notation the rightmost matrix is applied first, so `MatrixS() * MatrixH()` means "apply H then S":

```cpp
// S*H as a single gate descriptor -- built via matrix product
// (H is applied first, then S, right-to-left convention)
auto HS = gates::Custom(MatrixS() * MatrixH());

Simulator::Schedule(MicroSeconds(40), [node, q, HS]() {
    node->Apply(HS, {q});   // identical to Apply(H) then Apply(S)
});
```

Alternatively, the same gate can be defined entry-by-entry with `MakeMatrix` and `Complex{real, imag}`. The matrix product is

$$S \cdot H = \frac{1}{\sqrt{2}} \begin{pmatrix} 1 & 1 \\\\ i & -i \end{pmatrix}$$

```cpp
const double s = 1.0 / std::sqrt(2.0);
auto HS_explicit = gates::Custom(MakeMatrix({
    {Complex{s, 0.0}, Complex{s,  0.0}},   // row 0: (1/sqrt(2)) * [1,  1]
    {Complex{0.0, s}, Complex{0.0, -s}},   // row 1: (1/sqrt(2)) * [i, -i]
}));
```

Both representations produce exactly the same gate. The `MakeMatrix` form is useful when a custom unitary comes from an analytical derivation or an external specification and cannot be expressed as a product of built-in gates.

Any 2^k × 2^k unitary matrix can be wrapped this way. Using a custom gate is convenient when the composed matrix is computed once and reused across many qubits.

**Expected output:**

```
Test 1  H|0> in X-basis:           0  (expect 0)
Test 2  H|1> in X-basis:           1  (expect 1)
Test 3  S*H|0> in Y-basis:         0  (expect 0)
Test 4  Custom HS (S*H)|0> in Y:   0  (same as Test 3)
Test 5  Custom HS (explicit)|0> Y: 0  (same as Test 3)
```

<details>
<summary>
  Full example: q2ns-2-basis-measurement-example.cc
</summary>

```cpp
#include "ns3/core-module.h"
#include "ns3/q2ns-netcontroller.h"
#include "ns3/q2ns-qgate.h"
#include "ns3/q2ns-qnode.h"
#include "ns3/q2ns-types.h"
#include "ns3/rng-seed-manager.h"
#include <iostream>

using namespace ns3;
using namespace q2ns;

int main(int, char**) {
  RngSeedManager::SetSeed(42);
  RngSeedManager::SetRun(1);

  NetController net;
  net.SetQStateBackend(QStateBackend::Ket);
  auto N = net.CreateNode();

  // Allocate all qubits before the simulation starts
  auto q1 = N->CreateQubit();
  auto q2 = N->CreateQubit();
  auto q3 = N->CreateQubit();
  auto q4 = N->CreateQubit();
  auto q5 = N->CreateQubit();

  // Pre-compute custom gates (matrix product: rightmost applied first)
  auto HS = gates::Custom(MatrixS() * MatrixH());

  // Or, alternatively: S*H = (1/sqrt(2)) [[1, 1], [i, -i]]
  const double s = 1.0 / std::sqrt(2.0);
  auto HS_explicit = gates::Custom(MakeMatrix({
      {Complex{s, 0.0}, Complex{s,  0.0}},
      {Complex{0.0, s}, Complex{0.0, -s}},
  }));

  // Test 1 at t=10 us: H|0> -> |+>, measure in X-basis -> always 0
  Simulator::Schedule(MicroSeconds(10), [N, q1]() {
    N->Apply(gates::H(), {q1});
    std::cout << "Test 1  H|0> in X-basis:           "
              << N->Measure(q1, Basis::X) << "  (expect 0)\n";
  });

  // Test 2 at t=20 us: X then H -- |0> -> |1> -> |->, measure in X-basis -> always 1
  // |-> = (|0> - |1>)/sqrt(2) is the -1 eigenstate of X
  Simulator::Schedule(MicroSeconds(20), [N, q2]() {
    N->Apply(gates::X(), {q2});  // |0> -> |1>
    N->Apply(gates::H(), {q2});  // |1> -> |->
    std::cout << "Test 2  H|1> in X-basis:           "
              << N->Measure(q2, Basis::X) << "  (expect 1)\n";
  });

  // Test 3 at t=30 us: H then S -- |0> -> |+> -> |+i>, measure in Y-basis -> always 0
  // S maps |+> to (|0> + i|1>)/sqrt(2) = |+i>, the +1 eigenstate of Y
  Simulator::Schedule(MicroSeconds(30), [N, q3]() {
    N->Apply(gates::H(), {q3});  // |0> -> |+>
    N->Apply(gates::S(), {q3});  // |+> -> |+i>
    std::cout << "Test 3  S*H|0> in Y-basis:         "
              << N->Measure(q3, Basis::Y) << "  (expect 0)\n";
  });

  // Test 4 at t=40 us: same result using a single custom gate (matrix product form)
  Simulator::Schedule(MicroSeconds(40), [N, q4, HS]() {
    N->Apply(HS, {q4});
    std::cout << "Test 4  Custom HS (S*H)|0> in Y:   "
              << N->Measure(q4, Basis::Y) << "  (same as Test 3)\n";
  });

  // Test 5 at t=50 us: same result using a single custom gate (explicit matrix form)
  Simulator::Schedule(MicroSeconds(50), [N, q5, HS_explicit]() {
    N->Apply(HS_explicit, {q5});
    std::cout << "Test 5  Custom HS (explicit)|0> Y: "
              << N->Measure(q5, Basis::Y) << "  (same as Test 3)\n";
  });

  Simulator::Stop(MicroSeconds(100));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}
```

</details>

## 2. Quantum Noise with QMaps

Real quantum channels are imperfect. Q2NS models channel noise using **QMaps** -- objects that are sampled once per qubit transmission and applied at the receiving node before the receive callback fires.

Here we:

1. Create two nodes A and B connected by a quantum link
2. Attach a QMap to the link to model physical imperfections
3. Send a qubit from A to B and observe how the QMap transforms it

Start with the usual setup and a receive callback that prints the arriving state:

```cpp
NetController net;
auto A = net.CreateNode();
auto B = net.CreateNode();

B->SetRecvCallback([&net](std::shared_ptr<Qubit> q) {
    std::cout << "Qubit arrived at t=" << Simulator::Now() << "\n"
              << "  state: " << net.GetState(q) << "\n";
});
```

Q2NS' `QMap` provides a set of pre-defined noise maps, tunable according to the desired use-case. Some examples include, but are not limited to:

**Loss:** `LossQMap` models qubit absorption, corresponding physically to the **qubit erasure channel** (the quantum state is discarded and replaced by an orthogonal loss flag). When it fires the qubit is marked lost and the receive callback is **not** triggered:

```cpp
auto loss = CreateObject<LossQMap>();
loss->SetAttribute("Probability", DoubleValue(0.5));  // 50% erasure
```

**Depolarizing noise:** `DepolarizingQMap` implements the single-qubit depolarizing channel, a CPTP map defined by:

$$\varepsilon(\rho) = (1-p)\,\rho + \frac{p}{3}\bigl(X\rho X + Y\rho Y + Z\rho Z\bigr)$$

where p ∈ [0, 1] is the error probability. It accepts either a per-transmission probability or a physical rate that Q2NS converts using the channel flight time:

```cpp
// Fixed probability per qubit
auto depol = CreateObject<DepolarizingQMap>();
depol->SetAttribute("Probability", DoubleValue(0.02));

// Physical rate (events/s): Q2NS converts it via the Poisson formula at link install time
auto depol2 = CreateObject<DepolarizingQMap>();
depol2->SetAttribute("Rate", DoubleValue(1e8));
```

Use `Probability` for a per-transmission error budget; use `Rate` when your
noise model comes from a measured physical quantity (e.g. a fibre loss
coefficient in dB/km with a known propagation delay). When errors follow a **homogeneous Poisson process** with rate λ (errors/s), the probability of at least one error during flight time Δt is given by the Poisson survival formula, which Q2NS uses internally:

$$p = 1 - e^{-\lambda \, \Delta t}$$

where Δt is the link propagation delay passed to `InstallQuantumLink`.
As a worked example, the default `depolRate=1e8` with the 10 ns channel delay used
in the demo gives

$$p = 1 - e^{-10^8 \times 10^{-8}} = 1 - e^{-1} \approx 0.632$$

so roughly 63 % of transmitted qubits will receive a random Pauli error.
To stay in the low-noise regime you would choose a rate such that λΔt ≪ 1
(for example, `Rate=1e6` on a 10 ns link gives p ≈ 1%).

**Built-in QMap types:**

| Type                | Effect                                                |
| ------------------- | ----------------------------------------------------- |
| `LossQMap`          | Marks qubit lost -- receive callback is not triggered |
| `DepolarizingQMap`  | Random Pauli (X, Y, or Z) with equal probability      |
| `DephasingQMap`     | Z gate (phase flip) with given probability            |
| `RandomGateQMap`    | Draws from a user-supplied weighted gate set          |
| `RandomUnitaryQMap` | Applies a Haar-random unitary                         |

**Composing noise models:**

Multiple QMaps are combined with `QMap::Compose`, which chains them sequentially. The first argument is applied first:

$$\texttt{Compose}(\varepsilon_1, \varepsilon_2, \ldots, \varepsilon_n) \;:\; \rho \;\mapsto\; (\varepsilon_n \circ \cdots \circ \varepsilon_2 \circ \varepsilon_1)(\rho)$$

For instance, the following composes a loss channel followed by depolarizing noise: a qubit that survives loss then undergoes random Pauli errors.

```cpp
auto map = QMap::Compose({loss, depol});
auto ch = net.InstallQuantumLink(A, B);
ch->SetAttribute("Delay", TimeValue(NanoSeconds(10)));
ch->SetAttribute("QMap", PointerValue(map));
```

**Lambda QMaps:**

User-defined noise maps are also supported.  
For custom noise logic, use `QMap::FromLambda`. The simplest overload takes the receiving node and the qubit:

```cpp
// Always apply an S gate on arrival
auto sGate = QMap::FromLambda(
    [](QNode& node, std::shared_ptr<Qubit>& q) {
        node.Apply(gates::S(), {q});
    });
```

A second overload adds a random-variable handle and a `QMapContext` carrying flight-time metadata, enabling rate-based stochastic maps:

```cpp
const double rate = 5e6; // 1/s
auto custom = QMap::FromLambda(
    [rate](QNode& node, std::shared_ptr<Qubit>& q,
           Ptr<UniformRandomVariable> u, const QMapContext& ctx) {
        const double p = QMap::RateToProb(rate, ctx.elapsedTime);
        if (p > 0.0 && u->GetValue(0.0, 1.0) < p) {
            node.Apply(gates::X(), {q});
        }
    });
```

**Conditional QMaps** trigger a wrapped map only when a predicate on the
context is true. Here the qubit is always lost when the flight time exceeds 20 ns
(i.e., only long links are lossy):

```cpp
auto innerLoss = CreateObject<LossQMap>();
innerLoss->SetAttribute("Probability", DoubleValue(1.0));  // certain loss when triggered

auto cond = CreateObject<ConditionalQMap>();
cond->SetQMap(innerLoss);
cond->SetCondition([](const std::shared_ptr<Qubit>&, const QMapContext& ctx) {
    return ctx.elapsedTime > NanoSeconds(20);   // trigger only on links longer than 20 ns
});
```

Once the link is installed with a map, sending works the same as in the noiseless case. Since `Send` must be called during the simulation run, schedule it as an event. The QMap fires automatically at B before the receive callback:

```cpp
auto ch = net.InstallQuantumLink(A, B);
ch->SetAttribute("Delay", TimeValue(NanoSeconds(10)));
ch->SetAttribute("QMap", PointerValue(map));

auto q = A->CreateQubit();
A->Apply(gates::H(), {q});    // prepare |+> before the run

Simulator::Schedule(NanoSeconds(1), [A, B, q]() {
    A->Send(q, B->GetId());   // QMap fires at B after the 10 ns delay
});

Simulator::Stop(MilliSeconds(1));
Simulator::Run();

const bool lost = (q->GetLocation().type == LocationType::Lost);
std::cout << "Qubit status: " << (lost ? "LOST" : "delivered") << "\n";

Simulator::Destroy();
```

The qubit status line is always printed after the simulation ends -- if the loss map fired, the receive callback is never triggered but `q->GetLocation()` still reflects `LocationType::Lost`. **Expected output (default `--mode=loss+depol`, `--lossP=0.5`):**

```
Running loss+depol
Qubit received at +11ns          <- only when qubit survives loss
In state: QState{backend=Ket, id=1, n=1}
(0.707107,0)
(0.707107,0)                     <- or a Pauli-rotated variant when depol fires
Qubit status: delivered          <- or: LOST (when loss map fires)
```

<details>
<summary>
  Full example: q2ns-2-qmap-example.cc
</summary>

```cpp
#include "ns3/core-module.h"
#include "ns3/network-module.h"

#include "ns3/q2ns-netcontroller.h"
#include "ns3/q2ns-qmap.h"
#include "ns3/q2ns-qnode.h"
#include "ns3/q2ns-qstate.h"
#include "ns3/q2ns-qubit.h"
#include "ns3/simulator.h"

#include <random>
#include <iostream>
#include <string>

using namespace ns3;
using namespace q2ns;

int main(int argc, char* argv[]) {

  RngSeedManager::SetSeed(std::random_device{}() | 1u);

  std::string mode = "loss+depol"; // default demo
  double lossP = 0.5;
  double depolRate = 1e8;

  CommandLine cmd;
  cmd.AddValue(
      "mode",
      "Demo mode: loss+depol | randomgate | randomunitary | conditional | lambda | lambda-random",
      mode);
  cmd.AddValue("lossP", "Loss probability used in loss+depol mode", lossP);
  cmd.AddValue("depolRate", "Depolarizing rate [1/s] used in loss+depol mode", depolRate);
  cmd.Parse(argc, argv);

  NetController net;
  auto A = net.CreateNode();
  auto B = net.CreateNode();

  B->SetRecvCallback([&net](std::shared_ptr<Qubit> q) {
    std::cout << "Qubit received at " << Simulator::Now() << "\n";
    std::cout << "In state: " << net.GetState(q) << "\n";
  });

  Ptr<QMap> map;
  std::cout << "Running " << mode << "\n";

  if (mode == "loss+depol") {
    auto loss = CreateObject<LossQMap>();
    loss->SetAttribute("Probability", DoubleValue(lossP));

    auto depol = CreateObject<DepolarizingQMap>();
    depol->SetAttribute("Rate", DoubleValue(depolRate));

    map = QMap::Compose({loss, depol});
  } else if (mode == "randomgate") {
    auto rg = CreateObject<RandomGateQMap>();
    rg->AddGate(gates::X(), 1.0);
    rg->AddGate(gates::Z(), 2.0);
    rg->SetAttribute("Probability", DoubleValue(1.0));
    map = rg;
  } else if (mode == "randomunitary") {
    auto ru = CreateObject<RandomUnitaryQMap>();
    ru->SetAttribute("Probability", DoubleValue(1.0));
    map = ru;
  } else if (mode == "conditional") {
    auto loss = CreateObject<LossQMap>();
    loss->SetAttribute("Probability", DoubleValue(1.0));

    auto cond = CreateObject<ConditionalQMap>();
    cond->SetQMap(loss);
    cond->SetCondition([](const std::shared_ptr<Qubit>&, const QMapContext& ctx) {
      return ctx.elapsedTime > NanoSeconds(20);
    });
    map = cond;
  } else if (mode == "lambda") {
    map = QMap::FromLambda(
        [](QNode& node, std::shared_ptr<Qubit>& q) { node.Apply(gates::S(), {q}); });
  } else if (mode == "lambda-random") {
    const double rate = 5e6; // 1/s
    map = QMap::FromLambda([rate](QNode& node, std::shared_ptr<Qubit>& q,
                                  Ptr<UniformRandomVariable> u, const QMapContext& ctx) {
      const double p = QMap::RateToProb(rate, ctx.elapsedTime);
      if (p > 0.0 && u->GetValue(0.0, 1.0) < p) {
        node.Apply(gates::X(), {q});
      }
    });
  } else {
    NS_ABORT_MSG("Unknown mode: " << mode);
  }

  auto ch = net.InstallQuantumLink(A, B);
  ch->SetAttribute("Delay", TimeValue(NanoSeconds(10)));
  ch->SetAttribute("QMap", PointerValue(map));

  // Allocate and prepare the qubit before the simulation starts
  auto q = A->CreateQubit();
  A->Apply(gates::H(), {q});  // prepare |+>

  // Schedule the send; the QMap fires at B after the link delay
  Simulator::Schedule(NanoSeconds(1), [A, B, q]() {
    A->Send(q, B->GetId());
  });

  Simulator::Stop(MilliSeconds(1));
  Simulator::Run();

  const bool lost = (q->GetLocation().type == LocationType::Lost);
  std::cout << "Qubit status: " << (lost ? "LOST" : "delivered") << "\n";

  Simulator::Destroy();
  return 0;
}
```

</details>

## 3. Fidelity Analysis

After sending a qubit through a noisy channel you often want to quantify how close the received state is to the ideal one. Q2NS provides
`q2ns::analysis::Fidelity` for this purpose.

Fidelity is a number in [0, 1]: a value of 1 means the two states are identical, 0 means they are orthogonal. For two pure states the formula is

$$F(|\psi\rangle, |\phi\rangle) = |\langle\psi|\phi\rangle|^2$$

For mixed states Q2NS uses the full Uhlmann fidelity internally:

$$F(\rho,\sigma) = \bigl(\mathrm{Tr}\sqrt{\sqrt{\rho}\,\sigma\sqrt{\rho}}\bigr)^2.$$

So you do not need to distinguish cases yourself.

Include the analysis header:

```cpp
#include "ns3/q2ns-analysis.h"
```

Create the ideal reference state on a dedicated node inside the same
`NetController` used for the experiment. The reference node is never
connected to any link so the qubit it holds is never sent anywhere:

```cpp
NetController net;

auto ref_node = net.CreateNode();
auto qref = ref_node->CreateQubit();
ref_node->Apply(gates::H(), {qref});   // ideal |+>
auto ideal = net.GetState(qref);
```

Run the simulation with a noisy channel. In the receive callback, save a
handle to the received `QState`:

```cpp
std::shared_ptr<QState> received;
B->SetRecvCallback([&](std::shared_ptr<Qubit> q) {
    received = net.GetState(q);
});
```

After `Simulator::Run()`, compare the two states:

```cpp
double f = q2ns::analysis::Fidelity(*ideal, *received);
std::cout << "Fidelity: " << f << "\n";
```

A typical use is a fidelity-vs-noise sweep.
Between trials only the `Probability` attribute is updated -- no teardown or reconstruction is needed. The simulation is advanced with successive `Simulator::Run()` calls; `Simulator::Destroy()` is called once at the end:

```cpp
for (double p : {0.0, 0.10, 0.25, 0.50, 0.75, 1.0}) {
    received = nullptr;
    depol->SetAttribute("Probability", DoubleValue(p));

    auto q = A->CreateQubit();
    A->Apply(gates::H(), {q});

    Simulator::Schedule(NanoSeconds(1), [A, B, q]() {
        A->Send(q, B->GetId());
    });

    // Advance by 1 ms (>> 10 ns delay); simulation time accumulates across trials.
    Simulator::Stop(Simulator::Now() + MilliSeconds(1));
    Simulator::Run();

    const double f = received ? q2ns::analysis::Fidelity(*ideal, *received) : -1.0;
    std::cout << "p=" << p << "  F=" << f << "\n";
}

Simulator::Destroy();
```

Since `ideal` is always a pure state, `q2ns::analysis::Fidelity` uses the simplified form

$$F = \langle\psi|\sigma|\psi\rangle,$$

where σ is the (potentially mixed) received state. For the depolarizing channel this becomes

$$F = 1 - \frac{2p}{3}.$$

It falls from 1 at p = 0 to 1/2 at p = 3/4, where the channel reduces every input to the maximally mixed state I/2.

> `Fidelity` requires both arguments to use the same backend (both Ket, both DM, or both Stab). Passing mixed backends throws `std::runtime_error`.

<details>
<summary>
  Full example: q2ns-2-fidelity-example.cc
</summary>

```cpp
#include "ns3/core-module.h"
#include "ns3/network-module.h"

#include "ns3/q2ns-analysis.h"
#include "ns3/q2ns-netcontroller.h"
#include "ns3/q2ns-qmap.h"
#include "ns3/q2ns-qnode.h"
#include "ns3/q2ns-qstate.h"
#include "ns3/q2ns-qubit.h"
#include "ns3/simulator.h"

#include <iomanip>
#include <iostream>
#include <random>
#include <vector>

using namespace ns3;
using namespace q2ns;

int main(int argc, char* argv[]) {
  RngSeedManager::SetSeed(std::random_device{}() | 1u);

  NetController net;

  // Build the ideal |+> reference
  auto ref_node = net.CreateNode();
  auto qref = ref_node->CreateQubit();
  ref_node->Apply(gates::H(), {qref});
  const auto ideal = net.GetState(qref);

  // Experiment topology: A -> [DepolarizingQMap] -> B
  auto A = net.CreateNode();
  auto B = net.CreateNode();

  auto depol = CreateObject<DepolarizingQMap>();
  auto ch = net.InstallQuantumLink(A, B);
  ch->SetAttribute("Delay", TimeValue(NanoSeconds(10)));
  ch->SetAttribute("QMap", PointerValue(depol));

  // Receive callback installed once; `received` is reset at the start of each trial.
  std::shared_ptr<QState> received;
  B->SetRecvCallback([&net, &received](std::shared_ptr<Qubit> q) {
    received = net.GetState(q);
  });

  const std::vector<double> probs = {0.0, 0.10, 0.25, 0.50, 0.75, 1.0};

  std::cout << std::fixed << std::setprecision(4);
  std::cout << "Depolarizing fidelity sweep (one trial per point)\n";
  std::cout << "  p       F\n";

  for (double p : probs) {
    received = nullptr;
    depol->SetAttribute("Probability", DoubleValue(p));

    auto q = A->CreateQubit();
    A->Apply(gates::H(), {q});

    Simulator::Schedule(NanoSeconds(1), [A, B, q]() {
      A->Send(q, B->GetId());
    });

    // Advance the simulation by 1 ms -- well past the 10 ns channel delay.
    Simulator::Stop(Simulator::Now() + MilliSeconds(1));
    Simulator::Run();

    const double f = received ? q2ns::analysis::Fidelity(*ideal, *received) : -1.0;
    std::cout << "  " << p << "   " << f << "\n";
  }

  Simulator::Destroy();
  return 0;
}
```

</details>

## 4. Building and Running

From the ns-3 root:

```bash
# Configure (first time only)
./ns3 configure --enable-examples --enable-tests

# Build the Q2NS module and all examples
./ns3 build q2ns
```

Run the examples from this tutorial:

```bash
# Section 1: basis measurements
./ns3 run q2ns-2-basis-measurement-example

# Section 2: QMap noise models -- try each mode
./ns3 run "q2ns-2-qmap-example --mode=loss+depol"
./ns3 run "q2ns-2-qmap-example --mode=lambda"
./ns3 run "q2ns-2-qmap-example --mode=conditional"
./ns3 run "q2ns-2-qmap-example --mode=randomunitary"

# Section 3: fidelity sweep (one trial per probability point)
./ns3 run q2ns-2-fidelity-example
```

## Related Publications

[[1]](https://ieeexplore.ieee.org/document/11322738) <em>Quantum Internet Architecture: Unlocking Quantum-Native Routing via Quantum Addressing</em> (invited paper). Marcello Caleffi and Angela Sara Cacciapuoti -- in IEEE Transactions on Communications, vol. 74, pp. 3577–3599, 2026.

[[2]](https://arxiv.org/abs/2603.02857) <em>An Extensible Quantum Network Simulator Built on ns-3: Q2NS Design and Evaluation</em>. Adam Pearson, Francesco Mazza, Marcello Caleffi, Angela Sara Cacciapuoti -- 2026.

[[3]](https://doi.org/10.5281/zenodo.18980972) <em>Q2NS: A Modular Framework for Quantum Network Simulation in ns-3</em> (invited paper). Adam Pearson, Francesco Mazza, Marcello Caleffi, Angela Sara Cacciapuoti -- Proc. QCNC 2026.

[4] <em>Q2NS Demo: a Quantum Network Simulator based on ns-3</em>. Francesco Mazza, Adam Pearson, Marcello Caleffi, Angela Sara Cacciapuoti -- 2026.

## Acknowledgement

This work has been funded by the **European Union** under Horizon Europe ERC-CoG grant **QNattyNet**, n. 101169850. Views and opinions expressed are those of the author(s) only and do not necessarily reflect those of the European Union or the European Research Council Executive Agency. Neither the European Union nor the granting authority can be held responsible for them.
