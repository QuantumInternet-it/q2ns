# Q2NS Tutorial 4: Multipartite Entanglement and Simulation Experiments {#q2ns_tutorial_multipartite}

In the previous tutorials we learned how to:

- create and manipulate qubits
- send qubits through quantum channels
- model noise
- combine quantum and classical communication

In this tutorial we build a small simulation experiment around multipartite entanglement.

So far we have mainly worked with Bell pairs, which entangle two qubits. Here we move to a larger entangled state shared across several nodes. This is an important idea in quantum networking, especially for distributed protocols, entanglement overlays, and larger networked quantum systems.

We will build the tutorial in three steps:

1. create and distribute a 1D cluster state in a single run
2. add command-line arguments and wall-clock timing
3. run multiple trials and collect statistics

By the end you will know how to:

- create a multipartite entangled state in Q2NS
- distribute it across a network topology
- structure a simple simulation experiment
- compare backend runtime across repeated trials

## 1. Creating a 1D Cluster State

We begin with a single complete example.

The network topology will be a 5-node star: one central node connected to four remote nodes by quantum links. The center will create all five qubits locally, prepare a 1D cluster state, send one qubit to each remote node, and then all nodes will measure their qubits.

A 1D cluster state on $n$ qubits is prepared as

$$
|C_n\rangle = (CZ_{0,1}\, CZ_{1,2} \cdots CZ_{n-2,n-1})\, |+\rangle^{\otimes n}
$$

with

$$
|+\rangle = \frac{|0\rangle + |1\rangle}{\sqrt{2}}.
$$

So even though the network topology is a star, the entangled state we create has a chain structure.

We start with the includes:

```cpp
#include "ns3/core-module.h"
#include "ns3/simulator.h"

#include "ns3/q2ns-netcontroller.h"
#include "ns3/q2ns-qnode.h"
#include "ns3/q2ns-qubit.h"

#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>

using namespace ns3;
using namespace q2ns;
```

Next we create a network controller, choose a backend, and create a vector of five nodes:

```cpp
NetController net;
net.SetQStateBackend(QStateBackend::Stab);

std::vector<Ptr<QNode>> nodes;
nodes.reserve(5);

for (uint32_t i = 0; i < 5; ++i) {
  nodes.push_back(net.CreateNode());
}

Ptr<QNode> center = nodes[0];
```

Now we install the quantum links. This gives a star topology centered on `nodes[0]`:

```cpp
for (uint32_t i = 1; i < 5; ++i) {
  auto ch = net.InstallQuantumLink(center, nodes[i]);
  ch->SetAttribute("Delay", TimeValue(NanoSeconds(10)));
}
```

Each remote node will simply measure any qubit it receives:

```cpp
for (uint32_t i = 1; i < 5; ++i) {
  Ptr<QNode> node = nodes[i];
  node->SetRecvCallback([node](std::shared_ptr<Qubit> q) {
    node->Measure(q);
  });
}
```

Now we schedule the protocol itself.

At 1 ns, the center:

1. creates five qubits
2. applies a Hadamard to each qubit
3. applies CZ gates in a chain to create the 1D cluster state
4. sends one qubit to each remote node
5. schedules its own local measurement for the remaining qubit

```cpp
Simulator::Schedule(NanoSeconds(1), [center, &nodes]() {
  std::vector<std::shared_ptr<Qubit>> qs;
  qs.reserve(5);

  for (uint32_t i = 0; i < 5; ++i) {
    qs.push_back(center->CreateQubit());
  }

  for (uint32_t i = 0; i < 5; ++i) {
    center->Apply(gates::H(), {qs[i]});
  }

  for (uint32_t i = 0; i + 1 < 5; ++i) {
    center->Apply(gates::CZ(), {qs[i], qs[i + 1]});
  }

  for (uint32_t i = 1; i < 5; ++i) {
    center->Send(qs[i], nodes[i]->GetId());
  }

  Simulator::Schedule(NanoSeconds(19), [centerQubit = qs[0], center]() {
    center->Measure(centerQubit, Basis::Z);
  });
});
```

The center measures at 20 ns total simulation time: the outer event runs at 1 ns, and the nested event is scheduled 19 ns later. This is safely after the sends have occurred.

Finally, we run the simulation:

```cpp
Simulator::Stop(MicroSeconds(10));
Simulator::Run();
Simulator::Destroy();
```

<details>
<summary>Full example: single trial, fixed 5-node cluster-state distribution</summary>

```cpp
#include "ns3/core-module.h"
#include "ns3/simulator.h"

#include "ns3/q2ns-netcontroller.h"
#include "ns3/q2ns-qnode.h"
#include "ns3/q2ns-qubit.h"

#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>

using namespace ns3;
using namespace q2ns;

int main(int argc, char* argv[]) {
  RngSeedManager::SetSeed(1);
  RngSeedManager::SetRun(1);

  NetController net;
  net.SetQStateBackend(QStateBackend::Stab);

  std::vector<Ptr<QNode>> nodes;
  nodes.reserve(5);

  for (uint32_t i = 0; i < 5; ++i) {
    nodes.push_back(net.CreateNode());
  }

  Ptr<QNode> center = nodes[0];

  for (uint32_t i = 1; i < 5; ++i) {
    auto ch = net.InstallQuantumLink(center, nodes[i]);
    ch->SetAttribute("Delay", TimeValue(NanoSeconds(10)));
  }

  for (uint32_t i = 1; i < 5; ++i) {
    Ptr<QNode> node = nodes[i];
    node->SetRecvCallback([node](std::shared_ptr<Qubit> q) {
      node->Measure(q);
    });
  }

  Simulator::Schedule(NanoSeconds(1), [center, &nodes]() {
    std::vector<std::shared_ptr<Qubit>> qs;
    qs.reserve(5);

    for (uint32_t i = 0; i < 5; ++i) {
      qs.push_back(center->CreateQubit());
    }

    for (uint32_t i = 0; i < 5; ++i) {
      center->Apply(gates::H(), {qs[i]});
    }

    for (uint32_t i = 0; i + 1 < 5; ++i) {
      center->Apply(gates::CZ(), {qs[i], qs[i + 1]});
    }

    for (uint32_t i = 1; i < 5; ++i) {
      const bool ok = center->Send(qs[i], nodes[i]->GetId());
      if (!ok) {
        std::cerr << "[WARN] Send to node " << i << " failed\n";
      }
    }

    Simulator::Schedule(NanoSeconds(19), [centerQubit = qs[0], center]() {
      center->Measure(centerQubit, Basis::Z);
    });
  });

  Simulator::Stop(MicroSeconds(10));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}
```

