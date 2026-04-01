# Q2NS Tutorial 3: Adding Classical Communication to Quantum Protocols {#q2ns_tutorial_classical_comms}

This tutorial introduces classical communication in Q2NS by combining ns-3 UDP networking with quantum protocols.

Many quantum protocols require both quantum resources and classical messages. Q2NS simulations therefore often use:

- quantum channels for qubits or entanglement
- classical network stacks for measurement results or control messages

By the end of this tutorial you will be able to:

- create a simple UDP channel between Q2NS nodes
- combine quantum and classical communication in one protocol
- simulate protocols where quantum and classical information arrive at different times

We will build toward a realistic teleportation protocol in three steps:

1. Teleportation without classical communication
2. Minimal classical communication with UDP
3. Teleportation with classical corrections over UDP

## 1. Why Classical Communication Is Required

Entanglement can extend the capabilities of classical networks, but it cannot transmit information by itself. Classical communication is always required, a consequence of the no-signaling theorem.

A standard example is quantum teleportation in which Alice can "teleport" a quantum state to Bob using a pre-shared entangled pair, local quantum operations, and purely _classical_ communication:

1. Alice and Bob share an entangled pair and go to different locations
2. Alice prepares a local quantum state
3. Alice performs a Bell-state measurement (BSM)
4. Alice sends the two measurement results to Bob
5. Bob uses those two classical bits to apply corrections to his state

Without those classical bits, Bob cannot recover Alice's state.

---

## 2. Example 1: Teleportation Without Classical Communication

We begin with a simplified version that ignores the classical message. This demonstrates the basic quantum operations but is not physically realistic. From a coding perspective, this example does not introduce anything new, so it is good practice to work through it carefully.

```cpp
#include "ns3/q2ns-netcontroller.h"
#include "ns3/q2ns-qnode.h"
#include "ns3/q2ns-qubit.h"

#include "ns3/core-module.h"
#include "ns3/simulator.h"

#include <iostream>

using namespace ns3;
using namespace q2ns;

int main() {

  std::cout << "[DEMO] Teleportation (A->B) starting\n";

  ns3::RngSeedManager::SetSeed(1);
  ns3::RngSeedManager::SetRun(1);

  NetController net;
  net.SetQStateBackend(QStateBackend::Ket);

  auto A = net.CreateNode();
  auto B = net.CreateNode();

  auto ch = net.InstallQuantumLink(A, B);
  ch->SetAttribute("Delay", TimeValue(NanoSeconds(10)));


  std::pair<int, int> ms;
  Simulator::Schedule(NanoSeconds(1), [&]() {
    auto [qA, qBremote] = A->CreateBellPair();
    bool ok = A->Send(qBremote, B->GetId());
    std::cout << "[SEND][quantum] A->B: " << (ok ? "ok" : "failed") << "\n";

    auto qAToTeleport = A->CreateQubit();
    A->Apply(gates::H(), {qAToTeleport});
    ms = A->MeasureBell(qAToTeleport, qA);
    std::cout << "[A] BSM results: " << ms.first << ", " << ms.second << "\n";
  });

  // When Bob receives the qubit, he will apply the standard teleportation corrections
  B->SetRecvCallback([&](std::shared_ptr<Qubit> q){

    std::cout << "[RECV][quantum][B]: yes\n";

    if(ms.second)
      B->Apply(gates::X(), {q});

    if(ms.first)
      B->Apply(gates::Z(), {q});

    // Extra verification that Bob has the correct state
    // An X basis measurement of |+> will always be 0
    int mx = B->Measure(q, Basis::X);
    std::cout << "[B][VERIFY] Final state correct: " << ((mx==0) ? "yes":"no") << "\n";
  });

  Simulator::Stop(Seconds(10));
  Simulator::Run();

  std::cout << "[DONE] Teleportation finished\n";

  Simulator::Destroy();
}
```

### What this example shows

- Bell pair creation
- Sending a qubit through a quantum link
- Bell-state measurement
- Pauli corrections

### What is unrealistic

Bob uses Alice's measurement results immediately, but there is no way he could actually know them this way in practice. In reality, Alice must send them through a classical channel to Bob. Only after Bob has received both his half of the EPR pair and the classical message, can he perform these corrections. In some situations, these kinds of abstractions are perfectly fine, e.g. for learning about protocols. However, for more realistic simulations, we need to understand how to simulate classical communication.

