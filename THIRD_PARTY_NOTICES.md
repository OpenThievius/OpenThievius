# Third-party notices

OpenThievius includes credited work from OpenRAC and the maintainer-approved
SlyC00per/sly1-nx project, as scoped in
[docs/policy/SOURCING.md](docs/policy/SOURCING.md). The free tools it runs are
installed by each contributor and are not distributed with it, and the
compiler is proprietary and never part of it ([toolchains/README.md](toolchains/README.md)).

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
| `port/platform/window.cpp`, `port/platform/window.h`, `port/platform/gpu_driver.cpp`, `port/platform/gpu_driver.h`, `port/platform/display_hdr.h` | same paths at `eefdcd1243a1c9a9fe8ca1e58bf4a5c03dd0a204` | imported OpenRAC's SDL/OpenGL window lifecycle and desktop driver interface; Sly launcher adapts its own lifecycle and event handling; upstream GPL notices retained |
| `port/renderer/direct.*`, `framebuffer.*`, `gl.*`, `gl_warmup.*`, `program_cache.*`, `render_opt.*`, `shader.*`, `texture.*`, `texture_pack.*`, `texture_pool.*`, `port/renderer/shaders/{direct.vert,direct.frag,upscaled_texture_colours.glsl}` | same renderer paths at `6786a882d6b385458e05dc732af3c1bcd51e7437` | reused OpenRAC's generic GS direct renderer, OpenGL/shader support and texture conversion for Sly VIF DIRECT/GIF packets; Sly-specific DMA and window adapters are separate; upstream GPL notices retained |
| `port/runtime/kernel.cpp` | `tools/interpreter/src/sys/machine.cpp` | boot subset of the documented EE syscall model (thread setup, heap setup, and single-thread exit) |
| `port/runtime/vu1_native.cpp` | `tools/interpreter/src/ps2/vu.cpp` at `6786a882d6b385458e05dc732af3c1bcd51e7437` | adapts VU instruction semantics including lower-special opcode indexing, ILW/ISW and LQI/SQI/LQD/SQD operands and lane behavior, OPMULA, ERSQRT, WAITP and the documented XGKICK delay; native GIF validation remains strict |
| `baserom/README.md`, `toolchains/README.md`, `.gitignore` | the same files | this project's discs, compiler and paths |
| `.github/workflows/checks.yml` | rac3-decomp's `checks.yml` | this repository's checks |

## From sly1-nx

The project is licensed AGPL-3.0-or-later. Its maintainer has approved use in
OpenThievius. The review snapshot is sly1-nx
`2a7524a47c6fdbf1584469c7d33aaa7731bed17c`, which pins sly1-decomp
`91de6fc6364ea13b3cbe5d530e8857272d23aada`. The detailed reference and
adaptation plan are in
[`docs/port/SLY1_NX_REFERENCE.md`](docs/port/SLY1_NX_REFERENCE.md). Add each
adapted code module here with its source path, revision and changes when code
is imported; retain its original SPDX notice.
