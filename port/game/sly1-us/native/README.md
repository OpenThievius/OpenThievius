<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Native port backfills

This directory contains port-only host implementations for functions that
remain unavailable to the native build. They are separate from
`sly1-decomp/src`, do not affect its matching progress, and must be removed
when hostable matching source becomes available. The source decomp checkout is
never edited by the host staging adapter.

## Current build state

Last checked against `sly1-decomp` commit `23bf912b` (PR #108), whose NTSC-U
report is 4,736/4,807 matched functions and 91.26% matched code bytes. The
source tree has 41 commits beyond the previous `330d67e1` snapshot, including
seven newly exact functions. The direct host32 build compiles 168/171 source
units and 55 active native backfill units. It resolves 4,309 catalogued
function addresses and defines 809/828 functions in the direct-call frontier.
The three source-unit failures are the exception/RTTI runtime units under the
current `-fno-rtti` flags.

The old port-only `func_00162890` reader stored a serialized mesh index where
the renderer expected a `StripMesh*`. It is now archived under
`hand/superseded_backfills/`, and the direct build selects the upstream matched
`ReadPlacedObjectModel`. Startup passes the attachment DMA chain and advances
into state setup. It now faults when `func_001DBBD8` receives a null world while
registering an animation-player callback because the EE source carries its
world argument implicitly. The host staging layer now passes it explicitly,
which advances startup to a new invalid `ScriptValue` dispatch in
`func_0011B060`. The edge animation, owner, player, and animation world are
valid pointers. No game graphics appear. See
[`PLAYABLE_TODO.md`](../../../../docs/port/PLAYABLE_TODO.md) for the current
trace and next diagnosis.

The ordinary hostgen/SDL path has not been adapted to this source rework. Its
current probe faults in callback dispatch at `0xF108`. The configured
`build/port/native-resource` cache also points to the older
`/home/reaper/Code/sly1-decomp` checkout (`e1b39f1`).

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
`build/port/canonical-23bf912-host32/`. It is an experimental source
compatibility path, not the SDL graphics frontend.
