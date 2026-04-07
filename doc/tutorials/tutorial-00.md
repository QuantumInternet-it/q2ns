# Q2NS Tutorial 0: The Visualizer {#q2ns_tutorial_visualizer}

This tutorial introduces the Q2NS network visualizer -- a browser-based tool that replays recorded quantum network simulations. You can use it to explore what happens in a quantum network without writing a single line of code.

**No programming required!**

By the end of this tutorial you will be able to:

- Launch the visualizer in your browser
- Load one of the provided simulation traces
- Understand what you are seeing on screen
- Know what events are captured and what they mean

---

## 1. What Is Q2NS?

Q2NS is a quantum network simulator built on **ns-3**, the de facto standard for classical network simulation.
Q2NS implements quantum networking primitives -- nodes, links, qubits, gates, and noise models -- on top of the full classical networking stack that ns-3 already provides.

A Q2NS simulation can therefore model:

- quantum operations (gates, measurements, Bell-state measurements)
- qubit transmission over quantum channels (with propagation delay and loss)
- classical TCP/UDP traffic running in parallel and integrated with quantum protocols
- realistic timing: nanosecond-resolution event scheduling

---

## 2. What Is the Q2NSViz Visualizer?

The **Q2NSViz** visualizer is a single HTML page that replays a Q2NS simulation trace in your browser. The trace is a JSON-lines file (`.json`) that was produced automatically while the simulation ran.

The visualizer shows:

- **Nodes** -- boxes representing quantum/classical network hosts
- **Channels** -- lines connecting nodes
  - solid purple lines: quantum channels
  - dashed orange lines: classical channels
- **Qubits** -- small colored circles that live inside a node or travel
  along a channel
- **Entanglement** -- a visual link between two qubits indicating they are entangled
- **Events** -- qubit creation, gate application, measurement, sending and
  receiving
- **Trace panel** -- a scrolling feed of protocol-level messages tagged with
  simulation time

You can step through the simulation event-by-event, play it at any speed, or jump directly to a timestamp.

---

## 3. Launching the Visualizer

The visualizer is a static HTML file served over a local HTTP server.
From the project root (the directory that contains `tools/`):

```bash
./tools/q2nsviz/q2nsviz-serve.sh
```

This starts a Python 3 HTTP server on **port 8000** and opens the viewer URL in your default browser:

```
http://localhost:8000/tools/q2nsviz/viewer.html
```

To use a different port, pass it as the first argument:

```bash
./tools/q2nsviz/q2nsviz-serve.sh 8080
```

Press **Ctrl-C** in the terminal to stop the server when you are done.

> **Requirement:** Python 3 must be installed and available as `python3`.

---

## 4. Loading a Trace File

Once the viewer is open in your browser, click **"Choose local..."** in the top toolbar. Navigate to:

```
examples/example_traces/
```

Four pre-recorded traces are provided:

| File                                             | Protocol                                           |
| ------------------------------------------------ | -------------------------------------------------- |
| `q2nsviz-teleportation-example.json`             | Quantum teleportation (Alice -> Bob)               |
| `q2nsviz-entanglement-distribution-example.json` | Bell pair distribution across 3 nodes              |
| `q2nsviz-ghz-distribution-example.json`          | GHZ state distribution across 4 nodes              |
| `q2nsviz-graphstate-gen-example.json`            | Distributed graph state (Orchestrator + 3 Clients) |

Select any one of them and the simulation will load instantly.

---

## 5. Walkthrough: Quantum Teleportation

Load `q2nsviz-teleportation-example.json`.

You will see **two nodes**: Alice (left) and Bob (right), connected by one quantum channel (purple) and one classical channel (orange dashes).

**What happens step by step:**

1. **Alice creates a Bell pair** -- two qubits (`alice_epr`, `bob_epr`)
   appear inside Alice's box, linked by an entanglement link.
2. **Alice sends `bob_epr` to Bob** -- the qubit animates along the
   quantum channel.
