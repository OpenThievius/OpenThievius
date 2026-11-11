<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Native port backfills

This directory contains port-only host implementations for functions that
remain unavailable to the native build. They are separate from
`sly1-decomp/src`, do not affect its matching progress, and must be removed
when hostable matching source becomes available. The source decomp checkout is
never edited by the host staging adapter.

## Current build state

The canonical `OpenThievius/sly1-decomp` master is at `d56e797c` (Wave 23,
PR #116; checked 2026-11-11). It remains at 4,760/4,807 US functions and
93.25% matched code; Wave 23 adds no matched functions. The native build uses
this checkout for C and `/home/reaper/Code/sly1-decomp` only for selected US
retail assembly. The isolated hostgen build translates 732 functions plus
340 selected assembly functions, with 2 generated stubs, across 269 C units.
Build: `cmake --build build/port --target port_native -j2`.

The current Splash trace resolves the CountdownNotifier's child owner through
the port-only `0x0015A838` wrapper and `0x0015A4F0` search backfills. The
separate `0x0015A198` override now handles ID-hash searches, and `0x0015A418`
uses it as the canonical `FindFirstObjectById` wrapper. Splash still has no
object with ID `0x25f`, leaving the notifier's graph pointer null. The
`0x00147AB0` backfill transitions it from state -1 to 0 and requests event
`0x261`, but `CreateStateWalker` (`0x001b6290`) has no host C. The launcher
still renders no vertices or visible text, with no GIF image tags or DMA
submissions; the window stays black. Trace:
`/tmp/sly-canonical-f36f43ee-mode-lookup-next.log`. All behavior replacements
remain isolated from `sly1-decomp`. See
[`PLAYABLE_TODO.md`](../../../../docs/port/PLAYABLE_TODO.md) for the current
blocker and next trace target.

These build counts measure host compatibility, not decomp coverage. The port
is not playable. The direct host32 diagnostic is separate from the SDL graphics
frontend.

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
