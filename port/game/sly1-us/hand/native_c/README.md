<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Port-only C backfills

These C files provide functional replacements for Sly functions whose
matching C is not available to the native host build. They belong to the
port, not `sly1-decomp`, and do not count as matching decompilation progress.

`port/tools/sly1.py hostgen` copies this directory into its generated source
view under `build/port/`. When matching C becomes available, remove the
corresponding backfill and let the matching source take precedence. Keep any
port-specific changes here so the decompilation can later replace them cleanly.

These replacements cover the persistent frame loop, frame-state dispatcher,
state-transition helper, frame callback setup and queued display worker. The
worker waits through the native `WaitSema` service and then calls Sly's
existing queue and graphics routines. The generated host build still reaches
many functions with no matching C; it warns and leaves those calls as no-ops.
This is port-only startup scaffolding, not a playable frame path.
