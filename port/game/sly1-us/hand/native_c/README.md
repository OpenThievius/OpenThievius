<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Port-only C backfills

These C files provide functional replacements for Sly functions whose
matching C is not available or not usable in the native host build. They
belong to the port, not `sly1-decomp`, and do not count as matching
decompilation progress. Functions replacing a matched definition must also be
listed in `../overrides.tsv`; otherwise hostgen keeps the decomp definition
when both are present. Entries in `backfills.tsv` cover functions supplied
where matching C is absent.

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

`func_0011E580` is a port-only object-factory replacement listed in
`overrides.tsv`. It reads the retail descriptor table and uses the native
constructor dispatcher. Remove its override row when matching C for this
function is hostable. Its invalid-class checks are not evidence that the
resource parser is correct; the current startup trace shows the input bytes
and cursor need investigation.

`func_00154EE0` is a separate port-only override for a cross-file type
disagreement: its local source declares a one-word record return while the
canonical host prototype is a 32-bit scalar. The backfill stores the returned
word into the same four-byte item slots. Remove it when the matching sources
agree on the callable type.

`native_heap.c` overrides the retail bump allocator's `func_0018D500` and
`func_0018D5C0` only to reject allocations that would cross the guest heap
limit. Successful allocations retain the retail alignment, cursor update, and
zero-fill behavior. The native loader trace reports rejected requests under
`OPENRAC_TRACE_LOADER=1`. Remove these overrides when the native runtime can
provide equivalent checked guest-heap bounds around the matching allocator.

`resource_readers.c` overrides `func_00137C40` as a port-only safety guard:
it keeps ordinary little-endian reads identical, but turns values above
`0x100000` into zero after emitting loader stage 161. This currently prevents
malformed stream counts from wrapping allocations and running translated
loops past guest RAM. It is a diagnostic containment measure, not a fix for
the stream desynchronization; remove it when the upstream reader is hostable
and the resource parsing mismatch is corrected.

`func_001A9F78` runs the derived constructors for the 37 global screen
elements after screen resources load. The decomp checkout has the matching
loop in `001A8000.cpp`, but its US function status is still `no-c`, so hostgen
emits a no-op. This backfill is kept separate and should be removed once the
matching C is included in the host build. The native pad adapter now marks its
valid SDL pressure-mode report as connected (`f14=3`, `f18=4`). That prevents
`func_001E9108` from treating the controller as absent and forcing page 0
(`Paused`) at five seconds. Startup remains in frame state 3, but the title
scene still submits no sustained geometry.

`screen_fade.c` backfills retail `func_001EFD10` and `func_001EFEE0` from the
latest available decomp C, plus assembly-only `func_001F00F0`. Their containing
decomp units cannot currently compile in hostgen, so these stay isolated in
`backfills.tsv`. A bounded run with the functions built still captures a
black frame; this does not establish that the fade was hiding valid scene
draws. Remove these entries when matching decomp code is hostable.

`func_00124050` walks placed-object children during activation, and
`func_001240F8` registers active objects with the world update lists. Their
port-only replacements raised the direct-load probe's world `+0x54` and
`+0x60` lists from empty to 77 entries each. The retail deferred dispatcher
and request bucketer (`func_0019DC98`, `func_0019D4B0`) and ConeVolume/VU
processor (`func_00184DE8`) also have separate port-only C backfills.
`func_0019D410` copies a 0x80-byte request from a host stack pointer into the
fixed guest queue without truncating the host pointer.

The native hostgen build now translates the exact US retail assembly for
`func_00125C90`, `func_00126270`, and `func_00166690` using the existing
`asmc.py` path. These generated routines are kept in the ignored hostgen
output, not copied into this C-backfill directory or the decomp checkout.
Hostgen reports all three as translated from `asm/us/text.s`, with no stubs.
The bounded direct-level probe parses key `0x0100` and installs a world, but
does not connect it to gameplay. In default startup, the pad-state fix now
lets the frame remain in state 3, and a trace reaches retail
`func_00125C90` for placed-object updates. The mesh request builder and packet
callback still do not run in the trace window, and no scene geometry reaches
the renderer. Keep further functional C replacements here and register them
in `backfills.tsv`; replace them when matching decomp C becomes hostable.

`object_transform_primitives.c` restores scalar versions of the retail matrix
builders, matrix multiply, placed-object transform setup, and camera sphere
scale test, plus `func_00125BF0`, which submits the transformed object through
the camera's dispatch callback. `func_001453B0` receives its test point in a
VU qword register in retail; this backfill reads the four saved `vf6` lanes
while preserving the scalar radius in `$f12`. This is a port-only ABI adapter,
not a change to the retail function or matching decomp C.

The camera constructor `func_00143648` and its reachable assembly-only
dependency closure are selected from the US retail assembly in `hostgen.json`.
The camera updater `func_00143E40` and two more missing-C functions in its
direct-call closure are now selected too; hostgen translates 59 assembly functions. A declaration bug
in `world_update.c` passed the address of the camera global instead of its
stored pointer. The corrected call sees the valid camera stack (one active
controller at `+0x454`) and reaches its retail update callback without invalid
callback errors. Sampled `func_00125C90` calls pass their initial VU MAC gate
(`0xF0`), but the next sphere/frustum test still rejects sampled entries with
zero node position and radius. Keep these assembly selections in the port build
configuration and all functional C adapters in this directory.

`object_update_callbacks.c` keeps the current decompilation C body for
`func_001261C0` as a separate port backfill because hostgen still reports its
source unit as no-C. It dispatches m2C, copies the optional four-vector object
transform into the record, then dispatches m26. Remove it when the matching C
unit becomes hostable.

The opt-in startup trace now records the local translation and first basis
qword for nodes entering `func_00125C90`, plus their virtual callback slots
(stages 50–52 in `main.cpp`). The latest bounded run found eight identical
entries with zero local translation, identity basis, and zero culling radius.
The dispatcher arguments match retail `func_0019D6C0` assembly, so the next
trace should establish whether these entries are supposed to inherit a parent
transform or simply lack scene geometry. These stages only observe memory; they
do not bypass culling or change rendering.
