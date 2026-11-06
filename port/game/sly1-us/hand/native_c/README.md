<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Port-only C backfills

These C files provide functional replacements for Sly functions whose
matching C is not available to the native host build. They belong to the
port, not `sly1-decomp`, and do not count as matching decompilation progress.

`func_00144008`, `func_001EEBB0`, and `func_001389D0` are port-only scalar
translations of the retail camera basis update, tagged direction-list query,
and two-axis endpoint sorter. They are listed in `backfills.tsv` so the SDL
launcher uses them; same-address C++ versions under `native/` only serve the
freestanding host32 diagnostic and do not feed the SDL build. Remove both
port-side versions when matching decompilation code becomes available.

`collision_contact_tables.c` provides contact-pair flags, row-pool operations,
the collider compatibility check, and retry-counter reset after a collider is
loaded. These are port-only replacements for missing matching host C and can
be removed as those functions become available.

`func_0015EFE8` drains a completed frame's three intrusive lists and returns
their nodes to their owning objects. `func_00152600` is the corresponding
list unlink primitive. These keep the SDL frame-context cleanup separate from
the unfinished matching source; the display loop still needs its missing
active object callback and scene drawing path.

`port/tools/sly1.py hostgen` copies this directory into its generated source
view under `build/port/`. When matching C becomes available, remove the
corresponding backfill and let the matching source take precedence. Keep any
port-specific changes here so the decompilation can later replace them cleanly.

These replacements cover the persistent frame loop, frame-state dispatcher,
state-transition helper, frame callback setup, queued display worker, loader
request setup and worker, resource key and visual-state selection, resource
owner lists, descriptor setup, aligned pools and listener records, executable
checksum guard, packet-list finalization, display packet primitives, and
initial display-environment packet. The loader backfills reproduce the retail
request-list sizes, CRC table and semaphore ordering.
Native services create the loader semaphores and start its guest thread. The
loader thread now runs and blocks on its request semaphore, while the display
worker submits initialization GIF chains. The renderer still receives no
visible scene geometry, and several called engine functions remain unresolved
no-ops; this is startup progress, not a playable game.

`func_00154EE0` is a separate port-only override for a cross-file type
disagreement: its local source declares a one-word record return while the
canonical host prototype is a 32-bit scalar. The backfill stores the returned
word into the same four-byte item slots. Remove it when the matching sources
agree on the callable type.

`func_001A9F78` runs the derived constructors for the 37 global screen
elements after screen resources load. The decomp checkout has the matching
loop in `001A8000.cpp`, but its US function status is still `no-c`, so hostgen
emits a no-op. This backfill is kept separate and should be removed once the
matching C is included in the host build. It fixes the pause menu's
zero-initialized page, but startup still requests page 0 (`Paused`) at about
five seconds; the missing title/scene startup path is the current blocker.

`func_00124050` walks placed-object children during activation, and
`func_001240F8` registers active objects with the world update lists. Their
port-only replacements raised the direct-load probe's world `+0x54` and
`+0x60` lists from empty to 77 entries each. `func_0019D410` appends
retail-sized deferred draw requests. A native trace shows those requests are
still absent because every sampled update entry calls the substantial missing
`func_00125C90` camera/object-transform and culling routine. The retail
deferred dispatcher and request bucketing functions `func_0019DC98` and
`func_0019D4B0` now have separate port-only replacements, along with a
retail-derived `func_00184DE8` ConeVolume/VU backfill. That function survives a
bounded frame run, but its only current class-0x51 ConeVolume has mode/value
zero and produces no VIF vectors. The substantial missing `func_00125C90`
callback still prevents the update path from populating its transform outputs.
Remove each replacement when matching C is available to the host build.