## 3. Example 2: Minimal Classical Communication with UDP

Before combining classical and quantum communication, we first show a minimal example of just classical communication. In this example, we package and send the classical message in the simplest possible form that can work on the classical network stack, known as User Datagram Protocol (UDP) packets. The channel is also one of the simplest channels that just directly connects two nodes, known as a point-to-point (P2P) channel, and without any physical characteristics other than the delay. In the future, you can explore more advanced protocols, e.g. TCP, and channel types, e.g. specifying fiber optic with physical characteristics or broadcasting on wifi.

We start with our includes, in this case using both q2ns and standard ns-3 libraries that are necessary for installing the classical network capabilities. We also set the seed and run at the very beginning of main, as usual.

```cpp
#include "ns3/q2ns-netcontroller.h"
#include "ns3/q2ns-qnode.h"

#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

#include <iostream>

using namespace ns3;
using namespace q2ns;

int main() {

  ns3::RngSeedManager::SetSeed(1);
  ns3::RngSeedManager::SetRun(1);
```

Next, we create a `NetController` and 2 `QNode`s representing Alice and Bob. This might look a little strange to those familiar with ns-3, since we typically use `ns3::Node`s and `NodeContainer`s, but the goal here is to show the integration of Q2NS into ns-3, specifically that `QNode` inherits directly from `ns3::Node` and therefore has the full set of standard classical networking capabilities along with its new quantum networking capabilities.

```cpp
  NetController net;
  auto A = net.CreateNode();
  auto B = net.CreateNode();
```

Now we configure the classical networking capabilities. This is standard ns-3, with no Q2NS flavor other than using the nodes, `A` and `B`, directly rather than the often used `NodeContainer` helper. This also means that you can copy this code exactly for connecting any two `QNode`s with the P2P classical channel to send UDP packets.

To start, we use the `InternetStackHelper` so the nodes will be able to send IP/UDP packets.

```cpp
  InternetStackHelper internet;
  internet.Install(A);
  internet.Install(B);
```

Then we establish a point-to-point classical channel between the nodes, including configuring the `DataRate` and `Delay`.

```cpp
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("1ms"));
  NetDeviceContainer devices = p2p.Install(A, B);
```

Next we assign IPv4 addresses to the two ends of the link. ns-3 requires this before we can send UDP packets.

```cpp
  Ipv4AddressHelper ip;
  ip.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ip.Assign(devices);
```

Lastly, we create the UDP sockets for Alice and Bob to actually be able to communicate on this channel. This requires setting a port number, 9000 being the common choice for examples (NOTE: using it in a real world application can lead to security risks). This is handled slightly differently for each node since Alice is the sender and Bob is the receiver.

Though it is not required, the general convention is to set up the receiver's socket first to establish where packets will arrive, then we can establish who will send them. So we will start by creating Bob's socket. First we construct an address object representing that Bob's socket should receive packets from any interface (`Ipv4Address::GetAny()`) with this port number. Then we bind Bob's socket to this address object.

```cpp
  const uint16_t port = 9000;

  Ptr<Socket> bobSocket = Socket::CreateSocket(B, UdpSocketFactory::GetTypeId());
  InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), port);
  bobSocket->Bind(local);
```

We typically set the receiver's callback at this point. This must take a `Ptr<Socket> socket` so the function knows which socket has received a packet, which is especially important for reusability and clarity when there are many sockets in a simulation. Then, it gets packets from the socket through `socket->Recv()`, one at a time until there are none left. Then we can do whatever we want with each packet, in this case just printing the size as a small verification that we received the full, expected packet.

```cpp
  bobSocket->SetRecvCallback([](Ptr<Socket> socket) {
    while (Ptr<Packet> packet = socket->Recv()) {
      std::cout << "[B][classical] Received UDP packet at " << Simulator::Now().GetSeconds()
                << " s, size = " << packet->GetSize() << " bytes\n";
    }
  });
```

The last configuration step is to create Alice's socket. In this case, she only needs to establish where her socket should send packets. So, she gets Bob's IP address with `interfaces.GetAddress(1)`, creates an address object with this and the port number, and connects her socket to it. This means that `aliceSocket->Send(...)` will automatically transmit packets to Bob, without having to specify his address every time.

```cpp
  Ptr<Socket> aliceSocket = Socket::CreateSocket(A, UdpSocketFactory::GetTypeId());
  InetSocketAddress remote = InetSocketAddress(interfaces.GetAddress(1), port);
  aliceSocket->Connect(remote);
```

