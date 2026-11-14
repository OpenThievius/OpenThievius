<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Native port backfills

This directory contains port-only host implementations for functions that
remain unavailable to the native build. They are separate from
`sly1-decomp/src`, do not affect its matching progress, and must be removed
when hostable matching source becomes available. The source decomp checkout is
never edited by the host staging adapter.

## Current build state (2026-11-14)

The sibling decomp checkout is up to date at
`83f2b50d586602f67e03e42343737a0dd9072a3e` (public master, 2026-11-14). Its US
report matches 4,807/4,807 functions and 100% of code (4,498 exact C and 311
original assembly), plus 99.9675% of data. Recent decomp commits name the five
world bosses and rename the corresponding source/header files; no new US
functions or code bytes were added by that cleanup. The native host32 build has
been rebuilt successfully against this snapshot.

The native host32 build compiles 194 Sly source units and 61 port-only
backfills. Its combined object contains 255 objects and defines 4,380
catalogued function entries; the direct call frontier resolves 826/834
functions, without following indirect calls.
The freestanding launcher now supplies Itanium C++ local-static guard
primitives required by the current matched source. The combiner preserves the
48-byte game clock block's internal data offsets and 16-byte section alignment.
A bounded SDL run reaches pause page 3 and records 427 frames and 1,290
accepted DMAs with zero rejections; it has not rechecked Continue after the
clock-layout fix. The next confirmed blocker is the thread syscall boundary.
The matched retail wrappers contain EE `syscall` instructions, while the
generic guest-kernel runtime expects guest-RAM parameter pointers. The SDL
build now stages those 11 veneers to separate port-only Linux thread and
semaphore services. They read native stack parameters, resolve the compiled
thread entry through the retail-to-host map, and start it on native stacks. A
bounded trace confirms render, movie, and background workers start; the
launcher remains at the no-card page. These services stay under `native/` and
must be replaced if equivalent native-hostable source is decompiled.

The packet writers `BeginDmaTag`, `AddDmaRefTag`, `AddDmaReturnTag`, and
`CloseDmaTag` now compile from matched decomp C. Their stale hostgen backfill
selections were removed; other packet helpers in
`hand/native_c/display_packets.c` remain port-owned.

Wave 47 passes the prior world-loader fault via a port-only staged-source
adaptation for `ReadBufferedU32`'s i386 return ABI. Register capture then found
that `g_elementStack` was allocated as four bytes even though the complete
decomp layout is 0x34 bytes, causing its count field at `+0x2C` to overlap
neighboring host BSS. The staging adapter now sizes the host definition from
`sizeof(ElementStack)`. Wave 48 runs over 3,100 frames without crashing and
reaches retail no-card page 3 with two stack entries. A fresh frame dump
confirms the full warning panel and selected Continue; no title or playable
scene has been demonstrated. No card-format action was taken. See the roadmap
and restart checkpoint for evidence and
current blockers.

The Sly libpad backfill `func_002042C8` now swaps the report's two button bytes
before retail `PollPadInput` reads them. This is a Sly-only adaptation: retail
forms `(buf[2] << 8) | buf[3]` and consumes Cross as `0x0040`, while the shared
OpenRAC SDL serializer emits the standard DualShock word high-byte first.
Wave 57 confirmed Space reaches the selected Continue row and dismisses the
warning, but that trace used the split clock layout. Retest that transition
after the Wave 58 clock-layout correction with pad tracing enabled; the later
one-shot Space attempt did not log whether input reached Sly.
Wave 50 adds a port-only `func_0013A498` CPU backfill for the particle-filter
VU0 call site. It uses the matched caller's expiry test, compaction, bounds,
and spin-angle behavior, and applies CPU-side matrix/velocity integration
based on the sibling effect path. The VU0 microprogram's precise arithmetic
is undocumented, so this is explicitly an approximation and is not a
decompilation claim. The staged host source adds a declaration for the
backfill after renaming the matched implementation; neither the decomp
checkout nor its match score changes.

Wave 55 adds a port-only `func_00185150` CPU implementation for
`CollectLightsForSphere`. It tests kind-1 point-light ranges and kind-2
six-plane volumes, then preserves retail group filtering, mask ordering, the
light cap and result fields. This replaces the VU0 no-op adapters on the direct
host32 path; the legacy C-only hostgen path still has no VU0 implementation.
Plane tests use scalar floating point, so exact VU0 boundary/rounding behavior
is not verified. The function compiles and links, but no runtime scene has
exercised it. Both VU0 replacements remain port-only and are not decompilation
claims.

Wave 51 removes the port overrides for `SuppressScriptEvent` and `InitGauge`;
both now compile from the matched US C in the decomp. This keeps newly recovered
game logic authoritative and leaves platform-specific adapters separate.
Wave 52 removes the old 99.7% near-match override for `ApplyPlacedObjectTransform`
(`func_00125C90`); the native build now uses its 100%-matched C from
`placed_object.cpp`. The no-input startup run still reaches the no-card warning
without an immediate fault.
Wave 53 also removes the port overrides for `ReadMeshTables`,
`UpdateCameraMatrices`, and `InitSpawnRecord`; all three now compile from
100%-matched US C. The direct-call frontier rises to 827/835, and startup
remains on the no-card warning without an immediate fault.

See [`PLAYABLE_TODO.md`](../../../../docs/port/PLAYABLE_TODO.md) and
[`RESTART_CHECKPOINT.md`](../../../../docs/port/RESTART_CHECKPOINT.md) for
current evidence and blockers. Backfills remain isolated from
`sly1-decomp` and must be retired when matching source becomes hostable.

## Backfill policy

Keep behavior replacements here or under `hand/native_c/`, never in the
decompilation checkout. Keep host ABI and linker adaptations in the staged
copy generator so matched code and its matching score remain unchanged. The
current host staging compatibility work retains 16-byte alignment for
four-word arrays, makes selected EE argument-register carryover explicit, and
normalizes animation callbacks to their vtable signatures.

`func_001F5B08.cpp` implements overlap-safe `memmove` for a still-assembly
libc function used during camera-controller stack insertion.
`SuppressScriptEvent` (`func_00183668`) and `InitGauge` (`func_001ABE70`) now
use matched C from the decomp; their obsolete port copies were removed in
Wave 51. Other active files provide platform boundaries (CD reads, Linux
threads/semaphores, timer and DMA services), still-missing retail routines, or
host translations of handwritten EE/VU0 code. Keep each replacement narrow
and document its retail address and evidence in the source.

Functions that acquired matched source in the rework were moved to
[`hand/superseded_backfills/`](../hand/superseded_backfills/). Those preserved
files are reference only and are not compiled by the direct host32 build.
Some legacy mappings in `hand/backfills.tsv` remain for the older configured
hostgen checkout; retire those mappings when that build is updated to the
current decomp.

## Running the diagnostic

Use an extracted NTSC-U ELF and a cooked ISO or raw BIN:

```sh
build/port/host32/launcher/sly1_host32 <SCUS_971.98> <Sly 1 ISO or BIN>
```

The current diagnostic build lives under
`build/port/host32/upstream-647da5d6/`. It is an experimental source
compatibility path, not a playable frontend.
