<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Native port backfills

This directory contains functional replacements for the host-native C++ build
only. These files are separate from `sly1-decomp/src`, are not matching
sources, and never affect decompilation progress. The native host32 build
compiles them into `build/port/host32/objects/port_native/` and maps their
function names to retail addresses alongside compiled decomp functions. The
Last checked: 2026-11-10. The configured sibling
[`sly1-decomp`](https://github.com/OpenThievius/sly1-decomp) is at `d534006`
(PR #102); its NTSC-U report has C for 4,667/4,807 functions (85.76% code
bytes). The current port hostgen can read this reworked C++ source, but its
staging pass cannot yet lower many class and template constructs: a C-only
run read 238 units, translated 563 functions, stubbed 2, and left 4,323
without C. These are hostgen compatibility counts, not decomp coverage. The
active `build/port` CMake cache still points to `/home/reaper/Code/sly1-decomp`
at `e1b39f1`, where the build reports 2,044 C and 230 assembly translations
with zero stubs. The current source is not wired into the native launcher yet.
The port is not playable; see `docs/port/PLAYABLE_TODO.md` for the current
graphics and startup state. This does not describe the separate freestanding
host32 build.

The host32 launcher and its EE/device behavior are an experimental startup
diagnostic, not the target port architecture. The build counts and traces in
the sections below are historical results from earlier decomp snapshots and
must not be treated as current coverage. For the live native C-only path,
current black-screen diagnosis, and tasks that need contributors, see
[`SLY_GRAPHICS_HANDOFF.md`](../../docs/port/SLY_GRAPHICS_HANDOFF.md) and
[`ROADMAP.md`](../../docs/port/ROADMAP.md).

Channel 2 sends DMA-tagged chains to the DMA-chain walker; sampled chains are
accepted. Current traces show the 512x512 indexed font atlas and CLUT uploads,
two decoded draws per frame, and changing glyph positions. The window remains
black or garbled; indexed texture/CLUT conversion, UVs, blend state, and shader
output are the current graphics investigation. See the graphics handoff for
reproduction commands and next steps. Port-only
functional backfills stay separate from matching decomp C/C++; when matching
definitions become available and are hostable, remove the corresponding
backfill.

Run it with the extracted NTSC-U ELF and a cooked ISO or raw BIN:

```sh
build/port/host32/launcher/sly1_host32 <SCUS_971.98> <Sly 1 ISO or BIN>
```

Remove a backfill as soon as matching C/C++ for that address is available;
the combine step rejects an address defined by both trees. Keep these
replacements behavior-focused and derive their state changes from Sly 1's own
retail executable.

`linux_semaphores.cpp` supplies Linux i386 host services for Sly's retail
`CreateSema`, `DeleteSema`, `SignalSema`, `iSignalSema`, `WaitSema`, `PollSema`
and `iPollSema` wrappers. It uses host futexes and does not run PS2 instructions.
The semaphore boundary can support native host threads, but the freestanding
host32 diagnostic path still lacks a graphics setup service and a visible
presentation path.

`sly1_native_cd.cpp` replaces the retail sector-read call at the native
boundary. The launcher opens the user's ISO/BIN with Linux large
file support, detects cooked 2048-byte or raw 2352-byte sectors, and reads
logical sector payloads directly into the mapped Sly address space. Reads are
synchronous, so the wait service reports no outstanding request. This makes
the CD boundary available to native loader code; it does not implement Sly's
stream refill worker or prove that a level has loaded.

`func_001430F8.cpp` replaces the reachable CP0 Count read with a monotonic host
counter scaled to the NTSC-U 294.912 MHz tick rate used by Sly's timer code.
It is a port-only platform service; the matching source remains unchanged.

`func_00128018.cpp` replaces the common affine coordinate helper with native
float operations. It preserves the retail copy cases and computes the three
local-space coordinates from the transform basis and translation. The retail
routine also leaves intermediate values in VU0 registers; the native backfill
does not model those registers, and carries the input homogeneous lane through
the output. This is port-only math for host-native callers, not matching source.

`func_001EACA0.cpp` and `func_001BC260.cpp` implement the quadratic-root and
coordinate-normalization steps used by Sly's movement update. They use native
single-precision math and do not depend on the guest VU state.

`func_001F6C20.cpp` preserves Sly's DMA reset call as a native no-op because
the port completes DMA submissions synchronously and has no pending hardware
queue to reset. The previous-frame frontier showed this wrapper as a missing
direct call; the host32 map now resolves it.

`func_001388A0.cpp` removes a C03 body's contact-index entries from the three
fixed-size banks. It is a port-only implementation based on the matching
`func_00138828` layout and remains separate so the decompilation can replace it.
The current decomp checkout has an 88.7% near miss for this address in
`nonmatching/func_001388A0.cpp`; it is not yet a matching definition.

`func_00185D20.cpp` and `func_001860A8.cpp` prepare and dispatch Sly's
0x100-byte object draw entries without executing VU instructions.
`func_00185D20` uses the existing native affine transform backfill;
`func_001278A8.cpp` and `func_001280C0.cpp` provide the object transform walk
and relative-vector math with host scalar operations. The indirect callbacks
still need an adapter to the reused OpenRAC renderer or a Sly world-draw path.

`func_001B5F50.cpp` parses scene reference and entry records, consumes property
values without running their callbacks, and constructs nested child objects
through the matching factory. Its scene data shape follows the retail EE
assembly; missing property callbacks remain port work.

`func_0011FC90.cpp` consumes position-track data through the matching reader
and currently defaults its result to false. `func_0011F6B0.cpp` initializes
optional track output buffers but does not evaluate the track curves. These
are explicit startup fallbacks; they need full math translations or matching
decompilation before position-driven gameplay can work.

`func_00189150.cpp` translates the three-vector transform previously executed
by the EE VU0 macro into scalar host math. The host staging pass also adjusts
the `func_001EAD88` definition to the angle-first calling convention used by
its native callers; neither change modifies the decompilation checkout.

`func_00188D28.cpp` supplies the next matrix-to-axis-angle step using
scalar host math. Both transform functions remain port-only backfills until
matching decompilation code replaces them.

`func_001662A8.cpp` translates the scene mesh's two-pass VIF/GIF packet builder
from the NTSC-U retail routine. `func_00158288.cpp` translates its shared
relationship update and VU0 reciprocal-distance calculation with scalar host
math. Both remain port-only replacements, separate from matching sources.
`func_0011ECB8.cpp` reads nested scene object records. `func_0011E580.cpp`
implements the bounded 149-entry object factory through `D_00247448`, so
invalid class IDs return null instead of indexing outside the descriptor
table. `func_001686E8.cpp` submits VIF1 display packets through the synchronous
native DMA boundary and skips the retail interrupt wait. These backfills export
the prototype aliases used by native callers so the port implementations are
selected instead of stale compiled bodies. Remove them when matching functions
become available.

The scene-object path also has port-only replacements for the EE transform
helpers `func_00128150` and `func_00188520`, object initialization and parent
updates (`func_00124868`, `func_001245A8`), attachment transforms
(`func_00128FD0`), and the vector thresholds `func_00123DC0` and
`func_00123E00`. `func_0011ECB8.cpp` and `func_0011E580.cpp` export aliases for
the decompilation's alternate prototypes, so the null-safe reader and bounded
factory replace their stale compiled bodies. `func_001B4330.cpp` adds a
port-only HSV-to-RGB conversion and an explicit host wrapper for
`func_001B4580`. `func_001DCEA0.cpp` also exports the retail callback's
no-argument prototype alias; its child callback skips entries whose descriptor
is outside the mapped retail ELF. `func_001848B8.cpp` and `func_00144008.cpp`
translate the next two VU0 camera-vector routines with scalar host math.
`func_001EEBB0.cpp` translates the tagged direction-list query, including its
null-state `-1` result and threshold checks.
`func_001389D0.cpp` sorts the two populated endpoint banks and updates their
crossings through the existing native collision-list helper.

A freestanding host32 trace passes nested resource records, camera setup and
both endpoint-sort passes. It then runs for 15 seconds without another missing
function trap. This diagnostic does not display frames, so visible rendering
and gameplay remain unverified; the port is not yet playable.
