/*-----------------------------------------------------------------------------
 * Q2NS - Quantum Network Simulator
 * Copyright (c) 2026 quantuminternet.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *---------------------------------------------------------------------------*/

#include "ns3/q2ns-qstate-ket.h"
#include "ns3/q2ns-qstate-registry.h"
#include "ns3/q2ns-qubit.h"

#include "ns3/test.h"

#include <memory>
#include <vector>

using namespace ns3;
using namespace q2ns;

/*-----------------------------------------------------------------------------
 * Test Suite: QStateRegistry - State Creation
 *---------------------------------------------------------------------------*/

/**
 * \brief Test creating a single-qubit state
 * \see QStateRegistry::CreateState()
 */
class QStateRegistryCreateSingleQubitCase : public TestCase {
public:
  QStateRegistryCreateSingleQubitCase() : TestCase("QStateRegistry: create single qubit state") {}

private:
  void DoRun() override {
    QStateRegistry reg;
    auto result = reg.CreateState(1);

    NS_TEST_ASSERT_MSG_NE(result.stateId, 0u, "state ID should be non-zero");
    NS_TEST_ASSERT_MSG_EQ(result.indices.size(), 1u, "should have 1 index");
    NS_TEST_ASSERT_MSG_EQ(result.indices[0], 0u,
                          "index should be 0"); // index 0 is for the first qubit

    auto state = reg.GetState(result.stateId);
    NS_TEST_ASSERT_MSG_NE(state, nullptr, "state should be resolvable");
    NS_TEST_ASSERT_MSG_EQ(state->NumQubits(), 1u, "state should have 1 qubit");
  }
};

/**
 * \brief Test creating a multi-qubit state
 * \see QStateRegistry::CreateState()
 */
class QStateRegistryCreateMultiQubitCase : public TestCase {
public:
  QStateRegistryCreateMultiQubitCase() : TestCase("QStateRegistry: create multi-qubit state") {}

private:
  void DoRun() override {
    QStateRegistry reg;
    auto result = reg.CreateState(5);

    NS_TEST_ASSERT_MSG_NE(result.stateId, 0u, "state ID should be non-zero");
    NS_TEST_ASSERT_MSG_EQ(result.indices.size(), 5u, "should have 5 indices");

    for (unsigned int i = 0; i < 5; ++i) {
      NS_TEST_ASSERT_MSG_EQ(result.indices[i], i,
                            "indices should be sequential"); // indices should be 0, 1, 2, 3, 4
    }

    auto state = reg.GetState(result.stateId);
    NS_TEST_ASSERT_MSG_NE(state, nullptr, "state should be resolvable");
    NS_TEST_ASSERT_MSG_EQ(state->NumQubits(), 5u, "state should have 5 qubits");
  }
};

/**
 * \brief Test creating multiple independent quantum states
 * \see QStateRegistry::CreateState()
 */
class QStateRegistryCreateMultipleStatesCase : public TestCase {
public:
  QStateRegistryCreateMultipleStatesCase()
      : TestCase("QStateRegistry: create multiple independent states") {}

private:
  void DoRun() override {
    QStateRegistry reg;
    auto result1 = reg.CreateState(1);
    auto result2 = reg.CreateState(2);
    auto result3 = reg.CreateState(3);

    NS_TEST_ASSERT_MSG_NE(result1.stateId, result2.stateId, "states should have different IDs");
    NS_TEST_ASSERT_MSG_NE(result2.stateId, result3.stateId, "states should have different IDs");
    NS_TEST_ASSERT_MSG_NE(result1.stateId, result3.stateId, "states should have different IDs");

    auto state1 = reg.GetState(result1.stateId);
    auto state2 = reg.GetState(result2.stateId);
    auto state3 = reg.GetState(result3.stateId);

    NS_TEST_ASSERT_MSG_NE(state1.get(), state2.get(), "states should be different objects");
    NS_TEST_ASSERT_MSG_NE(state2.get(), state3.get(), "states should be different objects");
  }
};

/**
 * \brief Test creating a registry entry from an existing QState object
 * \see QStateRegistry::CreateStateFromExisting()
 */
class QStateRegistryCreateFromExistingCase : public TestCase {
public:
  QStateRegistryCreateFromExistingCase()
      : TestCase("QStateRegistry: create state from existing QState") {}

private:
  void DoRun() override {
    QStateRegistry reg;
    auto existingState = std::make_shared<QStateKet>(3);

    auto result = reg.CreateStateFromExisting(existingState);

    NS_TEST_ASSERT_MSG_NE(result.stateId, 0u, "state ID should be non-zero");
    NS_TEST_ASSERT_MSG_EQ(result.indices.size(), 3u, "should have 3 indices");
    for (unsigned int i = 0; i < 3; ++i) {
      NS_TEST_ASSERT_MSG_EQ(result.indices[i], i,
                            "indices should be sequential"); // indices should be 0, 1, 2
    }

    auto state = reg.GetState(result.stateId);
    NS_TEST_ASSERT_MSG_EQ(state.get(), existingState.get(), "should resolve to same state object");
  }
};