</details>

## 2. Taking Command-Line Arguments and Measuring Wall-Clock Time

Now we turn the fixed example into a configurable experiment.

We will add four command-line arguments:

- `numNodes`
- `backend`
- `seed`
- `run`

and then measure the wall-clock time of the program.

First we declare the experiment parameters and parse them using the ns-3 core module's `CommandLine` object. We can define any command-line arguments we want by calling `cmd.AddValue("command-line argument", "description", variable to store the argument in)`. Then we copy any values passed in using `cmd.Parse(argc, argv)`. NOTE: for this reason, we must declare main as `int main(int argc, char* argv[])` not `int main()`.

```cpp
uint32_t numNodes = 8;
std::string backend = "stab";
uint32_t seed = 1;
uint32_t run = 1;

CommandLine cmd;
cmd.AddValue("numNodes", "Total number of nodes (>= 2)", numNodes);
cmd.AddValue("backend", "ket | dm | stab", backend);
cmd.AddValue("seed", "ns-3 RNG seed", seed);
cmd.AddValue("run", "ns-3 RNG run number", run);
cmd.Parse(argc, argv);
```

There is no provided way to require that a user provides a given command-line argument, so it is important to set default values and perform checks for any unset values or other important conditions. The main automatic check is ensuring that the provided value can be parsed into the type of the target variable. For instance, `--run="q2ns"` would fail, output "Invalid command-line argument: --run=q2ns", and then provide the options and descriptions that you wrote. To run a check oneself, one can do something like this:

```cpp
  if (numNodes < 2) {
    NS_ABORT_MSG("--numNodes must be at least 2");
  }
```

Now that we have these values from the user, we can set the RNG values and backend. NOTE: `NetController::SetQStateBackend` has an overloaded version that takes a string from "ket", "dm", or "stab" that is particularly helpful for this situation.

```cpp
RngSeedManager::SetSeed(seed);
RngSeedManager::SetRun(run);

NetController net;
net.SetQStateBackend(backend);
```

To measure wall-clock time, we can use `std::chrono::steady_clock`:

```cpp
auto t0 = std::chrono::steady_clock::now();
```

We can record one timestamp after configuration of the network and one after running of the simulation:

```cpp
auto t1 = std::chrono::steady_clock::now();

// ... Simulator::Run(), Simulator::Destroy() ...

auto t2 = std::chrono::steady_clock::now();
```

From these we compute what we call configuration, simulation, and total runtime and print them out for the user:

```cpp
const double configMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
const double simMs = std::chrono::duration<double, std::milli>(t2 - t1).count();
const double totalMs = std::chrono::duration<double, std::milli>(t2 - t0).count();

std::cout << "\n[RESULT] backend=" << backend << "  nodes=" << numNodes
          << "  config_ms=" << std::fixed << std::setprecision(3) << configMs
          << "  sim_ms=" << simMs << "  total_ms=" << totalMs << "\n";
```

<details>

<summary>Full example: one trial with command-line arguments and timing</summary>

```cpp
#include "ns3/core-module.h"
#include "ns3/simulator.h"

#include "ns3/q2ns-netcontroller.h"
#include "ns3/q2ns-qnode.h"
#include "ns3/q2ns-qubit.h"

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace ns3;
using namespace q2ns;


int main(int argc, char* argv[]) {
  uint32_t numNodes = 8;
  std::string backend = "stab";
  uint32_t seed = 1;
  uint32_t run = 1;

  CommandLine cmd;
  cmd.AddValue("numNodes", "Total number of nodes (>= 2)", numNodes);
  cmd.AddValue("backend", "ket | dm | stab", backend);
  cmd.AddValue("seed", "ns-3 RNG seed", seed);
  cmd.AddValue("run", "ns-3 RNG run number", run);
  cmd.Parse(argc, argv);

  if (numNodes < 2) {
    NS_ABORT_MSG("--numNodes must be at least 2");
  }

  RngSeedManager::SetSeed(seed);
  RngSeedManager::SetRun(run);

  auto t0 = std::chrono::steady_clock::now();

  NetController net;
  net.SetQStateBackend(backend);

  std::vector<Ptr<QNode>> nodes;
  nodes.reserve(numNodes);

  for (uint32_t i = 0; i < numNodes; ++i) {
    nodes.push_back(net.CreateNode());
  }

  Ptr<QNode> center = nodes[0];

  for (uint32_t i = 1; i < numNodes; ++i) {
    auto ch = net.InstallQuantumLink(center, nodes[i]);
    ch->SetAttribute("Delay", TimeValue(NanoSeconds(10)));
  }

  for (uint32_t i = 1; i < numNodes; ++i) {
    Ptr<QNode> node = nodes[i];
    node->SetRecvCallback([node](std::shared_ptr<Qubit> q) {
      node->Measure(q);
    });
  }

  auto t1 = std::chrono::steady_clock::now();

  Simulator::Schedule(NanoSeconds(1), [center, &nodes, numNodes]() {
    std::vector<std::shared_ptr<Qubit>> qs;
    qs.reserve(numNodes);

    for (uint32_t i = 0; i < numNodes; ++i) {
      qs.push_back(center->CreateQubit());
    }

    for (uint32_t i = 0; i < numNodes; ++i) {
      center->Apply(gates::H(), {qs[i]});
    }

    for (uint32_t i = 0; i + 1 < numNodes; ++i) {
      center->Apply(gates::CZ(), {qs[i], qs[i + 1]});
    }

    for (uint32_t i = 1; i < numNodes; ++i) {
      const bool ok = center->Send(qs[i], nodes[i]->GetId());
      if (!ok) {
        std::cerr << "[WARN] Send to node " << i << " failed\n";
      }
    }

    Simulator::Schedule(NanoSeconds(19), [centerQubit = qs[0], center]() {
      center->Measure(centerQubit, Basis::Z);
    });
  });

  Simulator::Stop(MicroSeconds(10));
  Simulator::Run();
  Simulator::Destroy();

  auto t2 = std::chrono::steady_clock::now();

  const double configMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
  const double simMs = std::chrono::duration<double, std::milli>(t2 - t1).count();
  const double totalMs = std::chrono::duration<double, std::milli>(t2 - t0).count();

std::cout << "\n[RESULT] backend=" << backend << "  nodes=" << numNodes
          << "  config_ms=" << std::fixed << std::setprecision(3) << configMs
          << "  sim_ms=" << simMs << "  total_ms=" << totalMs << "\n";

  return 0;
}
```

