# Runtime

`runtime` contains the launcher-facing code that connects translated Sly code
to host memory, startup, input, presentation, and graphics services. Some
existing paths still model PlayStation hardware behavior for the experimental
launcher. That work is not the target gameplay architecture: the port's goal
is host-native services and a native renderer, as described in
[`docs/port/DESIGN.md`](../../docs/port/DESIGN.md).

## Main interfaces

| Header | Purpose |
|---|---|
| `include/openrac/guest.h` | Guest addresses, game-memory access, and translated-code entry points |
| `include/openrac/memory.h`, `include/openrac/elf.h` | Guest-memory reservation and executable loading |
| `include/openrac/ee.h`, `include/openrac/ee_ops.h` | Experimental EE compatibility support used by the current launcher |
| `include/openrac/native_window.h` | SDL window and frame-presentation boundary |
| `include/openrac/native_gs.h`, `native_vu1.h`, `native_dma.h`, `native_sif.h` | Experimental device-path interfaces; not the target native renderer |
| `include/openrac/host_env.h` | C access to the environment-variable table in `common/env.h` |
| `include/openrac/disc_image.h` | Disc-image access used by startup/resource loading |

Implementation files sit beside the headers. `sdl_renderer.cpp` contains the
current SDL/OpenGL presentation path. Native gameplay graphics should be
implemented there or in the renderer layer, not by expanding the VU/GS
compatibility path. The latest evidence and next graphics task are in the
[graphics handoff](../../docs/port/SLY_GRAPHICS_HANDOFF.md).

## Current limits

The current host32 launcher can reach Paris gameplay state and produce a
partial native OpenGL scene, but scene composition is badly distorted and the
game is not playable. The current status, evidence, and remaining milestones
are in the [port README](../README.md), [playable TODO](../../docs/port/PLAYABLE_TODO.md),
and [roadmap](../../docs/port/ROADMAP.md).

Port-only function replacements belong under `game/sly1-us/native/` or
`game/sly1-us/hand/native_c/`, outside the sibling `sly1-decomp` checkout.
Replace them with matching decompilation code when that code can be hosted.

The port does not currently define the unit-test targets described in older
versions of this page. Build targets and dependencies are declared in
[`../CMakeLists.txt`](../CMakeLists.txt).