/**
 * \brief Test setting and getting default quantum backend type
 * \see QStateRegistry::SetDefaultBackend(), QStateRegistry::GetDefaultBackend()
 */
class QStateRegistryBackendSelectionCase : public TestCase {
public:
  QStateRegistryBackendSelectionCase() : TestCase("QStateRegistry: backend selection") {}

private:
  void DoRun() override {
    QStateRegistry reg;

    NS_TEST_ASSERT_MSG_EQ(static_cast<int>(reg.GetDefaultBackend()),
                          static_cast<int>(QStateBackend::Ket), "default backend should be Ket");

    reg.SetDefaultBackend(QStateBackend::DM);
    NS_TEST_ASSERT_MSG_EQ(static_cast<int>(reg.GetDefaultBackend()),
                          static_cast<int>(QStateBackend::DM),
                          "backend should be DM after setting");

    reg.SetDefaultBackend(QStateBackend::Stab);
    NS_TEST_ASSERT_MSG_EQ(static_cast<int>(reg.GetDefaultBackend()),
                          static_cast<int>(QStateBackend::Stab),
                          "backend should be Stab after setting");
  }
};

/*-----------------------------------------------------------------------------
 * Test Suite: QStateRegistry - Qubit Registration
 *---------------------------------------------------------------------------*/

/**
 * \brief Test registering a qubit with its state
 * \see QStateRegistry::Register()
 */
class QStateRegistryRegisterQubitCase : public TestCase {
public:
  QStateRegistryRegisterQubitCase() : TestCase("QStateRegistry: register qubit") {}

private:
  void DoRun() override {
    QStateRegistry reg;
    auto result = reg.CreateState(1);
    auto qubit = std::make_shared<Qubit>(reg, result.stateId, result.indices[0], "q0");

    reg.Register(qubit);

    auto qubits = reg.QubitsOf(result.stateId);
    NS_TEST_ASSERT_MSG_EQ(qubits.size(), 1u, "should have 1 registered qubit");
    NS_TEST_ASSERT_MSG_EQ(qubits[0].get(), qubit.get(), "registered qubit should match");
  }
};

/**
 * \brief Test registering multiple qubits to the same state
 * \see QStateRegistry::Register(), QStateRegistry::QubitsOf()
 */
class QStateRegistryRegisterMultipleQubitsCase : public TestCase {
public:
  QStateRegistryRegisterMultipleQubitsCase()
      : TestCase("QStateRegistry: register multiple qubits") {}

private:
  void DoRun() override {
    QStateRegistry reg;
    auto result = reg.CreateState(3);

    auto q0 = std::make_shared<Qubit>(reg, result.stateId, result.indices[0], "q0");
    auto q1 = std::make_shared<Qubit>(reg, result.stateId, result.indices[1], "q1");
    auto q2 = std::make_shared<Qubit>(reg, result.stateId, result.indices[2], "q2");

    reg.Register(q0);
    reg.Register(q1);
    reg.Register(q2);

    auto qubits = reg.QubitsOf(result.stateId);
    NS_TEST_ASSERT_MSG_EQ(qubits.size(), 3u, "should have 3 registered qubits");
  }
};

/**
 * \brief Test unregistering a qubit from its state
 * \see QStateRegistry::Unregister()
 */
class QStateRegistryUnregisterQubitCase : public TestCase {
public:
  QStateRegistryUnregisterQubitCase() : TestCase("QStateRegistry: unregister qubit") {}

private:
  void DoRun() override {
    QStateRegistry reg;
    auto result = reg.CreateState(1);
    auto qubit = std::make_shared<Qubit>(reg, result.stateId, result.indices[0], "q0");

    reg.Register(qubit);
    NS_TEST_ASSERT_MSG_EQ(reg.QubitsOf(result.stateId).size(), 1u, "should have 1 qubit");

    reg.Unregister(qubit);
    NS_TEST_ASSERT_MSG_EQ(reg.QubitsOf(result.stateId).size(), 0u,
                          "should have 0 qubits after unregister");
  }
};

/**
 * \brief Test removing a qubit from all state registrations
 * \see QStateRegistry::UnregisterEverywhere()
 */
class QStateRegistryUnregisterEverywhereCase : public TestCase {
public:
  QStateRegistryUnregisterEverywhereCase()
      : TestCase("QStateRegistry: unregister qubit everywhere") {}

private:
  void DoRun() override {
    QStateRegistry reg;
    auto result = reg.CreateState(1);
    auto qubit = std::make_shared<Qubit>(reg, result.stateId, result.indices[0], "q0");

    reg.Register(qubit);
    reg.UnregisterEverywhere(qubit);

    auto qubits = reg.QubitsOf(result.stateId);
    NS_TEST_ASSERT_MSG_EQ(qubits.size(), 0u, "qubit should be removed from all states");
  }
};