</details>

You can now run the same experiment with different backends and node counts, for example:

```bash
./ns3 run q2ns-4-multipartite-scaling-example -- --numNodes=10 --backend=stab --seed=1 --run=1
./ns3 run q2ns-4-multipartite-scaling-example -- --numNodes=10 --backend=ket --seed=1 --run=1
./ns3 run q2ns-4-multipartite-scaling-example -- --numNodes=10 --backend=dm --seed=1 --run=1
```

## 3. Running Multiple Trials and Collecting Statistics

A single runtime measurement is often not very informative by itself. To get a better picture, we can repeat the experiment several times and summarize the results.

We will now add:

- a `trials` command-line argument
- a helper function `RunOnce(...)` that runs one trial
- a small `RunningStats` struct to collect the results

The first addition is the statistics helper. To allow flexibility for adding any statistics we might need, we will define a custom struct for this experiment:

```cpp
struct RunningStats {
```

This will store a vector of values that we collect for the times:

```cpp
  std::vector<double> valuesMs;
```

And add a simple function for adding new values, i.e. with each trial:

```cpp
  void Add(double x) {
    valuesMs.push_back(x);
  }
```

Lastly, we provide a way to calculate the actual statistics we want. In the code below we use the `std::accumulate()` function to make our lives a bit easier, but of course could do this more explicitly with a loop:

```cpp
  double Mean() const {
    if (valuesMs.empty())
      return 0.0;
    double s = std::accumulate(valuesMs.begin(), valuesMs.end(), 0.0);
    return s / static_cast<double>(valuesMs.size());
  }
};
```

Next we move the body of one simulation trial into a helper function. This helps keep the distinction between the whole experiment and a single trial clearer and more easily measurable. This approach lets `main()` focus on experiment control while `RunOnce()` contains the actual simulation logic:

`main()` still takes the command-line arguments, notably including `trials` here as well:

```cpp
int main(int argc, char* argv[]) {
  uint32_t numNodes = 8;
  uint32_t trials = 3;
  std::string backend = "stab";
  uint32_t seed = 1;
  uint32_t run = 1;
  bool verbose = false;

  CommandLine cmd;
  cmd.AddValue("numNodes", "Total number of nodes (>= 2)", numNodes);
  cmd.AddValue("trials", "Number of repeated runs", trials);
  cmd.AddValue("backend", "ket | dm | stab", backend);
  cmd.AddValue("seed", "ns-3 RNG seed", seed);
  cmd.AddValue("run", "ns-3 RNG run number", run);
  cmd.AddValue("verbose", "Print per-trial timing details", verbose);
  cmd.Parse(argc, argv);

  if (numNodes < 2) {
    NS_ABORT_MSG("--numNodes must be at least 2");
  }
```

It also creates a `RunningStats` instance:

```cpp
  RunningStats totalStats;
```

Now, it sets the seed, but waits to set the run for each trial, as `run + t` for instance, so that we are measuring statistically independent trials:

```cpp
  RngSeedManager::SetSeed(seed);
  for (uint32_t t = 0; t < trials; ++t) {
    RngSeedManager::SetRun(run + t);
```

Within this loop, it runs a single trial, neatly encapsulated by the `RunOnce()` function. It also adds the result of this to our stats collector:

```cpp
    totalStats.Add(RunOnce(numNodes, backend, verbose));
  }
```

Lastly `main()` can print the results from the stats collector by calling `totalStats.Mean()`. We also add `<< std::fixed << std::setprecision(3) << ` before this so that we do not end up printing an unnecessarily large number of decimal points:

```cpp
std::cout << "\n[RESULT] backend=" << backend
          << "  nodes=" << numNodes
          << "  mean_total_ms=" << std::fixed << std::setprecision(3) << totalStats.Mean()
          << "\n";
```

The core simulation logic of a trial lives in `RunOnce()`, which should return the data that the stats collector expects:

```cpp
double RunOnce(uint32_t numNodes, const std::string& backend, bool verbose) {
  // one full simulation run
}
```

The full example includes several other statistics and a `--verbose` flag that will print results for every trial if set to 1.

<details>

<summary>Full example: repeated trials with statistics</summary>

