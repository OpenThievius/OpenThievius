<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Native port backfills

This directory contains port-only host implementations for functions that
remain unavailable to the native build. They are separate from
`sly1-decomp/src`, do not affect its matching progress, and must be removed
when hostable matching source becomes available. The source decomp checkout is
never edited by the host staging adapter.

## Current build state (2026-11-14)

The sibling decomp checkout is up to date at
`1fdd36d70c89730bc9e155e0bbd8638fca164f74` (public master, 2026-11-13). Its US
report matches 4,807/4,807 functions and 100% of code, plus 99.9675% of data.
The native host32 build compiles 194 source units and 64 port-only backfills.
The combined object defines 4,380 catalogued function entries; the direct
call frontier resolves 822/830 functions, without following indirect calls.

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
Wave 49 compiles and links this fix; Cross/Space still needs a live runtime
check before menu progress is confirmed.
Wave 50 adds a port-only `func_0013A498` CPU backfill for the particle-filter
VU0 call site. It uses the matched caller's expiry test, compaction, bounds,
and spin-angle behavior, and applies CPU-side matrix/velocity integration
based on the sibling effect path. The VU0 microprogram's precise arithmetic
is undocumented, so this is explicitly an approximation and is not a
decompilation claim. The staged host source adds a declaration for the
backfill after renaming the matched implementation; neither the decomp
checkout nor its match score changes. The other porting-guide VU0 site,
`CollectLightsForSphere`, still has host no-op behavior. Re-run visual/startup
checks before treating the particle approximation as correct.

Wave 51 removes the port overrides for `SuppressScriptEvent` and `InitGauge`;
both now compile from the matched US C in the decomp. This keeps newly recovered
game logic authoritative and leaves platform-specific adapters separate.

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