/*-----------------------------------------------------------------------------
 * Test Suite: QStateRegistry - State Management
 *---------------------------------------------------------------------------*/

/**
 * \brief Test resolving a quantum state by its ID
 * \see QStateRegistry::Resolve(StateId)
 */
class QStateRegistryResolveByIdCase : public TestCase {
public:
  QStateRegistryResolveByIdCase() : TestCase("QStateRegistry: resolve state by ID") {}

private:
  void DoRun() override {
    QStateRegistry reg;
    auto result = reg.CreateState(2);

    auto state = reg.GetState(result.stateId);
    NS_TEST_ASSERT_MSG_NE(state, nullptr, "state should be resolvable");
    NS_TEST_ASSERT_MSG_EQ(state->GetStateId(), result.stateId, "state ID should match");
  }
};

/**
 * \brief Test resolving a quantum state from a qubit handle
 * \see QStateRegistry::Resolve(const std::shared_ptr<Qubit>&)
 */
class QStateRegistryResolveFromQubitCase : public TestCase {
public:
  QStateRegistryResolveFromQubitCase() : TestCase("QStateRegistry: resolve state from qubit") {}

private:
  void DoRun() override {
    QStateRegistry reg;
    auto result = reg.CreateState(1);
    auto qubit = std::make_shared<Qubit>(reg, result.stateId, result.indices[0], "q0");

    auto state = reg.GetState(qubit);
    NS_TEST_ASSERT_MSG_NE(state, nullptr, "state should be resolvable from qubit");
    NS_TEST_ASSERT_MSG_EQ(state->GetStateId(), result.stateId, "state ID should match");
  }
};

/**
 * \brief Test removing a quantum state from the registry
 * \see QStateRegistry::RemoveState()
 */
class QStateRegistryRemoveStateCase : public TestCase {
public:
  QStateRegistryRemoveStateCase() : TestCase("QStateRegistry: remove state") {}

private:
  void DoRun() override {
    QStateRegistry reg;
    auto result = reg.CreateState(1);

    auto stateBefore = reg.GetState(result.stateId);
    NS_TEST_ASSERT_MSG_NE(stateBefore, nullptr, "state should exist before removal");

    reg.RemoveState(result.stateId);

    auto stateAfter = reg.GetState(result.stateId);
    NS_TEST_ASSERT_MSG_EQ(stateAfter, nullptr, "state should not exist after removal");
  }
};

/*-----------------------------------------------------------------------------
 * Test Suite: QStateRegistry - State Merging
 *---------------------------------------------------------------------------*/

/**
 * \brief Test merging qubits that already share the same state (no-op)
 * \see QStateRegistry::MergeStates()
 */
class QStateRegistryMergeSameStateCase : public TestCase {
public:
  QStateRegistryMergeSameStateCase()
      : TestCase("QStateRegistry: merge qubits from same state (no-op)") {}

private:
  void DoRun() override {
    QStateRegistry reg;
    auto result = reg.CreateState(2);

    auto q0 = std::make_shared<Qubit>(reg, result.stateId, result.indices[0], "q0");
    auto q1 = std::make_shared<Qubit>(reg, result.stateId, result.indices[1], "q1");

    reg.Register(q0);
    reg.Register(q1);

    std::vector<std::shared_ptr<Qubit>> qubits = {q0, q1};
    auto mergedState = reg.MergeStates(qubits);

    NS_TEST_ASSERT_MSG_NE(mergedState, nullptr, "merged state should exist");
    NS_TEST_ASSERT_MSG_EQ(mergedState->GetStateId(), result.stateId,
                          "state ID should be unchanged");
  }
};

/**
 * \brief Test merging qubits from different states into a single state
 * \see QStateRegistry::MergeStates()
 */
class QStateRegistryMergeDifferentStatesCase : public TestCase {
public:
  QStateRegistryMergeDifferentStatesCase()
      : TestCase("QStateRegistry: merge qubits from different states") {}

private:
  void DoRun() override {
    QStateRegistry reg;
    auto result1 = reg.CreateState(1);
    auto result2 = reg.CreateState(1);

    auto q0 = std::make_shared<Qubit>(reg, result1.stateId, result1.indices[0], "q0");
    auto q1 = std::make_shared<Qubit>(reg, result2.stateId, result2.indices[0], "q1");

    reg.Register(q0);
    reg.Register(q1);

    std::vector<std::shared_ptr<Qubit>> qubits = {q0, q1};
    auto mergedState = reg.MergeStates(qubits);

    NS_TEST_ASSERT_MSG_NE(mergedState, nullptr, "merged state should exist");
    NS_TEST_ASSERT_MSG_EQ(mergedState->NumQubits(), 2u, "merged state should have 2 qubits");

    // Both qubits should now point to the same state
    NS_TEST_ASSERT_MSG_EQ(q0->GetStateId(), q1->GetStateId(),
                          "qubits should share same state after merge");
  }
};