```cpp
#include "ns3/core-module.h"
#include "ns3/simulator.h"

#include "ns3/q2ns-netcontroller.h"
#include "ns3/q2ns-qnode.h"
#include "ns3/q2ns-qubit.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

using namespace ns3;
using namespace q2ns;

namespace {

struct RunningStats {
  std::vector<double> valuesMs;

  void Add(double x) {
    valuesMs.push_back(x);
  }

  double Mean() const {
    if (valuesMs.empty())
      return 0.0;
    double s = std::accumulate(valuesMs.begin(), valuesMs.end(), 0.0);
    return s / static_cast<double>(valuesMs.size());
  }

  double StdDev() const {
    if (valuesMs.size() < 2)
      return 0.0;
    const double mean = Mean();
    double accum = 0.0;
    for (double x : valuesMs) {
      const double d = x - mean;
      accum += d * d;
    }
    return std::sqrt(accum / static_cast<double>(valuesMs.size() - 1));
  }

  double Median() const {
    if (valuesMs.empty())
      return 0.0;

    std::vector<double> tmp = valuesMs;
    std::sort(tmp.begin(), tmp.end());

    const size_t n = tmp.size();
    if (n % 2 == 1) {
      return tmp[n / 2];
    }
    return 0.5 * (tmp[n / 2 - 1] + tmp[n / 2]);
  }
};


double RunOnce(uint32_t numNodes, const std::string& backend, bool verbose) {
  auto t0 = std::chrono::steady_clock::now();

  NetController net;
  net.SetQStateBackend(backend);

  std::vector<Ptr<QNode>> nodes;
  nodes.reserve(numNodes);

  for (uint32_t i = 0; i < numNodes; ++i) {
    nodes.push_back(net.CreateNode());
  }

  Ptr<QNode> center = nodes[0];

  for (uint32_t i = 1; i < numNodes; ++i) {
    auto ch = net.InstallQuantumLink(center, nodes[i]);
    ch->SetAttribute("Delay", TimeValue(NanoSeconds(10)));
  }


  for (uint32_t i = 1; i < numNodes; ++i) {
    Ptr<QNode> node = nodes[i];
    node->SetRecvCallback([node](std::shared_ptr<Qubit> q) { node->Measure(q); });
  }

  auto t1 = std::chrono::steady_clock::now();

  Simulator::Schedule(NanoSeconds(1), [center, &nodes, numNodes]() {
    std::vector<std::shared_ptr<Qubit>> qs;
    qs.reserve(numNodes);

    for (uint32_t i = 0; i < numNodes; ++i) {
      qs.push_back(center->CreateQubit());
    }

    // Prepare a 1D cluster state:
    //   |C_n> = (CZ_{0,1} CZ_{1,2} ... CZ_{n-2,n-1}) |+>^{(x)n}
    for (uint32_t i = 0; i < numNodes; ++i) {
      center->Apply(gates::H(), {qs[i]});
    }

    for (uint32_t i = 0; i + 1 < numNodes; ++i) {
      center->Apply(gates::CZ(), {qs[i], qs[i + 1]});
    }

    // Distribute one qubit to each remote node
    for (uint32_t i = 1; i < numNodes; ++i) {
      const bool ok = center->Send(qs[i], nodes[i]->GetId());
      if (!ok) {
        std::cerr << "[WARN] Send to node " << i << " failed\n";
      }
    }

    // Center can measure its qubit after sending
    // This will run at 20 ns since the scheduled time is relative to the current simulation time
    // (Simulator::Now()). This overall lambda runs at 1 ns and then this one below for measurement
    // runs 19 ns after that.
    Simulator::Schedule(NanoSeconds(19), [centerQubit = qs[0], center]() {
      center->Measure(centerQubit, Basis::Z);
    });
  });

  // Leave plenty of time for all deliveries
  Simulator::Stop(MicroSeconds(10));
  Simulator::Run();
  Simulator::Destroy();

  auto t2 = std::chrono::steady_clock::now();

  const double configMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
  const double simMs = std::chrono::duration<double, std::milli>(t2 - t1).count();
  const double totalMs = std::chrono::duration<double, std::milli>(t2 - t0).count();

  if (verbose) {
    std::cout << "[RUN]  config=" << std::fixed << std::setprecision(3) << configMs << " ms"
              << "  sim=" << simMs << " ms"
              << "  total=" << totalMs << " ms\n";
  }

  return totalMs;
}

} // namespace

int main(int argc, char* argv[]) {
  uint32_t numNodes = 8;
  uint32_t trials = 3;
  std::string backend = "stab";
  uint32_t seed = 1;
  uint32_t run = 1;
  bool verbose = false;

  CommandLine cmd;
  cmd.AddValue("numNodes", "Total number of nodes (>= 2)", numNodes);
  cmd.AddValue("trials", "Number of repeated runs", trials);
  cmd.AddValue("backend", "ket | dm | stab", backend);
  cmd.AddValue("seed", "ns-3 RNG seed", seed);
  cmd.AddValue("run", "ns-3 RNG run number", run);
  cmd.AddValue("verbose", "Print per-trial timing details", verbose);
  cmd.Parse(argc, argv);

  if (numNodes < 2) {
    NS_ABORT_MSG("--numNodes must be at least 2");
  }

  RngSeedManager::SetSeed(seed);

  std::cout << "[DEMO] Multipartite entanglement distribution starting\n";
  std::cout << "  nodes    = " << numNodes << "\n";
  std::cout << "  backend  = " << backend << "\n";
  std::cout << "  trials   = " << trials << "\n";

  RunningStats totalStats;

  for (uint32_t t = 0; t < trials; ++t) {
    RngSeedManager::SetRun(run + t);
    if (verbose) {
      std::cout << "\n[TRIAL " << (t + 1) << "/" << trials << "]\n";
    }

    totalStats.Add(RunOnce(numNodes, backend, verbose));
  }

  std::cout << "\n[RESULT] backend=" << backend << "  nodes=" << numNodes
            << "  mean_total_ms=" << std::fixed << std::setprecision(3) << totalStats.Mean()
            << "  median_total_ms=" << totalStats.Median() << "  stddev_ms=" << totalStats.StdDev()
            << "\n";

  std::cout << "[DONE] Multipartite entanglement distribution finished\n";
  return 0;
}
```