3. **`bob_epr` arrives at Bob** -- the qubit settles in Bob's box. Alice
   and Bob now each hold one half of the Bell pair.
4. **Alice prepares the state to teleport** -- a new qubit (`psi`) appears
   at Alice.
5. **Alice performs a Bell-State Measurement (BSM)** -- `psi` and
   `alice_epr` are measured jointly. The trace panel shows the two
   classical bits that result (`m1`, `m2`).
6. **Alice sends the correction bits to Bob** -- a classical packet
   animates along the dashed orange channel.
7. **Bob applies corrections** -- based on `m1` and `m2`, Bob applies
   Pauli X and/or Z gates to `bob_epr`. The qubit color changes.
8. **Bob measures in the X-basis** -- the final measurement result
   confirms teleportation succeeded.

---

## 6. Walkthrough: Entanglement Distribution

Load `q2nsviz-entanglement-distribution-example.json`.

You will see **three nodes** (Node1, Node2, Node3) connected in a triangle, with both quantum and classical links on every edge.

The simulation distributes three independent Bell pairs so that every pair of nodes ends up sharing one (_all-to-all_ communication is enabled):

- **Node1 <-> Node2** share a Bell pair
- **Node1 <-> Node3** share a Bell pair
- **Node2 <-> Node3** share a Bell pair

Watch how each Bell pair is created at the source node, one qubit is kept locally, and the other is sent along the quantum channel. As each qubit arrives, the receiving node sends a classical **ACK** back to the sender -- animated along the dashed orange channel -- confirming delivery. The entanglement arcs appear as each pair is confirmed.

---

## 7. Walkthrough: GHZ State Distribution

Load `q2nsviz-ghz-distribution-example.json`.

Four nodes are arranged in a **star** topology: Node1 is the central node connected to Node2, Node3, and Node4 by both quantum and classical links. Node1 creates a **GHZ state**:

$$|GHZ_4\rangle = \frac{|0000\rangle + |1111\rangle}{\sqrt{2}}$$

which is a genuinely multipartite entangled state.

**Steps:**

1. Node1 creates qubits `q0`--`q3` (all blue).
2. Node1 applies a **Hadamard gate** to `q0` (color changes to purple, indicating superposition).
3. Node1 applies **CNOT gates** between `q0` and each of `q1`, `q2`, `q3` -- entanglement arcs connect all four qubits.
4. Node1 sends `q1` to Node2, `q2` to Node3, and `q3` to Node4.
5. Each remote node receives its qubit and sends a classical **ACK** back to Node1 -- visible as packets animated along the classical channels.
6. Node1 collects all three ACKs. Once confirmed, the entanglement links span across the network -- nodes that have never interacted directly are now all part of the same multi-qubit entangled state.

---

## 8. Walkthrough: Distributed Graph State

Load `q2nsviz-graphstate-gen-example.json`.

This is the most complex trace. One **Orchestrator** node coordinates three **Client** nodes.

A **graph state** is a multipartite entangled state which can be described by a graph:

$$G=(V,E)$$

where the nodes of the graph correspond to qubits, and edges correspond to **controlled-Z (CZ)** entangling operations.

$$|G\rangle = \prod_{(i,j) \in E} CZ_{ij}\,|{+}\rangle^{\otimes|V|}$$

> Graph states are the resource states for measurement-based quantum computation and several quantum network protocols.

**Steps:**

1. The Orchestrator creates 5 qubits (`q0`--`q4`).
2. A Hadamard gate is applied to each, putting them all in superposition.
3. CZ gates are applied between adjacent pairs, creating a **linear cluster state**.
4. The Orchestrator sends `q0`, `q2`, and `q4` to Client1, Client2, and Client3 respectively.
5. The Orchestrator then measures `q1` and `q3` in the X-basis -- this is the step that transfers the entanglement to the clients without the clients needing to interact with each other.
6. The Orchestrator sends the X-basis measurement outcomes over the classical channels: outcome m1 (from q1) goes to Client1 and Client2; outcome m3 (from q3) goes to Client2 and Client3. Client2 therefore receives two outcome packets -- one per measured neighbor. Each client uses the received outcome to determine whether a Pauli byproduct correction is needed to finalize its share of the distributed graph state.