/**
 * \brief Test merging an empty list of qubits (edge case)
 * \see QStateRegistry::MergeStates()
 */
class QStateRegistryMergeEmptyListCase : public TestCase {
public:
  QStateRegistryMergeEmptyListCase() : TestCase("QStateRegistry: merge empty qubit list") {}

private:
  void DoRun() override {
    QStateRegistry reg;
    std::vector<std::shared_ptr<Qubit>> empty;

    auto result = reg.MergeStates(empty);
    NS_TEST_ASSERT_MSG_EQ(result, nullptr, "merging empty list should return nullptr");
  }
};

/*-----------------------------------------------------------------------------
 * Test Suite: QStateRegistry - Location Tracking
 *---------------------------------------------------------------------------*/

/**
 * \brief Test setting and retrieving node location for a qubit
 * \see QStateRegistry::SetLocation(), QStateRegistry::GetLocation()
 */
class QStateRegistrySetLocationNodeCase : public TestCase {
public:
  QStateRegistrySetLocationNodeCase() : TestCase("QStateRegistry: set and get node location") {}

private:
  void DoRun() override {
    QStateRegistry reg;
    auto result = reg.CreateState(1);
    auto qubit = std::make_shared<Qubit>(reg, result.stateId, result.indices[0], "q0");

    Location loc{LocationType::Node, 42};
    reg.SetLocation(qubit, loc);

    auto retrievedLoc = reg.GetLocation(qubit);
    NS_TEST_ASSERT_MSG_EQ(retrievedLoc.type != LocationType::Unset, true, "location should be set");
    NS_TEST_ASSERT_MSG_EQ(static_cast<int>(retrievedLoc.type), static_cast<int>(LocationType::Node),
                          "location type should be Node");
    NS_TEST_ASSERT_MSG_EQ(retrievedLoc.ownerId, 42u, "owner ID should be 42");
  }
};

/**
 * \brief Test setting and retrieving channel location for a qubit
 * \see QStateRegistry::SetLocation(), QStateRegistry::GetLocation()
 */
class QStateRegistrySetLocationChannelCase : public TestCase {
public:
  QStateRegistrySetLocationChannelCase()
      : TestCase("QStateRegistry: set and get channel location") {}

private:
  void DoRun() override {
    QStateRegistry reg;
    auto result = reg.CreateState(1);
    auto qubit = std::make_shared<Qubit>(reg, result.stateId, result.indices[0], "q0");

    Location loc{LocationType::Channel, 123};
    reg.SetLocation(qubit, loc);

    auto retrievedLoc = reg.GetLocation(qubit);
    NS_TEST_ASSERT_MSG_EQ(retrievedLoc.type != LocationType::Unset, true, "location should be set");
    NS_TEST_ASSERT_MSG_EQ(static_cast<int>(retrievedLoc.type),
                          static_cast<int>(LocationType::Channel),
                          "location type should be Channel");
    NS_TEST_ASSERT_MSG_EQ(retrievedLoc.ownerId, 123u, "owner ID should be 123");
  }
};

/**
 * \brief Test querying location for a qubit without location set
 * \see QStateRegistry::GetLocation()
 */
class QStateRegistryGetLocationUnsetCase : public TestCase {
public:
  QStateRegistryGetLocationUnsetCase() : TestCase("QStateRegistry: get location for Unset qubit") {}

private:
  void DoRun() override {
    QStateRegistry reg;
    auto result = reg.CreateState(1);
    auto qubit = std::make_shared<Qubit>(reg, result.stateId, result.indices[0], "q0");

    auto retrievedLoc = reg.GetLocation(qubit);
    NS_TEST_ASSERT_MSG_EQ(retrievedLoc.type != LocationType::Unset, false,
                          "location should not be set");
  }
};

/**
 * \brief Test updating a qubit's location after initial setting
 * \see QStateRegistry::SetLocation(), QStateRegistry::GetLocation()
 */
class QStateRegistryUpdateLocationCase : public TestCase {
public:
  QStateRegistryUpdateLocationCase() : TestCase("QStateRegistry: update qubit location") {}

private:
  void DoRun() override {
    QStateRegistry reg;
    auto result = reg.CreateState(1);
    auto qubit = std::make_shared<Qubit>(reg, result.stateId, result.indices[0], "q0");

    Location loc1{LocationType::Node, 10};
    reg.SetLocation(qubit, loc1);

    Location loc2{LocationType::Channel, 20};
    reg.SetLocation(qubit, loc2);

    auto retrievedLoc = reg.GetLocation(qubit);
    NS_TEST_ASSERT_MSG_EQ(static_cast<int>(retrievedLoc.type),
                          static_cast<int>(LocationType::Channel),
                          "location type should be updated to Channel");
    NS_TEST_ASSERT_MSG_EQ(retrievedLoc.ownerId, 20u, "owner ID should be updated to 20");
  }
};


/**
 * \brief Test retrieving all qubits belonging to a specific state
 * \see QStateRegistry::QubitsOf()
 */