</details>

### Running the experiment

Build the Q2NS module:

```bash
./ns3 build q2ns
```

Run a small experiment with the stabilizer backend:

```bash
./ns3 run q2ns-4-multipartite-scaling-example -- --numNodes=10 --backend=stab --trials=5 --verbose=0
```

Example result:

```text
[RESULT] backend=stab  nodes=10  mean_total_ms=0.936  median_total_ms=0.174  stddev_ms=1.724
```

Now compare with the ket backend:

```bash
./ns3 run q2ns-4-multipartite-scaling-example -- --numNodes=10 --backend=ket --trials=5 --verbose=0
```

Example result with the ket backend:

```text
[RESULT] backend=ket  nodes=10  mean_total_ms=1.399  median_total_ms=0.541  stddev_ms=1.902
```

And with the density matrix backend:

```bash
./ns3 run q2ns-4-multipartite-scaling-example -- --numNodes=10 --backend=dm --trials=5 --verbose=0
```

Example result:

```text
[RESULT] backend=dm  nodes=10  mean_total_ms=2750.376  median_total_ms=2793.659  stddev_ms=74.766
```

Even at 10 nodes, the backend choice has a major effect on runtime. This is one of the main reasons Q2NS supports multiple quantum state representations: different backends are useful for different scenarios.

Running any code, Q2NS or otherwise, will often be slower in the first few trials before settling into more consistent behavior. This is usually due to one-time or front-loaded costs such as memory allocation, cache and branch-predictor warm-up, dynamic linking, and other host-system initialization effects. You can see this clearly here by enabling verbose mode:

```bash
./ns3 run q2ns-4-multipartite-scaling-example -- --numNodes=10 --backend=stab --trials=100 --verbose=1
```

```text
[TRIAL 1/100]
[RUN]  config=3.811 ms  sim=0.127 ms  total=3.938 ms


[TRIAL 2/100]
[RUN]  config=0.082 ms  sim=0.100 ms  total=0.182 ms

...

[TRIAL 11/100]
[RUN]  config=0.073 ms  sim=0.089 ms  total=0.162 ms

[TRIAL 12/100]
[RUN]  config=0.072 ms  sim=0.087 ms  total=0.159 ms

...

[TRIAL 99/100]
[RUN]  config=0.078 ms  sim=0.080 ms  total=0.158 ms

[TRIAL 100/100]
[RUN]  config=0.075 ms  sim=0.081 ms  total=0.156 ms

[RESULT] backend=stab  nodes=10  mean_total_ms=0.199  median_total_ms=0.157  stddev_ms=0.378
```

## 4. Exercises

### Exercise 1: Change the state to GHZ

Modify the preparation step so that the center creates a GHZ state rather than a 1D cluster state.

Recall that an $n$-qubit GHZ state has the form

$$
|GHZ_n\rangle = \frac{|0\cdots0\rangle + |1\cdots1\rangle}{\sqrt{2}}.
$$

One standard construction is:

1. apply `H` to the first qubit
2. apply `CNOT` from that qubit to every other qubit

There are multiple valid ways to organize the code, but the simplest change is to replace the cluster-state preparation loop inside `RunOnce()`.

<details>
<summary>Solution</summary>

Replace

```cpp
for (uint32_t i = 0; i < numNodes; ++i) {
  center->Apply(gates::H(), {qs[i]});
}

for (uint32_t i = 0; i + 1 < numNodes; ++i) {
  center->Apply(gates::CZ(), {qs[i], qs[i + 1]});
}
```

with

```cpp
center->Apply(gates::H(), {qs[0]});

for (uint32_t i = 1; i < numNodes; ++i) {
  center->Apply(gates::CNOT(), {qs[0], qs[i]});
}
```

This prepares a GHZ state before distribution.

</details>

## Exercise 2: Add classical communication

Now, with either the GHZ or cluster state, add UDP communication so that the nodes send an acknowledgement ("ACK") to the center node. It should measure its qubit only after all the other nodes have sent an ACK. NOTE: You do not have to explicitly encode "ACK" or any data, rather just receiving a packet of non-zero size is enough to be considered an ACK here.

<details>
<summary>Solution (for sake of clarity, we keep this to a single trial)</summary>

