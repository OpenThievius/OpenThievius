<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Native port backfills

This directory contains port-only host implementations for functions that
remain unavailable to the native build. They are separate from
`sly1-decomp/src`, do not affect its matching progress, and must be removed
when hostable matching source becomes available. The source decomp checkout is
never edited by the host staging adapter.

## Current build state

Canonical `sly1-decomp` master is `86682ddd` (Wave 32). The sibling checkout
at `/home/reaper/sly/sly1-decomp` remains read-only at `bfc74817`; the build
uses a separate Wave 32 source snapshot and the older US assembly snapshot
because current upstream no longer ships that tree. The Wave 32 host32 build
compiles all 190 source units and 61 port-only backfills. Its combined object
contains 4,344 catalogued function entries. The direct frame-call scan finds
814 of 822 definitions and eight unresolved addresses; this scan does not
follow indirect callbacks. The freestanding i386 launcher links, routing
unresolved functions to diagnostic traps.

This is a no-emulation compile/link diagnostic, not a playable graphics
frontend. It has not been run against the disc. The separate experimental
guest-runtime launcher displayed the retail unformatted-memory-card prompt,
but it is not evidence of native gameplay and no format action was taken.
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
`build/port/host32/upstream-86682ddd/`. It is an experimental source
compatibility path, not a playable frontend.