Finally, we set up the simulation itself by scheduling a simple event where Alice sends a packet consisting of 4 bytes. It's worth noting that network packets carry data in bytes, so we send messages as byte payloads even if the actual information content only contains a single bit.

```cpp
  // Schedule a UDP packet to be sent from A to B after 1 second of simulation time.
  Simulator::Schedule(Seconds(1.0), [aliceSocket]() {
    Ptr<Packet> packet = Create<Packet>(4);
    aliceSocket->Send(packet);
    std::cout << "[A][classical] Sent UDP packet at " << Simulator::Now().GetSeconds() << " s\n";
  });

  Simulator::Stop(Seconds(10));
  Simulator::Run();

  std::cout << "[DONE] Classical communication (A->B) finished\n";

  Simulator::Destroy();
  return 0;
}
```

<details> 
<summary>
By putting this all together, we get the final simulation file
</summary>

```cpp
#include "ns3/q2ns-netcontroller.h"
#include "ns3/q2ns-qnode.h"

#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

#include <iostream>

using namespace ns3;
using namespace q2ns;


int main() {

  std::cout << "[DEMO] Classical communication (A->B) starting\n";

  ns3::RngSeedManager::SetSeed(1);
  ns3::RngSeedManager::SetRun(1);

  // QNode inherits from ns3::Node, so it can be used directly with ordinary
  // ns-3 networking helpers (Internet stack, sockets, etc.).
  NetController net;
  auto A = net.CreateNode();
  auto B = net.CreateNode();


  // Classical networking setup
  // We install the ns-3 Internet stack so the nodes can send IP/UDP packets.
  InternetStackHelper internet;
  internet.Install(A);
  internet.Install(B);


  // Create a simple point-to-point classical link between A and B.
  // This gives us a direct network connection with fixed bandwidth and delay.
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("1ms"));
  NetDeviceContainer devices = p2p.Install(A, B);


  // Assign IPv4 addresses to the two ends of the link.
  // ns-3 requires this before we can send UDP packets.
  Ipv4AddressHelper ip;
  ip.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ip.Assign(devices);


  // UDP socket setup
  // We create a UDP receiver on B and a sender on A.
  const uint16_t port = 9000;

  // Create Bob's UDP socket, bind it to the chosen port, and attach the callback that will run
  // whenever a packet arrives.
  Ptr<Socket> bobSocket = Socket::CreateSocket(B, UdpSocketFactory::GetTypeId());
  InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), port);
  bobSocket->Bind(local);
  bobSocket->SetRecvCallback([](Ptr<Socket> socket) {
    while (Ptr<Packet> packet = socket->Recv()) {
      std::cout << "[B][classical] Received UDP packet at " << Simulator::Now().GetSeconds()
                << " s, size = " << packet->GetSize() << " bytes\n";
    }
  });

  // Create Alice's UDP socket and connect it to Bob's address and port.
  // interfaces.GetAddress(1) is Bob's IP address on the point-to-point link.
  // Now, aliceSocket->Send(...) will transmit packets to Bob.
  Ptr<Socket> aliceSocket = Socket::CreateSocket(A, UdpSocketFactory::GetTypeId());
  InetSocketAddress remote = InetSocketAddress(interfaces.GetAddress(1), port);
  aliceSocket->Connect(remote);



  // Simulation scheduling
  // Schedule a UDP packet to be sent from A to B after 1 second of simulation time.
  Simulator::Schedule(Seconds(1.0), [aliceSocket]() {
    Ptr<Packet> packet = Create<Packet>(4);
    aliceSocket->Send(packet);
    std::cout << "[A][classical] Sent UDP packet at " << Simulator::Now().GetSeconds() << " s\n";
  });

  Simulator::Stop(Seconds(10));
  Simulator::Run();

  std::cout << "[DONE] Classical communication (A->B) finished\n";

  Simulator::Destroy();
  return 0;
}
```

</details>

## 4. Example 3: Teleportation with Classical Communication

Now we combine both layers. The Bell-pair half travels through a quantum channel, while the measurement results are sent as a UDP packet. Bob must wait until both arrive to perform his corrections, only ever using locally available information at any given time.

We start with our includes, which is just the combination of the first two examples:

```cpp
#include "ns3/q2ns-netcontroller.h"
#include "ns3/q2ns-qnode.h"
#include "ns3/q2ns-qubit.h"

#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/simulator.h"

#include <iostream>

using namespace ns3;
using namespace q2ns;
```

Then we define a small struct to track Bob's locally available information. This will be updated as Bob receives the qubits and classical packets and only be used by Bob, while avoiding global variables or unrealistic information transfer.

```cpp
struct BobInfo {
  std::shared_ptr<Qubit> qBremote;
  bool qubitArrived = false;
  bool bitsArrived = false;
  int m1 = 0;
  int m2 = 0;
};
```

We also define a function that represents Bob checking if he has all the necessary information and, once he does, performing corrections. This will be part of the callback used by Bob's node for both classical packets and qubits so that it does not matter which one arrives last. Other than the first conditional, this is effectively identical to the callback used in the basic teleportation code from the beginning.

```cpp
void TryCorrections(Ptr<QNode> bob, BobInfo& bobInfo) {
  if (!bobInfo.qubitArrived || !bobInfo.bitsArrived) {
    return;
  }

  std::cout << "[B] Applying corrections: Z^" << bobInfo.m1 << " X^" << bobInfo.m2 << "|state>\n";

  if (bobInfo.m2) {
    bob->Apply(gates::X(), {bobInfo.qBremote});
  }
  if (bobInfo.m1) {
    bob->Apply(gates::Z(), {bobInfo.qBremote});
  }

  int mx = bob->Measure(bobInfo.qBremote, Basis::X);
  std::cout << "[B][VERIFY] Final state is correct: " << ((mx == 0) ? "yes" : "no") << "\n";
}
```

Now we can define our main, starting as usual:

```cpp
int main() {
  std::cout << "[DEMO] Teleportation (A->B) with classical communication starting\n";

  ns3::RngSeedManager::SetSeed(1);
  ns3::RngSeedManager::SetRun(1);

  NetController net;
  net.SetQStateBackend(QStateBackend::Ket);
```

The quantum network configuration is the same as our first example:

```cpp
  auto A = net.CreateNode();
  auto B = net.CreateNode();

  auto ch = net.InstallQuantumLink(A, B);
  ch->SetAttribute("Delay", TimeValue(NanoSeconds(10)));
```

The classical network configuration is nearly the same as our previous example, except we will wait to define Bob's classical callback later. This is purely for clarity's sake in this example and does not affect the simulation compared to defining the callback right after we bind Bob's socket to an address.

```cpp
  InternetStackHelper internet;
  internet.Install(A);
  internet.Install(B);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("1ms"));
  NetDeviceContainer devices = p2p.Install(A, B);

  Ipv4AddressHelper ip;
  ip.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ip.Assign(devices);

  const uint16_t port = 9000;

  Ptr<Socket> bobSocket = Socket::CreateSocket(B, UdpSocketFactory::GetTypeId());
  bobSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), port));

  Ptr<Socket> aliceSocket = Socket::CreateSocket(A, UdpSocketFactory::GetTypeId());
  aliceSocket->Connect(InetSocketAddress(interfaces.GetAddress(1), port));
```

Now we create an instance of Bob's local information struct and define his callbacks. The quantum and classical callbacks can be defined in either order, as long as both are defined before the events that trigger them are scheduled. We will adopt the convention of quantum and then classical here.

In the quantum callback, Bob records that the qubit arrived and stores it. Then he attempts corrections, which will only succeed if he already received the classical packet.

```cpp
  BobInfo bobInfo;

  B->SetRecvCallback([B, &bobInfo](std::shared_ptr<Qubit> q) {
    std::cout << "[RECV][quantum][B]: yes\n";
    bobInfo.qubitArrived = true;
    bobInfo.qBremote = q;

    TryCorrections(B, bobInfo);
  });
```

In the classical callback, Bob records that the classical packet arrived and decodes and stores it. The standard method for decoding a packet is `packet->CopyData(copyDataHere, numberOfBytesToCopy)`. Since we are only interested in the single bit storing the measurement result, rather than the whole byte contents, we use a mask of `& 1`. This is not strictly necessary here, but is good practice in general so we include it for completeness.

After this, he attempts corrections, which will only succeed if he already received the qubit.