```cpp
#include "ns3/q2ns-netcontroller.h"
#include "ns3/q2ns-qnode.h"
#include "ns3/q2ns-qubit.h"

#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/simulator.h"

#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>

using namespace ns3;
using namespace q2ns;

namespace {

struct CenterInfo {
  uint32_t ackCount = 0;
  uint32_t expectedAcks = 0;
  std::shared_ptr<Qubit> centerQubit;
  bool measured = false;
};

void TryMeasureCenter(Ptr<QNode> center, CenterInfo& info) {
  if (info.measured) {
    return;
  }

  if (info.ackCount < info.expectedAcks) {
    return;
  }

  if (!info.centerQubit) {
    return;
  }

  info.measured = true;
  center->Measure(info.centerQubit, Basis::Z);
  std::cout << "[CENTER][quantum] Measured local qubit after receiving all ACKs\n";
}

} // namespace

int main(int argc, char* argv[]) {
  uint32_t numNodes = 5;
  std::string backend = "stab";
  uint32_t seed = 1;
  uint32_t run = 1;

  CommandLine cmd;
  cmd.AddValue("numNodes", "Total number of nodes (>= 2)", numNodes);
  cmd.AddValue("backend", "ket | dm | stab", backend);
  cmd.AddValue("seed", "ns-3 RNG seed", seed);
  cmd.AddValue("run", "ns-3 RNG run number", run);
  cmd.Parse(argc, argv);

  if (numNodes < 2) {
    NS_ABORT_MSG("--numNodes must be at least 2");
  }

  RngSeedManager::SetSeed(seed);
  RngSeedManager::SetRun(run);

  std::cout << "[DEMO] Multipartite entanglement distribution with ACKs starting\n";
  std::cout << "  nodes    = " << numNodes << "\n";
  std::cout << "  backend  = " << backend << "\n";

  NetController net;
  net.SetQStateBackend(backend);

  std::vector<Ptr<QNode>> nodes;
  nodes.reserve(numNodes);

  for (uint32_t i = 0; i < numNodes; ++i) {
    nodes.push_back(net.CreateNode());
  }

  Ptr<QNode> center = nodes[0];

  // ---------------------------------------------------------------------------
  // Quantum star topology
  // ---------------------------------------------------------------------------
  for (uint32_t i = 1; i < numNodes; ++i) {
    auto ch = net.InstallQuantumLink(center, nodes[i]);
    ch->SetAttribute("Delay", TimeValue(NanoSeconds(10)));
  }

  // ---------------------------------------------------------------------------
  // Classical networking setup
  // ---------------------------------------------------------------------------
  InternetStackHelper internet;
  for (auto node : nodes) {
    internet.Install(node);
  }

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("1ms"));

  const uint16_t ackPort = 9000;

  // One classical link per remote node
  std::vector<Ipv4InterfaceContainer> interfaces(numNodes);
  for (uint32_t i = 1; i < numNodes; ++i) {
    NetDeviceContainer devices = p2p.Install(center, nodes[i]);

    Ipv4AddressHelper ip;
    std::string subnet = "10.1." + std::to_string(i) + ".0";
    ip.SetBase(subnet.c_str(), "255.255.255.0");
    interfaces[i] = ip.Assign(devices);
  }

  // Center ACK receiver
  CenterInfo centerInfo;
  centerInfo.expectedAcks = numNodes - 1;

  Ptr<Socket> centerAckSocket = Socket::CreateSocket(center, UdpSocketFactory::GetTypeId());
  centerAckSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), ackPort));

  centerAckSocket->SetRecvCallback([center, &centerInfo](Ptr<Socket> socket) {
    while (Ptr<Packet> packet = socket->Recv()) {
      ++centerInfo.ackCount;
      std::cout << "[CENTER][classical] Received ACK " << centerInfo.ackCount << "/"
                << centerInfo.expectedAcks << "\n";
      TryMeasureCenter(center, centerInfo);
    }
  });

  // One ACK sender per remote node
  std::vector<Ptr<Socket>> remoteAckSockets(numNodes);
  for (uint32_t i = 1; i < numNodes; ++i) {
    Ptr<Socket> s = Socket::CreateSocket(nodes[i], UdpSocketFactory::GetTypeId());
    InetSocketAddress remote = InetSocketAddress(interfaces[i].GetAddress(0), ackPort);
    s->Connect(remote);
    remoteAckSockets[i] = s;
  }

  // ---------------------------------------------------------------------------
  // Quantum receive callbacks on remotes:
  //   1) send ACK to center
  //   2) measure local qubit immediately
  // ---------------------------------------------------------------------------
  for (uint32_t i = 1; i < numNodes; ++i) {
    Ptr<QNode> node = nodes[i];
    Ptr<Socket> ackSocket = remoteAckSockets[i];

    node->SetRecvCallback([node, ackSocket, i](std::shared_ptr<Qubit> q) {
      ackSocket->Send(Create<Packet>(3)); // "ACK" as a 3-byte payload
      std::cout << "[NODE " << i << "][classical] Sent ACK to center\n";

      node->Measure(q, Basis::Z);
      std::cout << "[NODE " << i << "][quantum] Measured received qubit\n";
    });
  }

  // ---------------------------------------------------------------------------
  // Schedule the multipartite protocol
  // ---------------------------------------------------------------------------
  Simulator::Schedule(NanoSeconds(1), [center, &nodes, &centerInfo, numNodes]() {
    std::vector<std::shared_ptr<Qubit>> qs;
    qs.reserve(numNodes);

    for (uint32_t i = 0; i < numNodes; ++i) {
      qs.push_back(center->CreateQubit());
    }

    // Prepare a 1D cluster state:
    //   |C_n> = (CZ_{0,1} CZ_{1,2} ... CZ_{n-2,n-1}) |+>^{(x)n}
    for (uint32_t i = 0; i < numNodes; ++i) {
      center->Apply(gates::H(), {qs[i]});
    }

    for (uint32_t i = 0; i + 1 < numNodes; ++i) {
      center->Apply(gates::CZ(), {qs[i], qs[i + 1]});
    }

    centerInfo.centerQubit = qs[0];

    for (uint32_t i = 1; i < numNodes; ++i) {
      const bool ok = center->Send(qs[i], nodes[i]->GetId());
      if (!ok) {
        std::cerr << "[WARN] Send to node " << i << " failed\n";
      }
    }
  });

  Simulator::Stop(MilliSeconds(20));
  Simulator::Run();

  std::cout << "[DONE] Multipartite entanglement distribution with ACKs finished\n";

  Simulator::Destroy();
  return 0;
}
```

</details>

## Exercise 3: Add a second round of classical communication

Go a step further than Exercise 2 by simulating the following:

