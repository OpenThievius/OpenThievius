<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Native port backfills

This directory contains port-only host implementations for functions that
remain unavailable to the native build. They are separate from
`sly1-decomp/src`, do not affect its matching progress, and must be removed
when hostable matching source becomes available. The source decomp checkout is
never edited by the host staging adapter.

## Current build state (2026-11-14)

The sibling decomp checkout is up to date at
`b482e857d7000d881041d551eec3ea31fc78862f` (public master, 2026-11-13). Its US
report matches 4,807/4,807 functions and 100% of code, plus 99.9675% of data.
The native host32 build compiles 194 source units and 65 port-only backfills.
The combined object defines 4,380 catalogued function entries; the direct
call frontier resolves 822/830 functions, without following indirect calls.

Wave 41 links the SDL launcher, but its latest bounded run faults at address
`0x556` before graphics. The staging adapter for `game_progress.cpp` now gives
the key lookup an explicit host parameter after the decomp source reorganization;
this is a port-side ABI adaptation and does not modify matching source. The
last confirmed visual milestone is the complete native no-card warning panel
and highlighted Continue option from Wave 40. No title or playable scene has
been demonstrated. No card-format action was taken. See the roadmap and
restart checkpoint for evidence and current blockers.
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
`build/port/host32/upstream-647da5d6/`. It is an experimental source
compatibility path, not a playable frontend.
