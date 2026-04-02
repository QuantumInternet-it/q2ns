# Q2NS Tutorial 1: Running Your First Quantum Simulations {#q2ns_tutorial_first_simulation}

This tutorial walks through writing simulations using Q2NS. We focus on
practical usage rather than deep theory. By the end you will know how to:

- Create quantum nodes and links
- Create and manipulate qubits
- Create and distribute entanglement
- Add noise models on channels
- Drive a simulation with the ns-3 event scheduler

If you want to watch simulations run before writing code, see [Tutorial 0](tutorial-00.md) (the Q2NSViz visualizer).

## 1. Mental Model of Q2NS

Q2NS extends ns-3 with quantum networking primitives.

The key objects you interact with are:

| Object          | Purpose                                         |
| --------------- | ----------------------------------------------- |
| `NetController` | Global orchestrator for the quantum network     |
| `QNode`         | A node capable of storing and processing qubits |
| `Qubit`         | A logical quantum bit                           |
| `QState`        | The underlying quantum state backend            |

The `NetController` creates nodes and links, while `QNode` objects perform quantum operations locally and send/receive qubits to other `QNode` objects.

A simulation typically looks like this:

1. Configure the simulation with `NetController` and classical primitives
2. Schedule events with `Simulator::Schedule(...)`
3. Execute the timeline with `Simulator::Run()`

## 2. Running the Simulation

**Q2NS is a discrete event simulator!** Simulation events are scheduled in a queue and executed sequentially.
All Q2NS examples ultimately run inside the normal ns-3 scheduler. Most code you write will involve scheduling events with:

```cpp
Simulator::Schedule(...);
```

And, in the end, we run the simulation with:

```cpp
Simulator::Stop(Seconds(10)); // Good practice: add a hard cutoff for internal simulation time
Simulator::Run();
Simulator::Destroy();
```

During `Run()`, all of these can happen together:

- quantum operations
- qubit arrivals on channels
- classical packets
- timers and callbacks
- protocol logic scheduled for later times

This is important: most interesting protocols are not a straight-line program. You usually create nodes and links first, then schedule actions, then let ns-3 execute the timeline. Some of these actions may schedule other actions themselves. All time in ns-3 is simulation time, not wall-clock time. A simulation that runs for 10 simulated seconds may execute much faster (or slower) in real time depending on the complexity of the events being simulated.

Scheduling events using `Simulator::Schedule` requires specifying the time and a callable. The `[A, q]()` syntax is a C++ lambda -- a function defined inline that captures local variables so they remain accessible when the event fires later:

```cpp
Simulator::Schedule(MicroSeconds(1), [A, q]() {
  A->Apply(gates::H(), {q});       // runs at t = 1 us
});

Simulator::Schedule(MicroSeconds(2), [A, B, q]() {
  A->Send(q, B->GetId());          // runs at t = 2 us
});
```

## 3. Your First Quantum Simulation

We begin with the simplest possible program: a single node manipulating and measuring a qubit.

Here we:

1.  Create a network controller and set the backend we wish to use (the default options are Ket, DM, or Stab)
2.  Create a node
3.  Create a qubit at that node
4.  Apply a Hadamard gate, putting the qubit in an equal superposition
5.  Measure the qubit

The first step is to include the needed headers and namespaces:

```cpp
#include "ns3/core-module.h"
#include "ns3/q2ns-netcontroller.h"
#include "ns3/q2ns-qnode.h"
#include "ns3/q2ns-qgate.h"

using namespace ns3;
using namespace q2ns;
```

The randomness of quantum measurements is governed by two knobs, more details are given in [randomness.md](../design-notes/randomness.md).

```cpp
  ns3::RngSeedManager::SetSeed(45);
  ns3::RngSeedManager::SetRun(15);
```

The `NetController` is the orchestrator of the simulation. The backend must be configured before creating qubits:

```cpp
  NetController net;
  net.SetQStateBackend(QStateBackend::Ket);
  auto node = net.CreateNode();
```

**Q2NS supports three quantum state representations**, each with different trade-offs:

| Backend | Represents             | Best for                             | Limitations                                                             |
| ------- | ---------------------- | ------------------------------------ | ----------------------------------------------------------------------- |
| `Ket`   | Pure-state statevector | Exact simulation, small qubit counts | Exponential memory in qubit count; pure states only                     |
| `DM`    | Density matrix (CPTP)  | Mixed states, open systems, noise    | Exponential memory in qubit count                                       |
| `Stab`  | Stabilizer tableau     | Large Clifford circuits, scalable    | Clifford group only -- T gate and arbitrary unitaries are not supported |

```cpp
  net.SetQStateBackend(QStateBackend::Ket);   // Ket representation
  net.SetQStateBackend(QStateBackend::DM);    // Density matrix representation
  net.SetQStateBackend(QStateBackend::Stab);  // Stabilizer representation
```