class QStateRegistryQubitsOfCase : public TestCase {
public:
  QStateRegistryQubitsOfCase() : TestCase("QStateRegistry: retrieve qubits of state") {}

private:
  void DoRun() override {
    QStateRegistry reg;
    auto result = reg.CreateState(3);

    auto q0 = std::make_shared<Qubit>(reg, result.stateId, result.indices[0], "q0");
    auto q1 = std::make_shared<Qubit>(reg, result.stateId, result.indices[1], "q1");
    auto q2 = std::make_shared<Qubit>(reg, result.stateId, result.indices[2], "q2");

    reg.Register(q0);
    reg.Register(q1);
    reg.Register(q2);

    auto qubits = reg.QubitsOf(result.stateId);
    NS_TEST_ASSERT_MSG_EQ(qubits.size(), 3u, "should return 3 qubits");
  }
};

/*-----------------------------------------------------------------------------
 * Test Suite: QStateRegistry - Edge Cases
 *---------------------------------------------------------------------------*/

/**
 * \brief Edge case: Creating a zero-qubit state
 * \note CreateState(0) returns empty result and logs warning
 * \see QStateRegistry::CreateState()
 */
class QStateRegistryCreateZeroQubitCase : public TestCase {
public:
  QStateRegistryCreateZeroQubitCase()
      : TestCase("QStateRegistry: create zero-qubit state returns empty result") {}

private:
  void DoRun() override {
    QStateRegistry reg;

    // CreateState(0) returns empty result
    auto result = reg.CreateState(0);

    NS_TEST_ASSERT_MSG_EQ(result.stateId, 0u,
                          "zero-qubit state should return invalid result with ID 0");
    NS_TEST_ASSERT_MSG_EQ(result.indices.size(), 0u, "zero-qubit state should have empty indices");

    // Verify no state was actually created
    auto state = reg.GetState(result.stateId);
    NS_TEST_ASSERT_MSG_EQ(state, nullptr, "state ID 0 should not resolve to valid state");
  }
};

/**
 * \brief Edge case: Resolving non-existent or invalid state IDs
 * \note Tests behavior when querying with ID 0 or unknown IDs
 * \see QStateRegistry::Resolve(StateId)
 */
class QStateRegistryResolveInvalidIdCase : public TestCase {
public:
  QStateRegistryResolveInvalidIdCase() : TestCase("QStateRegistry: resolve invalid state ID") {}

private:
  void DoRun() override {
    QStateRegistry reg;

    auto state0 = reg.GetState(0);
    NS_TEST_ASSERT_MSG_EQ(state0, nullptr, "state ID 0 should resolve to nullptr");

    auto state999 = reg.GetState(999999);
    NS_TEST_ASSERT_MSG_EQ(state999, nullptr, "non-existent state ID should resolve to nullptr");
  }
};

/**
 * \brief Edge case: Registering the same qubit twice
 * \note Registry deduplicates - prevents duplicate registrations
 * \see QStateRegistry::Register()
 */
class QStateRegistryRegisterTwiceCase : public TestCase {
public:
  QStateRegistryRegisterTwiceCase() : TestCase("QStateRegistry: register same qubit twice") {}

private:
  void DoRun() override {
    QStateRegistry reg;
    auto result = reg.CreateState(1);
    auto qubit = std::make_shared<Qubit>(reg, result.stateId, result.indices[0], "q0");

    reg.Register(qubit);
    reg.Register(qubit); // Register again

    auto qubits = reg.QubitsOf(result.stateId);
    // second registration is ignored
    NS_TEST_ASSERT_MSG_EQ(qubits.size(), 1u, "should have 1 entry (deduplication)");
    NS_TEST_ASSERT_MSG_EQ(qubits[0].get(), qubit.get(), "registered qubit should match");
  }
};

/**
 * \brief Edge case: Unregistering a qubit that was never registered
 * \note Ensures the operation doesn't crash or corrupt state
 * \see QStateRegistry::Unregister()
 */
class QStateRegistryUnregisterNeverRegisteredCase : public TestCase {
public:
  QStateRegistryUnregisterNeverRegisteredCase()
      : TestCase("QStateRegistry: unregister never-registered qubit") {}

private:
  void DoRun() override {
    QStateRegistry reg;
    auto result = reg.CreateState(1);
    auto qubit = std::make_shared<Qubit>(reg, result.stateId, result.indices[0], "q0");

    // Don't register, just try to unregister
    reg.Unregister(qubit); // Should not crash

    NS_TEST_ASSERT_MSG_EQ(reg.QubitsOf(result.stateId).size(), 0u, "should have 0 qubits");
  }
};

/**
 * \brief Edge case: Unregistering the same qubit multiple times
 * \note Tests idempotency of the unregister operation
 * \see QStateRegistry::Unregister()
 */
