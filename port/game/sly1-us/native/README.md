<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Native port backfills

This directory contains port-only host implementations for functions that
remain unavailable to the native build. They are separate from
`sly1-decomp/src`, do not affect its matching progress, and must be removed
when hostable matching source becomes available. The source decomp checkout is
never edited by the host staging adapter.

## Current build state

Canonical `sly1-decomp` master is `647da5d6` (Wave 33). The sibling checkout
at `/home/reaper/sly/sly1-decomp` remains read-only at `bfc74817`; the build
uses a separate Wave 33 source snapshot and the older US assembly snapshot
because current upstream no longer ships that tree. The Wave 33 host32 build
compiles all 190 source units and 65 port-only backfills. Its combined object
contains 4,345 catalogued function entries. The direct frame-call scan finds
811 of 817 definitions and six unresolved addresses; this scan does not follow
indirect callbacks. The freestanding i386 and SDL launchers link, routing
unresolved functions to diagnostic traps.

This is a no-emulation host build, not a playable graphics frontend. The SDL
host32 launcher was run against the retail ELF and ISO on 2026-11-12. An
interior data alias initially resolved the pad field `D_00262D18` to absolute
address `0xa8`, causing a startup fault. The linker now defers data aliases
until final host data placement; `nm` confirms the field at `0x082fcee8`,
inside `D_00262C70`. Port-only no-card service replies let retail startup reach
pause page 3, and a hard-bounded 10-second run reached about 600 frames without
a fatal fault. Frame telemetry reports no visible UI/text, and no title or
scene has been demonstrated. No card-format action was taken. A previous run
also exposed invalid DMA channel value `0xe0`; the port boundary now avoids
dereferencing unknown channels, and host channel 2 is routed to the renderer.
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