After the protocol completes, the three clients collectively hold a distributed graph state, ready to be used in higher-level applications.

---

## 9. Visual Reference: Event Types

Every event in the JSON trace corresponds to something you can see in the visualizer:

| Event type           | What you see                                                                 |
| -------------------- | ---------------------------------------------------------------------------- |
| `createNode`         | A new node box appears                                                       |
| `createChannel`      | A line connecting two nodes (purple = quantum, dashed orange = classical)    |
| `createBit`          | A colored circle appears inside a node                                       |
| `setBitColor`        | A qubit changes color (reflects a gate or state change)                      |
| `entangle`           | A link connects two or more qubits                                           |
| `sendBit`            | A qubit circle animates from one node to another                             |
| `sendPacket`         | A classical packet label animates along the classical channel                |
| `measure`            | A qubit collapses (circle shrinks/fades) and the result appears in the trace |
| `graphMeasure`       | Like `measure` but highlights the graph neighbors affected                   |
| `removeBit`          | A qubit disappears (consumed by measurement)                                 |
| `traceText` (global) | A message appears in the persistent control panel                            |
| `traceText` (node)   | A message appears briefly next to the node                                   |

---

## 10. Producing Your Own Traces

Once you start writing your own Q2NS simulations (see [Tutorial 1]((tutorial-01.md))), you can emit a trace by including the `q2nsviz-trace.h` header and calling the provided tracing functions:

```cpp
#include "ns3/q2nsviz-trace-writer.h"
#include "ns3/q2nsviz-trace.h"

// Open the trace file
TraceWriter::Instance().Open("my-trace.json");

// Trace events
TraceCreateNode("Alice", 25, 50);
TraceCreateNode("Bob",   75, 50);
TraceCreateChannel("Alice", "Bob", "quantum");
TraceCreateBit("Alice", "q0", "quantum", "#88CCEE");
TraceEntangle({"q0", "q1"});
TraceSendBit(sendTime, recvTime, "q1", "Alice", "Bob", "quantum");
TraceMeasure("q0", "Z");

// Close
TraceWriter::Instance().Close();
```

Then load the resulting JSON file in the viewer as described in Section 5.

## Related Publications

[[1]](https://ieeexplore.ieee.org/document/11322738) <em>Quantum Internet Architecture: Unlocking Quantum-Native Routing via Quantum Addressing</em> (invited paper). Marcello Caleffi and Angela Sara Cacciapuoti -- in IEEE Transactions on Communications, vol. 74, pp. 3577–3599, 2026.

[[2]](https://doi.org/10.1016/j.comnet.2026.112292) Adam Pearson, Francesco Mazza, Marcello Caleffi, Angela Sara Cacciapuoti, _"An Extensible Quantum Network Simulator Built on ns-3: Q2NS Design and Evaluation"_, Computer Networks (Elsevier), 2026.

[[3]](https://doi.org/10.5281/zenodo.18980972) <em>Q2NS: A Modular Framework for Quantum Network Simulation in ns-3</em> (invited paper). Adam Pearson, Francesco Mazza, Marcello Caleffi, Angela Sara Cacciapuoti -- Proc. QCNC 2026.

[[4]](https://doi.org/10.48550/arXiv.2604.02112) Francesco Mazza, Adam Pearson, Marcello Caleffi, Angela Sara Cacciapuoti, _"Q2NS Demo: a Quantum Network Simulator based on ns-3"_, 2026.

## Acknowledgement

This work has been funded by the **European Union** under Horizon Europe ERC-CoG grant **QNattyNet**, n. 101169850. Views and opinions expressed are those of the author(s) only and do not necessarily reflect those of the European Union or the European Research Council Executive Agency. Neither the European Union nor the granting authority can be held responsible for them.