class QStateRegistryUnregisterTwiceCase : public TestCase {
public:
  QStateRegistryUnregisterTwiceCase() : TestCase("QStateRegistry: unregister same qubit twice") {}

private:
  void DoRun() override {
    QStateRegistry reg;
    auto result = reg.CreateState(1);
    auto qubit = std::make_shared<Qubit>(reg, result.stateId, result.indices[0], "q0");

    reg.Register(qubit);
    reg.Unregister(qubit);
    reg.Unregister(qubit); // Unregister again - should not crash

    NS_TEST_ASSERT_MSG_EQ(reg.QubitsOf(result.stateId).size(), 0u, "should have 0 qubits");
  }
};

/**
 * \brief Edge case: Removing the same state multiple times
 * \note Tests idempotency of state removal
 * \see QStateRegistry::RemoveState()
 */
class QStateRegistryRemoveStateTwiceCase : public TestCase {
public:
  QStateRegistryRemoveStateTwiceCase() : TestCase("QStateRegistry: remove same state twice") {}

private:
  void DoRun() override {
    QStateRegistry reg;
    auto result = reg.CreateState(1);

    reg.RemoveState(result.stateId);
    reg.RemoveState(result.stateId); // Remove again - should not crash

    auto state = reg.GetState(result.stateId);
    NS_TEST_ASSERT_MSG_EQ(state, nullptr, "state should not exist");
  }
};

/**
 * \brief Edge case: Removing a state that doesn't exist
 * \note Ensures graceful handling without crashes
 * \see QStateRegistry::RemoveState()
 */
class QStateRegistryRemoveNonExistentCase : public TestCase {
public:
  QStateRegistryRemoveNonExistentCase() : TestCase("QStateRegistry: remove non-existent state") {}

private:
  void DoRun() override {
    QStateRegistry reg;

    reg.RemoveState(0);      // Should not crash
    reg.RemoveState(999999); // Should not crash

    // Test passes if no crash occurs
  }
};

/**
 * \brief Edge case: Merging a list containing only one qubit
 * \note Should return the existing state without modification
 * \see QStateRegistry::MergeStates()
 */
class QStateRegistryMergeSingleQubitCase : public TestCase {
public:
  QStateRegistryMergeSingleQubitCase() : TestCase("QStateRegistry: merge list with single qubit") {}

private:
  void DoRun() override {
    QStateRegistry reg;
    auto result = reg.CreateState(1);
    auto qubit = std::make_shared<Qubit>(reg, result.stateId, result.indices[0], "q0");

    reg.Register(qubit);

    std::vector<std::shared_ptr<Qubit>> qubits = {qubit};
    auto mergedState = reg.MergeStates(qubits);

    NS_TEST_ASSERT_MSG_NE(mergedState, nullptr, "merged state should exist");
    NS_TEST_ASSERT_MSG_EQ(mergedState->GetStateId(), result.stateId, "should return same state");
  }
};

/**
 * \brief Edge case: Merging qubits from three different states at once
 * \note Tests multi-way merge beyond the typical two-state case
 * \see QStateRegistry::MergeStates()
 */
class QStateRegistryMergeThreeStatesCase : public TestCase {
public:
  QStateRegistryMergeThreeStatesCase()
      : TestCase("QStateRegistry: merge qubits from three different states") {}

private:
  void DoRun() override {
    QStateRegistry reg;
    auto result1 = reg.CreateState(1);
    auto result2 = reg.CreateState(1);
    auto result3 = reg.CreateState(1);

    auto q0 = std::make_shared<Qubit>(reg, result1.stateId, result1.indices[0], "q0");
    auto q1 = std::make_shared<Qubit>(reg, result2.stateId, result2.indices[0], "q1");
    auto q2 = std::make_shared<Qubit>(reg, result3.stateId, result3.indices[0], "q2");

    reg.Register(q0);
    reg.Register(q1);
    reg.Register(q2);

    std::vector<std::shared_ptr<Qubit>> qubits = {q0, q1, q2};
    auto mergedState = reg.MergeStates(qubits);

    NS_TEST_ASSERT_MSG_NE(mergedState, nullptr, "merged state should exist");
    NS_TEST_ASSERT_MSG_EQ(mergedState->NumQubits(), 3u, "merged state should have 3 qubits");

    // All qubits should now share the same state
    NS_TEST_ASSERT_MSG_EQ(q0->GetStateId(), q1->GetStateId(), "q0 and q1 should share same state");
    NS_TEST_ASSERT_MSG_EQ(q1->GetStateId(), q2->GetStateId(), "q1 and q2 should share same state");
  }
};

/**
 * \brief Edge case: Querying qubits for non-existent state IDs
 * \note Should return empty vector rather than crashing
 * \see QStateRegistry::QubitsOf()
 */
