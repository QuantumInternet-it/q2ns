#pragma once

/**
 * @mainpage Q2NS
 * <div class="q2ns-badges">
 *  <a href="https://www.gnu.org/licenses/old-licenses/gpl-2.0.txt"><img
 * src="https://img.shields.io/badge/license-GPL--2.0--only-blue.svg?style=flat-square">
 * </a>
 * <a href="https://doi.org/10.5281/zenodo.19370945"><img
 * src="https://zenodo.org/badge/DOI/10.5281/zenodo.19370945.svg" alt="DOI"></a>
 *  <a href="https://www.nsnam.org/"><img
 * src="https://img.shields.io/badge/ns--3-3.47-brightgreen?style=flat-square"></a>
 *  <a href="https://quantuminternet-it.github.io/q2ns/"><img
 * src="https://img.shields.io/badge/documentation-blue?style=flat-square&logo=read%20the%20docs"></a>
 * </div>
 *
 * Q2NS is a modular framework for quantum network simulation built on top of
 * ns-3. It adds quantum-state backends, qubits, quantum channels, channel
 * noise models, and node-level quantum operations while remaining compatible
 * with ns-3's event-driven simulation model.
 *
 * @tableofcontents
 *
 * @section q2ns_main_groups Topics
 * The Q2NS codebase can be navigated by topic as given below:
 * - @ref q2ns_api
 * - @ref q2ns_qstate
 * - @ref q2ns_network
 * - @ref q2ns_qmap
 * - @ref q2ns_core
 *
 *
 * @section q2ns_architecture Architecture
 *
 * Q2NS is organized around a small set of core roles.
 *
 * - NetController builds and configures the network.
 * - QNode is the main user-facing per-node API.
 * - QProcessor is an internal helper for local operations.
 * - QNetworker is an internal helper for transmission and reception.
 * - QChannel models a duplex quantum link.
 * - QMap models transmission-induced effects.
 * - QStateRegistry is the shared source of truth for backend states.
 * - QState defines the backend-agnostic state interface.
 * - Analysis provides common quantum networking analysis functions
 * supported.
 *
 * @section q2ns_workflow Typical workflow
 * 1. Create a NetController.
 * 2. Create QNodes through the controller.
 * 3. Install links.
 * 4. Create and manipulate qubits through QNode.
 * 5. Run the ns-3 simulation.
 * 6. Inspect outcomes and compute analysis.
 *
 * @section q2ns_design_goals Design goals
 * - separation of concerns
 * - modularity and extensibility
 * - integration with ns-3
 * - multiple quantum-state backends
 * - reproducible randomness
 *
 * @section q2ns_further_reading Further reading
 *
 * @subsection q2ns_tutorials_section Tutorials
 * - @ref q2ns_tutorial_visualizer "Tutorial 0: The Visualizer"
 * - @ref q2ns_tutorial_first_simulation "Tutorial 1: Running Your First Quantum
 * Simulations"
 * - @ref q2ns_tutorial_qops "Tutorial 2: Quantum Maps"
 * - @ref q2ns_tutorial_classical_comms "Tutorial 3: Teleportation with
 * Classical Communication"
 * - @ref q2ns_tutorial_multipartite "Tutorial 4: Multipartite Entanglement and
 * Simulation Experiments"
 *
 * @subsection q2ns_design_docs_section Design and reference notes
 * - @ref q2ns_architecture_doc "Architecture"
 * - @ref q2ns_randomness_doc "Randomness"
 *
 *
 * @defgroup q2ns_api Public API
 * @brief Main user-facing entry points for building and using Q2NS.
 */

/**
 * @defgroup q2ns_qstate Quantum State System
 * @brief Backend-agnostic and backend-specific quantum state abstractions.
 */

/**
 * @defgroup q2ns_network Quantum Networking Internals
 * @brief Internal components for processing, transmission, routing, and channel
 * delivery.
 */

/**
 * @defgroup q2ns_qmap Channel Maps
 * @brief Per-transmission channel models and received-qubit transformations.
 */

/**
 * @defgroup q2ns_core Core Types and Gates
 * @brief Shared enums, ids, locations, matrices, and gate helpers used across
 * Q2NS.
 */