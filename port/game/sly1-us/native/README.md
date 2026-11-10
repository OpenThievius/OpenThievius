<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Native port backfills

This directory contains port-only host implementations for functions that
remain unavailable to the native build. They are separate from
`sly1-decomp/src`, do not affect its matching progress, and must be removed
when hostable matching source becomes available. The source decomp checkout is
never edited by the host staging adapter.

## Current build state

The public `OpenThievius/sly1-decomp` master is at `2e00ac70` (PR #109,
2026-11-06); the local read-only checkout at `/home/reaper/Code/sly1-decomp`
is clean but still at `e1b39f1`. The current SDL hostgen build uses the
reworked snapshot and translates 2,043 C functions plus 234 selected US
assembly functions, with no generated stubs. It builds with
`cmake --build build/port --target port_native -j2`.

The current startup run now queues and loads the initial world, registers four
fonts, and submits 240 raw vertices in some frames. It remains visually black:
there are no recorded GIF image tags or image qwords, no blit writes, and no
visible text. `func_001E4E80` (`QueueWorldLoad`) and the screen image record
readers `func_001B2BA8` / `func_001B2CF8` are active port-only backfills under
`hand/native_c/`, registered in `hand/backfills.tsv`; their behavior must be
replaced by hostable decomp source when available. The latest trace is
`/tmp/sly-vif-dma-debug.log`. Exact US assembly is selected for the reached
image-mask uploader and related screen packet builders, but the trace still
shows no GIF image tags. See
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