1. Every node sends an ACK to the center when it receives the qubit
2. Once the center node has received an ACK from all other nodes, it sends an ACK to each of them that the distribution is complete, and then measures its own qubit
3. Once the other nodes received this ACK from the center, they measure their qubit

<details>
<summary>Solution (for sake of clarity, we keep this to a single trial)</summary>

```cpp
#include "ns3/q2ns-netcontroller.h"
#include "ns3/q2ns-qnode.h"
#include "ns3/q2ns-qubit.h"

#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/simulator.h"

#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>

using namespace ns3;
using namespace q2ns;

namespace {

struct RemoteInfo {
  std::shared_ptr<Qubit> qubit;
  bool qubitArrived = false;
  bool doneAckArrived = false;
  bool measured = false;
};

struct CenterInfo {
  uint32_t ackCount = 0;
  uint32_t expectedAcks = 0;
  std::shared_ptr<Qubit> centerQubit;
  bool measured = false;
};

void TryMeasureRemote(Ptr<QNode> node, uint32_t idx, RemoteInfo& info) {
  if (info.measured) {
    return;
  }

  if (!info.qubitArrived || !info.doneAckArrived) {
    return;
  }

  info.measured = true;
  node->Measure(info.qubit, Basis::Z);
  std::cout << "[NODE " << idx << "][quantum] Measured received qubit after center ACK\n";
}

void CompleteDistribution(Ptr<QNode> center,
                          CenterInfo& centerInfo,
                          const std::vector<Ptr<Socket>>& centerDoneSockets) {
  if (centerInfo.measured) {
    return;
  }

  if (centerInfo.ackCount < centerInfo.expectedAcks) {
    return;
  }

  // Notify all remotes that distribution is complete
  for (uint32_t i = 1; i < centerDoneSockets.size(); ++i) {
    if (centerDoneSockets[i]) {
      centerDoneSockets[i]->Send(Create<Packet>(4)); // "DONE"
      std::cout << "[CENTER][classical] Sent completion ACK to node " << i << "\n";
    }
  }

  centerInfo.measured = true;
  center->Measure(centerInfo.centerQubit, Basis::Z);
  std::cout << "[CENTER][quantum] Measured local qubit after receiving all ACKs\n";
}

} // namespace

int main(int argc, char* argv[]) {
  uint32_t numNodes = 5;
  std::string backend = "stab";
  uint32_t seed = 1;
  uint32_t run = 1;

  CommandLine cmd;
  cmd.AddValue("numNodes", "Total number of nodes (>= 2)", numNodes);
  cmd.AddValue("backend", "ket | dm | stab", backend);
  cmd.AddValue("seed", "ns-3 RNG seed", seed);
  cmd.AddValue("run", "ns-3 RNG run number", run);
  cmd.Parse(argc, argv);

  if (numNodes < 2) {
    NS_ABORT_MSG("--numNodes must be at least 2");
  }

  RngSeedManager::SetSeed(seed);
  RngSeedManager::SetRun(run);

  std::cout << "[DEMO] Multipartite entanglement distribution with two ACK rounds starting\n";
  std::cout << "  nodes    = " << numNodes << "\n";
  std::cout << "  backend  = " << backend << "\n";

  NetController net;
  net.SetQStateBackend(backend);

  std::vector<Ptr<QNode>> nodes;
  nodes.reserve(numNodes);

  for (uint32_t i = 0; i < numNodes; ++i) {
    nodes.push_back(net.CreateNode());
  }

  Ptr<QNode> center = nodes[0];

  // ---------------------------------------------------------------------------
  // Quantum star topology
  // ---------------------------------------------------------------------------
  for (uint32_t i = 1; i < numNodes; ++i) {
    auto ch = net.InstallQuantumLink(center, nodes[i]);
    ch->SetAttribute("Delay", TimeValue(NanoSeconds(10)));
  }

  // ---------------------------------------------------------------------------
  // Classical networking setup
  // ---------------------------------------------------------------------------
  InternetStackHelper internet;
  for (auto node : nodes) {
    internet.Install(node);
  }

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("1ms"));

  const uint16_t ackToCenterPort = 9000;
  const uint16_t doneToRemotePort = 9001;

  std::vector<Ipv4InterfaceContainer> interfaces(numNodes);
  for (uint32_t i = 1; i < numNodes; ++i) {
    NetDeviceContainer devices = p2p.Install(center, nodes[i]);

    Ipv4AddressHelper ip;
    std::string subnet = "10.1." + std::to_string(i) + ".0";
    ip.SetBase(subnet.c_str(), "255.255.255.0");
    interfaces[i] = ip.Assign(devices);
  }

  // ---------------------------------------------------------------------------
  // Classical sockets
  // ---------------------------------------------------------------------------
  // Center receives ACKs from remotes
  CenterInfo centerInfo;
  centerInfo.expectedAcks = numNodes - 1;

  Ptr<Socket> centerAckSocket = Socket::CreateSocket(center, UdpSocketFactory::GetTypeId());
  centerAckSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), ackToCenterPort));

  // Center sends completion ACKs to remotes
  std::vector<Ptr<Socket>> centerDoneSockets(numNodes);
  for (uint32_t i = 1; i < numNodes; ++i) {
    Ptr<Socket> s = Socket::CreateSocket(center, UdpSocketFactory::GetTypeId());
    InetSocketAddress remote = InetSocketAddress(interfaces[i].GetAddress(1), doneToRemotePort);
    s->Connect(remote);
    centerDoneSockets[i] = s;
  }

  // Remote sender sockets for ACK -> center
  std::vector<Ptr<Socket>> remoteAckSockets(numNodes);
  for (uint32_t i = 1; i < numNodes; ++i) {
    Ptr<Socket> s = Socket::CreateSocket(nodes[i], UdpSocketFactory::GetTypeId());
    InetSocketAddress remote = InetSocketAddress(interfaces[i].GetAddress(0), ackToCenterPort);
    s->Connect(remote);
    remoteAckSockets[i] = s;
  }

  // Remote receiver sockets for DONE <- center
  std::vector<Ptr<Socket>> remoteDoneSockets(numNodes);
  std::vector<RemoteInfo> remoteInfos(numNodes);

  for (uint32_t i = 1; i < numNodes; ++i) {
    Ptr<Socket> s = Socket::CreateSocket(nodes[i], UdpSocketFactory::GetTypeId());
    s->Bind(InetSocketAddress(Ipv4Address::GetAny(), doneToRemotePort));
    remoteDoneSockets[i] = s;
  }

  // ---------------------------------------------------------------------------
  // Remote classical receive callbacks:
  // wait for center completion ACK, then measure
  // ---------------------------------------------------------------------------
  for (uint32_t i = 1; i < numNodes; ++i) {
    Ptr<QNode> node = nodes[i];
    remoteDoneSockets[i]->SetRecvCallback([node, i, &remoteInfos](Ptr<Socket> socket) {
      while (Ptr<Packet> packet = socket->Recv()) {
        remoteInfos[i].doneAckArrived = true;
        std::cout << "[NODE " << i << "][classical] Received completion ACK from center\n";
        TryMeasureRemote(node, i, remoteInfos[i]);
      }
    });
  }

  // ---------------------------------------------------------------------------
  // Center classical receive callback:
  // count remote ACKs, then notify all remotes and measure center qubit
  // ---------------------------------------------------------------------------
  centerAckSocket->SetRecvCallback(
      [center, &centerInfo, &centerDoneSockets](Ptr<Socket> socket) {
        while (Ptr<Packet> packet = socket->Recv()) {
          ++centerInfo.ackCount;
          std::cout << "[CENTER][classical] Received ACK " << centerInfo.ackCount << "/"
                    << centerInfo.expectedAcks << "\n";
          CompleteDistribution(center, centerInfo, centerDoneSockets);
        }
      });

  // ---------------------------------------------------------------------------
  // Remote quantum receive callbacks:
  // store qubit, send ACK to center, wait for center DONE before measuring
  // ---------------------------------------------------------------------------
  for (uint32_t i = 1; i < numNodes; ++i) {
    Ptr<Socket> ackSocket = remoteAckSockets[i];

    nodes[i]->SetRecvCallback([ackSocket, i, &remoteInfos](std::shared_ptr<Qubit> q) {
      remoteInfos[i].qubit = q;
      remoteInfos[i].qubitArrived = true;

      ackSocket->Send(Create<Packet>(3)); // "ACK"
      std::cout << "[NODE " << i << "][classical] Sent ACK to center\n";
    });
  }

  // ---------------------------------------------------------------------------
  // Schedule the multipartite protocol
  // ---------------------------------------------------------------------------
  Simulator::Schedule(NanoSeconds(1), [center, &nodes, &centerInfo, numNodes]() {
    std::vector<std::shared_ptr<Qubit>> qs;
    qs.reserve(numNodes);

    for (uint32_t i = 0; i < numNodes; ++i) {
      qs.push_back(center->CreateQubit());
    }

    // Prepare a 1D cluster state:
    //   |C_n> = (CZ_{0,1} CZ_{1,2} ... CZ_{n-2,n-1}) |+>^{(x)n}
    for (uint32_t i = 0; i < numNodes; ++i) {
      center->Apply(gates::H(), {qs[i]});
    }

    for (uint32_t i = 0; i + 1 < numNodes; ++i) {
      center->Apply(gates::CZ(), {qs[i], qs[i + 1]});
    }

    centerInfo.centerQubit = qs[0];

    for (uint32_t i = 1; i < numNodes; ++i) {
      const bool ok = center->Send(qs[i], nodes[i]->GetId());
      if (!ok) {
        std::cerr << "[WARN] Send to node " << i << " failed\n";
      }
    }
  });

  Simulator::Stop(MilliSeconds(20));
  Simulator::Run();

  std::cout << "[DONE] Multipartite entanglement distribution with two ACK rounds finished\n";

  Simulator::Destroy();
  return 0;
}
```