A `QState` handle can be obtained from the `NetController` at any time. Note that `GetStateId()` is a registry-internal identifier assigned to the _state object_, not the qubit itself -- a state may represent multiple entangled qubits jointly:

```cpp
  auto state = net.GetState(q);
  std::cout << "Qubit state ID: "
            << state->GetStateId();   // Registry-assigned state ID
  std::cout << " and state: "
            << state;                 // Prints backend-dependent representation
```

Gates and measurements are applied through the node. Since the simulation is event-driven they are wrapped in lambdas and scheduled at specific simulation times. `Apply` takes a gate and a list of target qubits; `Measure` collapses the qubit and returns the classical outcome (0 or 1):

```cpp
  Simulator::Schedule(MicroSeconds(10), [node, q]() {
    node->Apply(gates::H(), {q});     // put qubit in |+> = (|0>+|1>)/sqrt2
  });

  Simulator::Schedule(MicroSeconds(20), [node, q]() {
    int result = node->Measure(q);    // collapses to 0 or 1 with equal probability
  });
```

<details> 
<summary>
  By putting all together, we have the final simulation file
</summary>

```cpp
#include "ns3/core-module.h"
#include "ns3/q2ns-netcontroller.h"
#include "ns3/q2ns-qnode.h"
#include "ns3/q2ns-qgate.h"
#include "ns3/q2ns-qstate.h"

using namespace ns3;
using namespace q2ns;

int main() {

  ns3::RngSeedManager::SetSeed(45);
  ns3::RngSeedManager::SetRun(15);

  NetController net;
  net.SetQStateBackend(QStateBackend::Ket);

  auto node = net.CreateNode();
  auto q = node->CreateQubit();
  auto state = net.GetState(q);
  std::cout << "Created node with ID: " << node->GetId() << "\n";
  std::cout << "Created qubit with ID: " << state->GetStateId() << "\n";
  std::cout << " and state: " << state << "\n";

  Simulator::Schedule(MicroSeconds(10), [node, q]() {
    node->Apply(gates::H(), {q});
    std::cout << "Applied Hadamard gate to qubit\n";
    std::cout << "State after gate application: " << node->GetState(q) << "\n";
  });

  Simulator::Schedule(MicroSeconds(20), [node, q]() {
    int result = node->Measure(q);
    std::cout << "Measurement result: " << result << "\n";
  });

  Simulator::Stop(MilliSeconds(10));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}
```

</details>

## 4. Distributing Entanglement

The key resource of the Quantum Internet is **entanglement**. Here we:

1. Create two `QNode`s connected by a quantum link
2. Create a Bell pair at node A
3. Send one qubit to B over the link
4. Register a receive callback at B
5. Measure both qubits to verify the entanglement correlation

We begin by creating two nodes and connecting them with a quantum link:

```cpp
auto A = net.CreateNode();
auto B = net.CreateNode();

auto ch = net.InstallQuantumLink(A, B);
ch->SetAttribute("Delay", TimeValue(NanoSeconds(10)));
```

This creates a bidirectional channel with a 10 ns propagation delay. The delay can be configured independently for each direction with `ch->SetDelayAB(...)` and `ch->SetDelayBA(...)`.

We then create a Bell pair at node A:

```cpp
auto [qA, qB] = A->CreateBellPair();
```

This is equivalent to:

```cpp
auto qA = A->CreateQubit();
auto qB = A->CreateQubit();
A->Apply(gates::H(), {qA});
A->Apply(gates::CNOT(), {qA, qB});
```

which prepares the maximally entangled state:

$$|\Phi^+\rangle = \frac{|00\rangle + |11\rangle}{\sqrt{2}}$$

A **callback** is a function you register in advance that the simulator will call for you when a specific event occurs. Here we register a receive callback at B before calling `Send` -- this is important because qubit delivery is asynchronous: `Send` returns immediately and the qubit arrives at B only after the channel delay has elapsed. If no callback is registered, the qubit still arrives but nothing happens with it.

`SetRecvCallback` fires only when a qubit arrives; it does **not** trigger on classical packets. The `[&]` capture passes all local variables by reference into the lambda:

```cpp
B->SetRecvCallback([&](std::shared_ptr<Qubit> arrived) {
    std::cout << "Qubit received at B\n";
});

// Schedule the send; the qubit arrives at B after the channel delay (10 ns)
Simulator::Schedule(NanoSeconds(1), [&]() {
    A->Send(qB, B->GetId());
});
```

Since `Send` is non-blocking and delivery is asynchronous, measurements must be scheduled _after_ the channel delay has elapsed. Here we use a 20 ns budget (1 ns send time + 10 ns delay + margin), capturing the result in a lambda:

