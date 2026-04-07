# Randomness {#q2ns_randomness_doc}

Q2NS follows the standard ns-3 randomness model and standards (as of ns-3.4, [described here](https://www.nsnam.org/docs/manual/html/random-variables.html)). Simulations are deterministic by default and different outcomes are obtained by changing the global Seed and/or Run values via `ns3::RngSeedManager`.

Q2NS centralizes stream binding and backend seeding in `NetController`. In typical usage, the `NetController` schedules stream binding to happen at simulation time 0 upon creation so that it occurs when `Simulator::Run()` is called and requires no extra calls, but there are alternatives for when these are not used. This is discussed below.

This document explains when and how to use `SetSeed`, `SetRun`, and `NetController::AssignStreams()`.

---

## What each function does

### RngSeedManager::SetSeed(S)

Sets the global RNG seed. This defines the overall “randomness universe.” Use a value greater than 0 or else you will get a segfault.

### RngSeedManager::SetRun(R)

Selects a substream (independent replication) within the chosen seed. This is the recommended way in ns-3 to obtain independent trials while keeping a fixed seed. Use a value greater than 0 or else you will get a segfault.

### NetController::AssignStreams(streamBase)

Explicitly binds stream indices to Q2NS objects (channels and states) and seeds backend PRNGs using a deterministic combination of:

- Seed
- Run
- Assigned stream index
- Backend-specific salt

Calling `AssignStreams()` multiple times is safe. Backend RNGs are reseeded only if the `(Seed, Run, stream)` combination changes. Repeating the same call has no effect. Note: QState backends use process-global RNGs internally. In those cases Q2NS reseeds the backend RNG deterministically whenever `(Seed, Run, stream)` changes. The reseeding guard is implemented in QState to avoid redundant reseeding when the same configuration is reused.

This does not create additional randomness; it ensures that randomness is deterministic and reproducible.

### Backend seed derivation

Each QState backend derives its RNG seed from the ns-3 Seed, Run, and assigned stream index together with a backend-specific salt. The deterministic seed derivation and reseeding logic are implemented in the QState base class so backend implementations only need to supply a salt and reseed their internal RNG.

Current salts:

- Ket backend: `QPPK` (0x5150504B)
- Density matrix backend: `QPPD` (0x51505044)
- Stabilizer backend: `STAB` (0x53544142)

These salts are internal implementation details but guarantee that different backends do not share identical random sequences even when all other parameters are identical. Any future backend developed should have its own unique salt.

---

## Two valid usage patterns

### Pattern A: Normal ns-3 simulation (recommended)

You call `Simulator::Run()` (or `NetController::Run()`).

Steps:

1. Call `SetSeed` and `SetRun` once and early, e.g. at the very beginning of `main()`.
2. Build your topology and schedule events.
3. Call `Simulator::Run()`.

Example:

```cpp
ns3::RngSeedManager::SetSeed(seed);  // > 0
ns3::RngSeedManager::SetRun(run);    // > 0

q2ns::NetController nc;
// build topology

ns3::Simulator::Run();
ns3::Simulator::Destroy();
```

You do not need to call `AssignStreams()` explicitly. Q2NS automatically binds streams at time 0 when the simulator starts.

Change `Run` to obtain independent replications. Change `Seed` to select a different overall randomness universe--in most cases this is not necessary, but it's good to explicitly set a seed so that another person should be able to exactly reproduce your results on a different machine (up to floating point and other largely negligible differences between machines, compilers, etc.).

---

### Pattern B: Immediate mode (no Simulator::Run())

You do not call `Simulator::Run()`, but you perform operations that involve randomness (e.g., measurement, noise, random channel effects).

Steps:

1. Call `SetSeed` and `SetRun` once and early, e.g. at the very beginning of `main()`.
2. Call `net.AssignStreams(0)` before the first random operation (substituting `nc` with whatever your `NetController` object is called). NOTE: To properly seed the backends, it's important to set the backend with `net.SetQStateBackend(...)` _before_ assigning streams.
3. Perform quantum operations.

Example:

```cpp
ns3::RngSeedManager::SetSeed(seed);  // > 0
ns3::RngSeedManager::SetRun(run);    // > 0

q2ns::NetController net;
net.SetQStateBackend(...);

net.AssignStreams(0);  // required in immediate mode

auto node = net.CreateNode();
// probabilistic operations here
```

Without `AssignStreams()`, backend RNGs may not be seeded deterministically before the first probabilistic operation, and results may differ between program runs.

---

## Ordering rules

- Always call `SetSeed` and `SetRun` before:
  - building topology that constructs RNG objects,
  - any probabilistic operation,
  - calling `Simulator::Run()`.

- In immediate mode (no `Run()`), call `AssignStreams()` before any operation that might use randomness.

- If you generate random quantum states before `Simulator::Run()`, either:
  - call `AssignStreams()` first, or
  - generate them after the simulator starts.

- `AssignStreams()` may be called multiple times safely before randomness occurs. If `(Seed, Run, streamBase)` changes, backend RNGs will be reseeded deterministically.

---

## Using your own ns-3 random variables

If you create your own `UniformRandomVariable` or other `RandomVariableStream` objects:

- Either let ns-3 assign streams implicitly (deterministic but sensitive to object creation order), or
- Assign streams explicitly in a reserved range, for example:

```cpp
rv->SetStream(10000 + run);
```

Avoid reusing stream indices used by Q2NS.

---

## Independent replications

To perform statistically independent replications:

- Fix `Seed`.
- Sweep `Run`.

For example:

```cpp
for (uint32_t i = 0; i < trials; ++i) {
  ns3::RngSeedManager::SetRun(baseRun + i);
  // build and run simulation
}
```

If using immediate mode, rebuild the controller per replication and call `AssignStreams()` again after setting the desired Seed and Run.

---

## Common mistakes

- Setting Seed or Run after randomness has already occurred.
- Forgetting to call `AssignStreams()` in immediate-mode examples.
- Assuming that changing `Run` mid-simulation reseeds everything retroactively.
- Using stream index 0 for custom RNGs without considering stream allocation conflicts.
- Using Seed or Run equal to 0.

---

## Quick decision guide

- If you call `Simulator::Run()`:
  Set Seed and Run early. Do not call `AssignStreams()` unless you know that this offers you something specific and important.

- If you do not call `Simulator::Run()` but perform random operations:
  Set Seed and Run early, then call `net.AssignStreams(0)` before the first random operation.

This is sufficient for reproducible and controlled randomness in Q2NS.

## Implementing randomness in a new `QState` backend

If you add a new `QState` backend that uses randomness (measurement sampling, noise, etc.), you should implement deterministic seeding via `AssignStreams()`.

Q2NS provides helper utilities in the `QState` base class so backend implementations only need to:

- choose a unique backend salt, and
- reseed their RNG from a provided 64-bit seed.

### 1) Choose a unique salt

Each backend should use a unique 64-bit salt so different backends do not accidentally generate identical random sequences when `Seed`, `Run`, and `stream` match.

The Q2NS convention is to use a readable ASCII tag packed into hex when possible (example below uses `"MYBA"`):

```cpp
constexpr uint64_t MYBACKEND_SALT = 0x4D594241ULL; // "MYBA"
```

The salt does not need to be secret. It just needs to be unique.

### 2) Implement `AssignStreams()`

Override `AssignStreams()` and delegate the boring stuff to the `QState` helper:

```cpp
int64_t QStateMyBackend::AssignStreams(int64_t stream) {
  constexpr uint64_t MYBACKEND_SALT = 0x4D594241ULL; // "MYBA"

  return QState::AssignStreamsGlobal<MYBACKEND_SALT>(
      stream,
      [&](uint64_t seed64) {
        // Reseed your backend RNG here using seed64. See the existing backends for examples, but ultimately this will depend on the specific libraries being used.
        MyBackendRng::Seed(seed64);
      });
}
```

`QState::AssignStreamsGlobal<SALT>()` handles:

- reading ns-3 `Seed` and `Run`
- deriving a deterministic 64-bit seed from `(Seed, Run, stream, SALT)`
- skipping reseeding if `(Seed, Run, stream)` did not change (for a given backend salt)
- returning `1` to indicate one stream index was consumed

### 3) Use your backend RNG normally

After seeding, use your RNG however your backend expects (uniform draws, sampling measurement outcomes, etc.):

```cpp
double u = MyBackendRng::Uniform01(); // example
```

## Related Publications

[[1]](https://ieeexplore.ieee.org/document/11322738) <em>Quantum Internet Architecture: Unlocking Quantum-Native Routing via Quantum Addressing</em> (invited paper). Marcello Caleffi and Angela Sara Cacciapuoti -- in IEEE Transactions on Communications, vol. 74, pp. 3577–3599, 2026.

[[2]](https://doi.org/10.1016/j.comnet.2026.112292) <em>An Extensible Quantum Network Simulator Built on ns-3: Q2NS Design and Evaluation</em>. Adam Pearson, Francesco Mazza, Marcello Caleffi, Angela Sara Cacciapuoti -- Computer Networks (Elsevier) 2026.

[[3]](https://doi.org/10.5281/zenodo.18980972) <em>Q2NS: A Modular Framework for Quantum Network Simulation in ns-3</em> (invited paper). Adam Pearson, Francesco Mazza, Marcello Caleffi, Angela Sara Cacciapuoti -- Proc. QCNC 2026.

[[4]](https://doi.org/10.48550/arXiv.2604.02112) <em>Q2NS Demo: a Quantum Network Simulator based on ns-3</em>. Francesco Mazza, Adam Pearson, Marcello Caleffi, Angela Sara Cacciapuoti -- 2026.

## Acknowledgement

This work has been funded by the **European Union** under Horizon Europe ERC-CoG grant **QNattyNet**, n. 101169850. Views and opinions expressed are those of the author(s) only and do not necessarily reflect those of the European Union or the European Research Council Executive Agency. Neither the European Union nor the granting authority can be held responsible for them.
