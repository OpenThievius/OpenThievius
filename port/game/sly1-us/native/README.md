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
388 selected assembly functions, with 2 generated stubs, across 269 C units.
Build: `cmake --build build/port --target port_native -j2`.

The current Splash trace resolves the CountdownNotifier's graph pointer to
StateGraph ID `0x25f` and creates its walker. The graph's animation player
advances, but remains on the `0x261` state-2 self-edge. The notifier stays in
state 0 with a null `+0x338` gate. A fresh trace found 22 reached visual/UI
functions running as empty stubs; they are now selected from exact US retail
assembly. The initialized font count rose from 35 to 37, but every screen
element remains mode 0 and the renderer reports zero vertices, visible text,
image tags, or GIF submissions. Trace:
`/tmp/sly-renderer-retail-asm-20s.log`. A second exact-assembly pass restores
11 reached UI state callbacks; those warnings are gone, but the next trace
still shows all 37 elements inactive and zero graphics output. Trace:
`/tmp/sly-ui-state-retail-asm-20s.log`. A 65-second run reached 32.15 guest
seconds but remained in app state 3 with no pad input, movie queue, world load,
or graphics. Log: `/tmp/sly-long-startup-65s.log`. All behavior replacements remain
isolated from `sly1-decomp`. See
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
