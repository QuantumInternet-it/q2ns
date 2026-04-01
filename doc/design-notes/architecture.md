# Architecture {#q2ns_architecture_doc}

This document describes the main structure of Q2NS and how its components fit together. It is intended as a conceptual guide covering the main points, not as a full API reference. For the detailed interface of each class, see the Doxygen documentation.

Q2NS is built around a modular architecture with a deliberate separation between network control and in-network operations. This reflects one of the central design principles described in the Q2NS paper: a quantum network simulator should separate orchestration logic from node-local and link-local execution while keeping the modules composable and replaceable.

## Architectural overview

At a high level, Q2NS revolves around three central ideas:

- **Network Control and Configuration**: The `NetController` builds and configures the network. Internally, this owns the `QStateRegistry`, which is the shared source of truth for backend states, qubit membership, and qubit locations.

- **Network Objects**: `QNode`s and `QChannel`s are the main network entities. They can process, transmit, and receive quantum information. `QNodes` can also engage with the classical networking stack since they derive from `ns3::Node`. Internally, `QNode`s delegate local quantum processing to a `QProcessor` and quantum networking to a `QNetworker`. `QChannel`s can be configured with various `QMaps` to simulate noise, loss, and more. They are connected to `QNode`s with `QNetDevices`, but the current API never requires users to create or interact with these for convenience.

- **Quantum Objects**: `Qubit`s are the main handles for quantum information that can be operated on and transmitted, connected internally to `QState`s through the `QStateRegistry`. `QState` is an interface with three different implementations or "backends": statevector (`q2ns::QStateBackend::Ket`), density matrix (`q2ns::QStateBackend::DM`), and stabilizer (`q2ns::QStateBackend::Stab`). Generally the user does not interact with `QState`s directly, unless they want to analyze various state characteristics with the `q2ns::analysis` methods (e.g., `q2ns::analysis::Fidelity(...)`, `q2ns::analysis::Purity(...)`, etc.).


## Main components

### NetController

`NetController` is the top-level orchestration object. Creating this object is where all Q2NS simulations start.

Its responsibilities are:

- owning the shared `QStateRegistry`
- creating `QNode` objects
- installing `QChannel` and `QNetDevice` links between nodes or in simple topologies such as chains and all-to-all networks
- assigning deterministic RNG streams to Q2NS-owned random sources
- assigning the default quantum-state backend for newly created states

Users typically use these methods:

- `SetQStateBackend()` / `GetQStateBackend()`
- `CreateNode()` / `GetNode()`
- `InstallQuantumLink()` / `InstallQuantumChain()` / `InstallQuantumAllToAll()`
- `GetChannel()`
- `GetState()`

### QStateRegistry

`QStateRegistry` is the central bookkeeping layer of Q2NS.

It is the shared source of truth for:

- registered backend states
- qubit membership within each state
- qubit ids and qubit handles
- current qubit location

This centralization is an important aspect of quantum networking. Qubits move between nodes and channels, and backend states may be split or merged after measurement and multi-qubit gates. By managing that bookkeeping in one place, Q2NS avoids scattering state-tracking logic across multiple components.

### QNode

`QNode` is the main user-facing per-node API. It is created with `NetController::CreateNode()`.

Its responsibilities are:

- creating qubits
- applying gates to and measuring qubits (internally with a `QProcessor`)
- sending and receiving qubits (internally with a `QNetworker`)
- sending and receiving classical packets

Users typically use these methods:

- `CreateQubit()`
- `CreateBellPair()`
- `Apply()`
- `Measure()`
- `MeasureBell()`
- `Send()`
- `SetRecvCallback()`

### QProcessor

`QProcessor` is an internal helper object owned by `QNode` that acts as the local quantum execution engine of a node.

Its responsibilities are:

- applying gates
- performing measurements
- creating Bell pairs
- coordinating state splitting and merging through `QStateRegistry`

### QNetworker

`QNetworker` is another internal helper owned by `QNode` that acts as the node-local networking front end for qubit transmission.

Its responsibilities are:

- maintaining node-local quantum interfaces (`QNetDevice`s)
- maintaining a minimal host-route table from destination node id to outgoing interface
- sending a local qubit toward another node
- receiving a qubit from a channel
- invoking the `QMapInstance` that was determined by the `QChannel`'s `QMap`
- invoking the application-level receive callback after destination-side processing is complete


### QNetDevice

`QNetDevice` is a thin bridge between `QNetworker` and `QChannel`.

It is intentionally minimal, but exists in parallel with the classical ns-3 `NetDevice`. It forwards outgoing qubits to the attached channel and forwards arriving qubits upward to the bound `QNetworker`.

This keeps the send and receive path compatible with the usual ns-3 approach of nodes, devices, and channels, while considering that quantum devices are not full classical packet devices that engage with ports and IP addresses.

### QChannel

`QChannel` models a duplex quantum channel between two endpoints.

Users typically configure a `QChannel` using `SetAttribute()` (part of the ns-3 attribute system), specifically for:
- `"Delay"` (symmetric) / `"DelayAB"` / `"DelayBA"`
- `"Jitter"` (symmetric) / `"JitterAB"` / `"JitterBA"`
- `"QMap"` (symmetric) / `"QMapAB"` / `"QMapBA"`