```cpp
Simulator::Schedule(NanoSeconds(20), [&]() {
    int mA = A->Measure(qA, Basis::Z);
    int mB = B->Measure(qB, Basis::Z);
    // mA == mB always (both 0 or both 1, with equal probability)
});
```

<details>
<summary>
  By putting all together, we have the final simulation file
</summary>

```cpp
#include "ns3/core-module.h"
#include "ns3/q2ns-netcontroller.h"
#include "ns3/q2ns-qnode.h"
#include "ns3/q2ns-qubit.h"
#include "ns3/simulator.h"

#include <iostream>

using namespace ns3;
using namespace q2ns;

int main(int, char**) {
  std::cout << "[DEMO] Bell send (A->B) starting\n";

  ns3::RngSeedManager::SetSeed(1);
  ns3::RngSeedManager::SetRun(2);

  NetController net;
  net.SetQStateBackend(QStateBackend::Stab);

  auto A = net.CreateNode();
  auto B = net.CreateNode();

  auto ch = net.InstallQuantumLink(A, B);
  ch->SetAttribute("Delay", TimeValue(NanoSeconds(10)));

  auto [qA, qBremote] = A->CreateBellPair();

  B->SetRecvCallback([&](std::shared_ptr<Qubit>) {
    std::cout << "[RECV][quantum][B]: yes\n";
  });

  // Schedule the send; the qubit arrives at B after the channel delay (10 ns)
  Simulator::Schedule(NanoSeconds(1), [&]() {
    bool ok = A->Send(qBremote, B->GetId());
    std::cout << "[SEND][quantum] A->B: "
              << (ok ? "ok" : "failed") << "\n";
  });

  // Schedule measurements after send time + channel delay (1 + 10 + margin = 20 ns)
  Simulator::Schedule(NanoSeconds(20), [&]() {
    int mA = A->Measure(qA, q2ns::Basis::Z);
    int mB = B->Measure(qBremote, q2ns::Basis::Z);
    std::cout << "[VERIFY] Z-measurements: A=" << mA
              << " B=" << mB << " (correlated expected)\n";
  });

  Simulator::Stop(MilliSeconds(1));
  Simulator::Run();

  std::cout << "[DONE] Bell send (A->B) finished\n";

  Simulator::Destroy();
  return 0;
}
```

</details>

## 5. Building and Running

From the ns-3 root:

```bash
# Configure (first time only)
./ns3 configure --enable-examples --enable-tests

# Build the Q2NS module and all examples
./ns3 build q2ns
```

Run the introductory examples to see the above concepts in action:

```bash
# Single-node gate and measurement demonstration
./ns3 run q2ns-1-basics-example

# Two-node Bell pair transmission and correlation check
./ns3 run q2ns-1-bell-send-example
```

## 6. Exercises

#### Exercise 1: X and Z gates

Create a single node, allocate a qubit, apply an X gate (bit-flip), then measure in the Z-basis. Verify the result is always 1. Then try applying a Z gate to ∣0⟩ and confirm the measurement is always 0: since Z is diagonal in the computational basis (Z∣0⟩ = ∣0⟩, Z∣1⟩ = -∣1⟩), it does not alter the Born-rule probabilities of any Z-basis measurement.

## Related Publications

[[1]](https://ieeexplore.ieee.org/document/11322738) <em>Quantum Internet Architecture: Unlocking Quantum-Native Routing via Quantum Addressing</em> (invited paper). Marcello Caleffi and Angela Sara Cacciapuoti -- in IEEE Transactions on Communications, vol. 74, pp. 3577–3599, 2026.

[[2]](https://arxiv.org/abs/2603.02857) <em>An Extensible Quantum Network Simulator Built on ns-3: Q2NS Design and Evaluation</em>. Adam Pearson, Francesco Mazza, Marcello Caleffi, Angela Sara Cacciapuoti -- 2026.

[[3]](https://doi.org/10.5281/zenodo.18980972) <em>Q2NS: A Modular Framework for Quantum Network Simulation in ns-3</em> (invited paper). Adam Pearson, Francesco Mazza, Marcello Caleffi, Angela Sara Cacciapuoti -- Proc. QCNC 2026.

[4] <em>Q2NS Demo: a Quantum Network Simulator based on ns-3</em>. Francesco Mazza, Adam Pearson, Marcello Caleffi, Angela Sara Cacciapuoti -- 2026.

## Acknowledgement

This work has been funded by the **European Union** under Horizon Europe ERC-CoG grant **QNattyNet**, n. 101169850. Views and opinions expressed are those of the author(s) only and do not necessarily reflect those of the European Union or the European Research Council Executive Agency. Neither the European Union nor the granting authority can be held responsible for them.