```cpp
  bobSocket->SetRecvCallback([B, &bobInfo](Ptr<Socket> socket) {
    while (Ptr<Packet> packet = socket->Recv()) {

      bobInfo.bitsArrived = true;

      uint8_t bytes[2] = {0, 0};
      packet->CopyData(bytes, 2);

      bobInfo.m1 = bytes[0] & 1;
      bobInfo.m2 = bytes[1] & 1;

      std::cout << "[RECV][classical][B] m1=" << bobInfo.m1 << ", m2=" << bobInfo.m2 << "\n";

      TryCorrections(B, bobInfo);
    }
  });
```

Finally, we setup the simulation itself by scheduling an event consisting of three actions by Alice:

1. Creating a Bell pair and sending half to Bob (same as the first example)
2. Creating a single qubit, preparing it in the |+> state, and performing a BSM (same as the first example)
3. Sending the BSM results as a packet through her socket

This will trigger the callbacks and result in a realistic simulation of quantum teleportation.

```cpp
  Simulator::Schedule(NanoSeconds(1), [&]() {
    auto [qA, qBremote] = A->CreateBellPair();
    bool ok = A->Send(qBremote, B->GetId());
    std::cout << "[SEND][quantum] A->B: " << (ok ? "ok" : "failed") << "\n";

    auto qAToTeleport = A->CreateQubit();
    A->Apply(gates::H(), {qAToTeleport});
    auto [m1, m2] = A->MeasureBell(qAToTeleport, qA);
    std::cout << "[A] BSM results: " << m1 << ", " << m2 << "\n";

    uint8_t bytes[2] = {static_cast<uint8_t>(m1), static_cast<uint8_t>(m2)};
    aliceSocket->Send(Create<Packet>(bytes, 2));
    std::cout << "[SEND][classical] A->B: m1=" << m1 << ", m2=" << m2 << "\n";
  });
```

<details> 
<summary>
By putting this all together, we get the final simulation file
</summary>