</details>

## Related Publications

[[1]](https://ieeexplore.ieee.org/document/11322738) Marcello Caleffi and Angela Sara Cacciapuoti, _"Quantum Internet Architecture: Unlocking Quantum-Native Routing via Quantum Addressing"_ (invited paper), in IEEE Transactions on Communications, vol. 74, pp. 3577–3599, 2026.

[[2]](https://arxiv.org/abs/2603.02857) Adam Pearson, Francesco Mazza, Marcello Caleffi, Angela Sara Cacciapuoti, _"An Extensible Quantum Network Simulator Built on ns-3: Q2NS Design and Evaluation"_, arXiv:2603.02857, 2026.

[[3]](https://doi.org/10.5281/zenodo.18980972) Adam Pearson, Francesco Mazza, Marcello Caleffi, Angela Sara Cacciapuoti, _"Q2NS: A Modular Framework for Quantum Network Simulation in ns-3"_ (invited paper), Proc. QCNC 2026.

[4] Francesco Mazza, Adam Pearson, Marcello Caleffi, Angela Sara Cacciapuoti, _"Q2NS Demo: a Quantum Network Simulator based on ns-3"_, 2026.

## Acknowledgement

This work has been funded by the **European Union** under Horizon Europe ERC-CoG grant **QNattyNet**, n. 101169850. Views and opinions expressed are those of the author(s) only and do not necessarily reflect those of the European Union or the European Research Council Executive Agency. Neither the European Union nor the granting authority can be held responsible for them.