On each transmission, `QChannel`:

1. determines the transmission direction
2. selects the direction-specific delay, jitter, and `QMap`
3. samples random jitter uniformly from $[-\text{jitter}, \text{jitter}]$
4. samples a per-transmission `QMapInstance`
5. schedules delivery to the opposite endpoint

A notable, but subtle design choice is that the sampled `QMapInstance` is not applied inside the channel immediately. Instead, it is carried with the transmitted qubit and executed later at the receiver after the qubit has become local to the destination node in order to use that node's quantum processing capabilities. This occurs before any receive callback or other possible actions and therefore accurately represents the cumulative effect of the channel transmission.

### QMap and QMapInstance

`QMap` models transmission-induced effects and produces a random `QMapInstance` based on this model for each transmission.

Examples include:

- loss
- dephasing
- depolarization
- random gates
- random single-qubit unitaries
- user-defined transformations via lambdas
- user-defined transformations via composition of maps

A `QMap` does not directly mutate a qubit when a channel is configured. Instead, the `QChannel` samples a `QMapInstance` for each transmission. This callable is run at the receiving node after the qubit has been adopted locally.

### Qubit

`Qubit` is a lightweight handle object. It does not own the underlying quantum state or anything other than several forms of identification:

- a stable, unique qubit id (based on creation order of qubits in the network)
- the current backend state id (which state the qubit is connected to)
- the current index within that state
- an optional human readable label

Actual state resolution and location tracking happen through `QStateRegistry`. Most user-facing operations (gates, measurements, transmission, state inspection, etc.) are intentionally provided on `QNode`, not on `Qubit` itself.

### QState Interface and Implementations

`QState` is the backend-agnostic quantum-state plug-in interface.

Concrete implementations, "backends", currently include:

- `QStateKet` (referrable through the enum value `QStateBackend::Ket`)
- `QStateDM` (referrable through the enum value `QStateBackend::DM`)
- `QStateStab` (referrable through the enum value `QStateBackend::Stab`)

These provide different tradeoffs:

- `QStateKet` stores a pure state ket and supports general unitary evolution
- `QStateDM` stores a density matrix and supports mixed-state evolution
- `QStateStab` stores a stabilizer state and supports efficient Clifford-state simulation

From the user perspective, the backend can be easily set using `NetController::SetQStateBackend()` without needing to change any other part of their code, other than verifying that it is simulatable by the given backend (notably that non-Clifford gates are used with `QStateStab`, which will throw an error).

### Analysis

`q2ns::analysis` is a namespace containing a number of quantum state analysis functions.

Examples include:

- fidelity
- purity
- Von Neumann entropy
- trace distance

## Typical workflow

A typical simulation proceeds as follows:

1. create a `NetController`.
2. choose a default backend if needed.
3. create `QNode` objects through the controller.
4. install quantum links between nodes.
5. create qubits or Bell pairs through `QNode`.
6. schedule local operations and sends.
7. run the ns-3 simulation.
8. measure results and analyze.

## Send and receive path

The send path is:

1. user calls `QNode::Send()`
2. `QNode` delegates to `QNetworker::Send()`
3. `QNetworker` selects an outgoing `QNetDevice`
4. `QNetDevice` forwards to `QChannel::SendFrom()`
5. `QChannel` samples delay, jitter, and a `QMapInstance`
6. the delivery event is scheduled in ns-3

The receive path is:

1. the scheduled event reaches the destination `QNetDevice`
2. `QNetDevice` forwards upward to `QNetworker::ReceiveFromDevice()`
3. the destination node adopts the qubit locally
4. the sampled `QMapInstance` is applied
5. if the qubit is not lost (from the `QMapInstance`), the registered receive callback is invoked

## Measurement and state splitting

The measurement path is:

1. user calls `QNode::Measure()`
2. `QNode` delegates to `QProcessor::Measure()`
3. `QProcessor` retrieves the qubit's location and `QState` from the `QStateRegistry` and verifies if the qubit is local to this node before proceeding
4. `QProcessor` calls `QState::Measure()`, receiving a measured 1-qubit state, a survivor state for the remaining qubits, and the classical outcome bit
5. `QProcessor` then rebinds qubit handles through `QStateRegistry` with the two new `QState`s and returns the classical outcome bit

Notably, every measurement results in a new single qubit state. If the qubit was originally in a multi-qubit state, this will be split and the measurement path will effectively produce one more `QState` object in the `QStateRegistry` than when it started. This helps keep the size of any one state small, thereby reducing the runtime and memory complexity.

## Gate application and state merging

The gate application path is:

1. user calls `QNode::Apply()`
2. `QNode` delegates to `QProcessor::Apply()`
3. `QProcessor` retrieves the qubit's location from the `QStateRegistry` and verifies if the qubit is local to this node before proceeding
4. `QProcessor` calls `QStateRegistry::MergeStates()` to produce a single merged (tensor product) `QState` for all qubits involved in this gate application
5. `QProcessor` retrieves the qubits' `QState` from the `QStateRegistry`
6. `Qprocessor` calls `QState::Apply()` and returns true

Notably, every multi-qubit gate results in a single, merged state. If the qubits were not already connected to a single `QState` object, they will be in the end--even if they are not necessarily entangled. This means multi-qubit gate application can increase the runtime and memory complexity.