```cpp
#include "ns3/q2ns-netcontroller.h"
#include "ns3/q2ns-qnode.h"
#include "ns3/q2ns-qubit.h"

#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/simulator.h"

#include <iostream>

using namespace ns3;
using namespace q2ns;

namespace {


// Bob's local information about the teleportation as it arrives.
// He learns the remote qubit through the quantum channel and the
// Bell-measurement outcomes through the classical channel.
struct BobInfo {
  std::shared_ptr<Qubit> qBremote;
  bool qubitArrived = false;
  bool bitsArrived = false;
  int m1 = 0;
  int m2 = 0;
};


// Bob can complete teleportation only after he has both:
//   1) the received qubit, and
//   2) Alice's two classical correction bits.
void TryCorrections(Ptr<QNode> bob, BobInfo& bobInfo) {
  if (!bobInfo.qubitArrived || !bobInfo.bitsArrived) {
    return;
  }

  std::cout << "[B] Applying corrections: Z^" << bobInfo.m1 << " X^" << bobInfo.m2 << "|state>\n";

  if (bobInfo.m2) {
    bob->Apply(gates::X(), {bobInfo.qBremote});
  }
  if (bobInfo.m1) {
    bob->Apply(gates::Z(), {bobInfo.qBremote});
  }

  int mx = bob->Measure(bobInfo.qBremote, Basis::X);
  std::cout << "[B][VERIFY] Final state is correct: " << ((mx == 0) ? "yes" : "no") << "\n";
}


} // namespace

int main() {
  std::cout << "[DEMO] Teleportation (A->B) with classical communication starting\n";

  ns3::RngSeedManager::SetSeed(1);
  ns3::RngSeedManager::SetRun(1);

  NetController net;
  net.SetQStateBackend(QStateBackend::Ket);

  auto A = net.CreateNode();
  auto B = net.CreateNode();



  // Networking setup
  // Create the quantum channel used to distribute Bob's half of the Bell pair.
  auto ch = net.InstallQuantumLink(A, B);
  ch->SetAttribute("Delay", TimeValue(NanoSeconds(10)));

  // Install a simple IP/UDP network between Alice and Bob for the two
  // correction bits that complete the teleportation protocol.
  InternetStackHelper internet;
  internet.Install(A);
  internet.Install(B);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("1ms"));
  NetDeviceContainer devices = p2p.Install(A, B);

  Ipv4AddressHelper ip;
  ip.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ip.Assign(devices);

  const uint16_t port = 9000;

  Ptr<Socket> bobSocket = Socket::CreateSocket(B, UdpSocketFactory::GetTypeId());
  bobSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), port));

  Ptr<Socket> aliceSocket = Socket::CreateSocket(A, UdpSocketFactory::GetTypeId());
  aliceSocket->Connect(InetSocketAddress(interfaces.GetAddress(1), port));


  // Bob's classical and quantum receive callbacks
  // Create a BobInfo instance to track Bob's local information updates
  BobInfo bobInfo;

  // When Bob receives the remote Bell-pair half, he records that the quantum
  // part has arrived. He then tries to finish teleportation, which succeeds
  // only if the classical correction packet has already arrived too.
  B->SetRecvCallback([B, &bobInfo](std::shared_ptr<Qubit> q) {
    std::cout << "[RECV][quantum][B]: yes\n";
    bobInfo.qubitArrived = true;
    bobInfo.qBremote = q;

    TryCorrections(B, bobInfo);
  });


  // When the classical correction packet arrives at Bob, decode the two bits
  // and check whether Bob now has everything needed to finish teleportation.
  bobSocket->SetRecvCallback([B, &bobInfo](Ptr<Socket> socket) {
    while (Ptr<Packet> packet = socket->Recv()) {

      bobInfo.bitsArrived = true;

      uint8_t bytes[2] = {0, 0};
      packet->CopyData(bytes, 2);

      bobInfo.m1 = bytes[0] & 1;
      bobInfo.m2 = bytes[1] & 1;

      std::cout << "[RECV][classical][B] m1=" << bobInfo.m1 << ", m2=" << bobInfo.m2 << "\n";

      TryCorrections(B, bobInfo);
    }
  });



  // Scheduling the protocol
  // Alice first sends Bob's Bell-pair half over the quantum channel.
  // She then prepares |+>, performs the Bell-state measurement locally,
  // and sends the two resulting correction bits to Bob over UDP.
  Simulator::Schedule(NanoSeconds(1), [&]() {
    auto [qA, qBremote] = A->CreateBellPair();
    bool ok = A->Send(qBremote, B->GetId());
    std::cout << "[SEND][quantum] A->B: " << (ok ? "ok" : "failed") << "\n";

    auto qAToTeleport = A->CreateQubit();
    A->Apply(gates::H(), {qAToTeleport});
    auto [m1, m2] = A->MeasureBell(qAToTeleport, qA);
    std::cout << "[A] BSM results: " << m1 << ", " << m2 << "\n";

    uint8_t bytes[2] = {static_cast<uint8_t>(m1), static_cast<uint8_t>(m2)};
    aliceSocket->Send(Create<Packet>(bytes, 2));
    std::cout << "[SEND][classical] A->B: m1=" << m1 << ", m2=" << m2 << "\n";
  });



  Simulator::Stop(Seconds(10));
  Simulator::Run();

  std::cout << "[DONE] Teleportation (A->B) with classical communication finished\n";

  Simulator::Destroy();
  return 0;
}
```

</details>

## Exercises

## Exercise 1: Change the arrival ordering

In the teleportation with classical communication example directly above, the qubits will arrive before the classical packet. Change the code so that the classical packet arrives first and confirm that the protocol is still successful.

<details>
<summary>Solution</summary>

There are several approaches here, but the simplest is to make the delay for the quantum channel significantly longer than the classical channel (without going over the upper limit set by `Simulator::Stop(...)`):

```cpp
  ch->SetAttribute("Delay", TimeValue(MilliSeconds(10)));
```

NOTE: For the classical network, end-to-end arrival time depends on both propagation delay and transmission time. The transmission time is packet-size divided by data rate. Since UDP packets still carry protocol overhead and pass through the network stack, making the classical packet arrive first may require changing both the point-to-point delay and the data rate, or increasing the quantum-link delay significantly enough to create a clear timing difference. In fact, simply playing with these parameters and seeing the results is a great way to start exploring how classical networks behave, if you are not already familiar.

</details>

## Exercise 2: Entanglement swapping with classical communication

Entanglement swapping is another core protocol in quantum networking. It uses the same basic idea as teleportation to allow nodes to share entanglement through intermediary nodes, known as repeaters. Let's consider a simple repeater chain: Alice -- Repeater -- Bob. The protocol goes as follows:

