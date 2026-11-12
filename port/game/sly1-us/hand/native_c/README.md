<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Port-only C backfills

These C files provide functional replacements for Sly functions whose
matching C is not available or not usable in the native host build. They
belong to the port, not `sly1-decomp`, and do not count as matching
decompilation progress. Functions replacing a matched definition must also be
listed in `../overrides.tsv`; otherwise hostgen keeps the decomp definition
when both are present. Entries in `backfills.tsv` cover functions supplied
where matching C is absent.

`func_001B5EA0.c` implements the state-walker free-pool pop, and
`func_001B6D78.c` dispatches a walker's class and registered listeners. They
are port-only backfills for the current state-graph startup path. Exact US
retail assembly supplies the surrounding graph loader/start/walker routines
in `hostgen.json`; retire these C helpers when their matching C is hostable.

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

`startup_async_state.c` is a port-only reconstruction of the async startup
semaphore state and worker setup (`func_0018B5C0`, `func_0018BD58`,
`func_0018B670`, and `func_0018B600`). The retail worker waits on its semaphore,
calls the queued callback, records the result, and signals completion. These
functions have no usable matching C in the native build yet. The SIF memory-card
probe called by `func_0018B580` remains on the native unavailable-service path.
Keep these replacements here so matching decomp C can replace them later.

The native kernel bridge maps the retail `CreateThread` and `iSignalSema`
wrappers used by this path. Matching C is still unavailable for
`func_0018D0A0`, `func_0018BD00`, and `func_00209CB8`; hostgen now translates
their exact NTSC-U retail assembly, along with the `PollSema` wrapper
`func_001F6B70`. A bounded startup run reaches the auxiliary semaphore-poll
threads without the earlier no-C warning. This removes no scene-rendering
blocker: the window remains on the black clear.

These replacements cover the persistent frame loop, state-transition helper,
frame callback setup, queued display worker, loader
request setup and worker, resource key and visual-state selection, resource
owner lists, descriptor setup, aligned pools and listener records, executable
checksum guard, packet-list finalization, display packet primitives, and
initial display-environment packet. The loader backfills reproduce the retail
request-list sizes, CRC table and semaphore ordering.
Native services create the loader semaphores and start its guest thread. The
loader now completes the forced level load and the main loop remains active for
at least 15 seconds. A stale frame node was being copied over by a large level
resource block; the port drains both render contexts and all three deferred
render lists before rewinding transient memory. The previous
`0x66002e34` frame-list fault no longer occurs in that run. The trace shows a
populated world and object lists, but no real scene geometry, image tags, or
blit writes. The lightweight GIF trace counter reports 240 vertices at some
snapshots; these are 40 GS points expanded into placeholder six-vertex quads,
with samples clustered at (0,0), rather than level meshes. There are no image
transfers, and the window remains black. Scene geometry submission and several
unresolved engine callbacks remain the next rendering blockers.

`func_001E9108` now uses the US retail assembly translation for the frame-state
dispatcher. Its earlier C reconstruction is retained under `hand/reference_c/`
for comparison, but is excluded from the active hostgen C set.

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

`front_heap_reset.c` backfills `func_0018D550` with the retail reset-mark to
front-cursor assignment. `world_list_cleanup.c` backfills the camera object
count reset and the global object-list clears (class 0x75, point objects,
target vectors, target curves, target points, and parts) used immediately before
world teardown resets transient memory. These stay isolated from the decomp
tree and should be removed when their matching US C becomes hostable.

`drain_render_frames.c` backfills the retail render-thread semaphore drain,
final-frame submission, and release of both frame contexts before world memory
is recycled. This is a port-only bridge around the native synchronous DMA path;
remove it when the matching renderer code is hostable.

`resource_descriptors.c` provides port-only C for `func_0011E680` and is listed
only in `backfills.tsv`, not `overrides.tsv`. Hostgen therefore compiles it as
a backfill without claiming it replaces matched decompilation. This reader
removes the prior `0x0021B4D8` missing-code call and lets the world loader reach
stage 5. When matching C is hostable and validated against the same resources,
remove the backfill registry entries and the port-only implementation.

`func_0011ECB8` now uses the configured NTSC-U retail assembly translation.
The previous `resource_object_record_loader.c` backfill treated this callback
list reader as a general object-record parser and added an unverified one-byte
alignment heuristic. Retail assembly shows it only reads a callback count and,
for each entry, a slot, key, and callback flag before invoking the object's
virtual read callback. The old source remains outside the active backfill and
override tables for reference; do not use its trace or zero count as evidence
that the level has no scene geometry. The level mesh path is separate and is
still unknown.
Use `OPENRAC_DUMP_ANIMATION_WINDOW=1` with `OPENRAC_TRACE_LOADER=1` to dump the
scratchpad around animation-table parsing for inspection. Keep all of these
diagnostics and heuristics port-only; validate the asset layout before
replacing them with matched C.

