# Third-party notices

OpenThievius holds **no third-party code** apart from what comes from OpenRAC, the sister
project of the same maintainers ([docs/policy/SOURCING.md](docs/policy/SOURCING.md#openrac)).
The free tools it runs are installed by each contributor and are not distributed with it, and
the compiler is proprietary and never part of it ([toolchains/README.md](toolchains/README.md)).

## From OpenRAC

OpenRAC code is included only where its own notices permit it, with its
original SPDX license retained. The hostgen component, shared Python
helpers, common utilities and generic runtime below are GPL-3.0-or-later;
section 13 permits combining them with
this AGPL-3.0-or-later project. These files were imported from OpenRAC
checkout commit `ae2fed40a9f4caa2e252db09b8507dc262689905`.
Later imports list their specific OpenRAC revision in the corresponding row.

| Here | Adapted from OpenRAC's | What changed |
|---|---|---|
| `docs/policy/SOURCING.md` | `docs/policy/SOURCING.md` | stricter: nothing from other projects or the prototype; the OpenRAC exception; the proprietary scan |
| `docs/LAYOUT.md`, `games/README.md` | `docs/LAYOUT.md`, `games/README.md` | this repository's layout, with the decompilation as a sister checkout |
| `CONTRIBUTING.md`, `AGENTS.md` | `CONTRIBUTING.md`, rac3-decomp's `AGENTS.md` | the commit, pull request and assistant conventions kept; the rules rewritten |
| `LICENSE.md` | `LICENSE.md` | one license, the AGPL-3.0-or-later |
| `tools/openthievius.py` | `tools/openrac.py` (`discs`, `setup`, `tables`) | written anew for game.json's shape here, with `check` |
| `port/tools/hostgen/`, `tools/openrac_common/` | `port/tools/hostgen/`, `tools/openrac_common/` | imported for Sly host generation; upstream GPL notices retained |
| `port/common/`, `port/runtime/` | `port/common/`, `port/runtime/` | imported as the generic native runtime foundation; RAC-specific expanded memory constants removed from the guest header |
| `port/platform/audio.cpp`, `port/platform/audio.h`, `port/platform/audio_ring.h` | same paths at `6786a882d6b385458e05dc732af3c1bcd51e7437` | imported as game-agnostic SDL3 PCM output and callback buffering; upstream GPL notices retained |
| `port/platform/pad.cpp`, `port/platform/pad.h` | same paths at `6786a882d6b385458e05dc732af3c1bcd51e7437` | adapted from the generic DualShock 2 state and read-buffer serialization; RAC-specific stick shaping omitted; upstream GPL notices retained |
| `port/platform/clock.cpp`, `port/platform/clock.h` | same paths at `eefdcd1243a1c9a9fe8ca1e58bf4a5c03dd0a204` | imported OpenRAC's generic monotonic clock and exact-rate frame pacer; upstream GPL notices retained |
| `port/runtime/kernel.cpp` | `tools/interpreter/src/sys/machine.cpp` | boot subset of the documented EE syscall model (thread setup, heap setup, and single-thread exit) |
| `baserom/README.md`, `toolchains/README.md`, `.gitignore` | the same files | this project's discs, compiler and paths |
| `.github/workflows/checks.yml` | rac3-decomp's `checks.yml` | this repository's checks |