1. Repeater shares half of one Bell pair with Alice and half of another Bell pair with Bob
2. Repeater performs a BSM on its two remaining local qubits
3. Repeater sends the results to one of the nodes, let's choose Bob, who then applies the same Pauli corrections as he did in teleportation
4. Alice's and Bob's qubits are now entangled, without ever directly communicating

Here is a basic version with no classical communication:

```cpp
#include "ns3/q2ns-netcontroller.h"
#include "ns3/q2ns-qnode.h"
#include "ns3/q2ns-qubit.h"

#include "ns3/core-module.h"
#include "ns3/simulator.h"

#include <iostream>

using namespace ns3;
using namespace q2ns;

int main(){

  NetController net;
  net.SetQStateBackend(QStateBackend::Ket);

  auto A = net.CreateNode();
  auto R = net.CreateNode();
  auto B = net.CreateNode();

  auto chRA = net.InstallQuantumLink(R, A);
  auto chRB = net.InstallQuantumLink(R, B);
  chRA->SetAttribute("Delay", TimeValue(NanoSeconds(10)));
  chRB->SetAttribute("Delay", TimeValue(NanoSeconds(10)));

  std::pair<int,int> ms;
  Simulator::Schedule(NanoSeconds(1), [&](){
    auto [qAremote, qR1] = R->CreateBellPair();
    auto [qR2, qBremote] = R->CreateBellPair();

    R->Send(qAremote, A->GetId());
    R->Send(qBremote, B->GetId());

    ms = R->MeasureBell(qR1, qR2);
    std::cout<<"[R] BSM results: "<<ms.first<<","<<ms.second<<"\n";
  });

  B->SetRecvCallback([&](std::shared_ptr<Qubit> q){

    if(ms.second)
      B->Apply(gates::X(),{q});

    if(ms.first)
      B->Apply(gates::Z(),{q});

    std::cout<<"[B] corrections applied\n";
  });

  Simulator::Run();
  Simulator::Destroy();
}
```

This faces the same problem as our basic teleportation protocol: there is no way Bob could actually know the results of the repeater's BSM instantaneously. Your goal in this exercise is to make this entanglement swapping protocol realistic by adding UDP packet communication and anything else necessary.

<details>
<summary>Solution</summary>

```cpp
#include "ns3/q2ns-netcontroller.h"
#include "ns3/q2ns-qnode.h"
#include "ns3/q2ns-qubit.h"

#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/simulator.h"

#include <iostream>

using namespace ns3;
using namespace q2ns;

namespace {

struct BobInfo {
  std::shared_ptr<Qubit> qBremote;
  bool qubitArrived = false;
  bool bitsArrived = false;
  int m1 = 0;
  int m2 = 0;
};

void TryCorrections(Ptr<QNode> bob, BobInfo& bobInfo) {
  if (!bobInfo.qubitArrived || !bobInfo.bitsArrived) {
    return;
  }

  std::cout << "[B] Applying corrections: Z^" << bobInfo.m1 << " X^" << bobInfo.m2 << "|state>\n";

  if (bobInfo.m2) {
    bob->Apply(gates::X(), {bobInfo.qBremote});
  }
  if (bobInfo.m1) {
    bob->Apply(gates::Z(), {bobInfo.qBremote});
  }

  std::cout << "[B] corrections applied\n";
}

} // namespace

int main() {
  std::cout << "[DEMO] Entanglement swapping (A-R-B) with classical communication starting\n";

  ns3::RngSeedManager::SetSeed(1);
  ns3::RngSeedManager::SetRun(1);

  NetController net;
  net.SetQStateBackend(QStateBackend::Ket);

  auto A = net.CreateNode();
  auto R = net.CreateNode();
  auto B = net.CreateNode();

  auto chRA = net.InstallQuantumLink(R, A);
  auto chRB = net.InstallQuantumLink(R, B);
  chRA->SetAttribute("Delay", TimeValue(NanoSeconds(10)));
  chRB->SetAttribute("Delay", TimeValue(NanoSeconds(10)));

  InternetStackHelper internet;
  internet.Install(R);
  internet.Install(B);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("1ms"));
  NetDeviceContainer devices = p2p.Install(R, B);

  Ipv4AddressHelper ip;
  ip.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ip.Assign(devices);

  const uint16_t port = 9000;

  Ptr<Socket> bobSocket = Socket::CreateSocket(B, UdpSocketFactory::GetTypeId());
  bobSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), port));

  Ptr<Socket> repeaterSocket = Socket::CreateSocket(R, UdpSocketFactory::GetTypeId());
  repeaterSocket->Connect(InetSocketAddress(interfaces.GetAddress(1), port));

  BobInfo bobInfo;

  B->SetRecvCallback([B, &bobInfo](std::shared_ptr<Qubit> q) {
    std::cout << "[RECV][quantum][B]: yes\n";
    bobInfo.qubitArrived = true;
    bobInfo.qBremote = q;

    TryCorrections(B, bobInfo);
  });

  bobSocket->SetRecvCallback([B, &bobInfo](Ptr<Socket> socket) {
    while (Ptr<Packet> packet = socket->Recv()) {
      bobInfo.bitsArrived = true;

      uint8_t bytes[2] = {0, 0};
      packet->CopyData(bytes, 2);

      bobInfo.m1 = bytes[0] & 1;
      bobInfo.m2 = bytes[1] & 1;

      std::cout << "[RECV][classical][B] m1=" << bobInfo.m1 << ", m2=" << bobInfo.m2 << "\n";

      TryCorrections(B, bobInfo);
    }
  });

  Simulator::Schedule(NanoSeconds(1), [&]() {
    auto [qAremote, qR1] = R->CreateBellPair();
    auto [qR2, qBremote] = R->CreateBellPair();

    bool okA = R->Send(qAremote, A->GetId());
    bool okB = R->Send(qBremote, B->GetId());
    std::cout << "[SEND][quantum] R->A: " << (okA ? "ok" : "failed") << "\n";
    std::cout << "[SEND][quantum] R->B: " << (okB ? "ok" : "failed") << "\n";

    auto [m1, m2] = R->MeasureBell(qR1, qR2);
    std::cout << "[R] BSM results: " << m1 << ", " << m2 << "\n";

    uint8_t bytes[2] = {static_cast<uint8_t>(m1), static_cast<uint8_t>(m2)};
    repeaterSocket->Send(Create<Packet>(bytes, 2));
    std::cout << "[SEND][classical] R->B: m1=" << m1 << ", m2=" << m2 << "\n";
  });

  Simulator::Stop(Seconds(10));
  Simulator::Run();

  std::cout << "[DONE] Entanglement swapping (A-R-B) with classical communication finished\n";

  Simulator::Destroy();
}
```