class QStateRegistryQubitsOfInvalidIdCase : public TestCase {
public:
  QStateRegistryQubitsOfInvalidIdCase()
      : TestCase("QStateRegistry: QubitsOf with invalid state ID") {}

private:
  void DoRun() override {
    QStateRegistry reg;

    auto qubits0 = reg.QubitsOf(0);
    NS_TEST_ASSERT_MSG_EQ(qubits0.size(), 0u, "should return empty vector for ID 0");

    auto qubits999 = reg.QubitsOf(999999);
    NS_TEST_ASSERT_MSG_EQ(qubits999.size(), 0u, "should return empty vector for non-existent ID");
  }
};

/**
 * \brief Edge case: Resolving state from a qubit that wasn't registered
 * \note Registration is for tracking ownership, not state access - resolution works regardless
 * \see QStateRegistry::Resolve(const std::shared_ptr<Qubit>&)
 */
class QStateRegistryResolveUnregisteredQubitCase : public TestCase {
public:
  QStateRegistryResolveUnregisteredQubitCase()
      : TestCase("QStateRegistry: resolve state from unregistered qubit") {}

private:
  void DoRun() override {
    QStateRegistry reg;
    auto result = reg.CreateState(1);
    auto qubit = std::make_shared<Qubit>(reg, result.stateId, result.indices[0], "q0");

    // Registration is optional for state access
    auto state = reg.GetState(qubit);

    // Resolution succeeds because qubit knows its state ID and state exists
    NS_TEST_ASSERT_MSG_NE(state, nullptr, "should resolve state even without registration");
    NS_TEST_ASSERT_MSG_EQ(state->GetStateId(), result.stateId,
                          "resolved state ID should match qubit's state ID");

    // Verify qubit is not registered (QubitsOf returns empty)
    auto qubits = reg.QubitsOf(result.stateId);
    NS_TEST_ASSERT_MSG_EQ(qubits.size(), 0u,
                          "qubit should not appear in QubitsOf without registration");
  }
};

/*-----------------------------------------------------------------------------
 * Test Suite: QStateRegistry - Null Pointer Defensive Tests
 *---------------------------------------------------------------------------*/

/**
 * \brief Null pointer handling for void-returning operations
 * \note Tests Register, Unregister, UnregisterEverywhere, SetLocation with nullptr
 * \see QStateRegistry::Register(), Unregister(), UnregisterEverywhere(), SetLocation()
 */
class QStateRegistryNullQubitOperationsCase : public TestCase {
public:
  QStateRegistryNullQubitOperationsCase()
      : TestCase("QStateRegistry: null qubit operations (void methods)") {}

private:
  void DoRun() override {
    QStateRegistry reg;
    std::shared_ptr<Qubit> nullQubit;
    Location loc{LocationType::Node, 42};

    // All should log warning and return without crash
    reg.Register(nullQubit);
    reg.Unregister(nullQubit);
    reg.UnregisterEverywhere(nullQubit);
    reg.SetLocation(nullQubit, loc);

    // Test passes if no crash occurs
  }
};

/**
 * \brief Null pointer handling for query operations
 * \note Tests GetLocation and GetState with nullptr - verifies safe return values
 * \see QStateRegistry::GetLocation(), GetState()
 */
class QStateRegistryNullQubitQueriesCase : public TestCase {
public:
  QStateRegistryNullQubitQueriesCase()
      : TestCase("QStateRegistry: null qubit queries (return values)") {}

private:
  void DoRun() override {
    QStateRegistry reg;
    std::shared_ptr<Qubit> nullQubit;

    // GetLocation should return nullopt
    auto loc = reg.GetLocation(nullQubit);
    NS_TEST_ASSERT_MSG_EQ(loc.type != LocationType::Unset, false,
                          "nullptr qubit should return no location");

    // Resolve should return nullptr
    auto state = reg.GetState(nullQubit);
    NS_TEST_ASSERT_MSG_EQ(state, nullptr, "resolving nullptr qubit should return nullptr");
  }
};

/**
 * \brief MergeStates with nullptr in qubit list
 * \note Fails entire operation if any qubit is null
 * \see QStateRegistry::MergeStates()
 */
class QStateRegistryMergeWithNullCase : public TestCase {
public:
  QStateRegistryMergeWithNullCase()
      : TestCase("QStateRegistry: merge states with nullptr in list") {}

private:
  void DoRun() override {
    QStateRegistry reg;
    auto result = reg.CreateState(1);
    auto qubit = std::make_shared<Qubit>(reg, result.stateId, result.indices[0], "q0");
    reg.Register(qubit);

    // List contains valid qubit and nullptr
    std::vector<std::shared_ptr<Qubit>> qubits = {qubit, nullptr};

    // Fails entire merge if any qubit is null
    auto mergedState = reg.MergeStates(qubits);

    NS_TEST_ASSERT_MSG_EQ(mergedState, nullptr, "merge should fail if any qubit is null");
  }
};

/**
 * \brief CreateStateFromExisting with nullptr
 * \note Should return empty result without crash
 * \see QStateRegistry::CreateStateFromExisting()
 */
