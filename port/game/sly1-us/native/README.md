<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Native port backfills

This directory contains port-only host implementations for functions that
remain unavailable to the native build. They are separate from
`sly1-decomp/src`, do not affect its matching progress, and must be removed
when hostable matching source becomes available. The source decomp checkout is
never edited by the host staging adapter.

## Current build state

Last checked against `sly1-decomp` commit `330d67e1` (PR #107), whose NTSC-U
report is 4,729/4,807 matched functions and 90.36% matched code bytes. The
direct host32 build compiles 168/171 source units and 55 active native
backfill units. It resolves 4,301 catalogued function addresses. A bounded
startup run gets past camera alignment, packet-header, and color-conversion
paths, then faults in the pose-curve path at `func_001303F0`; it displays no
graphics. See [`PLAYABLE_TODO.md`](../../../../docs/port/PLAYABLE_TODO.md) for
the live diagnosis and logs.

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
four-word arrays and makes selected EE argument-register carryover explicit.

`func_001F5B08.cpp` implements overlap-safe `memmove` for a still-assembly
libc function used during camera-controller stack insertion. The other active
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
`build/port/canonical-330d67e-aligned-host32/`. It is an experimental source
compatibility path, not the SDL graphics frontend.