Note that the nature of entanglement does not allow any physically realistic way for Bob to verify his state with just one pair and without communicating with Alice. However, sometimes it is important to verify for ourselves that protocols we are designing are working as intended. In these cases, we can print the state:

```cpp
std::cout << bob->GetState(bobInfo.qBremote) << "\n";
```

Or if you are generally trying to compare the overlap of two states, we have `analysis::Fidelity` and could do something like:

```cpp
auto [q0, q1] = bob->CreateBellPair();
std::cout << "Fidelity with Bell pair: " << analysis::Fidelity(bobInfo.qBremote, bob->GetState(q0)) << "\n";
```

</details>

## Related Publications

[[1]](https://ieeexplore.ieee.org/document/11322738) Marcello Caleffi and Angela Sara Cacciapuoti, _"Quantum Internet Architecture: Unlocking Quantum-Native Routing via Quantum Addressing"_ (invited paper), in IEEE Transactions on Communications, vol. 74, pp. 3577–3599, 2026.

[[2]](https://arxiv.org/abs/2603.02857) Adam Pearson, Francesco Mazza, Marcello Caleffi, Angela Sara Cacciapuoti, _"An Extensible Quantum Network Simulator Built on ns-3: Q2NS Design and Evaluation"_, arXiv:2603.02857, 2026.

[[3]](https://doi.org/10.5281/zenodo.18980972) Adam Pearson, Francesco Mazza, Marcello Caleffi, Angela Sara Cacciapuoti, _"Q2NS: A Modular Framework for Quantum Network Simulation in ns-3"_ (invited paper), Proc. QCNC 2026.

[4] Francesco Mazza, Adam Pearson, Marcello Caleffi, Angela Sara Cacciapuoti, _"Q2NS Demo: a Quantum Network Simulator based on ns-3"_, 2026.

## Acknowledgement

This work has been funded by the **European Union** under Horizon Europe ERC-CoG grant **QNattyNet**, n. 101169850. Views and opinions expressed are those of the author(s) only and do not necessarily reflect those of the European Union or the European Research Council Executive Agency. Neither the European Union nor the granting authority can be held responsible for them.
