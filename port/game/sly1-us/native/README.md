<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Native port backfills

This directory contains port-only host implementations for functions that
remain unavailable to the native build. They are separate from
`sly1-decomp/src`, do not affect its matching progress, and must be removed
when hostable matching source becomes available. The source decomp checkout is
never edited by the host staging adapter.

## Current build state

Last checked against `sly1-decomp` commit `2e00ac70` (PR #109), whose NTSC-U
report is 4,741/4,807 matched functions, 91.66% matched code bytes, and 93.71%
matched data bytes. Since the previous port snapshot `23bf912b`, upstream has
66 commits and five more exact US functions. The direct host32 build compiles
168/171 source units and all 57 active native backfill units. It maps 4,320
catalogued function addresses and defines 815/829 functions in the direct-call
frontier. The three source-unit failures are the exception/RTTI runtime under
the current `-fno-rtti` flags.

The old port-only `func_00162890` reader is archived under
`hand/superseded_backfills/`; the direct build uses upstream's matched
`ReadPlacedObjectModel`. Host staging passes the implicit EE world argument to
the callback allocator and replaces an undefined cross-object script cleanup
loop with a defined reverse array walk. Startup now reaches packet submission.
The previous crash in `func_001613C0` was caused by an uninitialized GIF packet
writer. The port-only `func_001FAD68` backfill now calls the retail
`ResetPacketWriters` callback in both host32 and SDL paths. SDL opens its window
and advances into repeated frames, but faults in
`game_sly1_native_call_regs4` during thunk-table lookup. Bounded tracing
resolves repeated guest methods to valid host functions before the lookup
fault. The trace still shows no
vertices, texture uploads, or framebuffer output. The diagnostic remains black
and is not playable. See
[`PLAYABLE_TODO.md`](../../../../docs/port/PLAYABLE_TODO.md) for the current
trace and next diagnosis.

The SDL hostgen path is built from the same `2e00ac70` snapshot. CMake now
directs hostgen output to its configured build directory. The last run is
recorded at `/tmp/sly-2e00ac70-sdl-packetinit-run.log`.

Build counts measure host compatibility, not decomp coverage. The port is not
playable. The separate freestanding host32 diagnostic does not provide a
visible presentation path.
## Backfill policy

Keep behavior replacements here or under `hand/native_c/`, never in the
decompilation checkout. Keep host ABI and linker adaptations in the staged
copy generator so matched code and its matching score remain unchanged. The
current host staging compatibility work retains 16-byte alignment for
four-word arrays, makes selected EE argument-register carryover explicit, and
normalizes animation callbacks to their vtable signatures.

`func_001F5B08.cpp` implements overlap-safe `memmove` for a still-assembly
libc function used during camera-controller stack insertion.
`func_00183668.cpp` is a separate behavior backfill for the still-unmatched
script-event filter; retire it once matching C is hostable. The other active
files provide platform boundaries (CD reads, Linux threads/semaphores, timer
and DMA services), still-missing retail routines, or host translations of
handwritten EE/VU0 code. Keep each replacement narrow and document its retail
address and evidence in the source.

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
`build/port/canonical-2e00ac70-host32/`. It is an experimental source
compatibility path, not the SDL graphics frontend.