class QStateRegistryCreateFromNullCase : public TestCase {
public:
  QStateRegistryCreateFromNullCase() : TestCase("QStateRegistry: create from nullptr state") {}

private:
  void DoRun() override {
    QStateRegistry reg;

    auto result = reg.CreateStateFromExisting(nullptr);

    NS_TEST_ASSERT_MSG_EQ(result.stateId, 0u,
                          "creating from nullptr should return invalid result with ID 0");
    NS_TEST_ASSERT_MSG_EQ(result.indices.size(), 0u,
                          "creating from nullptr should have empty indices");
  }
};

/*-----------------------------------------------------------------------------
 * Suite Assembly
 *---------------------------------------------------------------------------*/
/**
 * \ingroup q2ns-test
 * \brief Test suite for QStateRegistry state management and qubit tracking
 */
class Q2nsStateRegistryTestSuite : public TestSuite {
public:
  Q2nsStateRegistryTestSuite() : TestSuite("q2ns-qstate-registry", TestSuite::Type::UNIT) {
    // State creation
    AddTestCase(new QStateRegistryCreateSingleQubitCase, TestCase::Duration::QUICK);
    AddTestCase(new QStateRegistryCreateMultiQubitCase, TestCase::Duration::QUICK);
    AddTestCase(new QStateRegistryCreateMultipleStatesCase, TestCase::Duration::QUICK);
    AddTestCase(new QStateRegistryCreateFromExistingCase, TestCase::Duration::QUICK);
    AddTestCase(new QStateRegistryBackendSelectionCase, TestCase::Duration::QUICK);

    // Qubit registration
    AddTestCase(new QStateRegistryRegisterQubitCase, TestCase::Duration::QUICK);
    AddTestCase(new QStateRegistryRegisterMultipleQubitsCase, TestCase::Duration::QUICK);
    AddTestCase(new QStateRegistryUnregisterQubitCase, TestCase::Duration::QUICK);
    AddTestCase(new QStateRegistryUnregisterEverywhereCase, TestCase::Duration::QUICK);

    // State management
    AddTestCase(new QStateRegistryResolveByIdCase, TestCase::Duration::QUICK);
    AddTestCase(new QStateRegistryResolveFromQubitCase, TestCase::Duration::QUICK);
    AddTestCase(new QStateRegistryRemoveStateCase, TestCase::Duration::QUICK);

    // State merging
    AddTestCase(new QStateRegistryMergeSameStateCase, TestCase::Duration::QUICK);
    AddTestCase(new QStateRegistryMergeDifferentStatesCase, TestCase::Duration::QUICK);
    AddTestCase(new QStateRegistryMergeEmptyListCase, TestCase::Duration::QUICK);

    // Location tracking
    AddTestCase(new QStateRegistrySetLocationNodeCase, TestCase::Duration::QUICK);
    AddTestCase(new QStateRegistrySetLocationChannelCase, TestCase::Duration::QUICK);
    AddTestCase(new QStateRegistryGetLocationUnsetCase, TestCase::Duration::QUICK);
    AddTestCase(new QStateRegistryUpdateLocationCase, TestCase::Duration::QUICK);

    // Inspection
    AddTestCase(new QStateRegistryQubitsOfCase, TestCase::Duration::QUICK);

    // Edge cases
    AddTestCase(new QStateRegistryCreateZeroQubitCase, TestCase::Duration::QUICK);
    AddTestCase(new QStateRegistryResolveInvalidIdCase, TestCase::Duration::QUICK);
    AddTestCase(new QStateRegistryRegisterTwiceCase, TestCase::Duration::QUICK);
    AddTestCase(new QStateRegistryUnregisterNeverRegisteredCase, TestCase::Duration::QUICK);
    AddTestCase(new QStateRegistryUnregisterTwiceCase, TestCase::Duration::QUICK);
    AddTestCase(new QStateRegistryRemoveStateTwiceCase, TestCase::Duration::QUICK);
    AddTestCase(new QStateRegistryRemoveNonExistentCase, TestCase::Duration::QUICK);
    AddTestCase(new QStateRegistryMergeSingleQubitCase, TestCase::Duration::QUICK);
    AddTestCase(new QStateRegistryMergeThreeStatesCase, TestCase::Duration::QUICK);
    AddTestCase(new QStateRegistryQubitsOfInvalidIdCase, TestCase::Duration::QUICK);
    AddTestCase(new QStateRegistryResolveUnregisteredQubitCase, TestCase::Duration::QUICK);

    // Null pointer tests (consolidated)
    AddTestCase(new QStateRegistryNullQubitOperationsCase, TestCase::Duration::QUICK);
    AddTestCase(new QStateRegistryNullQubitQueriesCase, TestCase::Duration::QUICK);
    AddTestCase(new QStateRegistryMergeWithNullCase, TestCase::Duration::QUICK);
    AddTestCase(new QStateRegistryCreateFromNullCase, TestCase::Duration::QUICK);
  }
};

static Q2nsStateRegistryTestSuite
    g_q2nsStateRegistryTestSuite; ///< Static instance for auto-registration