`func_001BAC30` is a port-only C reconstruction of the retail class-3 collider
reader, kept in `backfills.tsv`. The latest decomp source contains a
`LoadCollider` body, but hostgen still reports its US function as `no-C`, so
the launcher otherwise links a no-op. The backfill stays isolated in
`native_c/` and should be removed when the upstream body becomes hostable. It
reads collision meshes, transforms and indexed arrays; its cursor and large
mesh allocations remain under investigation and are not a validated fix.

`resource_stream_vector_readers.c` backfills `func_00137E90` and
`func_00137F08`, the 16-byte-stride packed-vector readers used by class-4
resource parsing. The previous bounded trace showed both were no-op stubs, so
they failed to consume those fields from the resource stream. These are
port-only replacements and should be removed when matching C becomes hostable.

`func_001E20B8.c` is a port-only bounded formatter used by active screen-element
updates. The canonical body lives in a source unit hostgen cannot currently
compile because of unrelated C++ layout/header errors. Keeping the replacement
in `native_c/` lets it compile as its own unit; remove this backfill entry when
that canonical unit becomes hostable.

`resource_face_tree.c` backfills `func_00138278` (`ReadFaceTree`). The canonical
source contains this reader, but hostgen currently reports it as `no-c`; the
collider callback needs it to consume BSP records after collision mesh data.
The backfill follows the decomp field layout and bounds node counts and links.
Remove it when the matching C function is hostable.

The canonical rework supplies `ReadPlacedObjectModel` and
`BuildLitStripMeshPacket` in `file_15F678.cpp`; the port now selects those C
functions instead of their older retail assembly translations. The
`func_001BAC30` collider callback remains a separately registered port-only
backfill because hostgen cannot yet compile its upstream source. These changes
advance model parsing but do not yet produce visible vertices.
With both readers active, the class-3 collider parse advances from cursor
`0x700014d5` through `0x70001a1f`, then reaches stage 132 and finishes its 57
groups at stage 134. Its frame has flags `0x0763e73f`, but the flagged
nested-object link at `+0x84` is `0xffff823f`. The port-only `func_00125138` backfill now skips
nested links outside guest EE RAM and logs them at stage 64, allowing startup
to continue. A bounded run reaches loader stage 5 and stays alive, but GS
published vertices remain 0 and image-transfer counts remain 0; visible scene
rendering is still blocked. Stages 62–63 record the object, frame, and links.

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
Wave 32 identifies `func_0018F9D8` as the class-4 model-part queue callback in
`file_18F988.cpp`. That source unit is still not hostable because unrelated
aggregate/header errors prevent hostgen from reading it, so the port now also
selects the exact US assembly translation for `func_0018F9D8`. This remains a
build selection, not a replacement in the decomp tree; drop it when the
matching unit becomes hostable. The native launch remains stable, but the new
selection has not produced scene geometry or image transfers yet.
Hostgen reports all four as translated from `asm/us/text.s`, with no stubs.
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

Stages 55–56 also identify registrations for class `0x1e` and class `0x51`:
owner, parent, mesh, placement attachments, extra-data and radius. The direct
level trace confirms ten root class-`0x1e` world nodes and two class-`0x51`
ConeVolumes, all owned by world `0x0088d530`; their mesh/attachment/extra-data
and radius fields are zero. These diagnostic stages are gated by
`OPENRAC_TRACE_FONT=1` and do not affect renderer state.

The canonical decomp rework check also exposed two host-compatibility fixes
kept here: `image_records.c` now passes retail guest addresses as `u32` rather
than hostgen's unsupported `gaddr` type, and `font_text.c` guards null font
records encountered before font registration. These are port-only behavior
and ABI accommodations; remove them or replace them when matching C is
hostable and resource ordering is handled correctly.

`collision_mesh_reader.c` is an active port-only reader for `func_00137FB0`.
It follows the current decomp's `Mesh03` layout and read order in
`file_137290.cpp`, with native-safe count bounds and loader diagnostics. Keep
it separate from the matching source: the upstream function is still a near
match, and the whole upstream translation unit currently fails host staging
on conflicting aggregate headers. The second collision mesh reaches the final
serialized edge links. Comparing the reworked `LoadCollider` body also fixed
the port callback's `refCount` and face-start offsets; both collision meshes
and face trees now complete and the parser advances into placed-object records.
The bounded launcher reaches the frame loop without rendered vertices. See
`docs/port/RESTART_CHECKPOINT.md` for the exact source revision, hostgen errors,
and current graphics frontier.

`func_001E20B8` and `func_001E1FD0` are port-only varargs bridges to the
native formatter. The upstream translation unit containing these functions is
not hostable, so keeping them in `backfills.tsv` ensures the SDL launcher links
the functional C instead of generated no-op stubs. The unbounded retail entry
uses the formatter's capacity-zero mode; retain both separately from
`sly1-decomp` so matching source can replace them later.
