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
