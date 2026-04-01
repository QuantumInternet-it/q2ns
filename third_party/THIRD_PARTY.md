# Third-Party Software

This repository vendors selected third-party libraries under `third_party/` to ensure reproducible builds and avoid dependency drift.

Each dependency retains its original license. No third-party licenses have been modified.

Where local modifications have been made, they are explicitly documented below.

---

## Eigen

- **Project:** Eigen
- **Source:** https://gitlab.com/libeigen/eigen
- **License:** MPL-2.0
- **Version:** 5.0.0 (released September 30, 2025)
- **Local path:** `third_party/eigen/`
- **Modifications:** None

This is an unmodified vendored copy of Eigen at a known working version. It is included to ensure reproducibility and stability of linear algebra operations used within the simulator.

---

## qpp

- **Project:** qpp (Quantum++)
- **Source:** https://github.com/softwareQinc/qpp
- **Original author:** softwareQ
- **License:** MIT
- **Version:** 6.0 (released April 14, 2025)
- **Local path:** `third_party/qpp/`
- **Modifications:** None

This is an unmodified vendored copy of qpp at a known working version. It is used as a general-purpose quantum computing backend for state representation and manipulation.

---

## qasmtools

- **Project:** qasmtools
- **Source:** https://github.com/softwareQinc/qasmtools
- **Original author:** softwareQ
- **License:** MIT
- **Version:** Unknown (vendored snapshot from upstream repository, late 2025)
- **Notes:** No known functional modifications; differences from upstream are expected to be minimal (e.g., documentation updates).
- **Local path:** `third_party/qasmtools/`
- **Modifications:** None

This is an unmodified vendored copy of qasmtools at a known working version. It is included to support parsing and handling of OpenQASM representations.

---

## stab

- **Project:** stab
- **Source:** https://github.com/softwareQinc/stab
- **Original author:** softwareQ
- **License:** MIT
- **Version:** Unknown (vendored snapshot from upstream repository prior to local modifications, January 2026)
- **Local path:** `third_party/stab/`
- **Modifications:** Yes (see below)

### Vendored files

The following files are vendored and have been modified:

- `AffineState.h`
- `AffineState.cpp`
- `random.h`
- `random.cpp`

### Summary

These files are derived from the stab library, which implements Clifford simulation using the quadratic form expansion (QFE) method described in: N. de Beaudrap and S. Herbert, [*Fast Stabiliser Simulation with Quadratic Form Expansions*](https://quantum-journal.org/papers/q-2022-09-15-803/), Quantum 6, 803 (2022)

### Local modifications

The vendored stab files in this repository have been modified for integration into Q2NS. The main changes include:

- Added `AffineState::DropLastQubit()`
- Added `AffineState::TensorProduct(const AffineState&, const AffineState&)`
- Updated optional qpp include path from `<qpp/qpp.h>` to `<qpp/qpp.hpp>`
- Replaced the original stab random-number implementation with a Q2NS-specific version supporting:
  - explicit seeding via `Seed(uint64_t)`
  - deterministic reseeding behavior
  - integration with ns-3 logging
- Added stricter argument validation in random helper functions
- Minor formatting and maintenance edits

### License note

stab is licensed under the MIT License. A copy of the MIT license is included in `third_party/stab/LICENSE`.

The original copyright notices and license terms have been preserved. Modifications are documented above and are distributed in compliance with the MIT license.
