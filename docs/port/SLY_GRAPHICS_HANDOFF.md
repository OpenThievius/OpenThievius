<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Sly 1 native graphics handoff

Last checked: 2026-11-18, 18:00 CET. The active host32 staging snapshot and
sibling `sly1-decomp` checkout are both at public master `8f7a85a3`.

### Old-wave launcher re-run: owner evidence was loader-only (2026-11-18, 18:00 CET)

I ran the archived old-wave launcher (`canonical-55d39f28/sly1_native`) directly
with `OPENRAC_BOOT_LEVEL=0x0100` to check whether the build that reported the
1,962-part owner ever rendered a recognizable level. It produced only repeated
`native GIF DMA transfer ... bytes=144` lines and **zero** drawing evidence
(no native vertices, draws, triangles, or frame/present output). The 1,962-part
owner was a loader-side observation from a bounded trace, not a rendered frame;
there is no evidence any wave actually displayed that model as a level.

This reframes the whole "restore the owner" line: even the wave that had it
never demonstrated a visible, controllable level. The current SDL build's
textured sub-scene is the most that has been rendered. No code changed.

### Old-wave report comparison (2026-11-18, 17:30 CET)

I compared the archived old-wave build (`build/port/canonical-55d39f28/`) with
the current build at the generated-source level:
- `resource_object_common.c` differs: the old wave used a gaddr-based native
  backfill (`game_sly1_discard`, `game_trace_common_reader`), the current a
  host-pointer backfill. Neither writes `+0x238` drawParts directly.
- Old-wave `hostgen/us/report.json`: `func_00162890` was an exact ASM selection
  (call graph of retail readers `func_00137A60/.../func_00137F08`, no port
  `sly1_native_*` helpers), while `func_0011E580` used the port object factory
  (`sly1_native_call_constructor`, `sly1_native_trace_font`). The current build
  uses matched decomp C for `func_00162890`
  (`ReadPlacedObjectModel(ModelRecord*, void*, PlacedObject*)`).
- So the old 1,962-part owner was produced by exact retail ASM for the model
  reader plus the port object factory; the current build swaps in decomp C for
  the model reader. `TextureRenderObject::drawParts` (+0x238) is set downstream
  by `BuildTexturePackets`/range records, not by either reader.

Pinpointing which specific change removed the owner needs a runtime trace of the
old launcher (the archived `canonical-55d39f28/sly1_native` binary) with the
current model-owner probe, which is a separate experiment. No code changed.

### Loader comparison: where drawParts population diverged (2026-11-18, 17:00 CET)

I compared the current model-record loader against the older wave that produced
the 1,962-part owner, to find where drawParts population was lost:
- `func_00162890` = `ReadPlacedObjectModel` (`placed_object_model.cpp:455`). The
  current build uses the **matched decomp C** (symbol
  `func_00162890(ModelRecord*, void*, PlacedObject*)`), not the port partial
  reader. The old partial reader in `resource_object_common.c` is behind
  `#ifndef SLY1_RETAIL_ASM_FUNC_00162890` and is therefore excluded. So the old
  `nstrips=0xFF02` stream-misalignment / heap-exhaustion failure (PLAYABLE_TODO
  "Forced-level model-reader failure") is from that partial reader and does not
  apply to the current decomp-based reader.
- `TextureRenderObject::drawPartCount` / `drawParts` (+0x238/+0x23C) are set by
  `BuildTexturePackets` (`func_00152E48`, `pooled_object.cpp:2407`) from the
  range record — not by `ReadPlacedObjectModel`. Neither the decomp nor the port
  hand code writes +0x238 directly; it comes from the retail path.

So the older wave's 1,962-part owner was produced by the older mix of the port
partial reader + exact ASM, while the current build's decomp `ReadPlacedObjectModel`
takes a different path. The divergence is at the range-record / texture-packet
loader (`func_00152E48` and the object-record reader), not in the strip parser.
Pinpointing the exact lost step needs a side-by-side of the generated host
reports or staged sources for `func_00152E48` and the object-record reader
between the old wave build and now — a focused but separate comparison.

This is diagnostic only; no code changed, tree clean at `18148ff`, checks pass.
No fields forced.

### Definitive: no class-4 model owner in current builds (2026-11-18, 16:30 CET)

I implemented the requested descriptor-validated full-RAM scan safely: object
`desc` fields are host pointers, and class descriptors live in host
`.data.rel.ro` (`0x0825b000..0x082e3000`), so I restricted `desc` to that
mapped range before reading `+4` (class id) and `+0xC` (size), requiring class
`0x04` and size `0x3A0` before reading the object's `drawPartCount` (+0x238)
and `drawParts` (+0x23C). I also wired the retail `SetPlacedObjectSpecialFlag`
(`func_0012B550`, `_Z13func_0012B550P12PlacedObjecti`) to clear flag bit 32 from
the game-thread frame-stage hook (the context proven safe for retail calls).

Result on the forced-level path (`OPENRAC_BOOT_LEVEL=0x0100`): the scan runs to
completion without a fault but finds **zero** class-4 objects with a populated
parts array. Earlier looser descriptor ranges faulted (unmapped pages); the
validated range does not. So the README's "class-4 owner at 0xa811f0 with 1,962
parts" belongs to an older build/wave and is **not present** in the current
build's forced-level path. There is no populated model owner to unhide or route
into the renderer, on either the normal or forced-level path. The hidden-flag
mechanism is real (retail clears bit 32 to reveal objects), but it has no object
to act on here.

All experimental code reverted, tree clean at `0904912` plus earlier opt-in
hooks, checks pass (0 problems, 6 unit tests). Captures:
`/tmp/sly-unhide5..8-20261118/`. No field forced.

### Forced-level model owner: derived cause and scan limit (2026-11-18, 15:40 CET)

I followed the forced-level (`OPENRAC_BOOT_LEVEL=0x0100`) path to understand why
its populated class-4 owner's draw callbacks never fire. The authoritative
note in `RESTART_CHECKPOINT.md:824` derives the cause: the owner's flags are
`0x2115ffff00`, so flag bit 32 (`0x100000000`) is set, and
`ApplyPlacedObjectTransform` returns immediately
(`placed_object.cpp:1984`, `flags & (0x8000 << 17)`). Retail clears that bit
with `SetPlacedObjectSpecialFlag(self, 0)` during normal play; the forced probe
bypasses the reveal step, so the owner stays hidden and its
`DrawCollider`/`QueueModelPartDraws` (`0x0018f9d8`) never run. That is a real,
derived mechanism — bit 32 is the "hidden/special" flag, not a rendering bug.

With FRAME_TRACE + `--trace` on the current build, the `DrawPlacedObjectParts`
hook still shows only the single 1-part object (`0x018cedf0`, matrix null) in
world `0x870cf0`, both with and without the forced-level flag. I tried to scan
for the class-4 owner to clear its bit-32 flag and observe the draw, but:
- restricting descriptor pointers to the guest range missed them (object
  descriptors are host pointers, as established earlier);
- widening to host pointers caused an invalid read (rc=139) because not every
  value in that range is a valid descriptor.

A correct scan needs descriptor validation (class id + size `0x3A0`) before
dereferencing — a focused follow-up. All experimental code reverted, FRAME_TRACE
back to OFF, tree clean at `0904912` plus earlier opt-in hooks, checks pass.
Captures: `/tmp/sly-forced-20261118/`, `/tmp/sly-unhide*-20261118/`. No field was
forced.

### Render-list traversal trace: only one 1-part object submitted (2026-11-18, 15:00 CET)

I traced the camera/render-list traversal with the FRAME_TRACE build flag (the
guest-call `OPENRAC_TRACE_RENDER_CALLBACKS` counter sees nothing because decomp
C calls are direct, not through the guest bridge). With `--trace` (which sets
`g_startup_trace_enabled`), the existing `sly1_native_trace_draw_object` hook at
`DrawPlacedObjectParts` (`active_set_solver.cpp`) shows, at the post-Cross world
`0x870cf0`, exactly ONE object reaching the part drawer: `0x018cedf0`, kind 3,
`parts=1`, matrix pointer `0x00000000` — and all 16 sampled frames are that same
object. No populated model owner reaches `DrawPlacedObjectParts`.

I then scanned all mapped EE RAM for a class-4 object in world `0x870cf0` with a
`drawPartCount` >= 100 and a valid parts array. There are **none**. The README's
"populated class-4 owner at 0xa811f0 with 1,962 parts" was from the forced-level
probe (`OPENRAC_BOOT_LEVEL`), not the normal post-Cross path.

So on the ordinary post-Cross path the active world's render list contains no
populated character/level model to submit — there is nothing to route into the
native world-mesh renderer. The distant sub-scene is what that world actually
contains. This is why no amount of routing/capturing in the current world
produces a recognizable level: the model data simply isn't in that world on the
normal path. The forced-level path does load a populated world but still shows
the same distant sub-scene, so the prepared level model is not being drawn there
either — a separate, deeper gap in the level render path.

All diagnostics were opt-in (env/flags) and reverted; FRAME_TRACE set back to
OFF; tree clean at `0904912` plus the opt-in hooks from earlier commits. Checks
pass (0 problems, 6 unit tests). Captures: `/tmp/sly-drawobj3-20261118/`,
`/tmp/sly-owners2-20261118/`. No fields forced.

### Player draw path: no model parts (2026-11-18, 14:30 CET)

I traced why no character model is drawn. The player draws through
`QueuePlacedObjectModelParts` -> `DrawModelPart` (class 0x07 descriptor supplies
`QueuePlacedObjectModelParts`), the same path captured by
`sly1_native_submit_model_part`. But at a post-Cross frame the player object
`0x016c8ed0` has `drawPartCount = 0` and `drawParts = null` (PlacedObject +0x238
/ +0x23C), so `QueuePlacedObjectModelParts` iterates zero parts and submits
nothing. The player exists, is in view (previous note), but has no model parts
to draw.

This matches the long-standing note in `port/game/sly1-us/hand/native_c/README.md`:
a forced-level post-parse trace finds the populated class-4 owner at `0xa811f0`
with **1,962 parts**, `drawParts=0x00dc41a0`, `partStates=0x00df9c00`, but its
descriptor's `QueueModelPartDraws` (`0x0018f9d8`) and `DrawCollider`
(`0x001b93d8`) callbacks never appear in the draw trace — the populated model
object is excluded from the camera/render-list traversal. `func_00126270` is
only called for two empty class-0x51 objects. So the root cause is upstream of
mesh parsing: the render-list traversal does not include the populated model
owner (nor give the player parts), and that is where the character/level model
would be submitted. Finding why the traversal excludes these objects requires
the render-callback/render-list trace (`OPENRAC_TRACE_RENDER_CALLBACKS`), a
focused follow-up.

All diagnostics opt-in/off by default; checks pass. Capture:
`/tmp/sly-player5-20261118/`.

### Player exists, is in view, but its model is not rendered (2026-11-18, 14:00 CET)

I checked whether a player character object exists at all. `g_player`
(`D_00262E10`) is non-null at a post-Cross frame: object `0x016c8ed0`, class
**0x07**, id 0x05, world `0x00870cf0` (the active post-Cross world). Its `pos`
(Vec4 at +0x100) is `(-4373.5, 9332.9, -554.2)`. Projecting that through the
captured view-projection matrix (row-vector convention) gives clip
`(35.81, 61.3, -309.7, 448.8)` -> NDC `(0.080, 0.137, -0.690)` — **on screen**,
near center. So the camera is tracking the player and the player is in view.

The visible scene still shows only the distant background meshes (the
`0x870cf0` world-mesh submissions) and no character. Therefore the player's
character model is not going through the captured kind-0/6 world-mesh draw path
that the native renderer draws; it must use a different moby/draw path that the
port does not render. That is the concrete, final gap for a visible character:
the player object and camera are correct, but no model is drawn for the player.

This is opt-in diagnostics only (off by default); tree clean at `0904912` plus
hooks, checks pass. Capture: `/tmp/sly-player4-20261118/`.

### Why the 0x263 transition has no visible effect (2026-11-18, 13:30 CET)

I traced what reaching graph state `0x263` is supposed to do. In retail, a
walker state change calls `NotifyStateWalkerChange` -> `TriggerScriptEvent(self,
0xE, ...)` on the graph (`state_walker.cpp:919`, `base_object.cpp:617`). That
event fires only if `self->scriptScope != 0` and a handler exists.

I added an opt-in trace on `RunScriptEventHandler` for the Splash graph
(class 0x88, id 0x25F, `OPENRAC_TRACE_GRAPH_EVENT`) and confirmed **zero** script
events reach it, even after driving the walker onto state `0x263`. The graph's
`scriptScope` (offset 0x2C) is **0x00000000**. `CreateTypedRecord` sets it from
`GetScriptEntryScope(a4, world)`, which returns 0 when the script index is -1.
Logging the creation of every class-0x88 object shows only two graphs with a
non-`-1` script index (0x36 and 0x37); the Splash graph `0x25F` at `0x0084cfb0`
was created with `-1`, so its null script scope is the asset's own data, not a
loader failure.

Consequence: the Splash graph has states and walkers but no script handlers,
so reaching `0x263` fires no event action — there is nothing in that graph's
data telling it to load a level, fade, or activate objects. The level/title
transition must be driven by a different object or the game-mode logic, not by
this graph's script events. This is why the scene stays `0x870cf0` / `ui=3` after
the graph leaves its idle edge.

All hooks are opt-in and off by default. Tree clean at `0904912` plus hooks;
checks pass (0 problems, 6 unit tests). Captures: `/tmp/sly-graphevent3-20261118/`,
`/tmp/sly-scope-20261118/`, `/tmp/sly-scriptidx-20261118/`.

### Game-thread drive moves the graph off its idle self-edge (2026-11-18, 12:40 CET)

I reproduced the missing timer dispatch from the correct game-thread context:
an opt-in `sly1_native_drive_splash_notifier()` is injected into `RunMainLoop`
(right after `RunWorldFrameLists`) and called when
`OPENRAC_DRIVE_SPLASH_NOTIFIER` is set. It finds the countdown notifier created
at load (`0x00847ef0`, tracked from the CreateTypedRecord hook) and calls the
retail `CreateStateWalker` (`func_001B6290`, via its exact C++ symbol) on the
notifier's `stateGraph` (`0x25F` link, `0x0084cfb0`) with event `0x263`, storing
the result as the notifier's `host` — exactly what retail
`SetStateNotifierState(state 6)` does when `host==0`. No field is fabricated;
only the retail function is invoked.

Verified: the created walker's `current` is state index **3**, and the graph's
four state keys are `0x260, 0x262, 0x261, 0x263` — so index 3 is key **`0x263`**.
The graph's walker is therefore moved off its idle self-edge (index 2 / `0x261`)
onto the `0x263` transition state. Calling this from the trace hook segfaulted;
from `RunMainLoop` on the game thread it does not. Direct calls via
`resolve_native` hit the wrong address, which explained the earlier crash.

The visible scene still does not change: after the graph reaches `0x263`, the
world/ui state remains `0x870cf0` / `ui=3` with the same distant sub-scene and
no HUD or character. So the graph transition is necessary but not sufficient
for a recognizable, controllable level — the script event attached to `0x263`
does not (in this port) produce a playable level transition. Opt-in and off by
default; tree clean at `0904912` plus the hooks; checks pass. Captures:
`/tmp/sly-walker3-20261118/`, `/tmp/sly-gamedrive-20261118/`.

### Persistent CreateTypedRecord/notifier hooks; notifier IS created but not updated (2026-11-18, 12:00 CET)

I added persistent, opt-in hostgen C-level hooks (in
`compile_decomp_host32.py`, injected into the staged decomp C):
- `sly1_native_trace_create_typed_record` at the end of `CreateTypedRecord`
  (`OPENRAC_TRACE_CREATE_RECORD`) logs each created object's class index + pointer.
- `sly1_native_trace_notifier_timer` at the top of `CountDownNotifierTimer`
  (`OPENRAC_TRACE_NOTIFIER_TIMER`) logs whether the notifier update runs.

This **refutes** my earlier whole-RAM-scan conclusion that the notifier was
never instantiated (that scan required descriptor pointers in the guest range
`0x00200000..=0x01000000`, but object `desc` fields are **host** pointers like
`0x0827d100`, so it was a false negative). Evidence:
- `CreateTypedRecord` IS called for class index **0x57** twice, with
  `id=0x54c` — matching the `0057 054c ffff` record at asset offset `0x15cc`.
  The notifier objects are `0x00847ef0` and `0x013afb80`.
- The first notifier at `0x00847ef0` is healthy at frames 300..308: descriptor
  class **0x57**, size 0x3a0, `state=2`, `timerEnabled=1`, `timeLeft=0` —
  exactly primed per retail `SetStateNotifierState` case 2.
- But `CountDownNotifierTimer` is **never called** (0 calls with the timer hook
  on). `SetStateNotifierState` case 2 leaves the notifier in state 2, and
  `CountDownNotifierTimer` early-returns unless `state == 0`, so the countdown
  that reaches state 6 / event `0x263` never starts. The graph's walker stays
  on the idle `0x261` self-edge.

I attempted to drive the notifier off that edge by calling the retail state
setters (`func_00147AB0` state 2, `func_00148FF0` state 6) from the trace hook.
Both the guest-call bridge and a direct resolved-function call segfaulted
(rc=139) — calling game internals from that hook needs correct thread/ABI
context, which this diagnostic hook does not have. That experimental drive
code was reverted; only the two opt-in traces remain.

The precise blocker is now: the countdown notifier is created and primed but
its per-frame update (`m17` / `CountDownNotifierTimer`) is never dispatched, so
it cannot reach state 6 / event `0x263`. Resolving that requires finding why
the notifier isn't in the world's update list (or dispatching its timer
correctly) — a focused follow-up. Tree clean at `0904912` plus the two trace
hooks; checks pass. Captures: `/tmp/sly-createtrace*-20261118/`,
`/tmp/sly-notifstate*-20261118/`, `/tmp/sly-timer-20261118/`.

### Double-free allocator check (2026-11-18, 11:20 CET)

I ran three 35-second bounded post-Cross sessions with `MALLOC_CHECK_=3` and
`MALLOC_PERTURB_=165` to reproduce the intermittent `double free or corruption`
abort. All three exited 137 (timeout kill) with zero corruption/fault lines.
With no reproduction and no core available, there is no stack trace to act on;
the abort is intermittent and remains undiagnosed. Captures:
`/tmp/sly-malloc-20261118/`. 32-bit ASan is unavailable in this toolchain
(its linker script points at a missing `/usr/lib/libasan.so.8.0.0`).

### Definitive whole-RAM notifier scan (2026-11-18, 11:00 CET)

I corrected the earlier class histogram (it only saw world-list-linked objects)
with a bounded full-RAM sweep. Guest RAM is one RW mapping at `0x00010000..=
0x02000000` (mmap at `0x00010000`, size `0x01ff0000`), so a 16-byte-stride scan
dereferencing each candidate's descriptor is safe. I validated each match by
the descriptor's class id (`+4`) AND its size (`+0xC`) to reject false positives.

Results at a post-Cross frame (both with and without `OPENRAC_NATIVE_MEMCARD`):
- class 0x55 state notifier (size 0x390): 0 genuine objects
- class 0x57 countdown notifier (size 0x3A0): 0
- class 0x58 time range (size 0x3A0): 0
- class 0x5A deadline (size 0x390): 0 (the one raw hit at `0x002475b0` is the
  class-descriptor table entry at index 90, not an object)
- class 0x5B armed (size 0x390): 0

Without the size validation, a naive `desc+4==0x57` sweep produced 13 hits, but
their `state` fields held pointer-like garbage and `idleEvent` was `0x1d/0x1e`
(not a real event key like `0x261`) — all false positives.

I also confirmed the class table is correct: `g_classDescriptorTable[0x57]`
(`0x00247448 + 0x57*4`) = `0x0021cad8`, whose `+4` = 0x57 and `+0xC` = 0x3a0,
so `CreateTypedRecord(0x57, ...)` would build a valid notifier. The Splash
resource `build/port/extracted/lzss/splash.decoded.bin` (430858 bytes) has only
two u16 `0x0057` values (at `0x15cc`, `0x2c46`), neither clearly an object
record. So the asset-driven object-creation path never requests a notifier.

This is the definitive blocker: the retail graph relies on a notifier object
that this loader never instantiates. Tracing exactly why requires instrumenting
`CreateTypedRecord` calls in the resource stream, which is decomp C regenerated
every build (a persistent hook needs a hostgen tooling change). All experimental
hook code was reverted; tree clean at `0904912`. Captures:
`/tmp/sly-scan*-20261118/`. No fields were forced.

### Decisive: no notifier object exists to drive (2026-11-18, 10:15 CET)

I implemented an opt-in port-only hook (`drive_splash_notifier_stage8`, on the
always-called `sly1_native_trace_frame_stage`) to locate the CountdownNotifier
at runtime and drive it through its retail state setters (state 2 ->
`timerEnabled=1` via `func_00147AB0`, then state 6 / event `0x263` via
`func_00148FF0`), rather than forcing any field. To find it I walked the live
world's `objectsById` table and the `list3C/48/54/6C/78` + `hashLists` at the
world base read from `g_currentWorld` (`D_00275710`).

Class histogram of every reachable object (opt-in `seen class` trace) in both
the Splash world `0x6abdd0` and post-Cross `0x870cf0`:
`02,04,1e,4d,51,53,6c,6d,75,78,7a,7d,83,88,8e`.
Class `0x88` (StateGraph) **is** present, but **none** of the notifier classes
`0x55/0x57/0x58/0x5A/0x5B` exists. There is literally no CountdownNotifier
object in the loaded world to drive, so the transition cannot be reproduced
by driving it — the object the retail graph relies on was never created by
this loader path. This supersedes the earlier assumption that a notifier with
a null gate merely needed its state advanced.

I also confirmed the SDL host maps game memory at guest addresses directly
(`openrac_guest_base` is null; e.g. `*(volatile u32*)0x006abdd0u` works). The
list-walk hit an invalid `next` pointer and segfaulted once; I reverted all
experimental hook code, so the tree is clean at `0904912`. Captures:
`/tmp/sly-hist5-20261118/`, `/tmp/sly-drive*-20261118/`.

The remaining task is now clear: find why the asset's notifier object (class
0x57) is not instantiated during load, which requires the retail asset/object
reference. No fields were forced. No code changed.

### Full notifier/graph state chain (2026-11-18, 09:30 CET)

I completed the retail-source derivation of the transition chain in
`state_notifier.cpp`, which closes the `f338` question:
- `timerEnabled` (offset 0x338, the "null gate") is set to `1` in code at
  `SetStateNotifierState` case 2 (`state_notifier.cpp:958`), guarded by
  `timerDisabled == 0`. It is NOT a property record.
- `SetStateNotifierState` maps notifier states to graph events: state 0 ->
  `0x261` (idle/self-edge), state 6 -> `0x263` (the transition the countdown
  notifier waits for), state 7 -> `0x262`, state 3 -> `0x260`.
- `CountDownNotifierTimer` requires `timerEnabled != 0`, then on timeout calls
  `SetCountdownNotifierState` -> state 6 -> event `0x263`; only then does
  `HandleCountdownNotifierNotification` advance and notify the world.

So the retail sequence is: notifier asset/link puts the notifier into state 2
(enabling its timer), the timer expires, the notifier goes to state 6 / event
`0x263`, and the graph walker leaves the idle self-edge. The runtime notifier
is in state 0 with `timerEnabled` null, so it never starts the timer. All of
`SetStateNotifierState`, `SetCountdownNotifierState`, `CountDownNotifierTimer`
and `LoadStateGraph` are real decomp C in the staging build (not stubs/traps),
and the port stages the decomp `ReadPropertyRecords`. The initial notifier
state and `idleEvent` come from the asset's property records/object links.

I attempted to instrument the property reader at load time, but hostgen
regenerates the staged decomp sources on every build, so a direct staged-file
edit does not persist and `func_001B5F50`'s backfill is not the active path
(the decomp `LoadStateGraph` wins). A persistent trace needs a hostgen C-level
hook, which is a separate tooling change.

The remaining task is to identify which asset property record sets the
notifier's initial state / `idleEvent` to a non-idle value and reproduce that
link resolution. That value is present in the retail resource data, not in decomp
C, so it needs a retail asset reference to identify precisely. I did not force
the fields. No code changed; tree clean at `6a1eced`. Source:
`/home/reaper/sly/sly1-decomp/src/main/state_notifier.cpp`.

### Property-descriptor level of the graph arming (2026-11-18, 09:00 CET)

I traced the retail property system (`ApplyPropertyRecord` in `pose_source.cpp`,
`g_objectPropertyDescriptors` in `object_host_functions.cpp`) to pin down how
the graph fields are set:
- StateGraph property `0x113` maps to descriptor 513 = `type 0, flags 0x80,
  offset 68 (0x44)` — a direct U8 write to the `armed` field. Property `0x12A`
  maps to descriptor 514, `CreateScriptStateWalker`. The runtime graph has a
  walker but `armed=0`, consistent with the asset relying on the walker from
  `0x12A` rather than the `0x113` write being non-zero.
- `ReadPropertyRecords` (`pose_source.cpp:513`) applies each `s16` record id via
  `g_objectPropertyDescriptors + (r << 5)`; the port stages this decomp C, so the
  reader itself is present.
- The notifier gate `f338` (offset 0x338 = 824) has no matching direct-offset
  property descriptor for the notifier class; the descriptors at 824 belong to
  unrelated classes (indices 503/855). The gate is an object link (`host`), set
  outside the straightforward property-record path.

So the retail arming is a combination of a direct `0x113` byte write and object
links that the asset's own records supply. The runtime values (`armed=0`,
notifier `f338` null) come from the asset data as loaded; finding which specific
record/link is absent requires either a retail asset-loading reference or
instrumenting the retail property/link resolution, neither of which is present.
I did not fabricate or force either field. No code changed; tree clean at
`6a1eced`.

### Splash graph state transition derived from retail code (2026-11-18, 08:30 CET)

I traced the state machine the project TODO points at, using the canonical
build's `[state-graph-trace]` diagnostic. Actual Splash graph state:
`graph=0x84b620 states=4 edges=7 armed=0 walker current=2 state_key=0x261
state_next=2 edge 2->2 flags=0x1 weight=5 player_time advancing`.

Derivation from `src/main/state_walker.cpp` / `state_notifier.cpp`:
- `StartStateGraph` only spawns the initial walker when `armed != 0`; here
  `armed=0`, so no graph-driven transition is scheduled.
- The one walker that exists replays the 2->2 self-edge: `StartNextStateWalkerEdge`
  keeps selecting the self-loop because the walker's `target=0`, so
  `HandleStateWalkerTrackEnd` commits arrival back to state 2 forever.
- The transition out is not internal to the graph: `HandleCountdownNotifierNotification`
  only fires mode 1 / completion when its host walker reaches key `0x263`, and
  `CountDownNotifierTimer` only counts while field `f338` (the gate reference)
  is set. The runtime notifier is in state 0 with `f338` null and timer 0, so
  it never advances and `0x263` is never reached.
- `f338` and the `armed` flag are populated from the asset's property records
  / object links (via `ReadPropertyRecords` in `LoadStateGraph`), not from decomp
  C. So the retail transition depends on asset data that the current load path
  does not resolve into these fields.

This is a genuine asset-link/graph-arming gap, exactly the open question the
project TODO records ("determine whether the asset intentionally leaves +0x338
null"). Resolving it correctly means finding which retail property record or
linker step sets `armed` and `f338` for this object and reproducing that in the
port loader — not forcing either field. I did not force them. No code changed;
tree clean at `0904912`. Trace: `/tmp/sly-graph-20261118/run.log`.

### Opt-in memory-card emulation (2026-11-18, 08:00 CET)

I found the game was pinned to the retail no-card error state: `card_state=1`,
`card_result=11`. `PollMemoryCard` (`src/main/memory_card.cpp`) maps a
`GetCardInfo` result of 9/11 to `targetState=1` (no card), which keeps the
no-card prompt up. The port's card backfills deliberately reported no card
(`func_00209B80` operation 1 / result -1).

I traced the mapping: `SyncMemoryCardResult` maps operation 1 / result -1 to
error 9 via `MapMemoryCardErrorCode(1, -1)`; operation 1 / result 0 maps to
success, and with a non-zero `formatted` output `PollMemoryCard` reaches
`targetState=3` (formatted card). I added an opt-in `OPENRAC_NATIVE_MEMCARD` to
the two port backfills so `SyncCardRequest` returns operation 1 / result 0 and
`GetCardInfo` reports a formatted card with free blocks. This emulates the PS2
memory-card hardware the game requires; it writes no save data and is off by
default.

Verified: default run unchanged (`card_state=1 card_result=11`); opt-in run
shows `card_state=3 card_target=3 card_result=0` and the game advances through
Cross to world `0x870cf0` with `frame_stage=7`. The visible scene is still the
same distant sub-scene, so this alone does not make a playable level. Checks
pass: 0 problems, 6 unit tests. Captures: `/tmp/sly-memcard-20261118/`.

### Object/draw-path trace and controllability (2026-11-18, 07:30 CET)

I traced the post-Cross world `0x870cf0` object and draw path. The two flags in
the request (`OPENRAC_TRACE_OBJECTS`, `OPENRAC_TRACE_UI_CALLBACKS`) live in
`port/sly1/main.cpp`, the non-SDL host path; the SDL launcher uses
`port/sly1/native_host32/main.cpp`, which has no such flags. Its equivalents
(`OPENRAC_TRACE_SLY_WORLD`, `OPENRAC_TRACE_PLACED_OBJECT_*`,
`OPENRAC_TRACE_NATIVE_MODEL_BUILDS`) show:
- `sly1_native_submit_model_part` is called from `DrawModelPart` for every
  drawn part (`build/.../main/placed_object_model.cpp:2046`). The port captures
  all kind-0/kind-6 meshes: **zero** `native sly mesh miss` events, up to **276**
  drawn meshes per frame, with real uploaded textures. The draw path is not
  missing — there is nothing to "implement active" there.
- The largest submitted mesh is 106 vertices; the visible scene is these real
  background/environment meshes, not a wrong camera matrix.

Controllability test: holding the Right arrow for 4 seconds produced
`[pad] held=0020` (Right reached the pad) but the camera view matrix was
unchanged across the hold (only 8 slight positional drifts over the whole run,
no response to input). So input is delivered but does not drive the player or
camera — the game is in a non-interactive (Splash/cinematic) state, consistent
with the earlier note that the automatic Splash-to-title/state transition is
missing in several no-C graph functions. Those graph/state functions are not
`missing_functions.S` traps (zero called), so they would need to be implemented
from the decomp as new port backfills — a substantially larger task than a
compositing fix, requiring the Splash/world graph state machine to be completed
so the game leaves its non-interactive state.

No code changes from this audit (the trace-cap tweak was reverted); tree clean
at `77fa12d`. Captures: `/tmp/sly-objall-20261118/`, `/tmp/sly-move-20261118/`.

### No-C stubs are NOT the active blocker (2026-11-18, 06:50 CET)

I tested the premise that level-construction/update no-C functions are what
stops a controllable world. It is refuted by runtime evidence:
- A 20-second post-Cross run produced **zero** `sly1_missing_function` calls
  (that handler writes to fd 2, same as the captured log) and **zero** `ee`
  trap warnings. None of the 95 no-op stubs in `missing_functions.S` is reached
  during the loaded-world frame loop. Implementing them cannot change the
  running scene.
- The game does advance on Cross. `OPENRAC_STARTUP_TRACE` shows, across the
  injected press at frame 300: frame 301 `ui_state=5 pause_open=1
  pause_page=3 world=0x6abdd0 elapsed=0x409f778c`, frame 359 `ui_state=3
  pause_open=0 pause_page=-1 world=0x870cf0 elapsed=0x40d86e9c`. `0x870cf0` is
  the world that submits the rendered meshes. So input, state transition and
  world load all work.
- The view matrix is live, not a reset default: `view_projection` translation
  is `(-1679.88,-17.89,5127.31,-4975.52)`, versus `g_cameraResetPosition`
  `(0,-2000,500)`. The meshes are genuinely distant from a real camera.

The project's own `PLAYABLE_TODO.md` "Immediate next steps" says not to spend
the next slice on broad missing-function closure or loader repair; it calls for
validating one complete kind-6 strip against a retail capture. There is no
retail frame capture in the repository to validate against (and using an
emulator to make one is out of policy). That is the point where I need a
decision from the user: supply a retail reference frame/capture to compare the
strip against, or choose a different validation target. No code changed; tree
clean at `77fa12d`. Trace logs: `/tmp/sly-traps-20261118/`,
`/tmp/sly-startup-20261118/`, `/tmp/sly-camera2-20261118/`.

### Input formats and direct level boot (2026-11-18, 06:20 CET)

I verified both real input paths. `port/game/sly1-us/native/func_002042C8.cpp`
swaps the button bytes so standard DualShock masks reach Sly's internal masks:
X11 SPACE gives Cross (`held=0x4000` -> internal `0x0040`), X11 Return gives
Start (`held=0x0008` -> internal `0x0800`). All presses were observed in the
`OPENRAC_TRACE_PAD` output, so input is delivered correctly.

The decomp's prompt text is "Press START button for New Game"
(`g_promptTexts` in `screen_element.cpp`). I sent three Returns then three
SPACES via XTEST to a windowed launcher (`/tmp/send_start.py`): all reached the
pad, but the scene did not become a recognisable level — the world meshes even
disappeared at frame 700+, suggesting a state change without level activation.

I retried the `OPENRAC_BOOT_LEVEL=0x0100` direct-load probe (catalogued "A
Stealthy Approach", sector 867214) against the fixed renderer. It shows the
same distant sub-scene as the normal path, not a populated playable level.
Prior notes already record that several level-construction/update functions
remain no-C stubs, so a parsed resource does not imply a valid, controllable
world. The heap abort still did not reproduce in these runs. No code changed;
tree clean at `40b6e69`. Captures: `/tmp/sly-startinput-20261117/`,
`/tmp/sly-bootlevel2-20261117/`.

### Framing, input progression and crash audit (2026-11-18, 05:50 CET)

I looked at why the visible scene is only a distant sub-scene. Mesh clip bounds
(`OPENRAC_TRACE_SLY_WORLD_BOUNDS`, `/tmp/sly-bounds3-20261117/`) show the
submitted meshes are mostly large background geometry: e.g. instance
`0x5e53800000` has local coordinates `-50000..50000` and NDC
`-178..2.24`, with only ~39/106 vertices inside the clip volume. A handful of
small objects (instance `0x5e47...`) sit at NDC `-0.61..-0.08`, `0.06..0.30`.
There is no large, near-camera level mesh in the submission set — the distant
grate-like structure in the captures is the in-range part of that background
geometry, not a wrong camera matrix.

I injected a burst of ten Cross presses (frames 300..480, 20 apart) via the
comma-separated `OPENRAC_TEST_PRESS_CROSS_AT_FRAMES`; all ten reached the pad,
but the scene at frame 520+ was unchanged. So input reaches the game but does
not advance it to a recognisable level — either the game is still in a
pre-level state (title/cinematic) or more than Cross is required. Only Cross
can be injected by the current test harness (`port/platform/sly_input.cpp`).

I re-tested the alternate matrix convention (`OPENRAC_SLY_WORLD_AUTO_MATRIX=1`)
against the now-correct compositing: it produces a fan of huge triangles from
one point (`/tmp/sly-mode1-20261117/contact.png`), confirming mode 0 is right.

I could not reproduce the `double free or corruption` abort in two 60-second
runs (with and without the producer), so it is intermittent; the existing
fault-trace handler did not fire. 32-bit ASan is unavailable in this toolchain
(the `libasan.so` linker script points at a missing `/usr/lib/libasan.so.8.0.0`).
No code changed this audit; the tree is clean at `32a2588`.

### World geometry finally visible (2026-11-18, 05:15 CET)

Three compositing bugs were hiding the world. Fixing them made real, textured
Sly geometry visible by default for the first time:
- The producer's GIF packet wrote PRIM/XYZ2 but never RGBAQ, so its untextured
  vertices inherited colour (0,0,0,0) and drew nothing. Added an opaque RGBAQ
  write (`b226f4d`).
- The DirectRenderer interpreter was not cleared between the HUD pass and the
  world pass, so the world draw began at vertex 0 and bridged the HUD's
  full-screen quads (`b226f4d`).
- The textured GL world pass ran *before* the DirectRenderer blit, which then
  covered it. It now runs after the backdrop with the depth buffer cleared;
  the early pass is kept behind `OPENRAC_SLY_GL_EARLY` for diagnosis.

All 69 captured materials resolve to real uploaded textures (no placeholders),
so the geometry is genuinely textured. Verified post-Cross captures show the
game's sky/ground backdrop with textured 3D meshes drawn over it
(`/tmp/sly-defaulttex-20261117/contact.png`), instead of a solid black frame.

This is still not a playable, recognizable level — the visible geometry is a
distant grate-like structure near the horizon, and there is no clear character
or HUD. The long-run `double free or corruption` abort is also undiagnosed.
Checkpoints: `b2ffdeb` (draw block), `b226f4d` (colour + clear), and this
compositing change.

### Draw-buffer block mismatch (2026-11-18, 04:30 CET)

I traced Sly's FRAME register: all 35 writes are `0x4D = 0x0000000000010126`,
i.e. `fbp=0x126` → draw buffer block `0x24c0`, with `fbw=1`. The game's
`BlitFrameToDisplay` samples TEX0_2 at TBP `0x24c0` (decoded from the runtime
`0x0000000d980064c0`). The port never configured its draw buffer, leaving
Ratchet's `g_draw_buffer_block = 0x1000`, and `is_offscreen()` rejected `0x24c0`
on both counts, so the buffer was never registered and the blit sampled the
placeholder (`texture=0x1`, `fbmsk=0xffffffff` → no visible write).

I added an opt-in `OPENRAC_SLY_DRAW_BLOCK` override and changed `is_offscreen()`
so a configured display/draw buffer is on-screen even with a narrow `fbw`, while
unknown blocks stay offscreen (a first version wrongly registered 48 garbage
FRAME buffers; corrected). Verified with `OPENRAC_TRACE_FRAME_SOURCE`: the blit's
`frame_source(0x24c0)` now returns `found=1 xy=0,0` — the bug is genuinely fixed.

The post-Cross frame is still black. The reason is separate and deeper: after
Cross the DirectRenderer decodes `decoded_vertices=0 draws=0` for the game's
packets — the world geometry arrives only as VU1 XGKICK packets the interpreter
cannot turn into draws, and no writable UI/blit draw follows. The draw-block fix
is correct but cannot make the scene visible on its own. Capture:
`/tmp/sly-finalfix-20261117/`, `/tmp/sly-fs5-20261117/`.

### Display-state and separate-pass attempts (2026-11-18, 04:00 CET)

I tried the two fixes named in the previous note. Adding FRAME_1 (fbmsk=0),
TEST_1 (depth off) and an XYOFFSET reset to the producer packet did not make
the geometry appear: the DirectRenderer trace showed no draw with the
producer's vertex count. I then removed the duplicate `gif_stream_write` and
rendered the producer packet as a **separate** `g_direct->render()` pass from
the HUD packets. That also produced eight black post-Cross frames — the
DirectRenderer clears/restores the framebuffer on each pass when
`last_frame_writes_display` is false, so the second pass loses the first.

These results narrow the display gap: the HUD-only DirectRenderer cannot
composite a second geometry pass without a frame-retention change, and the
producer packet alone is not enough. The experiments are kept but remain
opt-in; the default client is unchanged and no playable scene exists. No
launcher remains. Capture: `/tmp/sly-state-20261117/`, `/tmp/sly-sep-20261117/`.

### Producer display-path audit (2026-11-18, 03:30 CET)

I audited why the committed producer's PRIM-4 packets do not produce a visible
frame. The GS summary confirms the producer emits correct packets after Cross
(`origin=sly-world-producer PRIM=0x4 PRIM_writes=234 XYZF2=9098`), and the
first run where the packets also passed through `gif_stream_write` recorded
`native_vertices=54276` — so the parser does turn the writes into vertices.

The DirectRenderer display path, however, does not show them. Two experiments:
- Removing the duplicate `gif_stream_write` (keeping only the DirectRenderer
  insert) left post-Cross frames black; the DirectRenderer needs additional GS
  state (TEX0/FRAME/fbmsk) that the synthetic packet does not set.
- Skipping the GL world pass (`OPENRAC_SLY_GIF_SKIP_GL`) with the producer on
  also produced black post-Cross frames; the game's own post-Cross packets are
  state-only, so there is nothing else to composite.

A longer run hit a `double free or corruption` abort that may relate to the
producer's packet lifetime, not yet diagnosed. These experiments were reverted;
the committed producer remains opt-in and the tree is clean. No launcher runs.

The remaining concrete gap is display state: the producer writes geometry but
no TEX0/FRAME/FBMSK, so the DirectRenderer's `last_frame_writes_display` is
false and the previous frame is restored instead of the new geometry. A future
change must set those registers in the synthetic packet. No playable scene.

### Port-side GIF vertex-packet producer (2026-11-18, 02:45 CET)

I implemented the producer the previous note called for. `build_world_gif_packet`
in `port/runtime/sdl_renderer.cpp` takes each captured model's source vertices,
transforms them with the game's `model * viewProjection`, and emits a real GIF
packet: XYOFFSET reset (register 0x18), PRIM 4 (triangle strip), and XYZ2
(register 0x05) writes with screen-space coordinates. The packet is submitted
through `openrac_guest_gif_stream_write_from` and the native DirectRenderer.

The opt-in `OPENRAC_SLY_GIF_PRODUCER` path works: the GS summary records
`origin=sly-world-producer PRIM=0x4 PRIM_writes=444 XYZF2=18980
prim_xyz=4:18980 native_vertices=54276` — for the first time, triangle-strip
vertex writes from Sly geometry reach a native GIF append path, entirely in
port code with no matched decompilation edited. With a per-request filter the
single mesh `0x005e4700` produces 3 PRIM + 93 XYZ2 writes.

The rendered result is still not a recognizable level. Resetting XYOFFSET made
one sampled post-Continue frame show varied geometry (685,831 nonblack pixels,
extrema 0-255) instead of a uniform fill, but a longer 30-second capture hit a
"double free or corruption" abort that I have not diagnosed. This is a
functional producer milestone, not a playable scene; every flag is opt-in and
the default client is unchanged. No launcher remains. Captures:
`/tmp/sly-producer-20261117/`, `/tmp/sly-prod-xyoff-20261117/`.

### The kicked packet contains floats with PRIM addresses (2026-11-18, 02:15 CET)

I added opt-in `OPENRAC_DUMP_KICK_PAYLOAD` to dump the accepted qword-35
XGKICK's payload directly from VU memory. The payload qwords are raw floats:
qword 36 `lo=0x3e36c8b43ea85879 hi=0x3f8000003f800000` (UV ~0.18/0.22, then
1.0/1.0), qword 38 `lo=0x41294d8441a2270b hi=0x4b008ff000000000`
(positions ~10.3/9.5 and a fog value), qword 39 another UV pair. Crucially the
A+D address byte (`hi & 0xFF`) is **0x00 (PRIM) for 3,534 payload qwords**, with
only state addresses (0x80, 0x34, 0x3b, …) elsewhere — **never 0x04/0x05**
(XYZF2/XYZ2). The VU stores the vertex data as plain floats into a packet whose
register descriptors say PRIM, so the GS sees state writes, not vertices.

The `ISW.z VI10,[0xd+VI4]` sites that should patch A+D addresses were traced:
they write values like 1, not 4/5, and only at VU qword 13, never at the kicked
payload qwords 36-105. So the port's VU does not assemble a vertex packet.

I re-tested the qword-461/469 rejected kicks with the reserved-bit check
relaxed (`OPENRAC_RELAX_GIFTAG`): 866 VU kicks then carry XYZ but **still no
PRIM 3/4**, and every frame is solid full-screen garbage. The rejected kicks are
malformed; relaxing validation does not recover geometry. The experiment was
reverted.

This is the concrete root cause: the game's vertex floats reach VU scratch but
no code path in the port turns them into a GIF packet with XYZZ/XYZF2 register
addresses. A faithful fix must implement that packet producer in the port, not
relax parsing or render raw floats. No launcher remains. Captures:
`/tmp/sly-kickpayload-20261117/`, `/tmp/sly-iswtrace3-20261117/`,
`/tmp/sly-relax-20261117/`.

### Default client rendered from VU output (2026-11-18, 01:40 CET)

I added the compile-time option `OPENRAC_NATIVE_VU_NDC_RENDERER` and enabled it
in the default `wave55-sdl32` cache. The default client now captures the VU's
final clip-space vertices and draws them with identity MVP, instead of
rendering nothing. A bounded capture after the process-local Cross at frame
300 shows eight frames; frames 2-7 are pixel-identical (SHA-256
`05cba9d00e26…`) with 353,084 nonblack pixels — the default post-Continue
output is **no longer black**. The settled image is large gray surfaces with a
small white sliver; it is not yet a recognizable level. A real X11 Return
keypress still reaches the game's pad path (`held` changes in
`OPENRAC_TRACE_PAD`). Capture: `/tmp/sly-default-vu-20261117/`.

Only `w == 1` VU stores are captured; the w≈8.4e6 skin-weight temporaries
divide to zero and were excluded. The 52 geometry groups span 1,253 vertices
across 52 MSCAL offsets. Strip boundaries and material/texture assignment are
still unverified, so this is a rendering milestone, not a playable scene.

### Per-mesh VU epochs (2026-11-18, 01:10 CET)

The VU write stream shows 39 descending-qword resets per 4,000 stores — one
per mesh, because each mesh restarts its scratch near qword 36. I changed the
NDC capture to split the write stream at those resets and keep the last write
per qword within each epoch, so earlier meshes are no longer overwritten by
the final one. Each epoch is emitted as a separate strip group.

The capture still renders large abstract surfaces rather than a recognizable
level. This indicates the remaining gap is material/texture assignment and
strip-boundary validation within each epoch, not loss of geometry — the
vertices are now genuine retail VU output. Capture:
`/tmp/sly-ndcepoch-20261117/`. No launcher remains.

### VU NDC renderer refinements (2026-11-18, 00:45 CET)

I refined the VU-output renderer: W division is now applied to every captured
vertex (not just `w == 1`), scratch qwords are deduplicated by keeping the
last write, and strip groups are split where the qword stride exceeds six.
These changes produce coherent large surfaces and angular objects rendered
entirely from retail VU output — with no source-position adapter involved.
`OPENRAC_SLY_VU_NDC_GROUP` isolates one strip group for comparison.

The result is still not a recognizable level: full-screen quads and large
environment surfaces dominate, and per-group material/texture assignment is
absent. Captures: `/tmp/sly-ndcdedup-20261117/` and
`/tmp/sly-ndcgroup{0,1,2}-20261117/`. No launcher remains.

### VU NDC output rendered directly (2026-11-18, 00:20 CET)

Capturing the full VU store stream for one submission (40,000 records) revealed
the decisive fact: the retail batch programs write **final normalized device
coordinates** into VU scratch qwords, with `w == 1` and `z == 1`. For example
qwords 36/39/42/45/48 hold
`(0.3288,0.1785,1)`, `(0.275,0.1847,1)`, `(0.2928,0.1361,1)`, … with a
three-qword stride — a triangle strip in write order. These are the vertices
the missing GIF packet would have carried, and they are already in clip space.

I added opt-in capture (`OPENRAC_CAPTURE_VU_NDC`) at `data_write()` for finite
vertices with `w == 1`, grouped by submission/MSCAL/entry, and an opt-in
renderer (`OPENRAC_SLY_VU_NDC_RENDER`) that draws each group as a
`GL_TRIANGLE_STRIP` with an identity MVP. A bounded full-scene capture now
shows large rectangular surfaces and a complex angular object rendered from
**actual retail VU output** rather than pre-VU source positions. This is the
first time the port renders the game's transformed geometry.

The output is not yet a recognizable level: full-screen quads cover much of
the frame and group boundaries still need validation. No launcher remains.
Captures: `/tmp/sly-vu-ndc.csv`, `/tmp/sly-ndcrender2-20261117/` and
`/tmp/sly-ndcall-20261117/`. All flags are opt-in; the default client is
unchanged and no matched decompilation source was edited.

### Matrix-mode conclusion corrected; adapter default restored (2026-11-17, 23:45 CET)

I logged the exact matrix uploaded for instance `0x5e54000000` and computed
the projected NDC of its local bounds under both conventions. With mode 0
(the row-major `model * viewProjection` uploaded column-major, so GLSL sees
its transpose) the vertices project to a small but valid region; with mode 1
they collapse near the origin. Re-examining the mode-1 image shows it is a
**scaled-up fan**, not a coherent 3D object as the 21:55 CET note claimed.
That interpretation was wrong. The adapter default is restored to mode 0,
which matches the game's row-vector convention and the decomp audit.
`OPENRAC_SLY_WORLD_MVP_MODE` remains available for comparison.

A 40-second capture at frame 600 with mode 0 shows the same stable distorted
scene as earlier frames, so the camera does not settle into a recognizable
view. Capture: `/tmp/sly-late-scene-20261117/`. The fan is therefore a
topology/data interpretation problem, not a matrix or timing problem. No
launcher remains.

### Real native input verified (2026-11-17, 23:20 CET)

I launched the default host32 launcher under the X11 SDL driver with
`OPENRAC_TRACE_PAD=1` and sent a real desktop Return keypress through XTEST
(a throwaway script under `/tmp`, not a repository file). The log records the
SDL key events (`[key] type=down key=0xd`) and the game's pad report changing
from `held=0000` to `held=4000` and back — `0x4000` is the Cross bit that the
retail no-card menu consumes. This confirms **real native keyboard input
reaches the game's pad path**; the process-local test press is an additional
diagnostic, not the only input mechanism. The world/VU activity after the
press is the same as with the injected press. No launcher remains. Log:
`/tmp/sly-realinput-20261117.log`.

Graphics remain the blocker: the default client output after Continue is
still black, and the opt-in adapter does not produce a recognizable level.

### Topology/scale audit and VU-source diff (2026-11-17, 23:00 CET)

I captured the adapter's per-vertex source data and the VU scratch output in
the same bounded run, filtered to source mesh `0x00a0e3c8` and DMA target
`0x016d5890`. The adapter's first vertices are
`(-7.683, 8.753, -59.207)` repeated, then `(7.683, 8.753, -59.207)`,
`(-7.683, 20.710, -58.689)`, … — a strip with a leading repeated vertex.
The VU stores at qwords 39,43,… from entry `0x401` are
`(9.174, 25.730, 42.290)`, the same value at qword 43, then
`(6.830, 25.740, 46.530)` — the repeated-vertex pattern is preserved, but
the values are not the adapter's source positions. Entry `0x401` is the skin
weight program, so those qwords are intermediate skinned data, not final
clip-space positions; no entry `0x356` store was captured in that DMA window.

I tested three topology/scale hypotheses on the full scene, all opt-in:
`OPENRAC_SLY_WORLD_SPLIT_RESTARTS` (split the strip where a vertex repeats the
current run's first pair), `OPENRAC_SLY_WORLD_SCALE` (10x source positions),
and raw triangle lists. None produced a recognizable level: the restart
split kept the fan pattern, scale 10 produced a large flat ground and a dark
polygon, and raw triangles were black. The adapter still does not reproduce
one retail object's silhouette. No launcher remains. Captures:
`/tmp/sly-topology-diff4-20261117/`, `/tmp/sly-restart-scene-20261117/` and
`/tmp/sly-scale10-20261117/`.

### Per-mesh matrix selection refuted (2026-11-17, 22:35 CET)

I tested the hypothesis that collision meshes (identity model matrix) need
matrix mode 0 while transformed objects need mode 1, via opt-in
`OPENRAC_SLY_WORLD_AUTO_MATRIX`. The bounded full-scene capture still shows
fan-shaped solid-colour geometry, not a recognizable level. There is no
per-mesh matrix convention that makes the current source-position adapter
produce a coherent scene. Capture: `/tmp/sly-auto-scene-20261117/`.
No launcher remains.

### Depth-state and native-input checks (2026-11-17, 22:20 CET)

I added `OPENRAC_SLY_WORLD_NO_DEPTH` to force depth test and depth writes
off for every world mesh. A bounded full-scene capture produced the same
fan-shaped gray geometry as the normal depth path, so depth state is not
the cause of the distortion. Native input is already wired: SDL events are
pumped in `port/runtime/sdl_renderer.cpp` (`g_input->update()`) and the game
reads them through `openrac_native_pad_read` in
`port/game/sly1-us/native/func_002042C8.cpp`; the process-local test press is
an additional diagnostic, not the only input path. No launcher remains.

Capture: `/tmp/sly-nodepth-scene-20261117/`.

### Topology experiment: raw triangle lists (2026-11-17, 22:10 CET)

A partial vertex dump (`OPENRAC_DUMP_SLY_MESH_VERTS`) showed that the
capture hook's `refs[].pos` indices come in repeating groups of three for
the character mesh (e.g. 0,1,2 then 3,4,5 then 0,1,6), suggesting the list
might be authored as independent triangles rather than a strip. I added
`OPENRAC_SLY_WORLD_RAW_TRIANGLES`, which calls `glDrawArrays(GL_TRIANGLES)`
directly over the source vertices, unlike the existing strip-expansion
mode. A bounded full-scene capture with this flag produced eight entirely
black frames, both with and without solid colours. The raw-triangle
interpretation is therefore refuted for this data; the earlier
strip-expansion comparison remains the only supported interpretation.

The dump also confirmed sane model-space source positions (symmetric
x/y pairs, z roughly -42..-7), so the fan-shaped output is not caused by
corrupt source vertices. No launcher remains. Captures:
`/tmp/sly-rawtri-scene-20261117/` and `/tmp/sly-rawtri-solid-20261117/`.

### Empirical matrix-mode audit of the opt-in adapter (2026-11-17, 21:55 CET)

I added an opt-in `OPENRAC_SLY_WORLD_MVP_MODE` (0-3) to the adapter to compare
the row-major product, its transpose, the reverse product and that transpose.
Four bounded captures each of request `0x005e5400` (class-2 lit/skinned
character, 38 meshes) and `0x005e4700` (class-4 collision geometry) gave:

| mode | character nonblack pixels (settled) | collision nonblack |
|---|---|---|
| 0 raw product | ~110-130 | ~5,350 |
| 1 transposed product | 112,203 / 190,352 / 301,782 | 0 |
| 2 reverse product | 0 | ~5,370 |
| 3 reverse + transpose | 367,574 / 600,282 | 0 |

Mode 1 turns the character from a ~100-pixel dash into a large coherent 3D
form (a limb/body surface); mode 3 is larger but more distorted. Modes 0 and
2 keep the flat collision sliver. This **contradicts the earlier 15:51 CET
conclusion** that the raw product was already correct: the empirical evidence
favours the transpose for the character meshes, so the adapter now defaults
to mode 1. The other modes remain available for comparison.

Related experiments in this round: capturing the `verts`/`ents` morph arrays
through the builder hook (`OPENRAC_SLY_WORLD_USE_SKINNED`) and splitting the
strip on `VertexRefs.flags & 0x80` (`OPENRAC_SLY_WORLD_SPLIT_STRIPS`) were
stable but did **not** change the visible result; the flag is the fog bias,
not a strip boundary. Partial vertex dumps show sane model-space positions
(symmetric x/y pairs, z roughly -42..-7), so the remaining distortion is in
the transform/topology rather than corrupt source data.

The full scene with mode 1 still shows fan-like distorted geometry, not a
recognizable level. The adapter remains opt-in and off by default; the
default client is unchanged and still black after Continue. No matched
decompilation source was edited. No launcher remains.

### PATH3 census closes the last append blind spot (2026-11-17, 21:30 CET)

I found a gap in the accepted-append census: GIF channel 2 (PATH3) DMA chains
are handed directly to `openrac_native_renderer_submit_dma_chain` by the
port-only `func_00202A80.c:14-32` and appended at
`port/runtime/sdl_renderer.cpp:833`, bypassing `gif_stream_write` and the
census hook. I added the same read-only summary at that append site with the
origin `gif-dma-chain` and reran the bounded 18-second capture with a
process-local Cross at frame 300.

The census now covers all three append paths: 691 GIF-DMA chains, 709 VIF
DIRECT appends and 2,696 VU1 XGKICKs. PATH3 does carry substantial XYZF2
traffic — 300 chains of about 206 KB with 376 XYZF2 writes each, and 336
chains with 40 each — but every PRIM observed is a sprite variant (0x6, and
values whose low three bits are 6), with no PRIM 3 or 4. The earlier
conclusion is therefore complete: **no triangle-list or triangle-strip XYZ
write reaches any native append path**, including PATH3. The missing world
geometry is not hidden in a queue the census could not see.

Capture: `/tmp/sly-path3-census-20261117/run.log`. The wrapper exited 137 and
no launcher remains. The default client still renders the no-card prompt and
goes black after Continue; no playable scene is confirmed.

### VU scratch position dump for the selected lit mesh (2026-11-17, 21:20 CET)

I added `OPENRAC_TRACE_SLY_VU_POSITIONS`, a bounded read-only dump at
`data_write()` for qwords 36-307, filtered with the existing VIF DMA address
window so it selects one submission. A bounded 18-second run with a
process-local Cross at frame 300 and target `0x016d5890` logged 2,176 stores:
1,088 from entry `0x401` and 1,088 from `0x411`.

The data is **not** clean clip-space geometry. Entry `0x401` writes qwords
39,43,...307 with x roughly 3.8-40, y -27-30 and z about 32-43 for the finite
records, but many lanes are `FLT_MAX` sentinels, and the fourth word is about
8.42e6 — a fog value, not a homogeneous W. Entry `0x411` is largely sentinel
values (about +/-1.8e19 or FLT_MAX). This confirms the backfill design's
caveat: a faithful VU-output renderer would have to filter culled vertices,
identify which program's parity is the final position set, reject the fog word
as W, and still apply the correct view transform and topology. Simply feeding
these qwords to GL with W division would be wrong.

I therefore did **not** implement the VU-output backfill. The opt-in adapter
remains unchanged and off by default. Capture:
`/tmp/sly-vupos-20261117/run.log`. The wrapper exited 137 and no launcher
remains. The default client still shows the no-card prompt and goes black
after Continue; no playable scene is confirmed.

For comparison, I also captured the best visible-object candidate, request
`0x005e5400` (class 2, lit+skinned, 38 meshes), with the existing adapter: it
is fully in-frustum (NDC `0.0613384,0.0243302..0.139417,0.0569177`, 1,506
vertices) but renders as only about 100 pixels, alternating with black
frames. That is consistent with the adapter's missing skinning. Capture:
`/tmp/sly-target-5400-20261117/`.

### Visible-geometry candidate and backfill design (2026-11-17, 21:10 CET)

Four more read-only audits narrowed the upstream question. Game-level culling
is not the cause: of 1,910 parts queued after Cross, the camera masks and
plane tests pass 23 and the alpha/fade gates are not firing (`fade=0`,
`zero=0`, `fade=3f800000`), so concrete mesh submissions do reach VU1. Of the
post-Cross draw requests, the best candidate for a visible object is
**request `0x005e5400`, index 21, owner `0x016d2fa0`**: class 2 (ordinary
PlacedObject, not the class-4 collider path), lit and skinned with four bones,
38 meshes, paired with a 47-strip companion from the same owner. Earlier
large-surface candidates (`0x005e4700`, `0x005e4980`, `0x005e4a00`) are all
class-4 collider geometry; `0x005e4700` is a flat sliver and `0x005e4a00` a
flat wedge. Its VU1 trace reaches XGKICK (rejected qword 469, accepted qword
35) but no XYZ-bearing kick is recorded for it.

A design study for an opt-in backfill concluded that the single faithful
seam is `data_write()` in `port/runtime/vu1_native.cpp`, where VU-transformed
clip-space positions are stored into qwords 36-307 before later programs
overwrite them. A capture keyed by submission/MSCAL/qword could feed GL
vertices with an identity MVP and W division, avoiding the source-position
adapter's unverified transform and missing skinning. It remains a diagnostic
that **bypasses** the missing retail PRIM-3/4 handoff, not a repair of it;
topology, vertex order, W semantics and depth convention are still unverified
and the default path must stay unchanged.

The authorized OpenRAC reference offers no shortcut: OpenRAC bypasses VU1 and
GS entirely with game-specific renderers, and its world renderers are
Ratchet-specific and cannot be reused. The sourced lesson is to work from
Sly's own draw-data boundary, which is what these audits now provide.

No source was changed for these audits. The next step is to decide, from the
request `0x005e5400` evidence, whether to build the opt-in VU-output capture
or to first capture that request's geometry directly. Keep the adapter and
all new diagnostics opt-in; no playable scene is confirmed.

### Clip decisions are mixed, not all-cull (2026-11-17, 21:05 CET)

I added `OPENRAC_TRACE_VU1_CLIP_DECISION`, a bounded read-only trace at the
CLIPw and FCAND execution sites in `port/runtime/vu1_native.cpp` (8192 lines,
submission/MSCAL/entry/PC, raw VF bit patterns, accumulated clip flags and the
FCAND result). A bounded 18-second run of the default launcher with a
process-local Cross at frame 300 produced 8,192 records: 4,577 from entry
`0x356` and 3,615 from `0x34a`. For `0x356`, FCAND wrote `vi1=0` in 1,386
decisions and `vi1=1` in 826; CLIPw's per-vertex `current` was zero in 1,537
cases and small nonzero values (0x15-0x2a) in the rest. Clipping is therefore
**mixed**: many vertices pass and some are culled. The "every vertex is
culled" hypothesis is refuted, consistent with the audit's finding that the
port's CLIPw tends to under-report clipping rather than over-report it.

The wrapper exited 137 and no launcher remains. Capture:
`/tmp/sly-clipdecision3-20261117/run.log`. Both new diagnostics are opt-in and
off by default; the default client still renders the no-card prompt and goes
black after Continue.

Every low-level hypothesis now examined is refuted or ruled down: VU PRIM
writes, JALR helpers, EE DIRECT world packets, VU upload completeness,
unhandled VIF commands, clipping, the GIF parser, the queue guard, XGKICK
validation, missing-function traps and material state. The remaining work is
to correlate the game-side world draw-request list with the specific requests
whose VU programs should feed a visible object, and to establish whether the
port is drawing only collision/background meshes rather than the level's
visible geometry. Do not promote the source-position adapter into the
default path on the strength of these negatives.

### EE path, VU uploads and unsupported VIF commands (2026-11-17, 20:55 CET)

Three further audits narrowed the paradox. The matched decomp has **no**
EE-built world DIRECT packet at all: every `WriteVifDirect*` caller is UI,
texture upload, screen fill or a HUD arc, and the sole PRIM-4 DIRECT tag in
the game is a HUD outline. The world path is VIF UNPACK/REF/MSCAL into VU1,
so there is no EE submission being blocked by a game-state gate. VU microcode
uploads are also complete: all 13 MPG sessions are accepted untruncated,
re-uploads overwrite correctly, and word dumps match the retail `.vutext`
(108/108 and 22/22 words). A clipping-culls-everything hypothesis is
undetermined; the port's CLIPw/FCAND/FCOR match PCSX2 for ordinary values and
its divergences tend to *under*-report clipping, i.e. to keep vertices.

I closed the remaining "a geometry stream is silently rejected" lead by
adding the opt-in `OPENRAC_TRACE_VIF_UNSUPPORTED_ALL`, which logs every
unhandled VIF1 command instead of only the first, with submission, command,
count, immediate and stream offset, capped at 256 lines. A bounded 18-second
run of the rebuilt default launcher with a process-local Cross at frame 300
logged **zero** unsupported commands. No VIF stream is being killed by an
unhandled opcode; that lead is refuted. Capture:
`/tmp/sly-vif-unsupported-all-20261117/run.log`. The wrapper exited 137 and
no launcher remains.

The next task is the bounded clip-decision trace proposed by the audit:
`OPENRAC_TRACE_VU1_CLIP_DECISION`, logging CLIPw inputs, accumulated clip
flags and the FCAND/IBEQ branch outcome per vertex in the `0x356` loop
(`vu1_native.cpp` CLIPw around :932-943, FCAND around :1462-1464, capped at
512 lines). That will establish whether every vertex takes the cull branch.
Keep all of this opt-in and port-only; the default client is still black
after Continue and no playable scene is confirmed.

### All VU helpers decoded; no PRIM writer exists (2026-11-17, 20:45 CET)

I decoded every JALR/BAL helper reachable from the batch entries. Helper `0x29b`
(667) is a per-vertex normal/lighting loop; helper `0x274` (628) is a clip-flag
check; helper `0x4a9` (1193) writes four scalar results; helper `0x5ef` (1519)
and helper `0x671` (1649) kick only the VIF-written state/tag region. None
writes PRIM 3 or 4, and none packs positions into a kicked packet. Helper
`0x671` does execute after the injected Cross (its kick PCs `0x6e2`, `0x6e8`,
`0x6f9`, `0x701`, `0x711`, `0x719` appear in live traces), but every kick is a
PRIM-0 state packet. The A+D register-address patches (`ISW.z` at `0x2da`,
`0x396`, etc.) are in the callers, not the helpers.

An exhaustive scan of all 13 MPG upload sessions in the retail `.vutext`
(VU PCs `0x000-0x755`, including re-uploads), all 25 XGKICK sites, all 34
`ISW` and 194 `SQ` writes, and every 16-byte-aligned tag found **zero** PRIM-3
or PRIM-4 writes and zero GIFtags with descriptor `0x04`/`0x05`. No VU program
in the game assembles a world triangle packet. The earlier "decode the JALR
helpers" task is therefore complete and its premise refuted.

The consequence is a change of direction: retail world triangles must be
submitted by the EE through VIF DIRECT (PATH2) or GIF DMA (PATH3), not by VU1.
The post-Cross census sees no such packet. The live questions are now (a)
whether EE-side world DIRECT packets are being built but blocked, and (b)
whether VU clipping or the port's float-flag model culls every vertex so the
EE's draw path never reaches a submission. Three read-only audits of those
questions are running. Keep the adapter opt-in and the default renderer OFF;
no playable scene is confirmed.

### Where the world triangles stop: VU scratch vs kicked packet (2026-11-17, 20:30 CET)

Sixteen parallel read-only audits converged on one concrete boundary. I decoded
the retail `.vutext` upload stream for the lit and unlit batch entries
(`0x2`, `0x51`, `0x242`, `0x356`, `0x34a`, `0x5f`) with PCs cross-checked against
live VU word dumps. Entry `0x356` stores transformed clip-space XYZ into VU data
memory at qwords 36/39/40/43/44/47/48 and nearby scratch; entry `0x242` writes
more vertex scratch. **No store in any of those programs writes qword 460 or
above.** The 460/468/476 region is written only by EE-side VIF UNPACKs, which
place the GIFtags; the VU programs only load it (`0x2` PC `0xa`, `0x5f` PC
`0x66`). The kicked packets therefore carry the copied VIF state/tag words and
never the computed positions. That is the missing producer/consumer link: the
positions stay in VU scratch while the kicked packet carries only GS state.

The only address-byte write in these programs is `ISW.z` at `0x356` PCs
`0x396/3a7/3b8/3c9`, which patches the A+D register-address word of the frame
header. No entry writes GS register `0x00` with PRIM 3 (triangle list) or 4
(triangle strip); the kicked tags all have PRIM 0. The intended retail primitive
is triangle-strip, but no PRIM-3/4 packet is ever produced in the native stream.

The remaining candidate is a VU helper reached by JALR from entry `0x34a` (PC
`0x34c`, target VI14=667) and from other entries (targets around 628, 1193 and
1649). Those helper bodies lie outside the six decoded entry ranges and were
not decoded. That task is now complete: the 20:45 CET section below shows no such
instruction sequence exists in the game's VU microcode. Do not change the VU
translator, the kick address, or the GIF parser on the basis of that premise.

Every alternative examined is now ruled down or refuted:

- FTOI/ITOF shift error is real (`vu1_native.cpp:986` gives 8/12 where 12/15 is
  required) but only `vftoi0`/`vitof0` execute on this path, with shift 0.
- Silent VU abort is refuted: failures are loud and fail the whole VIF stream;
  all six entries reach E-bit and use only implemented opcodes. Historical
  stops were instruction-budget limits.
- The XGKICK reserved-bit rejection is a host over-validation (hardware ignores
  tag bits 16-45), but the rejected qword 461/469 payloads are GS state with no
  XYZ; backstepping or relaxing it would forward about 1 MB of garbage.
- GIF parser carry is correct and matches PCSX2; the target-filtered probe found
  a clean EOP boundary (`tag_loaded=0`, `remaining_qwords=0`).
- There is no VIF UNLOAD command in the PS2 ISA. Both real VU1→GIF paths,
  XGKICK and DIRECT/DIRECTHL, are implemented.
- The 16 MiB queue guard drops whole buffers content-blind, but it fired only
  in the interrupted run after all eight frames were dumped; the corrected run
  never hit it, and world meshes bypass that queue entirely.
- No missing-function trap lies on the draw path (the only reachable one is
  `ScheduleAlarm`), and a trap would exit loudly, not drop triangles.
- Segment 612's 40-XYZ2 PRIM-6 packet is a HUD sprite list, not a world model.
- The five VU XYZ writes in the census are UI points at MSCAL offsets
  `0x57920/0x71220/0x579f0/0x712f0`, far outside the mesh window.
- Ignored material state (ALPHA1, FOGCOL, texture registers) affects appearance,
  not geometry; `TEST1` depth state is honored.
- Commits `8e44809` and `545ead7` changed only opt-in renderer paths and cannot
  affect the default client, which remains black after Continue.

A transient, unrequested source edit appeared during this audit (20:19-20:23)
that split the adapter's single strip using a `VertexRefs` flag bit. It built,
but the opt-in client crashed with SIGSEGV after Cross. The source was reverted
by 20:26; I rebuilt both targets from the clean tree and they are stable. No
such change is in the working tree.

For a future silhouette comparison, the best fully in-frustum candidate is
request `0x005e4700` (one mesh, source `0x00a0db58`, 31 vertices, NDC
`-0.607409,0.064102..-0.0760409,0.303055`, about 255x86 pixels), using the
existing request filter with FIRST_PASS=1 and SOLID_COLORS=1. I captured it on
the clean rebuilt ON binary: the filter matched, 31/31 vertices were inside,
the settled NDC was `-0.610377,0.0641558,-0.981956..-0.0763433,0.304376`,
and the image is a small flat reddish sliver of about 5,353 pixels — not a
recognizable object. This confirms the source-position adapter produces flat
fragments even for a fully in-frustum target; the earlier in-frustum request
`0x005e4a00` was likewise a flat wedge. Captures:
`/tmp/sly-target-4700-20261117/`.

### Accepted-kick XYZ census and three refuted leads (2026-11-17, 20:25 CET)

I added one opt-in, read-only diagnostic, `OPENRAC_TRACE_ACCEPTED_KICK_XYZ`,
that logs one bounded line per accepted GIF append at both append sites
(`kick_gif` in `port/runtime/vu1_native.cpp` and the VIF DIRECT path in
`port/runtime/vif1_dma.cpp`). Its first 64-line cap filled with startup sprites
before the injected Cross, so I raised the bound to 4096. The rebuilt
OFF/default binary ran 18 seconds with a process-local Cross at native frame
300; the wrapper exited 137 and no launcher remains. The census in
`/tmp/sly-accepted-kick-xyz2-20261117/run.log` covered 3,819 accepted appends
(718 VIF DIRECT, 3,101 VU1 XGKICK) without saturating the cap. Cross is logged
at line 614; 3,211 appends follow it.

Only four VU kicks carried any XYZ at all: two `XYZF2` and three `XYZ2`
writes, five point writes total, all with PRIM 0 and no triangle PRIM. The
post-Cross VIF DIRECT XYZ traffic is the same PRIM 6 sprites and PRIM 0x200
points already documented. No accepted packet, VU or DIRECT, contains a
triangle-list or triangle-strip XYZZ/XYZF2 write. This puts the loss upstream
of the GIF append: no world triangle packet is ever submitted, so the parser
and SDL renderer are not dropping it.

Three candidate explanations are now ruled down by parallel audits:

- **VU FTOI/ITOF shift error.** The code at `port/runtime/vu1_native.cpp:986`
  computes `(special - base) * 4`, giving shifts 8/12 where
  `port/tools/hostgen/asm.py:310-311` requires 12/15 for `vftoi12`/`vftoi15`.
  The arithmetic bug is real (8-16x mis-scale), but decoding the retail
  `.vutext` shows no `vftoi12`/`vftoi15` in the lit or unlit batch programs:
  only `vftoi0`/`vitof0` execute, and shift 0 is computed correctly. It cannot
  remove XYZ writes even hypothetically, so no fix is warranted for this
  symptom.
- **Silent VU abort on an unimplemented instruction.** Translate failures are
  loud, fail the whole VIF stream, and do not skip to E-bit
  (`vu1_native.cpp:1665-1689`, `1748-1772`; `vif1_dma.cpp:797-834`). Decoding
  entries `0x2`, `0x51`, `0x242`, `0x356`, and `0x5f` shows only implemented
  opcodes, and all reach E-bit. Historical stops were instruction-budget
  stops, not unsupported opcodes.
- **XGKICK reserved-bit rejection.** The `kGifReservedMask` check at
  `vu1_native.cpp:597-623` is a host over-validation: GIFtag bits 16-45 are
  unused on hardware (PS2Tek GIFtags; PCSX2 `Gif_Unit.h` names them
  `_dummy0/_dummy1`, and `_vuXGKICK` passes VI[is] directly with no tag/payload
  check). The guest `IADDIU vi01, vi06, 1` at PC `0xb` makes the kick from
  461 genuine. Relaxing the check would forward about 1.5 MB of state garbage
  and add no geometry, so strict parsing stays; it must not be described as
  hardware behaviour.

The one remaining upstream lead is a VIF path-2 / UNLOAD handoff from VU data
memory to GIF PATH1: if the retail mesh sends transformed vertices that way,
an unimplemented UNLOAD would explain why no XYZ packet is appended. That is
under investigation. Keep every diagnostic opt-in; the default client still
produces black after Continue.

Capture and trace: `/tmp/sly-accepted-kick-xyz2-20261117/run.log`,
`/tmp/sly-one-silhouette-20261117/run.log`,
`/tmp/sly-silhouette-attempt1-20261117/` and
`/tmp/sly-silhouette-attempt2-20261117/`. Two further bounded silhouette
attempts found request `0x005e4a00` fully in-frustum (NDC
`-0.408702,0.00783889,-0.980044..-0.118778,0.209965,-0.934758`, 260/260
vertices inside, 6,226 pixels in a 137x72 box in settled frames 04-07) but
still a flat reddish wedge;
`0x005e4e00` projected to 56 pixels and is not usable. No recognizable target
silhouette exists yet.

### One-object silhouette capture and adapter topology audit (2026-11-17, 20:10 CET)

I captured a flat-color filtered view of a single opt-in world object to check
the optional adapter's primitive topology and vertex order (`GL_TRIANGLE_STRIP`
over one shared `VertexRefs` list at `port/runtime/sdl_renderer.cpp:581`). The
solid-purple filtered single-object image is a flat-color filtered view of that
one object, not a color change of the whole opt-in scene; the unfiltered opt-in
scene is still the distorted gray horizon and stretched structures recorded
earlier.

A silhouette was captured, but it is clipped and unrecognizable, so it cannot be
compared with a retail reference yet. An in-frustum target must be chosen before
any silhouette comparison is meaningful: a clipped or off-screen object does not
test topology or vertex order. Resolving that topology and vertex order remains
the adapter's open question, and this capture does not settle it.

The retail-output task is unchanged: trace one representative visible model
through its VU1 execution and GIF output to find where its triangle packet
disappears, then compare that output with the adapter. Keep every diagnostic
opt-in and port-only, and keep the default renderer OFF.

### Corrected three-part visual audit (2026-11-17, 19:55 CET)

I finished the interrupted bounded capture. The corrected three-part audit was
performed while the branch was at `545ead7` and is recorded in commit `58e162b`;
the sibling decomp was not touched. Two corrections were needed first.

The stale capture was driven by one zsh orchestrator (PID 499811, started
17:19) that had only completed its `default-pre` stage before hanging. The
launcher ignores `SIGTERM`, so `timeout 8s` without `--kill-after` waited
indefinitely and left a zombie child. I killed the wrapper and its descendants;
no launcher remained. Re-running `timeout -s KILL` gave a reliable exit 137 on
each bounded stage.

More importantly, `OPENRAC_NATIVE_SLY_WORLD_RENDERER` is a **compile-time
option only**. `port/CMakeLists.txt:21-31` turns it into
`--native-sly-world-renderer` for `port/tools/compile_decomp_host32.py`, which
rewrites only `main/placed_object_model.cpp` to call the mesh bridges in
`port/sly1/native_host32/main.cpp` that feed `g_world_meshes` in
`port/runtime/sdl_renderer.cpp`. No source reads it from the environment. The
interrupted script used the `wave55-sdl32-release` binary (option ON) for all
three stages and set `OPENRAC_NATIVE_SLY_WORLD_RENDERER=1` only for the last,
so that variable did nothing and its "default" labels were wrong. Use two
distinct binaries instead.

Both on-disk launchers predated HEAD (`sdl_renderer.cpp` was newer than the
OFF binary), so I rebuilt both with
`cmake --build build/port/{wave55-sdl32,wave55-sdl32-release} --target
port_native_sdl_host32 -j4`; each succeeded with 99 missing-function traps.
Then I recaptured three genuinely separate cases under
`/tmp/sly-corrected-audit-20261117/`, 960x720, hidden window, one process-local
Cross at native frame 300 for the two post-input cases, and eight frames per
case:

- **OFF/default, no input:** all eight frames show the complete no-card
  Continue prompt (about 374,452 nonblack pixels); the hashes differ only in
  the animated highlight.
- **OFF/default, after Cross:** all eight frames are pixel-identical pure
  black (SHA-256 `768f866988705498...` for all eight). The log injects Cross
  at native frame 300 before the first dump. This confirms the known
  post-Continue failure on the rebuilt default binary; it is not a regression
  to startup black.
- **ON/release, after Cross:** all eight frames are pixel-identical distorted
  gray geometry, a horizon with stretched structures (SHA-256
  `dab343deece79ddf...` for all eight). This is the opt-in adapter, not the
  default path.

These offscreen captures cannot test desktop-compositor flicker. No launcher
remains. The visual-regression question from the paused session is now settled:
default post-Continue is black, opt-in output is the same distorted gray scene
recorded earlier. A single-object silhouette has now been captured, but it is
clipped and unrecognizable, so an in-frustum target must be chosen before any
comparison. Next, choose that target and, separately, trace one representative
visible model through VU1 and its GIF output to find where its triangle packet
disappears; the VU/GIF trace remains the retail-output task. Do not add renderer
features until that boundary is established and a usable silhouette is available.

### Correct strip-to-list comparison and whole-part probe (2026-11-17, 17:16 CET)

The earlier `GL_TRIANGLES` debug mode grouped each three source vertices. That
was not a valid conversion from a triangle strip and its changed fan pattern
was not evidence about the source topology. The optional adapter now expands
a strip into triangles with alternating winding. For source mesh
`0x00a0eb48` (85 vertices), a paired visible frame is pixel-identical between
strip and expanded-list draws. This rules out GL primitive selection for that
captured sequence, but does not establish the retail VU output. Paired captures
are in `/tmp/sly-valid-strip-list-20261117/`.

The mesh maps to draw request `0x005e4980` (draw index 9), class 4, unskinned;
its request has four meshes and the selected mesh has three material sources.
I added `OPENRAC_DEBUG_SLY_WORLD_REQUEST=<address>` to isolate the whole request.
The full request yields a larger clipped slab. Adjacent unskinned request
`0x005e4a00` yields a small fragment. Neither is recognizable world graphics.
Captures are in `/tmp/sly-full-part-20261117/` and
`/tmp/sly-adjacent-part-20261117/`.

The default `wave55-sdl32` build still has world rendering OFF and remains
black after Continue. Keep the diagnostic adapter opt-in. Next, identify a
complete visible world object from its owner and all of its parts, then
validate its transforms, material uploads, and screen bounds before changing
rendering behavior.
### Default and diagnostic client check (2026-11-17, 16:40 CET)

Eight settled default-client frames sampled 30 eligible presents apart all
show the no-memory-card Continue prompt. Only the Continue highlight changes.
This offscreen capture did not reproduce a full-frame flicker, but cannot
evaluate desktop-compositor flicker. The ordinary `wave55-sdl32` cache has
`OPENRAC_NATIVE_SLY_WORLD_RENDERER=OFF`; its known post-Continue result remains
black. The separate `wave55-sdl32-release` cache has the option ON and its
captured output is gray fan-like geometry. No default-path startup regression
was observed. The bounded run ended at timeout (137), and no launcher remains.

The optional adapter still submits a whole shared `VertexRefs` sequence as a
single GL triangle strip for every material pass. Its captured distortion is
therefore an adapter defect; it is not a reason to change the default path.
Matched source offsets point to VU-memory locations and do not define vertex
range boundaries. Derive primitive topology and pass coverage from one
retail object's VU/GIF output before changing this adapter. Captures:
`/tmp/sly-client-foreground-audit-20261117/` and
`/tmp/sly-native-world-strip-audit-20261117/`.

### Fresh client output audit (2026-11-17, 16:27 CET)

I reran the current default `wave55-sdl32` binary offscreen and inspected the
captured images. With no input, the full no-card Continue prompt is present;
its highlight animates and the known small white dash remains at the left
edge. A process-local Cross at native frame 300 was sampled across eligible
frames 280–315: the prompt shrinks during dismissal, then the display becomes
black. Eight consecutive later frames are pixel-identical black. This confirms
the known post-Continue black state, not a regression to startup black.

One separate short run captured a single frame of thin green lines after the
input. It did not recur in the longer run, which produced eight identical
black frames. Treat it as an unexplained transient capture, not stable world
graphics. These SDL offscreen captures cannot confirm or rule out flicker in
the desktop window. Contact sheets and logs are in
`/tmp/sly-client-verify-20261117-1624/`,
`/tmp/sly-client-transition-verify-20261117-1626/`,
`/tmp/sly-client-postcontinue-verify-20261117-1624/`, and
`/tmp/sly-client-postcontinue-verify2-20261117-1625/`. Runs ended at their
diagnostic timeouts (137); no launcher remains.

The input transition itself is captured now, so the next graphics task remains
the same: trace a representative world mesh through VU1 to determine where its
triangle output is lost. Do not treat the shrinking UI or transient green-line
frame as level rendering.

### Current visual and GS audit (2026-11-17, 16:08 CET)

I rebuilt and inspected the current default `wave55-sdl32` client using SDL's
offscreen driver. A no-input capture after 100 eligible frames shows the
complete no-memory-card Continue prompt, plus the known small white dash at the
left edge. After one process-local Cross at frame 300, eight captures beginning
at eligible frame 330 are black. This reproduces the known prompt-to-black
transition; I found no new default-path regression. Offscreen captures cannot
confirm or rule out the desktop-window flicker from the user's recording.
Images and bounded-run logs are in `/tmp/sly-visual-page-20261117/` and
`/tmp/sly-visual-recheck-20261117/`.

The default `wave55-sdl32` client still reaches the same black post-Continue
state. I added opt-in `OPENRAC_TRACE_GS_XYZ_SUMMARY=1`; it reports each GIF
input call that writes XYZ2/XYZF2, without the prior 64-write global cap. With
`OPENRAC_TRACE_GS_XYZ=1`, VU origins include submission, MSCAL offset, and VU
entry. VIF DIRECT origins include submission and byte offset. This is
port-runtime diagnostics only and does not change guest packet handling.

One process-local Cross at frame 300 produced 56 XYZ-writing GIF input calls
in the bounded post-input capture: 52 VIF DIRECT and four VU1 XGKICK calls. A
per-write primitive counter proves those calls wrote 5 points
(`PRIM & 7 == 0`) and 1,354 sprites (`PRIM & 7 == 6`), with zero triangle-list
or triangle-strip writes (3 or 4). The four VU1 packets write only points:
submission 326 at MSCAL offsets `0x57920` (entry `0x2`, two points) and
`0x71220` (entry `0x6da`, one point), then submission 327 at `0x579f0` (entry
`0x2`, one point) and `0x712f0` (entry `0x6da`, one point). These offsets are
outside the selected Sly mesh window `[177312,182688)` and do not establish
model geometry. The accepted XGKICK packets previously traced for that
selected mesh contain only GS state.

This is the clearest current explanation for the blank world: after Continue,
the native GS input sees UI points/sprites and a few VU points, but no world
triangles. It does not yet identify why the retail mesh path fails to emit
triangle primitives or whether the native VU/GIF handoff drops the triangle
packet before the parser. Next, correlate a representative `DrawModelPart`
range with the per-call summaries and inspect the VU packet's PRIM writes
before changing the parser or renderer. The authoritative per-write capture
is `/tmp/sly-gif-xyz-prim-accurate-20261117/run.log`; the earlier origin
capture is `/tmp/sly-gif-xyz-context-20261117/run.log`. The latest bounded run
ended at 14 seconds (137), with no launcher left running.

### Matrix-convention audit of the opt-in adapter (2026-11-17, 15:51 CET)

I tested whether the experimental world adapter's distorted geometry came
from matrix order or OpenGL packing. The matched decomp settles the convention:
`BuildProjectionMatrix` places the projective terms at row-major `m[2][3]`
and `m[3][2]`, captured model transforms put translation in row 3, and
`BuildViewProjectionMatrix` combines the view and projection in that same
layout. The game transforms row vectors, so its final transform is
`model * viewProjection`. Passing the row-major product buffer to OpenGL as
column-major gives the transpose required by the GLSL column-vector multiply.
The pre-existing adapter code already did this; the apparent packing/order
bug was a false lead.

Two temporary opt-in builds changed the product order and/or packing and were
captured after one process-local Cross. Both remained unrecognizable gray
geometry; the changed order produced near-zero NDC bounds. Those experiments
did not improve rendering and their source edits have been reverted. They did
not change the default renderer, which remains OFF. Captures/logs are under
`/tmp/sly-world-matrix-fix-20261117/` and
`/tmp/sly-world-matrix-order-fix-20261117/`; the latter launcher ended at its
14-second timeout (137) and no launcher remains.

Do not revisit the matrix convention or repeat the global XYZ capture. Next,
map a representative retail `DrawModelPart` and its DMA/VIF range through the
matched strip builder, VU program, XGKICK/DIRECT output, and the new per-write
PRIM trace. The goal is to find whether triangle packets are never emitted or
are lost before reaching the GIF parser. Keep the incomplete source-position
adapter disabled by default.

### Re-audit of the selected lit-mesh finish (2026-11-17, 15:41 CET)

For submission 322, DMA segment 287 begins the selected lit/skinned mesh at
stream offset 177312. At offset 182272, VIF UNPACK writes a 71-qword packed A+D
GIF packet at VU qword 460. The `g_litBatchFinishProgram` symbol is the retail
VU1 program at entry `0x2` (`D_00000010`). Its captured words show the first
XGKICK is intentional: PC `0xb` (`0x10013001`) computes `VI1 = VI6 + 1`, then
PC `0xc` (`0x80000efc`) issues `XGKICK VI1`; the live state is `VI6=460`, so
the requested address is 461. The parser therefore reads the first A+D payload
qword as a tag and rejects it. This is what the retail microcode requests, not
an off-by-one from the host C writer or an accidental native VI calculation;
do not normalize it to qword 460. The same program later kicks qword 35, which
is accepted and contains A+D state only, with no XYZ2/XYZF2 writes.

The EE VU manual specifies that XGKICK passes VI[is] as the leading GIFtag
address, and PCSX2's current implementation passes the raw VI value to GIF
packet-size validation and cancels a transfer when the tag is invalid. That
supports treating this particular rejected transfer as non-drawable rather
than as a port arithmetic bug. It still does not tell us why the retail
microcode attempts it, and its relationship to the running GIF path is not
fully modeled here. The accepted qword-35 state packet is not the missing world
geometry; continue tracing the other draw paths until a packet with XYZ2/XYZF2
is tied to the world model and GS.

The separate rejected qword-469 warning comes from another VU finish path and
remains unattributed to this selected mesh. Do not conflate it with the
qword-461 attempt. Exact lines are in
`/tmp/sly-lit-final-qword-audit-filtered-20261117/run.log` (submission 322,
stream offsets 182272 and 182672); the earlier instruction capture is in
`/tmp/sly-finish-entry2-audit-20261117.log`.

Reference material: [Sony VU User's Manual, XGKICK](https://docs.alexrp.com/mips/ee_vu.pdf)
and [PCSX2 VUops.cpp](https://github.com/PCSX2/pcsx2/blob/master/pcsx2/VUops.cpp#L1818-L1922).
The canonical decomp checkout remains clean at `8f7a85a3`, matching
`origin/master`; the active `wave55-sdl32-release` CMake source snapshot is an
exact file copy of that checkout.

### Fresh default-client audit and mesh finish trace (2026-11-17, 14:03 CET)

I rebuilt the default `wave55-sdl32` client from the current branch with the
optional Sly world renderer OFF. Its framebuffer shows the no-card prompt. A
process-local Cross at native frame 300 reaches the same transition as before;
eight captured frames for native presents 318–325 are all black. This
reproduces the known post-Continue failure, with no new visual regression in
the default path. The run used SDL's offscreen driver, so it does not assess the desktop
compositor or the flicker in the user's visible-window recording. The captures
and logs are under `/tmp/sly-latest-current-audit-20261117/`,
`/tmp/sly-latest-postcross-20261117/`, and
`/tmp/sly-latest-transition-20261117/`. The bounded launches ended with exit
137 and left no launcher running.

The fresh build compiled 194/194 staged decomp units and 85/85 port backfills;
the host link catalogs 4,400/4,807 function entries, with 95 unresolved
addresses and 99 missing-function traps. Those are port link/runtime gaps, not
unfinished decompilation. The sibling decomp checkout is clean at `8f7a85a3`,
matching `origin/master`. No decomp source changed.

The capture-window correction matters: the first eight-frame sample filled its
limit before the injected press, so it only showed startup. The corrected run
pairs each screenshot with present-frame logging and confirms the post-Continue
black output specifically at frames 318–325. Its log and contact sheet are under
`/tmp/sly-latest-verified-frames-20261117/`.

The selected kind-0 lit mesh is submission 322, DMA segment 287, stream range
`[177312,182688)`. Its finish entry `0x2` at stream offset 182672 kicks from
qword 461. The preceding UNPACK writes a valid 71-qword A+D GIF packet at
qword 460; qword 461 is its first payload word. The kick is rejected when the
strict parser reads that payload as a GIFtag. The same entry later accepts an
A+D state packet from qword 35. Neither packet contains XYZ2/XYZF2 writes, so
this trace identifies no drawable world geometry and is not a reason to shift
the kick address. This is distinct from the older kind-5 qword-469 warning;
that warning is not evidence for this selected mesh. Exact log:
`/tmp/sly-target-mesh-all-mscal-20261117/run.log`.

Next: trace the selected model's other finish/direct path until a retail
XYZ2/XYZF2 packet is tied to that model and reaches the GS. Keep the optional
source-position world adapter disabled by default; it produces distorted
geometry and does not yet reproduce the retail VU output.

### Explicit VU submission filter (2026-11-17, 13:55 CET)

The `OPENRAC_TRACE_VIF_DMA_ADDRESS` filter can be invalidated before a delayed
XGKICK dispatch when nested DMA changes the active VU target window. An
explicit `OPENRAC_TRACE_XGKICK_SUBMISSION` now takes precedence over that
address window; pair it with `OPENRAC_TRACE_XGKICK_MSCAL_OFFSETS` to retain
submission and command-offset provenance. The opt-in renderer build passed
(194/194 staged units, 85/85 port backfills, 99 missing-function traps).

With the address window active, the selected mesh was submission 322 and its
`0x411` program ran at VU trace offset `0x2c4a0`, completing after 1800 cycles.
No parser acceptance/rejection record was emitted for that invocation, so the
trace does not show that this program submits the final GIF packet. A follow-up
run intended for the later `0x356` call did not reach the selected mesh before
the 12-second timeout; it only logged startup DIRECT traffic. Generic
malformed-tag warnings therefore remain unattributed. Logs:
`/tmp/sly-target-mesh-vu-offset-20261117/run.log`,
`/tmp/sly-target-mesh-vu-parser-20261117/run.log`,
`/tmp/sly-target-mesh-vu-parser2-20261117/run.log`,
`/tmp/sly-target-mesh-vu-accepted-20261117/run.log`, and
`/tmp/sly-target-mesh-vu356-20261117/run.log`.

Superseded by the exact selected-mesh finish trace above: its `0x2` finish call
and both packet contents are now captured.

### VU trace target lifetime fix (2026-11-17, 13:48 CET)

The VU XGKICK target window was kept thread-local after its matching VIF DMA
submission. Since later DMA chains restart their stream offsets at zero, an
unmatched chain with the same offsets could inherit stale target provenance.
`vif1_dma.cpp` now updates or clears the VU target window for every successful
DMA decode. The opt-in renderer build passed (194/194 staged units, 85/85
port backfills, 99 missing-function traps). A 12-second offscreen run matched
mesh address `0x016d5890` to submission 322 and its narrow stream window
`[177312,182688)`; the VU finish call still needs a clean callsite-specific
capture before assigning any generic XGKICK rejection to this mesh. No
decomp source changed. Log: `/tmp/sly-target-mesh-exact-20261117/run.log`.

Superseded by the exact selected-mesh finish trace above. The actual drawable
XYZ output for this model remains to be found.

### Mesh packet provenance correction (2026-11-17, 13:40 CET)

The broad trace followed the frame's 773-segment VIF chain and mixed several
parts' draw packets. The mesh packet run begins at DMA segment 286, stream range
`[177312,182688)`: segment 287 is tag `0x003c8560`, data `0x016d5890`, QWC
141. It contains VIF UNPACKs and no VIF DIRECT command.

Opt-in instrumentation in `compile_decomp_host32.py` adds range callbacks to
the generated host32 staging source around `DrawModelPart` and each mesh or
strip writer loop. The target tag maps to request `0x005e5400`, part
`0x016d4460`, mesh 0 of 38, a lit and skinned part. Mesh 0's writer range is
`[0x003c8380,0x003c85d0)` and includes the tag; the detached payload is at
`0x016d5890`. This establishes provenance from the decomp call to the DMA
input without changing the sibling decomp repository.

The wide trace's 44-QWC DIRECT packet at segment 612 (40 XYZ2 writes, PRIM 6)
and two-QWC packet at segment 771 belong to a different part: request
`0x005e5480`, part `0x016d44d0`, unlit with 47 strips. Likewise, broad VU XYZ
observations at segments 587 and 723 are outside the selected mesh range. None
of these observations establish that the selected mesh reaches the GS.

The opt-in DIRECT-origin label in `vif1_dma.cpp` remains useful for narrowed
captures. The host32 build passed with 194/194 staged units, 85/85 port
backfills, and 99 missing-function traps. Staged changes are diagnostic only;
no sibling decomp source changed. Bounded offscreen captures ended at timeout
137 and left no launcher process. Logs:
`/tmp/sly-target-direct-xyz-20261117/run.log`,
`/tmp/sly-focused-model-direct-20261117/run.log`,
`/tmp/sly-model-part-range-20261117/run.log`,
`/tmp/sly-model-part-target-map-20261117/run.log`, and
`/tmp/sly-model-part-item-map-20261117/run.log`.

Superseded by the exact selected-mesh finish trace above. Continue with the
separate finish/direct path; broad frame-chain matches still include unrelated
draws.

### Rebuilt default-client output (2026-11-17, 13:14 CET)

Rebuilt `wave55-sdl32` from current source with
`OPENRAC_NATIVE_SLY_WORLD_RENDERER=OFF`. Eight delayed framebuffer samples
show the complete memory-card warning; the Continue highlight animates. Eight
earlier eligible samples, taken before that UI draw, are black. This is a
startup interval, not evidence of alternating black frames after the prompt
appears. A separate process-local Cross at frame 300 produced prompt samples
followed by a black frame, reproducing the known post-Continue failure. The
current default client therefore has not regressed at startup, but it still
does not display the level. These captures use SDL's offscreen driver and do
not determine whether the desktop-window flicker from the user's video is
present. Both runs ended on their bounded timeout (exit 137); no launcher
process remained. No matched decomp source changed.

The framebuffer samples and logs are local under
`/tmp/sly-current-audit-20261117/`. Build:
`cmake --build build/port/wave55-sdl32 --target port_native_sdl_host32 -j4`
(success; 194/194 staged decomp units, 85/85 port backfills, 99
missing-function traps).

### Three lit-packet windows and rejected-kick correction (2026-11-17, 13:05 CET)

The addresses `0x016d5890`, `0x016d6160`, and `0x016d66d0` are the three
detached VIF packet buffers on the sampled `ModelStrip`, with QWC sizes 141,
87, and 77. They are not three `FrameSource` records. The matched
`BuildLitStripMeshPacket` builds these three streams, and `DrawModelPart`
combines them with finish programs. Their selected DMA windows overlap and
reach the same finish call at stream offset `0x2c990` (decimal 182672).

In repeated submissions, that finish call rejects XGKICK address 461, then
accepts address 35. A bounded snapshot proves address 461 is exactly one
qword after a valid 71-qword packed A+D packet beginning at 460. Four
repeated snapshots are byte-identical (SHA-256
`b7988f273b7141e471902508598e79a11fb05bad486982e4c3a908cdf1fa6de7`); the
packet contains no XYZ2/XYZF2 destinations. The accepted 1,136-byte packet at
35 is also a 70-loop A+D state packet, with no XYZ destinations. Neither
observation justifies changing the kick address: both sampled packets are
state, not drawable geometry.

The filtered source windows show MSCAL entries `0x401`, `0x411`, `0x242`,
`0x356`, then `0x2`; only the final entry kicks in the selected range. This
does not establish that all world geometry lacks XGKICK output. It establishes
that this strip's sampled lit-packet finish is not the drawable vertex source.
Next trace the separate finish/direct path in `DrawModelPart` and its VIF
DIRECT/GIF submissions until the first XYZ2/XYZF2 writes are tied to the
queued mesh. Keep the experimental adapter disabled by default.

The matched source review was read-only. New logs and snapshots remain local
under `/tmp/sly-vu-lit-mscal-sequence-20261117/`,
`/tmp/sly-vu-rejected-late-packet-20261117/`,
`/tmp/sly-vu-source6160-confirm-20261117/`,
`/tmp/sly-vu-source66d0-20261117/`, and `/tmp`. No decomp source changed.

### Targeted world VU/GIF trace (2026-11-17, 12:56 CET)

Rebuilt the opt-in `wave55-sdl32-release` client and captured eight paired
world frames after one process-local Cross press at native frame 300. The first
world frame is a gray horizon; subsequent frames show the same two stretched,
distorted structures over the horizon. The stable symptom is therefore
reproducible in the opt-in adapter. The default renderer remains OFF and still
has no level draw after Continue.

Filtered the VIF1 chain containing source address `0x016d5890`: submission 322,
segment 287, stream range `[177312,182688)`. In that range, VU entries `0x401`,
`0x411`, `0x242`, and `0x356` write transformed vectors and per-vertex fields
into VU data memory. Entry `0x2` at MSCAL offset `0x2c990` then rejects a kick
at address 461 and accepts a 1,136-byte kick at address 35. Its GIF tag is a
70-loop, packed A+D tag. Decoding all 70 A+D address bytes in the captured
packet gives 53 writes to GS address `0x00` (PRIM) and 17 to `0x80`; it contains
no XYZ2/XYZF2 destinations. The accepted-packet parser verifies GIF structure,
not that the packet contains drawable geometry. This is evidence that this
specific accepted kick is not the world-vertex packet; it does not establish
that no other kick or DMA chain submits the geometry.

The opt-in adapter still renders the level from source `VertexRefs` positions,
not the observed VU output. This trace does not justify treating its distorted
surfaces as a faithful rendering. Next, enumerate accepted kicks from the same
selected stream and adjacent world submissions, decode their GS register
destinations, and identify the packet that carries XYZ writes before changing
the VU translator or renderer. Keep the adapter disabled by default.

Logs, captures, and the accepted kick are local under
`/tmp/sly-vu-packet-store-trace-20261117/`,
`/tmp/sly-vu-submission-kicks-20261117/`, and
`/tmp/sly-vu-mesh-capture-20261117-3/`; none are repository inputs. The
offscreen launch ended with timeout exit 137 and left no launcher process.
Build compiled 194/194 decomp units and 85/85 port backfills; no matched
decompilation source changed.

### Per-present texture and world-frame check (2026-11-17, 12:45 CET)

The gray adapter capture prompted a follow-up on texture uploads. The previous
material log was a first-lookup record: it reported 66 of 69 material addresses
as placeholders in one run. A new per-present counter shows that result is not
stable. In this fresh opt-in run, all 69 unique material addresses resolved to
uploaded textures on world-present frame 0; every later stable sample had all
54 addresses uploaded and zero missing. The frame remains distorted gray at
`/tmp/sly-world-texture-counts-20261117/frame-07.png`. Therefore missing
texture uploads do not explain this run's image. The actual material decode or
the geometry path can still be wrong; uploaded status alone does not verify
texture pixels, palette, alpha, or GS state.

Added a read-only, single-TBP diagnostic: set
`OPENRAC_TRACE_TEXTURE_MISS_TBP=0x2a80` to log the closest uploaded GS range
when that exact TBP misses. In the traced run, TBP `0x2a80` resolved as a
64x128 upload and emitted no miss. The diagnostic is disabled by default.
Run log and frames: `/tmp/sly-world-texture-counts-20261117.log` and
`/tmp/sly-world-texture-counts-20261117/`; material trace:
`/tmp/sly-texture-miss-2a80-traced-20261117.log`.

Next: trace one representative mesh's final retail VU1 output and compare its
positions/order with the adapter's source `VertexRefs` list. The current
triangle-strip approximation and base vertex positions remain unverified.

### Post-Continue world-path trace (2026-11-17, 12:32 CET)

I rebuilt the default `wave55-sdl32` target (`OPENRAC_NATIVE_SLY_WORLD_RENDERER=OFF`)
with a capped, opt-in VU qword-write trace, then ran it offscreen with a
process-local Cross at native frame 300. The capture after the press is fully
black (`/tmp/sly-qword-consumer-20261117/frame-02.png`), so the default client
still fails at the same visible milestone. The input stayed inside the
launcher and did not send a desktop key event.

The trace adds evidence that the game is doing work after Continue: it loaded
the world object (`world=0x6abdd0`), read six disc blocks (2,426 sectors), and
`QueueModelPartDraws` reported 23 queued parts per call in the sampled frame.
That means “world code is not running” is not a sufficient explanation for
the black screen. In the VIF chain containing source address `0x016d5890`
(submission 327), entries `0x401` and `0x411` write VU data; `0x242` and `0x356`
then rewrite it; entry `0x2` updates qword 35 to an A+D GIFtag and clears
qwords 37–85. The adjacent packet at qword 469 is still rejected for reserved
GIFtag bits, and the accepted qword-35 packet remains state-only. These facts
do not yet establish that the final consumer or normal renderer is the cause:
the default build's world-mesh adapter is OFF, and this trace does not capture
all GS input paths.

A follow-up compared the two rendering paths directly. In the default build,
the DirectRenderer receives the small UI packets: the transition has
1,224/870 decoded vertices, followed by a 943,776-byte transfer packet with no
vertices or draws; later packets decode only 6- or 36-vertex corner elements.
The world meshes are in a separate host queue, so disabling the experimental
world adapter leaves no level draw in the default client. With the adapter ON,
the same post-Continue path submits 234 meshes / 8,955 vertices per stable
sample and presents the captured distorted gray scene at
`/tmp/sly-world-pass-20261117/frame-07.png`. Its projected bounds are mostly
inside the view, so simple camera clipping is not the whole issue. The first
material trace appeared to show 66 placeholder textures, but the subsequent
per-present check above supersedes that snapshot: all unique textures were
uploaded in the fresh run, and the scene remained distorted. The adapter also
uses untransformed source mesh positions and approximates each vertex list as
one triangle strip; it does not yet reproduce the retail VU output.

The new diagnostic is disabled unless
`OPENRAC_TRACE_VU1_STORE_QWORD=first-last` is set. Pair it with
`OPENRAC_TRACE_VIF_DMA_ADDRESS=0x016d5890` and the VIF segment-follow setting
to cap output to a selected VU data range. Run log and captured frames:
`/tmp/sly-qword-consumer-20261117.log` and
`/tmp/sly-qword-consumer-20261117/`. The DirectRenderer comparison is in
`/tmp/sly-direct-bounds-20261117.log`; the opt-in adapter captures and bounds
are in `/tmp/sly-world-pass-20261117.log` and
`/tmp/sly-world-pass-20261117/`. The source/camera matrix trace is in
`/tmp/sly-world-transform-20261117.log`. Build compiled 194/194 decomp units
and 85/85 port backfills; no matched decomp files changed.

Next: compare one adapter mesh's source vertices against its retail VU1 output
and preserve the retail vertex order and transforms in the adapter. Keep the
optional mesh adapter disabled by default until it produces a recognizable
scene.

### Default-client visual recheck (2026-11-17, 12:03 CET)

I rebuilt `wave55-sdl32` from the current branch and confirmed
`OPENRAC_NATIVE_SLY_WORLD_RENDERER=OFF`. Eight steady no-input captures show
the full no-card prompt and Continue highlight, with the known small white dash
at the left edge. After a process-local Cross at native frame 300, eight
samples starting after frame 350 are completely black. The trace repeats the
qword-469 rejected XGKICK and the accepted qword-35 state packet; that sequence
is not yet causal proof. The current default client therefore reproduces the
known startup prompt and known black post-Continue state. The gray distorted
scene from the prior experiment belongs only to the separate opt-in renderer.
The capture used SDL's offscreen driver, so it says nothing about visible
desktop flicker. Both bounded launches ended with timeout exit 137 and left no
launcher process. Captures/logs:
`/tmp/sly-default-fresh-noinput-contact-20261117.png`,
`/tmp/sly-default-fresh-check-20261117/`,
`/tmp/sly-default-fresh-check-20261117.log`, and
`/tmp/sly-default-fresh-postcross-20261117.log`.

### Multi-source material-pass experiment (2026-11-17, 11:54 CET)

The previous 11:30 note called the `FrameSource` entries separate geometry
strips. A targeted native trace after process-local Cross at frame 300 corrects
that: one representative kind-0 mesh (`0x00a0db58`) has one shared 31-vertex
`VertexRefs` list and three 8-qword `FrameSource` packets. The sources
share the same six-register GIF A+D layout while `TEX0_1` changes per pass to
`0x2003e5c5d940aa80`, `0x2003e5e5d940aa80`, and `0x2003e605d940aa80`. A second
captured mesh (`0x00a0d4a0`) confirms per-source `TEST1` values `0x717eb`,
`0x53001`, `0x53001`, and `ALPHA1` values `0x44`, `0x48`, `0x48`. A separate
83-vertex mesh's header recorded its source offsets as `451`, `459`, and `467`.
This is strong evidence the sources hold material/state passes over shared geometry;
they do not partition `VertexRefs` into independent vertex spans. The earlier
direction to reconstruct one geometry boundary per source was incorrect.

The opt-in adapter now captures each valid source packet and submits the
shared geometry once per source. The trace shows 276, then 237–234 submissions
per sampled frame, versus about 80 before. The settled framebuffer changed
from purple surfaces to gray distorted surfaces, still without a recognizable
level. This is a measured output change, not a correct renderer yet: GL
blending/alpha test and the exact VU/GS draw behavior remain unverified. The
adapter still approximates each `VertexRefs` list as one `GL_TRIANGLE_STRIP`.
Keep the renderer option OFF by default. Next, correlate each `StripUnpack`
source offset with its VU output and the matching GS `PRIM`, `ALPHA`, `TEST`,
and texture state before adjusting raster behavior.

Build: `cmake --build build/port/wave55-sdl32-release --target
port_native_sdl_host32 -j4` succeeded (194/194 decomp units, 85/85 port
backfills, 183 compiler warnings, 99 missing-function traps). The offscreen
12-second run injected one Cross press at native frame 300, dumped eight
frames, ended with timeout exit 137, and left no launcher process. The build
uses `OPENRAC_NATIVE_SLY_WORLD_RENDERER=ON`; the default renderer was not
changed. Captures and trace:
`/tmp/sly-multipass-capture-20261117/contact.png`,
`/tmp/sly-multipass-visual-20261117.log`,
`/tmp/sly-multipass-run-20261117.log`, and
`/tmp/sly-strip-register-audit-20261117.log`. No matched decomp files changed.

### Fresh client-output and trace audit (2026-11-17, 11:30 CET)

I inspected the current actual framebuffer captures, including the settled
frames after one process-local Cross at native frame 300. The prompt visibly
contracts and overlaps during the transition. Later samples show a black
interval, then a gray horizon and distorted purple surfaces. The transition
artifact appears in native framebuffer captures; sparse samples do not
establish whether the desktop window flickered between presents or whether the
same transition occurs in PCSX2. The settled
gray/purple frames are from the experimental `wave55-sdl32-release` build with
`OPENRAC_NATIVE_SLY_WORLD_RENDERER=ON`. The default configuration remains OFF
and its last settled post-Continue capture is black. These are separate client
configurations and must not be described as one renderer producing both
results.

The normal DirectRenderer's post-Continue packet audit is separate from that
adapter image. It decoded 7,383,744 bytes into 76,053 vertices and 3,157 draws;
the computed bounds span NDC x `-11.467..14.931`, y `-33.504..23.699`, with
889 vertices inside clip space. This confirms the normal path receives a large
batch, but the packet audit alone does not establish what the desktop window
presented.

The opt-in adapter then read only `sources[0]` for material state and copied
the full `VertexRefs` list as one `GL_TRIANGLE_STRIP`, while the matched lit
and unlit decomp builders queued every `FrameSource` at a distinct VU offset.
The 11:54 follow-up below
shows these are state passes over shared geometry, not separate geometry
spans; it supersedes the earlier geometry-boundary interpretation. This
adapter mismatch did not explain the normal DirectRenderer's black frame.

There is also a trace-limit correction: the all-input-path XYZ run filled its
shared 64-write capture budget with startup `vif1-direct` sprite writes before
Cross at frame 300. Treat it as saturated; it cannot establish post-input XYZ
traffic. The filtered VU-only run recorded five XYZ writes, which is likewise
not a complete view of VIF DIRECT or PATH1. Logs:
`/tmp/sly-all-input-path-draw-audit-20261117.log` and
`/tmp/sly-vu-only-world-xyz-audit-20261117.log`.

Visual captures: `/tmp/sly-actual-postcontinue-20261117/contact.png` and
`/tmp/sly-actual-settled-postcontinue-20261117/contact.png`. Their run logs
are the matching `*-20261117.log` files in `/tmp`. The decomp checkout was
fetched and checked at `8f7a85a3`; it had no new commits or working-tree
changes. No decomp files were edited.

At 11:30 no source changes had been made during that audit. The follow-up
experiment and its port-only changes are documented above. The commit email
is configured as `oeps46@gmail.com`.

### Rejected kind-5 packet snapshot (2026-11-17, 11:22 CET)

Added an opt-in, read-only snapshot at the first-invalid-GIFtag rejection in
`kick_gif`. With `OPENRAC_DUMP_REJECTED_XGKICK_PREV_PACKET=1`, it saves up to
four bounded packets to `/tmp` from the qword immediately before the rejected
XGKICK address. It does not adjust the address or submit the snapshot to the
renderer. The 10-second native run captured the kind-5 path at entry `0x5f`,
PC `0x68`: the rejected address is qword 469, while qword 468 contains a valid
110-qword EOP packet (`0x100040000000806d`, 109 A+D writes). All four snapshots
have the same SHA-256 and decode identically. Their A+D destinations are PRIM
(`0x00`, 104 writes), TEX0_2 (`0x07`), CLAMP_2 (`0x09`), TEX1_2 (`0x15`), plus
two reserved/unrecognized addresses (`0x43`, `0x48`). None target XYZ2
(`0x05`) or XYZF2 (`0x04`), so this packet is GS setup state and cannot explain
the missing world geometry. The separate PC `0x6e` kick at qword 35 remains
accepted. This confirms the preceding-tag interpretation but still does not
justify retrying at qword 468: that would submit state, not vertices.

Build succeeded for `port_native_sdl_host32` (194/194 decomp units and 85/85
port backfills). The bounded launch ended with timeout exit 137 because the
launcher did not exit on SIGINT; no launcher remained afterward. Log:
`/tmp/sly-first-packet-window-dump-20261117.log`; snapshots:
`/tmp/sly1-vu1-rejected-prev-packet-00.bin` through `-03.bin`.
Instrumentation is host-side and disabled unless the environment variable is
set; no matched decompilation source changed.

### Fresh current-client visual review (2026-11-17, 10:46 CET)

I rebuilt the default host32 launcher with diagnostic-only trace changes and
captured eight actual framebuffer images during a process-local Cross press at
native frame 300. The first captured frame shows the complete no-card prompt;
the following sampled frames show that prompt shrinking toward the center.
The dump limit ended while the prompt was still visible, so these eight frames
do not themselves establish the settled post-Continue image or explain whether
the shrink is the intended transition. The prior settled post-Continue
captures remain black. Contact sheet and raw images:
`/tmp/sly-current-client-review-20261117/contact.png` and
`/tmp/sly-current-client-review-20261117/sly1-native-frame-00.ppm` through
`sly1-native-frame-07.ppm`; bounded run log:
`/tmp/sly-current-client-review-20261117.log`.

The same fresh run traced the full 773-segment VIF1 chain containing the
dynamic target address `0x016d5890` (submission 326, selected stream range
`[177312,497328)`). With the renderer's XGKICK origin filter, it reports three
writes to XYZ2/XYZF2 at MSCAL offsets `0x57920` and `0x71220`; the matching
later submission 327 has one more at `0x712f0`. The broad follow range includes
many other draw packets: offsets `0x57920` and `0x71220` map to segments 587
and 723, while the selected mesh buffers are at segment 287. These writes
cannot be attributed to that mesh. The trace does **not** cover every renderer
input path or independently verify GS PATH1 state. Diagnostic capture:
`/tmp/sly-whole-chain-xyzstate-20261117.log`.

The prior investigation was too narrowly summarized as “no XYZ output.” There
are a few XYZ register writes, but their observed coordinates/state are not
drawable world geometry. The next investigation must correlate the shrinking
transition frames and all renderer input paths with their framebuffer output,
then establish the first expected world draw at a specific call boundary. Do
not change matched decompilation or relax GIF parsing based on the current
evidence.

### Current default-client and adapter check (2026-11-17, 08:21 CET)

The current default launcher was rebuilt with
`OPENRAC_NATIVE_SLY_WORLD_RENDERER=OFF`. Eight later hidden frames after native
frame 500 show the full no-memory-card prompt, with the known short white dash
at the left edge. The Continue highlight varies slightly in this sample; the
prompt does not disappear or turn into a full-screen black frame. See
`/home/reaper/sly/sly-default-client-contact-20261117.png`. A separate
process-local Cross at native frame 300 still produces a black post-Continue
frame (`/home/reaper/sly/sly-default-postcontinue-frame-20261117.png`). The
initial eight frames at process startup were black, so use later captures when
checking the settled prompt.

The opt-in `wave55-sdl32-release` renderer experiment is isolated from that
default build. Its capture map now binds each strip header's captured geometry
to the detached packet address used by `PartMesh::addr0`. This removes the
source-mesh-key collision where distinct strip packets reused one source mesh.
The bounded fresh trace records 32 successful packet-address submissions
without a lookup miss in that logged sample. The resulting frame is a flat
gray ground/horizon, still without a recognizable level:
`/home/reaper/sly/sly-packet-key-check-frame-20261117.png` and
`/home/reaper/sly/sly-packet-key-check-20261117.log`. Solid-color mode shows
one teal full-cover mesh; skipping that mesh leaves only a teal horizon
against black sky. See
`/home/reaper/sly/sly-packet-key-solid-check-frame-20261117.png` and
`/home/reaper/sly/sly-packet-key-skip-check-frame-20261117.png`. The packet-key
fix removes one adapter aliasing error, but the experiment still fails to
produce recognizable world geometry. The default option remains OFF; matched
decompilation sources were not edited.

### Fresh client and dynamic-target trace (2026-11-17)

The rebuilt foreground host32 client was checked again. The startup prompt
renders, but a fresh capture also shows the short white dash on the left edge
of the game area (about 12 by 3 pixels):
`/home/reaper/sly/sly-fresh-client-check-20261117.png`. The earlier conclusion
that there was no visual regression was incomplete; this known artifact is
still present. A process-local Cross at frame 300 again leads to a black
post-Continue frame:
`/home/reaper/sly/sly-fresh-postcontinue-check-20261117.png`. The still capture
does not determine whether the user's reported flicker occurs between
samples. The launcher was stopped after the bounded check.

### Post-Continue pacing check (2026-11-17)

The frame-done semaphore is not simply missing: the render thread continues to
signal semaphore 5, with observed gaps of about 0.7–0.8 seconds. The VIF1 DMA
work between those signals processes roughly 470–498 KB in 700–775 MSCAL calls
and takes 0.77–0.85 seconds; DMA and VIF completion both succeed. The targeted
VU timing capture includes only nine calls (15–19 ms total), so it does not
identify which of the other calls consume the time. This confirms a separate
post-Continue pacing problem, while the black frame remains a missing drawable
scene-geometry problem. Keep the next visual task on one real world object;
do not broaden timing instrumentation without a specific boundary question.
Traces: `/tmp/sly-frame-done-timing-20261117.log`,
`/tmp/sly-vif-frame-done-timing-20261117.log`, and
`/tmp/sly-target-vu-call-timing-20261117.log`.

A separate no-input run captured the full memory-card warning at 3 and 6
seconds (`/tmp/sly-noinput-visual-check-3s-20261117.png` and
`/tmp/sly-noinput-visual-check-6s-20261117.png`). The updated post-Cross
capture is also black (`/tmp/sly-target-mscal-frame-after-20261117.png`). The
startup prompt remains intact; the black post-Continue state is still the
known graphics blocker.

The earlier VU read capture used hard-coded submission 327, although these
IDs change between runs. That could silently filter out the relevant DMA
submission. The trace now binds the VU read filter to the submission containing
target address `0x016d5890` and its configured neighboring segments. In the new
capture this is submission 328, segment 287, with selected stream range
`[177312, 193088)`. VU entry `0x2` at offset `0x2c990` first kicks qword 461,
whose payload is rejected as an invalid GIFtag. Qword 460 immediately before
it holds a valid tag; a second kick from qword 35 is accepted and emits that
tag. A prior run's UNPACK trace at the corresponding stream offset records the
valid tag at qword 460 followed by its payload at 461. The accepted packet is
A+D state, not a vertex packet. This explains one rejected kick and why the
accepted packet does not draw geometry, but it is not the only missing scene
transfer. The later target trace has now followed entries `0x401`/`0x411`
through `0x51`, `0x242`, `0x356`, and finish `0x2`. See the current call
sequence below; the remaining task is to follow strip UNPACKs after `0x356` to
their later GIF submission. Do not relax GIF validation or modify matched
decompilation.

Trace logs: `/tmp/sly-kind0-xgkicks-follow16-20261117.log` and
`/tmp/sly-kind0-allstores-20261117.log`. Host-only instrumentation is in
`port/runtime/vif1_dma.cpp` and `port/runtime/vu1_native.cpp`.

### Full target lit-mesh call sequence (2026-11-17)

New host-only instrumentation logs MSCAL calls only while the dynamically
selected DMA target address `0x016d5890` and its configured neighboring stream
range are active. In the first mesh the observed sequence is `0x318`, `0x401`,
`0x411`, `0x51`, `0x242`, `0x356`, then finish entry `0x2`. Later meshes repeat
the lighting and finish stages; cached meshes can omit the initial `0x318`
setup. Full bounded sequence:
`/tmp/sly-target-mscal-20261117.log`.

XGKICK filters at the exact VIF offsets for entries `0x51` and `0x242` recorded
no kicks. Entry `0x356` likewise completed with no pending kick. Finish entry
`0x2` does kick, but its qword-461 payload is rejected and its qword-35 kick is
an A+D GS-state packet. These calls are preparation and state stages, not the
missing scene-vertex submission. Continue tracing how the strip data queued
after `0x356` reaches the renderer. Keep the separate kind-5 qword-469
rejection out of this explanation.

The MSCAL filter is `OPENRAC_TRACE_VIF_MSCAL_TARGET=1`; it uses the same
dynamic target DMA address/range filter as the VU traces. The instrumentation
is diagnostic only and does not change the retail packet path.

### Target UNPACK and DIRECT trace (2026-11-17)

`OPENRAC_TRACE_VIF_TARGET_COMMANDS=1` now logs VIF UNPACK and DIRECT metadata
only while the dynamically matched target DMA address/range is active. In
submission 328, the `0x51`, `0x242`, `0x356`, `0x2` call sequence matches
`BuildLitVertexPacket` in the read-only decomp checkout. After entry `0x356`,
three V4-32 UNPACKs write eight vectors each at VU1 qwords 460, 468, and 476.
The first vector at each destination is a GIFtag; the one at qword 460 is
`0x1000400000008046`. Qword 461 is the first A+D payload.

Finish entry `0x2` attempts XGKICK from qword 461, which the strict GIF parser
rejects as a malformed tag. It then copies qword 460 to qword 35 and emits an
A+D state packet. No VIF DIRECT occurs in the selected stream range. The trace
therefore proves a tag/payload boundary mismatch in this finish path, but the
accepted packet still contains no vertex registers and this does not alone
explain all missing scene geometry. Do not change the guest start address or
relax GIF validation without resolving the packet contract. Log:
`/tmp/sly-target-strip-qwords-20261117.log`.

This narrows the next step to the `MeshFrameData` copied after entry `0x356`:
trace how its qwords are meant to be consumed by finish entry `0x2`, and locate
the producer that turns the transformed position vectors into XYZ GIF writes.

### Finish-entry instruction trace (2026-11-17, 10:17 CET)

A fresh hidden native run with a process-local Cross captures the matched Sly
target range and the retail VU1 program at entry `0x2`. The program dump ends
at PC `0x1c` (E bit). In this VIF call sequence, the MSCAL values match the
decomp's byte-address labels divided by eight: `g_flushBatchProgram`
`0x18c0/8 = 0x318`, `g_skinWeightProgram` `0x2008/8 = 0x401`,
`g_litSkinWeightProgram` `0x2088/8 = 0x411`, `g_litBatchProgram`
`0x288/8 = 0x51`, `g_litBatchSecondProgram` `0x1210/8 = 0x242`,
`g_litBatchThirdProgram` `0x1ab0/8 = 0x356`, and
`g_litBatchFinishProgram` `0x10/8 = 0x2`. This ties the observed sequence
to the named retail packet stages.

At entry `0x2`, the VU loads qword 460 at PC `0x0a`. It contains the valid
GIFtag `0x1000400000008046`. At PC `0x0c`, instruction `0x80000efc` issues
XGKICK from VI1, whose value is 461; that address holds the following payload,
not a GIFtag, and strict parsing rejects it. At PC `0x0e`, the program copies
qword 460 to qword 35; at PC `0x12`, it kicks VI1=35. That second kick is
accepted as a 70-loop A+D packet, with no XYZ register writes. The microprogram
completes normally in 49 cycles with no pending kick. The exact-filter dump of
the accepted qword-35 packet is decoded later in this handoff: it has no XYZ
writes and includes invalid A+D addresses. The accepted state packet still
does not account for the missing world geometry. Full bounded capture:
`/tmp/sly-finish-entry2-audit-20261117.log`.

Keep this lit-batch finish trace distinct from the other qword-469 rejection
at entry `0x5f`, which belongs to the separate kind-5 strip path. The next
graphics step remains tracing where the target model's transformed vectors
become XYZ GIF writes and whether a preceding PATH1 packet is still active.

### Finish-entry parser carry check (2026-11-17, 10:24 CET)

Added an opt-in, target-filtered probe of the native GIF parser immediately
before the qword-35 XGKICK append. A fresh run with one process-local Cross
press reports `tag_loaded=0`, `remaining_qwords=0`, and a nonzero prior EOP
count for every captured `0x2c990` finish call. In the native parser path,
there is no incomplete GIFtag for the finish packet to continue. This does
not observe hardware PATH1 state independently, so it does not justify
changing packet boundaries or GIF validation. The next graphics step remains
locating the VU output path that should produce XYZ writes. Capture:
`/tmp/sly-parser-carry-check-20261117.log`.

### Fresh client and queue check (2026-11-17)

A foreground launch of the current host32 binary showed the complete
no-memory-card prompt and Continue button before any input. Screenshot:
`/tmp/sly-live-client-check-20261117.png`. A separate 15-second idle trace
captured 894 native presents. Packet data averaged 208,935 bytes per present
and peaked at 416,256 bytes, with no overflow warning. `openrac_native_window_present`
swaps and drains the queue each frame, and this run confirms that path is
working at idle. Trace: `/tmp/sly-queue-idle-15s-20261117.log`.

A process-local Cross at native frame 300 produced 718 presents in 12 seconds
with no queue overflow; the largest packet batch was 2,098,896 bytes. After
the transition, Direct decode frames 317-323 each received large batches but
decoded zero vertices and zero draws. Later frames decoded only occasional
small draws, with no GIF parser errors. The captured client was black:
`/tmp/sly-postcross-client-20261117.png`. Trace files:
`/tmp/sly-postcross-present-20261117.log` and
`/tmp/sly-postcross-direct-decode-20261117.log`. This confirms that the queue
drains through the black transition; the relevant gap is that these packet
batches contain no Direct-renderable scene geometry. An earlier run did emit
a real 16 MiB overflow warning (`queued=16776752`, `incoming=704`), but it has
not recurred. Keep the cap unchanged; correlate present and append traces if
the warning returns.

### Entry-`0x2` packet construction (2026-11-17)

A follow-up capture confirms the dataflow for submission 328 at MSCAL offset
`0x2c990`: VU entry `0x2` loads qword 460, whose tag is
`0x1000400000008046`. It attempts XGKICK from qword 461, and strict parsing
rejects that payload as a tag. The program then copies qword 460 to qword 35
and XGKICKs qword 35 successfully. That tag has NLOOP 70, one register
descriptor, A+D. It is GS register traffic rather than XYZ vertex output,
consistent with the renderer audit's A+D-only packets. The rejected kick is
confirmed, but entry `0x2`'s accepted output does not explain the missing
scene geometry. Do not relax GIF validation on this evidence.

Capture: `/tmp/sly-kind0-filtered-20261117.log`. VU debug output was adjusted
to honor the dynamic DMA and MSCAL filters; the remaining 42,882-line capture
still includes neighboring microprograms because the selected range spans 16
DMA segments. For the next run, add
`OPENRAC_TRACE_XGKICK_MSCAL_OFFSETS=0x2c990` to isolate this entry.

The accepted kick was then dumped with the exact MSCAL filter to
`/tmp/sly1-vu1-kick-00.bin` and decoded: 71 qwords total, with a 70-loop packed
A+D tag. The 70 address bytes are `0x00` (53 writes) and `0x80` (17 writes);
there are no XYZ2/XYZF2/XYZ3/XYZF3 addresses, and `0x80` is outside the GS
register range. This packet contains no vertex writes and includes invalid
A+D addresses. Treat it as a malformed state packet, not yet as the sole cause
of the black frame. The exact filtered run is
`/tmp/sly-kind0-2c990-packet-20261117.log` (2,912 lines).

### Sly kind-0 packet follow-up (2026-11-17)

A rebuilt host32 client traces Sly's saved first kind-0 mesh address,
`0x016d5890`, to DMA segment 287: 141 QWC at VIF stream offset 177776. The
adjacent segment starts at offset 180032. In the same stream, selected MSCAL
calls occur at offsets 178904 (`0x318`), 180020 (`0x401`), and 181408
(`0x411`). These are now tied to Sly's packet address rather than inferred
from shared VU entry numbers.

An XGKICK trace filtered to those three command offsets records no kick from
any of them. In contrast, the qword-469 rejection is logged at entry `0x5f`,
VIF offset `0x1790`, followed by the accepted qword-35 kick. That is a separate
kind-5 path in the same large VIF batch, not a demonstrated failure in Sly's
kind-0 calls. This corrects the earlier assumption that tracing those shared
entries would directly expose the failing Sly kick. A follow-up store trace
shows the three calls complete and write intermediate VU data, but do not
kick. Their later consumer and the transfer that should put their output into
the GIF stream remain unknown; see the output capture below.

The offset-filtered run injected one process-local Cross press; it did not
capture a confirming image. The playable screen remains unverified beyond the
previously confirmed black frame. No matched decompilation or gameplay
behavior was changed. Build and run evidence:
`/tmp/sly-kind0-offset-xgkick-20261117.log`;
the earlier submission-filtered run is
`/tmp/sly-kind0-xgkick-20261117.log`.

### Kind-0 VU output capture (2026-11-17)

A bounded foreground run captured the selected VU stores and normal E-bit
completion for the first Sly mesh. Submission 327's `0x318` call completed at
micro-PC `0x31f` after 30 cycles and wrote four setup qwords (14–17). The
`0x401` call completed at `0x411` after 1,803 cycles and wrote 68 vectors to
qwords 39–307 at stride four. The `0x411` call completed at `0x45c` after
1,800 cycles and wrote another 68 vectors to qwords 36–304 at stride four.
All three returned with no pending XGKICK. This corrects the earlier narrow
watch of qwords 136–142, which missed the actual output buffers. It does not
yet show how the written vectors reach the GIF parser or graphics queue.

The captured VU qwords now have a traced consumer sequence through entries
`0x51`, `0x242`, and `0x356`, followed by finish entry `0x2`. These intermediate
calls do not issue XGKICK; the finish output is only A+D state. Follow the strip
UNPACKs after `0x356` to their later GIF submission and correlate it with a
queued renderer packet. Keep the kind-5 qword-469 anomaly separate. Capture:
`/tmp/sly-kind0-allstores-20261117.log`.

### Latest client and VU/GIF boundary check (2026-11-17, 04:50–04:52 CET)

The foreground host32 window remains fully black after one process-local
Cross at native frame 300. The fresh framebuffer `/tmp/sly1-native-frame-00.png`
shows the same. The 14-second diagnostic ended by timeout (exit 137), not a
reported guest crash.

This run corrects the previous “still on Splash world” diagnosis. The periodic
startup sampler reports world `0x6abdd0`, stage 2 and 2,426 sectors, but the
post-input draw-list hook repeatedly sees level world `0x00870cf0`, 1,910 model
parts and 23 parts queued. Deferred lists contain 39–48 requests, and model
draw index 21 is Sly. Treat the periodic sampler as incomplete or separate
state; it does not establish that level loading stalled. These are game-side
draw requests, not proof the optional native GL mesh adapter rendered them.
`OPENRAC_NATIVE_SLY_WORLD_RENDERER` is OFF in the active CMake cache.

The ordinary DirectRenderer sees GIF packets after the transition, but from
direct frames 330–716, 373 of 387 batches decode to zero vertices and zero
draws with no parser error. The other 14 batches have at most 36 vertices;
some are clipped to a 64-by-64 corner and the larger examples are outside clip
space (frame 359 and 418). The same run logs a rejected XGKICK from qword 469
followed by accepted kicks from qword 35. This puts the visible failure after
game-side level draw-list creation and before useful scene geometry reaches
the host renderer. The precise VU/GIF contract fault is still unproven.
Evidence: `/tmp/sly-direct-postcontinue-20261117.log`,
`/tmp/sly-level-packets-after-continue-20261117.log`, and
`/tmp/sly1-native-frame-00.png`.

I parsed the 16 queued-packet dumps for eligible frames 330–345. Every GIF tag
uses a single A+D register descriptor; none uses an XYZ2, XYZF2, XYZ3 or XYZF3
descriptor. Those packets carry GS register state, not scene vertices. Local
packet dumps:
`/tmp/sly1-native-render-packets-00.bin` through `-15.bin`.

The saved first Sly kind-0 mesh packet at
`/tmp/sly-world-part-0.bin` (141 qwords) is a VIF stream. It writes a
three-vector header, UNPACKs 68 V4-32 vectors at VU1 address `0x027`, calls
`MSCAL 0x318`, then UNPACKs 68 more V4-32 vectors at `0x188` and calls
`MSCAL 0x401`. This identifies the two guest program entries to trace for
Sly's geometry. The current traces do not yet show those programs' XGKICK
payloads or link them to a queued GIF packet, so the cause of black output is
still open.

The qword-469 trace is for kind-5 mesh source `0x00a113c0`, sent through
`BuildStripMeshPacket`; it is not the Sly model draw. Sly is model draw index 21,
a class-2 draw with 38 kind-0 lit meshes. Do not treat the kind-5 mismatch as
the proven cause of missing Sly geometry. In that kind-5 trace, guest microcode
sets VI6 to 468 at PC `0x63`, increments it to 469 at PC `0x67`, then issues
XGKICK VI6 at PC `0x68`; later it decrements VI7 from 36 to 35 before the kick
at PC `0x6e`. The
[VU instruction manual](https://www.ps2dev.com/files/vu-instruction-manual.pdf)
defines XGKICK's PATH1 source as the value of VI[is]. Thus 469 is that
microcode's explicit operand, not a host-side pointer calculation. A prior
diagnostic backstep to 468 passed packet validation but left the frame black.
Trace `/tmp/sly-detach-mesh-trace-cross-20261117.log`. The next trace should
follow Sly's kind-0 lit mesh through VU entries `0x318` and `0x401`, capture
their XGKICK output, then match that output to one of the queued GIF packets.

The earlier no-input cadence capture still shows the full no-card prompt in
eight internal samples. It does not reproduce the intermittent black gaps
visible in the user's Nov 15 recording; the desktop-compositor behavior is
unverified. See `/tmp/sly-current-cadence-montage.png` and
`/tmp/sly-current-cadence-capture-20261117.log`.

### Earlier packet and renderer trace (2026-11-17)

An earlier actual Wayland window capture showed the no-card prompt before
input and black output after one process-local Cross. The newer trace above
adds the missing distinction: game-side model requests are present, while
ordinary GIF batches still produce almost no vertices. The earlier claim that
the native Sly world queue had no submissions applied only to the optional
host mesh adapter; it did not describe retail `QueueModelPartDraws`. Do not
conflate those two queues. The Nov 15 screen recording still shows intermittent
black flashes; the later internal cadence samples do not reproduce or explain
them. Captures and traces are listed in `docs/port/PLAYABLE_TODO.md`.

### VU exceptional-float correction (2026-11-17)

The translator now models the observed VU exceptional-float behavior for
vector arithmetic, accumulator operations, `ERLENG`, `ERSQRT`, `DIV`, and
`RSQRT`: exponent-zero values flush to signed zero, unsupported exponent-255
values and divide-by-zero clamp to finite signed max. This keeps raw register
loads/stores unchanged and normalizes only arithmetic. On entry `0x411`, the
trace now shows `ERLENG` mapping zero length to `P=0x7f7fffff`; `MFP` no longer
receives host IEEE infinity and the ensuing NaN transform cascade is gone.
The visual remains black. Entry `0x5f`'s qword-469 XGKICK, whose address points
at payload after the valid GIFtag at qword 468, is a confirmed packet-boundary
anomaly, but the newer direct-render trace above shows that the complete cause
is not established. Strict packet validation remains enabled. The ten-second
foreground run injected one process-local Cross at frame 300 and ended at the
diagnostic timeout (137), not a guest crash. The sampled post-input framebuffer
is still black. Evidence:
`/tmp/sly-vu-float-model-20261117.log` and
`/tmp/sly1-native-visible-after-cross-20261117.png` (the previous baseline).
Matched C and VU assembly were not changed. The model uses finite `0x7f7fffff`
as a host representation of VU max; exact bit-level saturation/flag behavior
remains to be validated.

### Earlier source-mapped foreground trace (2026-11-17)

The latest source-mapped VIF run also confirms that the first world XGKICK
failure precedes later writes to qword 468. Submission 328's UNPACK at offset
5916 writes the GIFtag at 468, with payload at 469; entry `0x5f` kicks from
469 and strict parsing rejects it. The source bytes map to guest address
`0x018d2650` in a REF segment starting at `0x018d1270` (326 qwords). Later
UNPACKs in the same submission rewrite qword 468 after this rejected kick.
The exact C producer is now identified: kind-5 mesh `0x00a113c0` goes through
`ReadPlacedObjectModel` to `BuildStripMeshPacket` (`0x00163ED8`). Its trailing
strip loop reserves format-C/count-7 data at VU offset `0x1D4` (112 bytes).
The live packet cursor minus detached start is 5,088 bytes; adding that to REF
base `0x018d1270` gives `0x018d2650`, the qword-468 input source. The command
is at `0x018d264c`. Trace: `/tmp/sly-strip-target-reserve-cross2-20261117.log`.
The qword-469 kick remains rejected, while the newer trace accepts the second
kick from qword 35; the target renderer still receives no native Sly world
meshes. Preserve strict GIF validation and matched C while tracing that
transfer and scene-render contract. The separate non-finite transform path has
a targeted VU arithmetic correction described above. A later client check is
recorded at the top of this document.
Evidence:
`/tmp/sly-vif-model-packet-source-cross-20261117.log` and
`/tmp/sly-vif-gif-range-cross-20261117.log`.

The DMA submission is now tied to the retail render thread. A foreground trace
shows VIF1 `TADR=0x003c3b00` submitted from caller `0x081328ab`, inside native
`RunRenderThread` (`0x0015EE80`); its chain includes REF tag `0x003c3d60`,
QWC 326, data `0x018d1270`. A prior model trace records the same pointer/QWC
as mesh 0 of Sly's first skinned model (`source=0x00a113c0`, four bones).
The later targeted reserve/cursor probe identifies the exact writer as
`BuildStripMeshPacket`, not `BuildLitStripMeshPacket`'s `pkt[1]` skin-weight
stream. Evidence:
`/tmp/sly-qword468-chain-visible-20261117.log`,
`/tmp/sly-qword468-dma-caller-cross-20261117.log`, and
`/tmp/sly-object-68-final-pose-20261116.log`.

The visible Wayland client was freshly checked after the VIF diagnostics. Its
no-input framebuffer shows the complete no-card warning; one process-local
Cross at frame 300 dismisses the warning and the next captured world frame is
black. Captures:
`/tmp/sly1-native-visible-before-cross-20261117.png` and
`/tmp/sly1-native-visible-after-cross-20261117.png`; diagnostic log:
`/tmp/sly-vif-unpack-writer-cross-20261117.log`. This reproduces the known
post-Continue failure. The 8-second timeout stopped the run; no launcher is
left running.

The host-only writer trace maps `MSCAL 0x411` to matched
`BuildLitStripMeshPacket` (`0x00164868`) and confirms the source uses
`g_litSkinWeightProgram` through `pkt[1]`. The `0xC/count 8/address 0x1CB`
reserve comes through the same function's strip loop on `pkt[2]`; the shared
caller does not prove the UNPACK is skin-weight input. The command that writes
qword 468 (`0xC1D4`, count 7, address `0x1D4`) was not identified by either
targeted helper probe. Its origin remains open. Preserve this distinction
before attempting a packet-order or VU fix.

### Native client re-audit (2026-11-17)

A fresh no-input run shows the complete retail no-card prompt in all eight
sampled frames (`/tmp/sly1-native-frame-{00..07}.ppm`). A separate run injected
Cross at frame 120 and captured eight post-input frames; each 960x720 image is
exactly black. These runs reproduce the known prompt-then-black behavior, not a
new regression. Both processes ended at their diagnostic timeouts (exit 137),
not a confirmed crash. Fresh logs: `/tmp/sly-final-visual-audit-20261117.log`
and `/tmp/sly-final-continue-audit-20261117.log`; the Cross run captured two
prompt frames followed by six black frames.

The qword-469 rejection is an address/transfer-flow question, not evidence that
the GIFtag itself is invalid. Entry `0x5f`, PC `0x68` reads `VI6=469`. VIF
UNPACK wrote a structurally valid GIFtag to qword 468 (`0x100040000000806d`,
one A+D register per loop, 109 loops, EOP set), and qword 469 is its A+D
payload. The qword-469 XGKICK is rejected; later, the guest copies that tag to
qword 35 and XGKICKs from 35, which the parser accepts. We have not established
why the guest issues both kicks or whether native VU/GIF transfer timing is
missing a required state transition. Do not change the address or weaken GIF
validation until the sequence is understood.

The VU1 entry `0x411` trace now correlates the writes and reads by VIF
submission. Submission 144 writes qword 460 as `0x0005b001, 0, 0x47, 0` at
stream offset 124968, then invokes entry `0x411` at offset `0x2c4a0`. The
entry's loop starts reading at qword 392 and uses `VI6=316` as its target;
the last sequential loads are qwords 460, 461, and 462 before it exits. The
same VIF submission had earlier written ordinary float data at qword 460,
but offset 124968 overwrites it before this VU call. The remaining question
is which packet builder emitted that UNPACK and whether its payload layout is
what this retail VU program expects. The `MULA` at PC `0x43b` uses the tiny subnormal
`VF3.x` with finite `VF20`, making ACC subnormal. `ERLENG` at PC `0x44a` then
squares the tiny vector to zero and sets P to positive infinity. `MFP` copies
P into `VF24.x`; zero-times-infinity later contaminates the transform. The
first bad `SQ` at PC `0x445` stores **VF27**, not VF11; this corrects the
earlier trace attribution error. Do not clamp vertices or change matched
decompilation. See
`/tmp/sly-transform-correlated-20261117.log` and
`/tmp/sly-vu-store-q300-320-20261117.log`. Matched game C was not changed; the
VU microprogram is retail assembly and remains outside the decompilation.

### Post-Continue normal-renderer capture (2026-11-16, later run)

The client is still black after the no-card prompt is dismissed. A synchronized
hidden capture shows the normal DirectRenderer receives large packets and
decodes hundreds of draws, but this does not mean it has visible geometry:
for several consecutive world frames it decoded 2,196–5,064 vertices whose
NDC bounds were only `(-7.402, 19.286)..(-7.398, 19.286)`, with zero vertices
inside the clip cube. The following frames begin to include other coordinates,
but this capture still produced no recognizable scene. The opt-in flat-color
diagnostic, which bypasses alpha/depth tests and framebuffer masks, also left
those sampled frames black. Thus the current next question is why the decoded
GS vertices collapse outside the view and which queued stream supplied them;
the first world XGKICK rejection at qword 469 remains a separate verified
fault and must not be waved away by the DirectRenderer draw counts.

The aggregate `OPENRAC_TRACE_DIRECT_BOUNDS=1` trace is bounded by the existing
`OPENRAC_TRACE_DIRECT_ALL=1` switch. Evidence:
`/tmp/sly-direct-bounds-20261116.log`; frame capture:
`/tmp/sly1-native-frame-00.ppm` through `-07.ppm`. The current source remains
unmodified in matched decomp files; the optional flat diagnostic is not a
playable-path change.

## Latest client output audit (2026-11-16)

The normal no-input screen is the retail no-card prompt. A short capture that
saved only the first eight packets showed black because those were startup
frames. A tracing-disabled 12-second hidden Wayland run sampled 703 presents:
the first 27 were black during startup, then the prompt was complete by sample
35 and remained visible through sample 703. The Continue highlight and border
animate between shades. Contact sheet: `/tmp/sly-flicker-perframe-contact.png`;
run log: `/tmp/sly-flicker-perframe-audit-20261116.log`.

The prior normal host32 build had no wait in its staged `RunMainLoop` unless
trace instrumentation was enabled. This let the guest loop outrun the SDL
presenter and flood the packet queue. The staging tool now inserts the SDL
16 ms wait at every host32 frame boundary. A bounded hidden run reported 1,293
accepted DMA submissions, zero rejections, and 428 game frames before its
timeout; it did not crash. A tracing-disabled run completed 703 presents
without a crash. The shared 16 MiB overflow guard remains enabled. Both
changes are port host plumbing; matched decomp source is untouched.

After a process-local Continue input, the panel shrinks during dismissal and
the current capture then goes black; it is not playable. A scoped 16,384
instruction limit lets VU1 entry `0x26d` finish, removing the earlier VIF
stream abort at PC `0x2aa`. XGKICK at PC `0x68` still rejects qword 469, whose
preceding qword 468 is the valid GIFtag. Captures:
`/tmp/sly-current-noinput-contact.png` and
`/tmp/sly-current-16k-contact.png`; logs:
`/tmp/sly-clean-client-audit-20261116.log` and
`/tmp/sly-vu16k-client-audit-20261116.log`. The normal output does not include
the stretched purple diagnostic scene.

An opt-in lifecycle trace corrected the earlier object-96 diagnosis. In the
latest run object 96's local rotation is finite, while its position becomes
NaN later. Object 68's mode-3 animation callback also returned a NaN matrix.
The cause was a host i386 callback ABI mismatch: `GetTrackPosition` and
`GetTrackRotation` were cast into vtable slots that pass time before output
pointers, but their definitions put time last. The host staging adapter now
reorders those signatures without editing the matched decomp source. A fresh
trace shows finite results from `getRot`, finite object-68 rotation after
`MovePlacedObject`, and no invalid model ancestor report. This removes the
transform regression but does not fix rendering: the post-continue framebuffer
is still black. The normal VIF/VU/GIF packet failure at qword 469 remains a
separate graphics blocker. The lifecycle probe is in
`port/tools/compile_decomp_host32.py` and
`port/sly1/native_host32/main.cpp`. Captures:
`/tmp/sly-object-68-pose-source-20261116.log`,
`/tmp/sly-object-68-abi-verify-20261116.log`, and
`/tmp/sly-native-frame-fixed-abi-20261116.log`.

The trace does not support the earlier assumption that all VIF inputs are
finite. One model UNPACK supplies finite positions at VU1 qwords 138 and 141,
but an earlier setup UNPACK supplies canonical NaNs at qwords 348 onward. Those
same NaNs are already present in the DMA source block at `0x003c3c50`, inside
the inline CNT payload beginning at `0x003c3b80`; the block is submitted by the
render worker. VU1 `MSCAL 0x401` then stores NaNs over qwords 137 and 140, and
entry `0x34a` propagates them over qwords 138 and 141. The first bad values
therefore precede VU1 execution for this block. The next trace must find who
writes the DMA source block; do not sanitize its values or claim a VU semantic
fix based on downstream propagation. The caller trace confirms the alternating
`0x003c3b00`/`0x004c3b00` buffers are submitted by the frame worker; it does not
yet identify the code that constructs their inline payload.

The instrumented host32 target rebuilt successfully with 194 matched units,
85 separate port backfills, and 99 missing-function traps. No matched decomp
source changed. Trace/build logs:
`/tmp/sly-vu-range-capture-20261116.log`,
`/tmp/sly-vif-source-capture-20261116.log`,
`/tmp/sly-dma-caller-capture-20261116.log`, and
`/tmp/sly-vif-source-build-20261116.log`.

### VIF DMA abort classification (2026-11-16)

A clean standard-client capture still shows the no-card prompt followed by
black frames. A bounded trace of the subsequent VIF failure establishes that
command `0x14` is a valid `MSCAL`, not an unsupported or malformed VIF
command. It invokes VU1 entry `0x26d`, which reaches the 4,096-instruction
limit at PC `0x2aa` (`word=paired=0xffffffff`). The host VIF parser returns
failure immediately, so the rest of this DMA stream is skipped. Lookahead
shows another `MSCAL 0x1db`, `BASE`, `OFFSET`, and an `UNPACK` after the
failing call. This is a verified cause of stream truncation, not yet a
verified explanation or fix for the black framebuffer. The runtime warning
now distinguishes a VU stop from malformed VIF input, and the opt-in
`OPENRAC_TRACE_VIF_AFTER_MSCAL=1` lookahead is bounded to four events and 64
bytes each. No matched decompilation source changed.

Evidence: `/tmp/sly-vif-failure-kind-20261116.log`.

### Bound the shared GIF packet queue (2026-11-16)

The Release client previously reached `std::bad_alloc` in the renderer after
the XGKICK append callback grew `g_packets` beyond the 16 MiB limits enforced
by the other submission paths. Those guards also used subtraction that could
underflow once the existing queue exceeded the limit. The port runtime now
checks both current and incoming sizes before every append, with a shared
overflow-safe 16 MiB cap. A hidden Release run reached the cap and logged
rejections instead of throwing `bad_alloc`; the process was stopped after 10
seconds (timeout exit 137). Since overflow packets are dropped, this confirms
host memory stability only and leaves the black/invalid world output
unresolved. Log: `/tmp/sly-release-queue-guard-20261116.log`. No matched
decompilation source changed.

The optional renderer previously mapped every non-`Always` GS depth test to
OpenGL `LEQUAL`, incorrectly treating `Greater`, `Gequal`, and `Never` as the
same comparison. It now maps all four decoded GS modes to their matching GL
comparisons. This exposes broad purple surfaces after world startup, but they
stretch across the screen and do not form a recognizable object. This is a
real renderer-state correction, not proof that the VIF/VU/GIF packet path
works; continue from strip output and per-object transforms.

The projected-bounds diagnostic reports many vertices within the clip cube in
later frames, but it is only a vertex/bounds estimate: it does not clip
triangles or account for depth/material state, so it does not prove that those
meshes should cover visible pixels. Queue deduplication did not fix the image,
so the next step is to establish correct strip topology and compare the
prototype's draw with the retail GS/VU packet path. Do not infer success from
the rising mesh count or bounds estimate.

An opt-in solid-color capture identifies one 106-vertex mesh (`source=0x00a0fd90`)
whose projected bounds cover the view and whose GS TEST uses depth-always. When
the prototype skips that mesh, the image is black above a flat ground plane;
the later frames still show no recognizable objects. It is therefore not the
only cause of missing geometry. These debug controls exist only in the host
prototype: `OPENRAC_DEBUG_SLY_WORLD_SOLID_COLORS=1` colors meshes by instance,
and `OPENRAC_DEBUG_SLY_SKIP_FULL_COVER=1` skips full-view, depth-always meshes.
The skip capture is `/tmp/sly-skip-cover-contact-20261116.png` and trace is
`/tmp/sly-skip-cover-run-20261116.log`. Keep both controls off for normal runs.

These purple fragments are from the experimental
`OPENRAC_NATIVE_SLY_WORLD_RENDERER=ON` path, which submits captured Sly meshes
directly to OpenRAC's host renderer. They do not verify the normal VIF/VU/GIF
render path; that path still has the malformed kick at qword 469. The optional
renderer remains diagnostic and must not become the basis for a playable-state
claim.

The path is now traced end to end through `QueueModelPartDraws`. The first
1,910-part pass previously queued zero requests because the port-only
`func_001453B0` backfill ignored its `distance` argument and used the vector's
fourth lane instead. It now computes `distance * camera[0xB0] + radius`, which
matches the decomp's function description. The first pass then queued 23 parts;
later passes queued up to 78. The call is still made from staged matched code;
the functional correction is isolated in `port/game/sly1-us/native/` and the
decomp checkout is untouched.

The DMA9 scratchpad path is also verified for the first 12 model/state blocks:
the runtime compared every copied word against its RAM source and found zero
mismatches. The old `0x99999999` scratch hook is null in the traced draw
requests. DMA8 scratchpad-to-RAM behavior is implemented but has not yet been
observed on this path. Detailed evidence is in
`/tmp/sly-camera-fade-arg-build-20261116.log` and
`/tmp/sly-camera-fade-arg-run-20261116.log`.

The normal VIF/VU/GIF path remains unresolved independently: its first world
XGKICK still starts at payload qword 469 and strict validation rejects it.
The host mesh prototype must not be treated as proof that the native PS2 packet
renderer works. Keep functional game backfills in `port/game/sly1-us/native/`
and matched decomp sources untouched.

## Direction reset

### VU store bug found after the Space transition (2026-11-16)

The port's lower SQ/SQI/SQD decoding reversed the integer address register and
vector source. This made `SQ VF11, VI4(0)` at PC `0x336` write zeros over VU
qword 11, erasing metadata needed by the next `MSCAL 0x401`. The store
operand mappings are now corrected in `port/runtime/vu1_native.cpp`; SQ also
keeps unselected lanes, and SQI/SQD preserve lanes selected by their mask. A
bounded capture shows the metadata survives and the
normal `0x401` helper reaches its E-bit. The following stream still encounters
the malformed GIF tag at qword 469 and VU instruction-budget stops at entry
`0x26d` PC `0x2a9` and entry `0x318` PC `0x24e`. No recognizable world graphics
were produced. OPMULA and ERSQRT from the next VU helper are also implemented
in the port runtime. No matched decomp source changed.

Captures: `/tmp/sly-vu-store-fix-capture-20261116.log`,
`/tmp/sly-vu-opmula-capture-20261116.log`, and
`/tmp/sly-vu-store-final-capture-20261116.log`. Latest build:
`/tmp/sly-vu-store-final-build-20261116.log`.

### The `0x26d` VU copy loop (2026-11-16)

The entry `0x26d` budget stop is a live counter loop. PC `0x2ad` increments
VI8; PC `0x2d6` branches back while VI9 differs from VI8. With the normal
4,096-pair cap VI8 is 273 against VI9=314. A 10,000-pair diagnostic cap on
entry `0x26d` gets to PC `0x2c6`, with VI8=366 and VI9=400, but still stops
before the helper returns. WAITP is now handled as a no-op because this port
computes P synchronously. The default cap remains 4,096. Entry `0x318` still
hits that cap at PC `0x24e`; XGKICK continues to reject the malformed GIF tag
at qword 469. There is still no recognizable world image.

Captures: `/tmp/sly-vu-loop-detail-capture-20261116.log`,
`/tmp/sly-vu-26d-limit-10000-capture-20261116.log`, and
`/tmp/sly-vu-waitp-8192-capture-20261116.log`. Build:
`/tmp/sly-vu-26d-limit-build-20261116.log`.

### Lower VU special-op decode (2026-11-16)

The Space transition's offscreen trace exposed a bad lower VU dispatch at PC
`0x3d4`: instruction `0x81e43b7c` is LQI (special index `0x34`), but the native
translator used the function field as the opcode and stopped the MSCAL call.
The translator now decodes the lower-special index from bits 6-10 and bits 0-1,
and handles the LQI/SQI/LQD/SQD and MTIR/MFIR/ILWR/ISWR families separately.
Regular ILW reads only the first selected lane. These are port-runtime changes;
the matched Sly decomp files are unchanged. The OpenRAC interpreter semantics
used for this adaptation are credited in `THIRD_PARTY_NOTICES.md`.

The fresh host32 build compiled 194/194 Sly units and 85/85 port backfills.
The bounded offscreen run no longer reports a translation stop at PC `0x3d4`,
but entries `0x26d` and `0x318` still hit the 4,096-instruction limit. The
capture remains nonvisual: it does not verify the flicker fix or show a
recognizable level. Continue from the VU loop counters and GIF packet traces;
do not raise the default instruction limit based on this run.

Evidence: `/tmp/sly-vu-special-index-capture-20261116.log` and build
`/tmp/open-thievius-host32-8f7a85a3-fresh/host32/sdl-launcher/sly1_native_sdl32`.

### Verify the loaded VU1 mesh program (2026-11-16)

A bounded hidden run with `OPENRAC_TRACE_VU1_PROGRAM_WORDS=842` confirms the
VU1 instructions loaded by the native VIF runtime match the retail US
executable. The `.vutext` range referenced by `g_groupMicroprogram` is a VIF
stream with MPG command headers interleaved between 256-instruction chunks;
compare the reconstructed VU memory, not a raw `.vutext` file offset. At PC
842 the loaded five-instruction routine matches the corresponding source words
after the four preceding MPG headers are consumed. This rules out a 32-byte
VU upload offset error.

The same run shows the first world XGKICK at PC `0x68` starts at qword 469,
whose low word is `0x00039001`; strict GIF parsing rejects its reserved bits.
The next XGKICK at PC `0x6e` starts at qword 35 and is accepted. The input
qword 469 is written directly by VIF `UNPACK` command `0x6c`, immediate
`0xc1d4`. Trace which stream producer and packet boundary supply that kick
before changing the VU translator or GIF validation. The runtime word trace is
opt-in through `OPENRAC_TRACE_VU1_PROGRAM_WORDS=<PC>`.

A follow-up trace confirms the parser is idle at both XGKICKs. The VIF1 DIRECT
parser had finished its prior packet (`active=0`, `remaining=0`) before PC
`0x68`; this kick is not a continuation of an incomplete DIRECT packet. In the
UNPACK output, qword 468 is the valid EOP GIFtag `0x100040000000806d` with 109
A+D writes, while qword 469 begins its payload (`value=0x39001`,
`address=0x48`). The verified retail VU instruction at PC `0x67` increments
VI6 from 468 to 469 just before XGKICK. The unresolved issue is therefore why
the retail VU program starts this kick at the first payload item, or what
earlier transfer is expected to have supplied its tag. Keep the strict GIF
parser unchanged until that boundary is accounted for. Capture:
`/tmp/sly-gif-origin-capture-20261116.log`.

The per-kick outcome trace repeats this pattern as the VI6 address changes: PC
`0x68` fails on the same first payload word, while PC `0x6e` succeeds from
VI7=`35` with an EOP A+D packet. Those accepted packets write GS registers;
they do not carry mesh vertices. No PC `0x68` packet reaches the renderer, so
the world geometry remains absent. `OPENRAC_TRACE_XGKICK_STATE=1` now logs
bounded per-kick acceptance and failure details for the next comparison.
Capture: `/tmp/sly-xgkick-outcomes-capture-20261116.log`.

Capture: `/tmp/sly-vu-program-words-842-20261116.log`.

### World submission bounds capture (2026-11-16)

A fresh offscreen capture after the test Cross press entered world `0x870cf0`,
queued 41 mesh callbacks, and resolved five distinct uploaded textures rather
than the placeholder. This confirms that the temporary world path receives
both mesh submissions and texture resources. The projected bounds remain
wrong: the first three large submissions span extreme/off-screen NDC ranges
(`-178..2`, `-0.40..2.17`, and `-0.42..1.40` on X), while the many repeated
33-vertex submissions collapse to roughly `0.008 x 0.003` NDC near the screen
center. Some vertices are inside the clip volume, but the result is still a
flat gray horizon. A bounded follow-up trace now also records local vertex
ranges and the first four model/camera matrices. It confirms that the 33-vertex
mesh has local coordinates of ordinary scale (about 0.8–20 units); its small
projected size is not caused by decoding every coordinate as an enormous
value. The model translation is stored in the final matrix row, matching the
row-major transform layout used by the port-only `func_00188C10` backfill. The
current row-vector `model * view_projection` composition is consistent with
that layout; swapping it to `view_projection * model` projects the sample
thousands of units off screen. Do not transpose or reorder matrices without
new evidence. The next renderer investigation should focus on captured strip
connectivity and GS state/visibility, while keeping the native VU/GIF path
independent. Nothing here justifies changing matched game code or relaxing
VU/GIF validation.

The user's visible-window recording reports intermittent black flicker. In
the separate offscreen run, per-present samples at the prompt's text and panel
stay nonblack from frame 35 through frame 698, after the prompt has appeared.
That trace did not reproduce the reported flicker. A hidden-window run through
SDL's Wayland driver then exercised the actual double-buffered OpenGL surface:
427 pre-swap samples after frame 35 show the expected text and border, with no
black sample. This verifies the Wayland backbuffer after the prompt appears,
but cannot verify pixels after the compositor presents them. Capture:
`/tmp/sly-hidden-wayland-present-20261116.log`.
Captures: `/tmp/sly-world-bounds-20261116.log`,
`/tmp/sly-matrix-trace-20261116.log`, `/tmp/sly-matrix-data-20261116.log`, and
`/tmp/sly1-native-world-00.ppm`.

A fresh hidden Wayland run with one test Cross press reproduces the same
boundary without taking focus: the no-card panel's text and border samples are
stable on all 86 sampled frames from 35 through 120. Cross at frame 120
transitions into world `0x870cf0`; the prompt-area samples go black by
frame 130. At frame 146, 41 native world meshes are queued and rendered, but
the captured image is only the gray horizon and ground. The same trace still
rejects the first mesh GIF kick at PC `0x68`, address 469, because it starts on
payload qword 469 instead of the valid GIFtag at qword 468; the second kick is
accepted as an A+D register packet and submits no mesh geometry. Thus this run
did not reproduce post-startup prompt flicker; after Space, the prompt
disappears and the incomplete gray-horizon world frame has no recognizable
scene.
Evidence: `/tmp/sly-space-flicker-recheck-20261116.log`,
`/tmp/sly-space-world-frame-recheck-20261116.log`,
`/tmp/sly-space-world-before-hud-20261116.png`, and
`/tmp/sly-space-world-after-hud-20261116.png`.

The host32 DMA adapter now routes VIF1 submissions into the existing VIF1/VU1
runtime. The observed world chain's masked VIF `UNPACK` command `0x78` now
passes for the selectors seen in the capture. The current failure is later:
VU1 reaches XGKICK at PC `0x68` with VI6=`469`; VU data qword 469 is
`0x0000000000000048:0x0000000000039001`, whose low half has reserved GIF-tag
bits set. Lower instruction `0x800036fc` is XGKICK itself, paired with upper
NOP `0x000002ff`; the opcode is implemented, and execution fails because GIF
packet validation rejects its data. The VIF trace now identifies the writer:
UNPACK command `0x6c`, immediate `0xc1d4`, vector 1 writes qword 469 directly
from `{0x00039001, 0, 0x48, 0}`. Thus this bad value is not from a VU store or
GIF packet decoder byte order. The earlier dispatcher stopped the VU program
when this packet was rejected. A port-only change now rejects just that packet
and lets the VU program continue, with XGKICK dispatched after the next
instruction pair as in OpenRAC's VU interpreter. The hidden capture from
commit `558e60b` confirms the first packet is still rejected and execution
reaches the second XGKICK at PC `0x6e`, address 35. That packet is rejected
too: qword 35 remains zero and qword 36 contains vector data. No native world
meshes are queued, and the captured post-input frame is only the gray
background. This advances the trace, but does not produce world graphics.
Capture:
`/tmp/sly-vu-continuation-capture-20261116.log` and
`/tmp/sly1-native-world-00.ppm`.

An earlier capture of `JALR VI15,VI15` appeared to jump to PC 1193 and was
reverted. A newer run that preserves the pre-link target advances through the
world setup path, so that behavior is retained in the port-only translator.
The resulting frame still contains no recognizable scene geometry; keep
validating the helper against the retail VU execution before treating this as a
finished graphics path.

### VU helper execution update (2026-11-16)

The port-only VU translator now covers the captured lower MOVE, MR32, DIV Q,
WAITQ, MFP, and ERLENG instructions; upper CLIPw and accumulator broadcast and
component forms were also added or corrected from the public
[Sony VU User's Manual v3.1](https://www.manuallib.com/download/pdf0/VU-USER-MANUAL.PDF).
All changes remain in `port/runtime/vu1_native.cpp`; matched decomp files were
not modified.

A fresh offscreen capture with one Cross press at native frame 120 enters world
`0x870cf0`, queues 41 world mesh callbacks on a single present, and reaches 594
accepted graphics DMA submissions with zero rejected. The corresponding dumped
frame is only a flat gray background; no Sly character or level geometry is
visible. The later VU execution reaches the 4,096-cycle interpreter safety
limit at PC `0x25e`; the `0xffffffff` words in that diagnostic are the limit
sentinel, not an unsupported retail opcode. The scene is not playable yet, and
the user's reported screen flicker remains unresolved. Continue by tracing the
last VU PCs and expected E-bit termination/branch behavior before changing the
interpreter budget or synthesizing geometry.

Evidence: `/tmp/sly-vu-accsem-finalcapture-20261116.log`,
`/tmp/sly-vu-accsem-finalbuild-20261116.log`, and
`/tmp/sly1-native-world-00.ppm`.

### VU loop limit follow-up (2026-11-16)

The opt-in instruction-budget trace (`OPENRAC_TRACE_VU1_LIMIT=1`) identifies
the 4,096-cycle stop as a real loop, not an unsupported opcode. At PCs
`0x26e`–`0x271`, the lower instructions increment VI5 by four, store a vector,
branch while VI6 differs from VI5, and execute a decrementing vector load in
the branch delay slot. VI6 is zero at the captured limit. Re-running with a
temporary 65,552-cycle budget lets VI5 wrap to zero and the branch fall through
PC `0x272`; execution continues to PC `0x243` and exhausts the budget again
without reaching an E-bit. Other VU entries also hit the larger limit. This
shows that raising the default limit alone would only move the stop and could
hide a second loop. Keep the 4,096 default and trace the setup/outer-loop path
and VU state transitions before changing semantics or the budget.

A focused `OPENRAC_TRACE_VU1_LOOP=1` run records the normal batch setup: VI5
starts at 36 while VI6 is 316, then VI6 becomes 308 and the loop advances VI5
toward that value. A later pass enters with VI6=0; its loop increments VI5 by
four and PC `0x272` unconditionally returns to PC `0x243`. This makes the next
lead the data that initializes VI6 at the routine entry, and the zero-target
case that follows. The trace is capped and opt-in; it changes no game state.

Evidence: `/tmp/sly-vu-limit-context-20261116.log`,
`/tmp/sly-vu-loop-extended-20261116.log`,
`/tmp/sly-vu-loop-termination-20261116.log`,
`/tmp/sly-vu-loop-state-20261116.log`, and
`/tmp/sly-vu-loop-detail-20261116.log`. The captures still show black frames
after Cross; no playable visual output was reached.

### VU scalar pipeline state (2026-11-16)

Inspection found that lower instructions ran against a temporary state, then
the interpreter merged vector and integer results but dropped P and Q. The
port-only state merge now preserves P/Q updates. A fresh host32 build compiles
and links; a bounded hidden run advances past the previous `MSCAL 0x242` stop
to `MSCAL 0x401`. That program exhausts the existing 4,096-cycle budget at PC
`0x3ee`, and the following VIF stream is rejected. The first world XGKICK
still rejects the same malformed GIFtag, and the dumped post-Cross frame is
black. This is interpreter progression, not a visual pass. Keep the fix
isolated from decomp source and trace the `0x401` program and its input before
considering a budget change.

Evidence: `/tmp/sly-vu-pq-merge-build-20261116.log`,
`/tmp/sly-vu-pq-merge-capture-20261116.log`, and
`/tmp/sly-vu-pq-merge-frame07.png`.

The opt-in trace for `MSCAL 0x401` records initial VU data values VI5=36,
VI6=348, VI8=364, and VI9=3; setup adjusts VI6 to 350. Its loop at PC `0x3fd`
advances VI10 by three while stepping VI8 down. With a temporary one-million
cycle limit, it still reaches PC `0x3ee` without an E-bit (VI10=49,846,
VI6=2). This confirms the 4,096 limit was not the only issue and does not
justify raising the default. Trace how the VIF input and branch termination
are meant to end this long-running program.

Evidence: `/tmp/sly-vu-entry401-state-20261116.log`,
`/tmp/sly-vu-entry401-final-20261116.log`, and
`/tmp/sly-vu-entry401-1m-20261116.log`.

A bounded 48-second run with a two-million-cycle override was accepted but
did not resolve the VU work: entries `0x242` and `0x318` still exhausted that
budget, with `0x242` stopping at PC `0x25f`. Those budgeted calls did not reach
their E-bit, and the run did not reach entry `0x401` before the wall-time bound
ended. The opt-in override ceiling is now four million; the default remains
4,096. Do not keep extending synchronous VU runs until the repeated program
entries and VIF stream ownership are clear.

Evidence: `/tmp/sly-vu-entry401-2m-20261116.log` and
`/tmp/sly-vu-extended-cap-build-20261116.log`.

### Host32 prompt ABI fix and flicker (2026-11-15)

The copied decomp headers retained the matching declaration
`AddGifRegisterWrite(PacketWriter*, u64, u64)`, while the host32 port adapter
expects a 32-bit register address and a 64-bit value. Normalizing only source
definitions left an i386 caller/callee stack mismatch. The new
`normalize_gif_register_writer_abi` adapter applies to staged headers and
sources, leaving matching decomp files unchanged. The next native trace showed
correct game-generated values: `SCISSOR_1=0x00de0001027f0000` and
`0x00df0000027f0000`, decoded across the 640x224 display area. TEX0 now has
`TW=TH=10` rather than zero.

The user's Nov 15 recording shows the no-card prompt fully visible, but it
flickers to black. Frame analysis found 49 black game-area frames out of 393;
the longest run is 7 frames (about 117 ms). Three native readback samples
(two text pixels and one border pixel) go black together even when packets
arrive and DirectRenderer decodes 5–7 draws. A delayed capture had one black
image among eight. A present trace had packets on 10,386 of 10,388 calls, so
missing packet input alone cannot account for the flicker. Skipping the
per-present clear in a diagnostic still produced about 9% black samples; the
clear is not the only cause. The short white dash at the far left remains
unexplained.

On Nov 16, a draw-state trace identified one cause: on black frames, a 120-vertex
full-screen black fill appears after the prompt draws. The DirectRenderer's
replacement-fill filter required a scissor at least 400 lines high, but the
game uses a 224-line scissor (`0..223`). The filter now recognizes a full-height
200–240-line scissor as low-resolution output, so the native black clear
handles these replacement fills. A fresh five-second capture saved eight
consecutive visible prompt frames; only 2 of 227 readback frames after startup
were still black. Those two frames contained a single full-screen textured
draw and no prompt geometry, so they are a separate unresolved display-copy
case. Evidence: `/tmp/sly-flicker-fixed-capture-20261116.log` and
`/tmp/sly-flicker-fixed-contact.png`.

A follow-up trace correlated those remaining black frames with a full-screen
120-vertex missing-texture draw. The draw was deliberately color-masked, so it
wrote nothing; the host cleared the new backbuffer first and then presented
that blank buffer. The Sly present path now saves the last completed image in a
texture and blits it back on frames with no effective screen draw. It tracks
explicit replacement fills separately, so a real game clear still clears the
display. Each present also binds framebuffer 0 and restores color/depth write
masks before clearing. A six-second hidden capture showed ten no-op frames
restoring the prior image and zero fully black pixel samples after the prompt
first appeared in 312 subsequent samples. See
`/tmp/sly-flicker-final-capture-20261116.log`; frame 07 is
`/tmp/sly1-native-frame-07.png`.

Reproduce the correlated trace with `OPENRAC_TRACE_PRESENT=1`,
`OPENRAC_TRACE_PRESENT_PIXEL=1`, `OPENRAC_TRACE_DIRECT=1`,
`OPENRAC_TRACE_DIRECT_ALL=1`, and `OPENRAC_TRACE_DIRECT_STATE=1`. Add
`OPENRAC_DEBUG_PRESERVE_FRAME=1` only for the comparison run.

DirectRenderer resolves texture handles 2–4 for the visible panel draws. A
separate GS stream diagnostic continues to report `TBP0=0` and placeholder
texture state; it does not describe those resolved draw handles and should not
be treated as the flicker cause without a draw-boundary correlation.

The user's prior traced Space press dismissed the card prompt and loaded world
`0x870cf0`. The current long-running foreground process remained on no-card
page 3 after another Space press; because it started without `OPENRAC_TRACE_PAD`,
event delivery is unconfirmed. A separate offscreen run injected one Cross at
native frame 120 and observed `held=0x4000` followed by release, then reached
the world draw path without taking focus from the visible window.

The first XGKICK boundary is now localized. VIF UNPACK `0x6c`/`0xc1d4`, count
7, writes a valid EOP GIFtag at qword 468 (`low=0x100040000000806d`,
`high=0x000000000000000e`) and payload at qword 469
(`low=0x0000000000039001`, `high=0x0000000000000048`). The VU program loads
VI6=`468`, increments it at PC `0x67`, then issues XGKICK VI6 at PC `0x68`
with VI6=`469`. Traced preceding DIRECT packets all end with EOP, so the parser
is not continuing one of those packets. An opt-in retry from qword 468 submitted
the 110-qword packet but the captured frames remained black; the next XGKICK at
PC `0x6e` from VI7=`35` fails too. The retry was removed. Trace the VIF source
records and both VU pointers next; do not backstep XGKICK addresses or relax GIF
validation. A follow-up run places the VIF command, tag vector and payload
vector at stream offsets 5916, 5920 and 5936. It also records UNPACK writes to
qwords 36–38 but none to 34–35; the previous retry found qwords 34–35 zero
before the second XGKICK. Transfer 196 places all three offsets inside one DMA
segment (`0x018d1270` plus byte offsets `5084`, `5088`, and `5104`), so this
packet does not cross a source-segment boundary. The US overlay table
identifies the exact microcode as program `437275`, loaded at VU address 0;
the instruction words at PCs
`0x5f`–`0x6e` match the retail executable. At the first failure VI3=`628` and
VF7 is `{0x4149d0e5, 0xc0d92618, 0x42877fa4, 0x4b008958}`. The following SQ
targets qword 627, not 35, leaving the second XGKICK's tag unexplained. Keep
matched decomp source unchanged.
Keep VIF/VU behavior in the runtime and any Sly function replacements under
`port/game/sly1-us/native/`, separate from matched decomp source. Do not expand
the optional GL prototype or resume broad model ranking. The pass remains a
valid VU1 XGKICK/GIF packet and a non-black frame attributable to loaded Sly
data.

## Clean reset review (2026-11-15)

The window and frame loop are no longer the issue. The game dismisses the
no-card page, loads a world and reaches model draw callbacks. A same-frame
before/after-HUD capture shows that the HUD pass does not erase the world
image. At this checkpoint the host32 route passes the observed masked UNPACK
and reaches a VU1/GIF failure; the older masked-UNPACK stop below records the
state before that implementation.

The supplied 62.76-second PCSX2 recording shows the opening cinematic,
rooftop views, and the city title screen. These establish useful visual
references, but the native capture has no recognizable object or skyline to
match to a particular frame or camera. Similar nighttime city art is not a
draw-level match. Do not resume renderer implementation from this comparison
alone.

The source adapter builds against public decomp master `8f7a85a3` without
changing matching source. Both the active `1cbb33fd` checkout and the public
snapshot compile 194 matched units and link. This compatibility work is done
for the current revisions; do not keep broadening it without a new build
failure.

The first experiment is not ready for more rendering features. The decomp
defines each `FrameSource` as a stride and base for a frame-selected payload
unpacked into a VU input range. The captured 128-byte kind-6 payload is a
packed GIF A+D packet with six register writes. The active cursor selects the
frame offset for each source. Source records are input ranges, not independent
whole-mesh GL draws; submitting the same mesh once per source was tested and
did not improve the gray horizon image.

The adapter still substitutes base expanded positions for Sly's active VU
output and does not apply morph or skinning transforms. It also approximates
Sly's VU output with OpenGL. A native capture before HUD rendering proves
some world geometry reaches GL, but the image is still only a gray horizon
and ground. The saved tag confirms six A+D writes, including the expected GS
register addresses. The native adapter validates that tag, uses the active
cursor frame to select its payload, and extracts TEST1, TEX0_1, TEX1_1, and
CLAMP1 by register address. It still draws expanded source positions rather
than VU output, so valid material state alone does not make the scene match.

The focused trace recorded early kind-0 sources with a null cursor and frame
zero, and the world pass submitted 41 draws. Animation selection is not the
current blocker for those static meshes. The latest evidence supersedes the
older VU-output hypothesis: first complete masked VIF UNPACK before diagnosing
the downstream VU/GIF result.

The fourth component written by `BuildStripMeshPacket` is fog bias
(`fogBias + 28672` or `fogBias + 32768`). The kind-6 VU transform uses source
XYZ and builds clip W from the matrix and homogeneous constant. Therefore the
native `vec4(x,y,z,1)` is right for clip position; the flat-color prototype
still omits fog. Do not pass the raw fog value as clip W.

The owner trace separates the likely backdrop from the screen scene. The
kind-6 meshes are owned by class `0x54` (`CameraPositionedObject`), which
moves with the camera. The 30 kind-5 meshes are owned by class `0x61`
(`TextureRenderObject`), whose parts draw into a texture. A finite-transform
class `0x02` object has 38 kind-0 lit meshes, but its first mesh is too small
to serve as the first visible target.

Keep the kind-6 prototype disabled. A broad trace of
`BuildLitStripMeshPacket` calls did not match the final part pointer at draw
time, so it was removed. A narrow draw-boundary capture was decoded instead.
The VIF UNPACK at packet-0 offset `0x20` declares 68 V4-32 positions at
`0x24`; using their XYZ, homogeneous 1, and the captured model/view-projection
matrices yields bounds of about 11 by 2 pixels in a 960x720 viewport. That is
only an upper bound before strip filtering, so this part is rejected as the
first graphics target. Do not extend general VIF/VU/GS execution. Owner trace:
`/tmp/sly-class-owner-trace.log`.

The bounded follow-up run reached active world `0x00870cf0` at about 56 FPS
after one process-local Cross press. It submitted a finite-transform class-2
part at `0x016d4460` with 38 lit meshes. The first mesh's three draw-boundary
streams were captured at their declared sizes: 141, 87 and 77 quadwords
(2,256, 1,392 and 1,232 bytes). This verifies that the candidate reaches the
draw handoff; the streams are still PS2 command/data packets, not decoded
vertices, so it does not establish visible geometry. Decoding the first
position stream gives only about 11 by 2 pixels of projected coverage; it is
too small for the first visible-graphics target. The run log and raw captures
are local at `/tmp/sly-targeted-world-part.log` and
`/tmp/sly-world-part-{0,1,2}.bin`. Next rank the remaining active draws by
coverage and compare the camera-positioned candidate with the corresponding
PCSX2 scene before changing the renderer.

## Reset the graphics plan (2026-11-15)

The recent work improved observability but did not move the visible milestone:
the no-emulation window still shows no world graphics. That is not a playable
build. Stop adding startup backfills and trace probes unless they are shown to
block the first native world draw. Keep the current black-scene capture as the
baseline and make the next deliverable one captured frame with a loaded Sly
mesh visible.

The mesh-build capture gives a narrower route than the earlier plan. A
`FrameSource` record is an 8-byte stride/base descriptor. Its base points to
V4 input data copied to a VU memory range; it is not a GIF material packet or
a primitive index list. `BuildStripMeshPacket` expands positions and UVs in
`VertexRefs` order, then constructs the VU input packet. Preserve those
builder inputs and the active cursor at the native handoff before attempting
the Sly-specific VU conversion. This is a bounded question; it does not
require a general VU or PS2 device emulator.

An opt-in native adapter copies expanded kind-6 positions and tint colors,
matches them to `PartMesh.kind`, and submits them with part and camera matrices
to an OpenGL triangle-strip pass. It does not preserve UVs or strip boundaries.
The hidden run confirmed three mesh captures, three matched draw submissions,
and repeated GL draws. Its saved 960x720 frame is uniform dark gray with no
recognizable Sly geometry, so the first-graphics gate is still failed. The
prototype is disabled by default. Do not present the gray frame as visible
game graphics or treat it as evidence that clipping is the cause.

`BuildStripMeshPacket` expands positions and UVs in `VertexRefs` order, and
records each VU input range before building VIF packets. The captured kind-6
payload's GIF tag and register IDs confirm that it carries material state
through the VU path. The builder's fourth position lane is fog bias;
inspection of the selected VU transform shows it uses XYZ and supplies clip
W from the matrix and homogeneous constant. This does not explain the screen
coverage by a W=1 error. Continue identifying a foreground mesh by owner and
draw order, using the active cursor and VU inputs. Keep OpenGL work on the
render thread.

## Builder input capture (2026-11-15)

The host32 build now has an opt-in trace at `BuildStripMeshPacket`, before Sly
submits the mesh to VU1. A hidden run reached world `0x870cf0` and captured
three unskinned kind-6 builds with 68, 89 and 106 vertices. Each has one
128-byte strip source. The 106-vertex candidate references positions 0–41 and
UVs 0–49. The trace saved its source arrays and strip bytes under `/tmp`; no
game data was added to the repository. Log:
`/tmp/sly1-mesh-builder-capture.log`.

This made a source-level Sly mesh adapter practical. The opt-in adapter now
consumes the builder's expanded vertex references, positions and tints
without decoding the generated VIF packet. It submits native draws, but its
captured result is not recognizable geometry and does not pass the visual
gate.

The host32 port compiles 194 matched Sly units and 85 separate port-only
backfills. After one process-local Cross press, it leaves the no-card page,
loads world `0x870cf0`, and advances at about 56 frames per second. The prior
Wave 492 claim that frames and world stayed at zero was caused by a disabled
counter; it was not evidence of a hang.

The HUD `DirectRenderer` still consumes only its own packet bucket. The
opt-in Sly kind-6 adapter is a separate world path, and its current captured
output is uniformly dark gray rather than recognizable geometry. The two
decoded HUD draws and 18 vertices remain unrelated to the level scene.

The Wave 503 “no model draw” result was a filter bug: the probe selected a
target world based on the first model callback, suppressing the target when
that callback was also the first observed. Wave 512 observes the world change
from the main loop and confirms that the loaded world queues
`DrawPlacedObjectParts` for an object with one model part. The callback reaches
`DrawModelPart`, which sees a skinned kind-5 part, then three unskinned
kind-6 meshes. The first kind-6 mesh has 106 vertices and one strip source of
eight quadwords. It needs no bone transforms and its draw matrix is finite.
The captured 225-quadword packet contains its UV, position, color, and
material-state input. The VU's clipping and GS output semantics are still not
reproduced. Evidence:
`/tmp/sly-wave512-model-correct-filter.log`,
`/tmp/sly-kind6-model-trace.log`, and `/tmp/sly-kind6-model-packet.bin`.

`BuildStripMeshPacket` selects `g_unlitBatchProgram` (`D_00001A50`) for kind 6
and later calls `g_unlitBatchFinishProgram` (`D_000002F8`). The next named
program after the first entry is `g_unlitKind4BatchProgram` at `D_00001A80`,
six 8-byte VU instruction pairs later. That spacing is not the routine's
control-flow boundary: the kind-6 entry branches into adjacent VU code and a
later branch loops back to PC `0x341`, before the entry. The branch exit at
`0x384` is a shared return. `QueueMeshStrip` copies one frame of the mesh's
eight-quadword source into the packet's VU data area. The exact reachable
control-flow slice and finish behavior still need mapping before implementing
the host conversion. This remains bounded to Sly's mesh path; it does not
justify a general VU implementation. The retail `.vutext` bytes stay local
and are not copied into this repository.

The renderer inspection now shows an opt-in but visually incorrect Sly world
renderer. Leave `sly1-decomp` untouched and keep functional backfills under
`port/game/sly1-us/`. Keep VIF/VU/GS execution as bounded diagnostics. The gate
is an image capture with loaded Sly geometry positioned by the game's
transform and camera. No title or playable level is confirmed. See the
[restart checkpoint](RESTART_CHECKPOINT.md) and [playable TODO](PLAYABLE_TODO.md)
for the current plan.
Historical Wave 39 context: two staging-only returns prevent matched
non-void wrappers from falling through after void calls; the decomp source
remains untouched. A previous false page-22 disc-change pause was traced to
absent native IOP services and a status shim writing to the wrong address
space. Port-only CD services now report the host ISO ready and unchanged and
update relocated `D_002613F0`. The latest native run also fixed an interior
data alias that had resolved the pad field `D_00262D18` to `0xa8`; it now
resolves to `0x082fcee8` inside `D_00262C70`. Native no-card service
backfills let retail startup reach pause page 3, and a bounded 10-second run
reached about 600 frames without a fatal fault. A later native capture saved
eight SDL framebuffer frames after that state was reached; those startup
frames were black. The trace's global UI/text counters do not include the
pause-menu layer.
A 2026-11-12 follow-up found the native `func_00201D60` backfill wrote `0x250`
bytes through a two-record (`0x50` byte) retail display context, overwriting
the GIF channel global with display height `0xe0`. The backfill now writes
only those records. A bounded host32 run kept the GIF channel at `0x082ffee8`
and submitted 735/735 DMAs with zero rejections. A separate staged host ABI
mismatch in `AddGifRegisterWrite` shifted the GS value by one word because its
writer used a 64-bit register argument while callers passed 32-bit registers.
This reduced full SCISSOR_1 values to `0xde0001` and `0xdf0000`, clipping all
output to one row. The host staging adapter now normalizes the writer
signature; the trace shows full scissor bounds and textured draws. The renderer
now uses Sly's 640x224 field dimensions. A bounded capture shows the complete
no-card warning panel, border, and highlighted Continue option at normal
proportions. A follow-up confirms the no-card prompt's notifier receives key
`0x263`, passes its retail completion gate, and destroys the state walker; the
previous self-loop-only diagnosis was incomplete. One Down input closes page
3 and advances the memory-card display state, but the Splash world remains
active. No title or playable scene has been demonstrated. See the
[restart checkpoint](RESTART_CHECKPOINT.md) for evidence and next steps.

The user-confirmed visible unformatted-memory-card format prompt was observed
in the separate experimental guest-runtime launcher. The no-emulation host32
path reaches the retail no-card warning and renders its full panel and
highlighted Continue option at normal proportions, but has not reached a
demonstrated title or playable scene. No format action was taken. The graphics
findings below are guest-runtime research only, not playable milestones. See the
[restart checkpoint](RESTART_CHECKPOINT.md) and [playable TODO](PLAYABLE_TODO.md)
for the current architecture and next work.

## Experimental guest-runtime graphics notes

The following VIF1 and GS notes document the bounded device-model experiment.
Do not extend it as the gameplay renderer.

The experimental VIF1 DMA backfill preserves the TTE bit set by the retail
caller. Previously it overwrote CHCR with `0x105`, dropping inline VIF codes,
so the interpreter tried to decode VU microinstructions as commands. The
fixed startup transfer parses as 64 bytes with one MPG and one MSCAL; the next
large transfer parses eight MPGs and 736 DIRECT bytes. The window still
reaches Splash with zero vertices/image tags and no visible text. The
unhandled VIF0 upload begins at `.vutext` address `0x00211e10`; do not expand a
hardware VU emulator. If VU0 work is needed by a reachable path, port its
specific call-site behavior into native C. Latest trace:
`/tmp/sly-vif1-tte-startup-8s-20261111.log`.

A 20-second bounded follow-up reaches 9.5 guest seconds, through the
CountdownNotifier's configured 4–6 second interval. Splash remains in element
stack state 3, with no movie queued, all 37 UI elements inactive, no visible
text, and zero image tags/qwords. The vertex counter briefly varies from 6 to
48, but there is no visible artwork. The title transition remains blocked.
Log: `/tmp/sly-vif1-tte-progression-20s-20261111.log`.

Current visual diagnosis: startup reaches frame state 3 and creates the UI
objects/fonts, but their draw methods are inactive. The frame's UI dispatchers
run, text-buffer setup runs five times, and no text/glyph/screen-element draw
or menu draw is reached. The user pressed Enter; the pad bridge saw Start
(`0x0008`), but the title/menu state did not change. Retail `UpdateElementStack`
shows why: state 3 consumes R1 (`0x0800`) to open the pause layer and L2
(`0x0100`) to open an available map; it does not consume Start. Keyboard
bindings are Q for R1 and `1` for L2. This explains the Enter result, but the
automatic Splash/title transition and missing graphics remain the blocker.
No vertices, image transfers, or GIF submissions reach the window. The loaded
Splash world contains no class-`0x65` StepSequence objects. Its class-`0x57`
CountdownNotifier is state 0 with a null `+0x338` gate pointer, so retail
`CountDownNotifierTimer` returns without counting down. The state graph stays
unarmed on the `0x261` self-edge and no movie is queued. Trace the normal level
resource/state activation path that should provide the sequence and gate.
Latest capture: `/tmp/sly-title-step-sequence-final.log`.

**VIF writer recovery (2026-11-11):** A forced level load reached its lit-strip
mesh packet builder and exposed four no-op VIF writer helpers. Hostgen now
selects exact US retail assembly for `func_001EE4C0` (`WriteVifStrow`),
`func_001EE528` (`WriteVifStmask`), `func_001EE560` (the unpack-header helper),
and `func_001EE718` (`WriteVifUnpack`). This removes their no-op behavior
without introducing replacement C. The normal Splash run still produces no
visible artwork. The forced level load now fails at the 32 MiB guest heap
boundary during mesh packet construction; it does not reach a rendered level.
See `PLAYABLE_TODO.md` for logs and the exact failure point.

A fresh SDL native launch showed the remaining no-op cluster and exact US
assembly is now selected for 22 reached visual/UI functions, including text
panel and arc/item setup, UI state helpers, packet buffers, masked-image
packet submission, resident-image transfer, and texture-register setup. The
build succeeds with 732 C translations, 362 assembly translations, and 2
stubs. In the 20-second follow-up, these functions no longer warn and the
sampled initialized font count rises from 35 to 37. However, all 37 elements
remain in mode 0, the Splash animation player advances on the `0x261` self-edge
(`2 -> 2`), and the trace still reports zero vertices, visible text, image
tags, and GIF submissions. Trace: `/tmp/sly-renderer-retail-asm-20s.log`.

A second recovery restores 11 more reached UI state callbacks from exact US
retail assembly: text-panel and timer state, element hiding, cursor-frame
selection, texture-register lookup, and near-camera group updates. The current
build has 732 C translations, 373 assembly translations, and 2 stubs. The
follow-up trace removes these no-op warnings but still finds all 37 elements
inactive and zero vertices, visible text, image tags, or GIF submissions. The
Splash player stays on the `0x261` `2 -> 2` edge. Log:
`/tmp/sly-ui-state-retail-asm-20s.log`.

Exact US assembly is also selected for 15 more reached callbacks covering GIF
ST entry creation, text-panel text and subject, frame transforms, and
world-list/camera updates. The launcher now builds with 732 C translations,
388 assembly translations, and 2 stubs. A 65-second native run reaches 32.15
guest seconds but remains in global element-stack state 3 (its normal state),
with no input, no movie request, no world load, inactive screen elements, and
zero graphics output. Wave 24 identifies this as `g_elementStack`, not an app
state waiting for input. Log:
`/tmp/sly-long-startup-65s.log`.

Exact US assembly is now selected for the reached frame setup helpers
`func_00193210`, `func_00193270`, and `func_00193288`, which the port's frame
initializer had been calling as no-op stubs. The 15-second native trace shows
those stubs are gone, but still reports zero vertices, visible text, and GIF
submissions. The Splash graph object is present at ID `0x25f`; it remains on
state 2 / event `0x261` / self-edge `2 -> 2`, so the earlier missing-graph-link
diagnosis does not fit this run. Trace: `/tmp/sly-frame-init-asm-15s.log`.

The reached resource/font setup no-ops `func_001E2268` and `func_001E22C0` are
now also selected from exact US assembly. A second 15-second native trace
confirms the warnings are gone, but still shows all 37 UI objects in mode 0
and zero vertices, visible text, or GIF submissions. The CountdownNotifier
stays in state 0 with its `+0x338` timer gate null and timer at 0 (configured
range 4–6 seconds); the event `0x261` graph edge remains an unarmed `2 -> 2`
self-loop. Trace: `/tmp/sly-font-layout-init-asm-15s.log`.

Runtime tracing found additional no-op screen constructors during the active
`InitScreenElements` pass. Exact US assembly is now selected for timer, gauge,
framed-text, pointer-caption, letterbox, and arc-sprite setup, along with the
reached text measurement helpers. The 10-second follow-up has no warnings for
these functions, but every sampled UI object stays at mode 0 and renderer
counters remain at zero vertices, image tags, and GIF submissions. Restoring
the setup callbacks did not arm the Splash UI or produce graphics. Trace:
`/tmp/sly-wave22-screen-init-asm-10s.log`.

Exact US assembly is now selected for the 24 missing functions reached from
the port's frame-system initialization callback table, including
`func_001688B8` (display setup) and `func_0015E810` (render-frame setup). The
12-second default and direct-level traces still report zero vertices, visible
text, and GIF-chain submissions. This closes a no-op initializer gap but does
not produce game graphics. Logs: `/tmp/sly-canonical-55d39f28-init-callback-asm-12s.log`
and `/tmp/sly-canonical-55d39f28-init-callback-asm-direct0100-12s.log`.

The active animator helpers `func_001B4AA0` and `func_001B4AD0`, UI updater
`func_001AB058`, and commentary updater `func_001E00B0` are also selected from
exact US assembly after runtime traces showed them being called as no-op
stubs. They no longer warn, but the 10-second follow-up remains at zero
vertices, visible text, and GIF submissions; all 37 UI objects have mode 0.
Trace: `/tmp/sly-canonical-55d39f28-reached-anim-ui-asm-10s.log`.

The Splash StateGraph link is present (`0x00847800`, ID `0x25f`), correcting
the earlier missing-link diagnosis. The graph start and initial walker
routines are no longer no-op stubs; its small pool-pop and listener
dispatch helpers are port-only C backfills. The latest bounded run still has
zero scene vertices, visible text, and GIF submissions, so there are no
confirmed game graphics yet.

Older SDL traces reached the initial resource, set `g_currentWorld=0x006abdd0`,
and briefly reported 240 raw vertices; those samples were not confirmed game
art. The latest experimental guest-runtime run remains visually black with
zero scene vertices, visible text, and GIF submissions. This is not a report
from the no-emulation host32 launcher. The resource tables contain 61
image records and 63 palette records; four font IDs are registered. Port-only
`QueueWorldLoad` and image record readers remain separate under
`hand/native_c/` and `hand/backfills.tsv`. Exact US assembly is enabled for the
reached image-mask upload and screen packet builders. Those functions remove
several no-op stubs, but image tags remain at zero. The VIF trace shows later
direct streams are accepted and an earlier 48-byte startup transfer has a
malformed `DIRECT` count. Latest trace: `/tmp/sly-vif-dma-debug.log`. Next,
inspect why accepted texture streams produce no GIF image tags and why visible
UI geometry is black. Detailed progress is in
[`PLAYABLE_TODO.md`](PLAYABLE_TODO.md).

Most older graphics traces and captures below are historical from the earlier
SDL build. Their render-list and texture observations remain useful, but do
not describe the current host32 execution. The current frame packet trace is
`/tmp/sly-canonical-55d39f28-frame-packet-trace-8s.log`; the five mesh queue helpers are
now selected from exact retail assembly and verified to run, while the
direct-level run still shows zero scene vertices/GIF submissions. See the
latest details in `PLAYABLE_TODO.md` and `RESTART_CHECKPOINT.md`.

The latest direct-level trace reaches loader stage 5 but submits no scene
vertices or visible text. It found a live, unskipped render-list entry at `0x00847ce0`
with three primary records. Those records point through `0x00844450` to two
non-null structures (`0x0083fc60` and `0x0083f4b0`); the latter references
three 512-byte payloads at `0x0064d6e0`, `0x0064d8e0`, and `0x0064dae0`.
These are GS image-upload data, not mesh vertices. Retail `func_001662A8`
produces two 8-qword
buffers for this entry, but both contain only DMA/VIF control tags and zero
addresses; they do not reference those image payloads. Other direct-level
groups are still skipped by their resource flag. The scene draw list still
has zero geometry submissions. A separate blocker remains:
default startup still stays on Splash rather than reaching the intro/title
flow. The source now identifies `D_00269A00` as `MoviePlayer`; offsets `+4`,
`+8`, `+0xC`, and `+0x134` are queued key, successor, reader, and decode
thread step. This corrects the earlier trace label `thread_dispatch`. See
dated checkpoints below for details. Port-only C overrides remain under
`hand/native_c/` (with retired
experiments under `hand/superseded_backfills/`); neither those nor generated
assembly translations modify the decomp repository.

### Current unverified startup repair (2026-11-07)

Retail US `func_001E4EA8` has a shared per-frame service tail that runs even
when its resource request slot is empty. The port-only replacement previously
returned immediately in that case, skipping camera/world services and the
`func_001C06D8` initializer. The current working tree restores the tail and
selects exact retail assembly for `func_001C0070` and `func_001C06D8` because
matching decomp C is unavailable. The native build succeeds, but this change
has not yet been exercised in a launcher run; do not count it as fixing the
black screen until a default-startup trace shows visible output or a state
transition. See `RESTART_CHECKPOINT.md` for the immediate next steps. All
functional C remains a port-only backfill.

**Verification update:** the first default-startup run after restoring the
service tail stayed at frame state `0,3,0` with `visible_text=0`. The service
tail also called `func_00160CE8`, which hostgen emitted as a no-op despite its
simple counter-reset body in the decomp source. A port-only `+4 = 0` backfill
now removes that no-op, and the rebuilt launcher runs without its warning; an
8-second default-startup trace still shows frame state `0,3,0` and no visible
text. The service-tail issue is corrected, but the Splash-to-title transition
remains blocked. See `PLAYABLE_TODO.md` and `RESTART_CHECKPOINT.md` for the
updated evidence and next trace target.

Exact US assembly has since been enabled for five reached no-C frame/update
routines (`func_001E77C0`, `func_001E9C78`, `func_001E9EE8`, `func_001EA0A8`,
and `func_001EAAE0`). They now execute instead of no-op stubs, with no crash,
but the follow-up default run still reports no visible text and frame state
`0,3,0`. The decomp remote currently has no commits beyond local
`e1b39f1`. The next investigation is the retail state transition and its movie
or resource request producer, not those update helpers. A 35-second native
run reached about 17 seconds of guest time and still showed no state change,
menu, or visible text. The trace's previous `movie` labels were incorrect:
`0x00269A04..0x00269A10` are worker-dispatch fields used around
`func_0018E4F0`; their values say nothing about PSS playback. New traces call
them `thread_dispatch`.

The previously missing `func_001DC9A0` is now selected from exact US retail
assembly because the matching decomp C is still unavailable. Its 800-byte
routine conditionally creates a class-`0x51`/subtype-`0x201` object and applies
level effect parameters. It no longer runs as an empty generated stub, though
the latest bounded run remains at frame state 3/game mode 0 with zero scene
vertices. Trace: `/tmp/sly-native-direct0100-dc9a0-asm.log`.

Pointer and packet captures: `/tmp/sly-native-direct0100-mesh-sections.log`
and `/tmp/sly-native-direct0100-packet-dump.log`.

The useful pointer values are preserved here so the lead survives deletion of
temporary `/tmp` logs. The descriptor at `0x0083f4b0` contains three 24-byte
records:

```text
01000000 00020080 0064d6e0 00000020 01300000 2003c040
01000000 00020080 0064d8e0 00000020 01300000 2003c080
01000000 00020080 0064dae0 00000020 01300000 2003c0c0
```

The first 32 bytes of each pointed-to payload are nonzero. The first has words
`03ffa51f 2dffa034 80ffb520 3eff9a67 40ffbcb5 80ffbf20 38ff9b52 41ffac95`;
the second has `02ffa51f 2dff9c3e 3eff9969 17ffa420 41ffc2bb 80ffbf2b
3cff9955 40ffad96`; the third has `02ffa51f 80ffa840 1eff9e3c 41ffac93
0fffa61d 3dff9b67 0affa423 2effa789`. Their format has not been identified.

The first `func_001662A8` output buffer at `0x0085c380` consists of eight
qwords: `40000000`, `10000001`, `03000028 02000000`, `40000000`,
`10000001`, `03000214 02000000`, `40000000`, `60000000`. The second buffer
at `0x0085c400` contains the two STCYCL command qwords in the opposite order.
Neither buffer contains the pointers at `0x0064d6e0`–`0x0064dae0`; this scene
packet builder has not connected the image data to scene geometry. A separate
resource-initialization GIF chain transfers these payloads as images below.

A native GDB hardware watchpoint on guest `0x0064d6e0`, armed before the
direct-level boot, fired during the reader copy path
`func_001DCEA0 -> func_001DBA38 -> func_00137A60 -> func_0018D800`. At the
copy routine the arguments were destination `0x0064cae0`, size `0x0c00`, and
the host-mapped source buffer; the watched block is exactly `0x0c00` bytes
after that destination. `func_001DBA38` advances its reader by `0x10`, then
reads `D_00275720` bytes into `D_00275718`. This confirms the watched bytes
arrive through direct-level resource parsing. The three 512-byte payloads
were confirmed byte-for-byte at decoded level resource offsets `0x4d0470`,
`0x4d0670`, and `0x4d0870`, and at decoded Splash resource offsets `0xac00`,
`0xae00`, and `0xb000`, respectively. Both sequences use a `0x200` stride.
This establishes that the same image data is present in both decoded
resources.

Filtered native traces with `OPENRAC_TRACE_DMA_TARGET` confirm all three are
image uploads in one GIF DMA chain rooted at `0xe0418ac0`. Each is a REFS tag
with QWC 32, followed by a GIF IMAGE payload of 512 bytes. Their payloads begin
at GIF stream offsets 704 (`0x64d6e0`, tag `0xe0418bb0`), 1312 (`0x64d8e0`, tag
`0xe0418c30`), and 1920 (`0x64dae0`, tag `0xe0418cb0`). Before each payload,
GS transfer setup sets `TRXREG` to width 16, height 8 and `TRXDIR` to
host-to-local. The image data is PSMCT32, so each transfer is a 16x8 image.
GDB confirms the retail call path is `func_001DCEA0 -> func_001B3450 ->
func_001692B0 -> func_001686E8 -> func_00202A80 ->
openrac_native_renderer_submit_dma_chain`. These are resource uploads during
level initialization, not the scene's vertex path; the renderer still submits
zero scene vertices. Do not treat these payloads as mesh or palette tables.

A targeted GDB breakpoint on `func_001662A8` for entry `0x00847ce0` showed
the call stack `func_001662A8 -> func_00165728 -> func_00124A48 ->
func_00146470 -> func_001DCEA0`. This confirms the observed command-buffer
construction also occurs while the direct-level resource is being
initialized. It is not evidence of a per-frame scene draw. Capture:
`/tmp/sly-dump-render-entry.gdb` and `/tmp/sly-dump-render-entry.log`.

Latest direct-level trace: `/tmp/sly-native-class1-vtable.log`.
Renderer traces show accepted GIF chains but `vertices=0`; those packet counts
are initialization traffic, not evidence of a rendered level.

The latest direct-level render-list capture narrows that result further. It
reaches five level groups (`0x00fe903c`, `0x00fe942c`, `0x00fe981c`,
`0x00fe9c0c`, `0x00fe9ffc`). Their primary-record counts are 1, 1, 1, 1, and
6. In each group's first entry, the word at `+0x28` is 1. Retail assembly for
`func_001662A8` returns before packet generation whenever that word is
nonzero; the post-call output counts remain zero for all five groups. This is
upstream of GS rendering and explains why the initialization GIF traffic does
not contain level geometry; the blocker is upstream of renderer submission.
A native debugger watchpoint on the first
entry showed the value being written by retail `func_00162890`, called from
`func_001289C0` during `func_0011ECB8` resource-object parsing. The assembly
stores the first byte read by `func_00137B90` at entry `+0x28`; the input byte
is `1`. The final group has one outer entry with six primary records, not six
outer entry flags. Treat the nonzero byte as resource data and preserve the
retail skip. Next identify what resource object kinds actually produce
renderable entries, or move to the normal title/gameplay transition; this
direct-load probe does not establish a playable scene. Captures:
`/tmp/sly-native-direct0100-render-groups-final.log`,
`/tmp/sly-watch-entry-flag-detail.log`.

The earlier fragmented menu captures were produced by a port-only forced menu
page and handwritten menu renderer. Those overrides have now been retired:
menu filtering and text rendering use exact US retail assembly, and matching
decomp C is used where available. Standard startup remains at Splash with no
fonts, text, or scene vertices. Do not count the old forced-page glyph
fragments as current vanilla game graphics.

The user-provided 63-second PCSX2 recording shows the European release
(`Sly Raccoon`): publisher credits, the Paris rooftop opening sequence, and
the animated title screen with Sly moving under the game logo and Start/Select
prompts. It does not show the user starting a new game. The native build uses
the US release, so use the recording as a sequence and feature reference
rather than a pixel-identical target or evidence about the native build.

Earlier default-startup traces reached menu text drawing and uploaded
the font atlas, though the text was fragmented; that run used the old pad
backfill and selected the pause menu at five seconds. With the updated native
pad mode, the latest trace remains in frame state 3 but reaches zero scene
vertices after 5.8 seconds. The latest run invokes retail `func_00125C90`
for at least eight placed-object entries, confirming that the world update
callback is active. Translating the `func_00143648` camera constructor now
lets sampled `func_00125C90` calls pass the first VU0 MAC gate (`0xF0`) and
reach the port-only `func_001453B0` sphere/frustum adapter. The adapter rejects
the sampled point: radius at node `+0x2A8` is zero, node position words at
`+0x140..+0x148` are zero, camera word `+0x40` is `0x4500D8D8`, and `+0x44`
is zero. The test point has x bits `0xC500D8D8` and y/z zero. This narrows
the current problem to scene/camera inputs or startup list entries; it does not
justify forcing the culling result. The direct-load probe still does not connect its loaded world to gameplay.
A bad declaration in the port `world_update.c` treated the camera pointer
global as an array and passed the global storage address to `func_00143E40`.
Declaring it as the pointer it is fixes the camera argument: its retail stack
now has one entry at `camera+0x454`, and the update callback resolves to
`0x001493C0`. The retail updater and two more missing-C functions in its
direct-call closure are now selected from assembly; a bounded native run reaches it without the former invalid callback
errors. This fixes the camera update call path, but sampled object positions
and radii remain zero, so the scene is still not rendered.

The request queue `func_0019D410` now preserves its host stack pointer while
copying the request into guest EE memory. Keep all functional C replacements
under `port/game/sly1-us/hand/native_c/`, listed in `backfills.tsv`, so
matching decomp C can replace them cleanly. The assembly translations are
generated from the separate assembly source and are not copied into
`sly1-decomp`.

The latest native trace reaches the menu text callback and submits two draws
with 372–480 vertices. It uploads a 512x512 PSMT4 atlas at GS block `0x1400`
and an 8x2 PSMCT32 palette at `0x1600`; both uploads contain nonzero data. The
font uses 133 glyph records. The full atlas and palette upload payloads
match the decoded Splash resource byte for byte at offsets `0x6160` and
`0x5B50`, respectively, when correlated with the image-table offsets and
image-cache read start. This makes missing/zero image data an obsolete
explanation. A diagnostic run with `OPENRAC_DEBUG_T4_INDEX=all` replaced
palette colors with index greyscale; the text stayed fragmented, so palette
RGB values alone do not explain the corruption.

The earlier pause-menu trace confirms that screen-element constructors run and
put menu object `0x26FF68` on its normal hidden page (`-1`). With the old pad
backfill, pad-state `+0x14 == 1` and edge flags 0; at game time 5.0 seconds,
`func_001E9108` requested page 0, which decomp data identifies as `Paused`
with eight items. The latest trace fixes that controller-state cause: the host
adapter supplies an already-negotiated pressure report, so the port now sets
pad mode 3/count 4. After 5 seconds the frame remains in state 3 and does not
request the pause page. Visible title/menu geometry is still absent. The old
direct text draw had 480 vertices; its first vertex used UV `(0.533, 0.916)`
and RGBA `(127,127,127,17)`.

The current trace uses `OPENRAC_STARTUP_TRACE=1 OPENRAC_TRACE_FONT=1` with the
NTSC-U ELF and ISO. Port-only stages 30 and 37–42 capture the
`func_00125C90` return, VU0 accumulator lanes, selected vector lanes, and
sampled node/camera fields. Stages 46–49 record the camera stack and callback
pointers. Stages 50–52 record each sampled node's local translation, virtual
callback slots, and first basis qword. In the 7-second run, the eight
sampled nodes all had zero local translation at `+0x100..+0x10C`, identity
basis at `+0xD0..+0xDC`, and zero world position/radius at `+0x140..+0x148` and
`+0x2A8`. Their vtable slots `+0x58`, `+0x84`, `+0xA0`, and `+0x118` resolved
to `0x00125108`, `0x001273F0`, `0x00125C90`, and `0x00183350`. The retail
`func_0019D6C0` assembly confirms this update call passes node, world/camera,
and zero in `a0..a2`, matching the port dispatcher. Stage 53 now records each
sampled node's child list and mesh-data pointers. The MAC gate reports
`0xF0`; the sphere test rejects the sampled objects with radius zero. A later
12-second direct-level trace added each sampled entry's child-list, mesh-data,
extra-data, and radius fields. All sampled entries had zero child list, mesh
pointer, extra data, and radius. The retail child-walk/draw callback therefore
has no sampled child or mesh to submit; forcing the sphere test would not
create geometry. The native frame remains state 3 with game mode 0 and menu
page `-1`, so this direct-load probe has not reached a playable game state.
Which retail transition and resource callbacks create the renderable level
objects remains unknown. These probes are diagnostic only and do not alter the
retail branch or rendering.

The screen-element constructor loop `func_001A9F78` exists in the current
decomp source but its US status is still `no-c`, so hostgen emitted a no-op.
The port now supplies that loop as a clearly separate backfill. Remove it when
the matching C is included in the host build. Trace stages 31–33 record the
reset condition, requested page transition, and pause-menu constructor.

Keep functional game-code replacements in
`port/game/sly1-us/hand/native_c/`, separate from matched decompilation C.
No emulator is used by the native launcher. Current build and launch commands
are below.

## What runs today

Build and launch from the repository root:

```sh
cmake --build build/port --target port_native -j2
build/port/sly1_native build/port/extracted/boot/SCUS_971.98 \
  '/home/reaper/pcsx2/Games/Sly Cooper and the Thievius Raccoonus (USA).iso'
```

To exercise the level loader before title/movie support exists:

```sh
OPENRAC_BOOT_LEVEL=0x0100 build/port/sly1_native \
  build/port/extracted/boot/SCUS_971.98 \
  '/home/reaper/pcsx2/Games/Sly Cooper and the Thievius Raccoonus (USA).iso'
```

The opt-in loader uses a reserved upper EE-RAM range starting at `0x01000000`
for resource reads larger than the renderer's shared `D_002C3B00` buffer. Treat
this fixed scratch range as development-only until broader memory use is
audited. `func_001DB838` world teardown is kept as a separate port-only
backfill in `hand/native_c/` and can be replaced by matching decomp C later.

The source ELF and ISO above are already present in the current development
environment. The host-native launcher uses SDL/OpenGL and OpenRAC renderer
code, with temporary guest-addressed EE DMA, VIF1, VU1 and GS services to run
and diagnose Sly's draw path. Those compatibility services are not an emulator.
The target design remains native game code, native platform services and
Sly-specific draw-data adapters; see [DESIGN.md](DESIGN.md) and
[OPENRAC_REUSE.md](OPENRAC_REUSE.md).

The display path reaches port-only `func_00202A80`. The `0x08000000` word in
an earlier chain dump begins a GIF IMAGE tag with 8,192 qwords; it is not a
VIF opcode. The port-only initializer fills `D_00261F1C` from the channel table
at `0x00279340`; traces show GIF channel 2 at `0x1000A000` and VIF1 at
`0x10009000`.

The renderer reports texture state `TBP=0x1400`, `TBW=8`, `PSMT4`, `TW=TH=9`,
`CBP=0x1600`, `CPSM=PSMCT32`, with `TCC=1`, `TFX=MODULATE`. Font glyph UVs and
the image/palette payloads are present. The grayscale-index diagnostic
preserves the malformed layout, narrowing the current renderer investigation
to glyph geometry/UVs and alpha/fade state, alongside the unexpected menu state.

## Historical resource-reader investigation (superseded by the live trace)

The Splash compressed resource is read from ISO sector `820372` for `150849`
bytes. The exact matched decomp LZ reader is now active; its previous port
backfill is archived. The independent offline replay matches all `430858`
decoded bytes in `resource_0002.decoded.bin`. The refill state is kept in
reader field `+0x2C`
so it does not overwrite the sector reader's `+0x38`/`+0x3C` bookkeeping.

A bounded native run with `OPENRAC_DUMP_LZ_WINDOWS=1` captured the runtime LZ
windows. The first 430,858 bytes match `resource_0002.decoded.bin` byte for
byte, with no mismatch; this verifies the live decompressor output for the
Splash stream, not just the offline replay. The trace wrote 55 8 KiB windows
before timeout, so only the expected 430,858-byte prefix was compared; later
captured bytes may belong to subsequent output and are not evidence about the
Splash resource. The optional dump writes `/tmp/sly1-lz-window-*.bin` and is
enabled alongside `OPENRAC_TRACE_LOADER=1`.

The detailed parser and collider notes below record earlier investigations.
Their claims that the image cache is zero-filled, the atlas is absent, and the
renderer has no texture state are superseded by the current measurements above.
Retain them only as historical debugging context, not as descriptions of the
current build.

* The second collider starts at decoded offset `0x1643`. Its mesh has 6,626
  12-byte vertices; face count `0xFFFF` follows at `0x14CE1`. The previous
  port reader imposed a 16,384-face cap and returned early, which made later
  property IDs and nested counts unreliable.
* Retail `func_00137FB0` assembly treats that value as a positive unsigned
  16-bit count: it allocates the face arrays and enters the face loop. The
  port backfill now accepts the full 16-bit count and stops safely when the
  decoded stream reports EOF. In the latest native run, EOF occurs while
  parsing this face block, before the second mesh or common object properties
  are reached. The previous `0x1555` property at `0x54D72` was an artifact of
  the early return and is no longer current evidence.
* The callback returns at EOF and frame setup continues, but
  `func_001DBA38` requests `0x5F4F0` image bytes with the stream already
  exhausted. The destination remains zero-filled. Separate direct-renderer
  tracing has recorded title quads entering the renderer, while the captured
  framebuffer remains black.

The next investigation is the decoded container layout and the ownership of
the suspected image block. The Splash decoded size is 430,858 bytes, the image
read requests `0x5F4F0` (390,384) bytes, and their difference is `0x9E1A`.
The 390,384 bytes beginning at decoded offset `0x9E1A` have high entropy and
look like image data. Image-A metadata has a maximum `offset + size` of exactly
`0x5F4F0`, the retail cache allocation size. This strongly supports the tail
being the serialized image block, although its start has not been verified as
the reader's position when the retail image callback runs. The object list starts at
`0x14D5`; its first type-4/key-1 row enters the retail C03 collider callback
and returns at `0x1594`. At that offset the bytes are `00 53 00 4B 05 FF`, and
the next 6,626-vertex collider begins at `0x1643`. The generic property loop
and observed scalar widths agree with current retail C. The image callback
uses a separately stored byte count, aligns the reader to 16 bytes, then reads
that count. New opt-in trace stages capture the header/cache state (25–26) and
callback cursors before alignment, after alignment, and after the read
(27–29). The reader cursor is a circular 8 KiB window address, not a
decoded-resource file offset; correlate its refill state and cumulative output
before assigning decoded offsets. Compare these values with retail cache
control flow before changing the cursor, face count, texture upload, or GS
behavior. Keep functional corrections in `hand/native_c/` as port-only
backfills. The earlier
9,542-record and `0x128BC` dimension claims were from a different,
misattributed parser trace and should not guide fixes.

The port-only animation/table reader now emits opt-in stage 20–22 records for
the image-B, image-A, and font table counts, including the circular reader
cursor and its remaining-byte field immediately after each count. These probes
are intended to correlate the table boundaries with cumulative LZ output; the
cursor itself is not a decoded-file offset. Source comparison against the
refreshed `sly1-decomp` snapshot confirms retail `func_001B3040` calls
`func_001B2BA8`, `func_001B2CF8`, reads animation and animator counts, then
parses the font table. The port follows these table widths: image-B records are
12 bytes, image-A records are 0x14 bytes, animator type zero has no payload,
nonzero animator types consume 0x1C bytes, and font headers are 0x10 bytes
followed by `glyph_count * 8` bytes. No table-width mismatch is established;
the animator payload's type-specific fields remain simplified in the port but
do not affect stream alignment. Keep any behavioral completion as a port-only
backfill until matching C is available in the decomp project.

A bounded native trace with `OPENRAC_TRACE_FONT=1 OPENRAC_TRACE_LOADER=1`
measured image-B count 63 at cursor `0x7000008D`, image-A count 61 at
`0x70000383`, and font count 4 at `0x70000EDD`; all remaining-byte fields were
positive. The reader reaches `0x700014D5` immediately after `func_001B3040`,
where the object-list count 36 begins. Thus the preceding table reader reaches
the expected serialized boundary; the first invalid-looking row still occurs
inside object-list parsing, after the first collider callback returns at
`0x1594`. This trace does not explain why that callback boundary disagrees with
the following bytes.

Retail `func_00162890` still supplies the useful reference for its nested
record sizes: `c0 * 12`, `c1 * 12`, `c2 * 4`, `c3 * 8`, `c4 * 4`, followed by
`refs * c0 * 4`; optional unsigned dimensions each consume `dimension * 12`.
Those sizes describe the function, but the current trace does not reach it for
the second collider. Keep all functional reader and sentinel handling as
port-only backfills, replacing it with matching C when the decompilation covers
those functions. Do not adjust GS texture interpretation while image data
remains uninitialized.

The US boot ELF's retail `func_00137FB0` disassembly confirms the mesh stream
order used by the port backfill: float radius, unsigned 16-bit vertex count,
12-byte input vertices, unsigned 16-bit face count, link-capacity count, then
per-face three unsigned indices, float plus 12-byte face payload, unsigned
byte edge count, and three unsigned 16-bit fields per edge. The mesh loop calls
`func_00137B90`, whose instruction is `lbu`; the adjacent signed-byte helper
`func_00137CC0` is not called there. The first Splash collider trace follows
this layout through mesh/tree cursors `0x14DE`→`0x14E8`→`0x14EC`, then its
second mesh/tree at `0x152A`→`0x1534`→`0x1538`. Its property terminator,
nested zero-count record list, and callback return land at `0x1587`, `0x1594`,
and `0x1594`, respectively. This validates the reader widths for the first
collider; it does not explain why the following top-level row begins with
`00 53 00 4B 05 FF`.

Reproduce the focused startup trace with:

```sh
OPENRAC_TRACE_FONT=1 OPENRAC_TRACE_LOADER=1 timeout 12s \
  build/port/sly1_native build/port/extracted/boot/SCUS_971.98 \
  '/home/reaper/pcsx2/Games/Sly Cooper and the Thievius Raccoonus (USA).iso'
```

To also dump each runtime LZ output window for comparison with the extracted
resource, add `OPENRAC_DUMP_LZ_WINDOWS=1` to that command. Remove stale
`/tmp/sly1-lz-window-*.bin` files before capturing a fresh sequence.

Set `OPENRAC_TRACE_DMA_TARGET` to a guest address to log the GIF DMA tag,
payload offset, and preceding 80 bytes when a chain transfers that address.
This was used to identify the shared 16x8 PSMCT32 resource images above.

Channel 2 (`0x1000A000`) is a DMA chain and correctly goes through
`openrac_native_renderer_submit_dma_chain`; it must not be treated as a raw
GIFtag stream. In the latest bounded trace, 438 of 438 submitted chains were
accepted with zero rejects. Channel 1/VIF1 remains separate and goes through
the VIF/VU translation path before DIRECT or XGKICK GIF data reaches the
renderer. The raw GIF submission helper exists for genuinely raw streams, but
there is no current evidence that the menu callback supplies one.

The VIF1 runtime remains available for actual VIF1 work. It collects bounded
DMA chains, including references into the port heap at `0xE0000000`, interprets
VIF commands, runs the existing VU1 instruction translator on MSCAL, and
forwards DIRECT data or XGKICK output to the GIF/GS parser. Its runtime API is
listed in `port/game/sly1-us/hostgen.json` to avoid a colliding missing-function
stub.

## Prior measured graphics evidence (archived tail experiment)

Before the stream-unit staging fix, the port-only `func_001378A0.c`
experiment copied decoded bytes `0x9E1A..0x6930A` into the image cache for the
observed Splash revision and cache size. Stage 88 confirmed the `0x5F4F0`-byte
copy, and a captured frame showed nonzero PSMT4/CLUT uploads with faint
lettering. This did not establish the retail image block's decoded offset. The
experiment is archived under `hand/superseded_backfills/`; matched
`func_001378A0` from the decomp is active now. Those atlas and menu captures
are historical and do not describe the current build.

## Decomp refresh and matched-C check (historical snapshot)

The checkout has since advanced beyond the snapshot described here. See
[ROADMAP.md](ROADMAP.md) for the current `dd6f80b` upstream status. Relevant matching
source at the time included
`func_001BAC30`, `func_00138278`, `func_0011ECB8`, and the animation/image/font
table readers. The low-level mesh decoder `func_00137FB0` is still classified
as assembly-only (`handwritten VU0 macro instruction`) in the source
catalogue.

Hostgen staging now reads both `00138000.cpp` and `001B8000.cpp`, and the native
build uses their matching `func_00138278` and `func_001BAC30` bodies. The
callback row parser `func_0011ECB8` remains a port-only backfill; the separate
mesh decoder `func_00137FB0` is still assembly-only, and `func_001289C0`
remains a port backfill. Exact matched `func_00196410` now handles the nested
group callback. A current bounded run advances past the old `0x5300` read,
dispatches type `0x53` / key `0x054b`, and then finds a zero cursor in the
nested reader; Splash image and CLUT data are still zero.

## Exact stream-reader integration (historical startup state)

`port/tools/sly1.py` now lowers the local static helper methods in
`00134000.cpp` to file-local C during disposable host staging, and drops one
unused inline setter while preserving its record layout. The upstream decomp
checkout remains read-only. This makes the entire matched stream-reader unit
readable to hostgen: it currently contributes 24 exact functions, including
`func_001378A0`, `func_00137A60`, `func_001377C0`, `func_00137820`,
`func_00137B60`, and the scalar readers. The previous port implementations
are retained in `hand/superseded_backfills/` and unregistered in
`backfills.tsv`; hostgen reports no unreadable `00134000` unit.

Two separate host adapters are active: `func_001F5A58` maps the still
assembly-only byte-copy operation to `memmove`; `func_00140C28` and
`func_00140CC8` synchronously read through the native ISO service. The latter
functions have matching source in `00140000.cpp`, but that whole unit still
fails C staging on C++ constructors/iterators. A traced native run confirms a
successful read of sector 820372 (74 sectors) into `0x002C3B00`.

At the time this trace was captured, the image callback ran at EOF and left the
cache zero-filled. This is superseded: a later live trace reaches the font
callback with populated image and palette data, and the two payloads match
their expected decoded Splash ranges. Do not use the old zero-upload finding
as the current rendering diagnosis.

To reproduce the current exact-stream startup trace, run from the repository root:

```sh
OPENRAC_TRACE_LOADER=1 OPENRAC_TRACE_FONT=1 OPENRAC_TRACE_CD=1 \
  OPENRAC_TRACE_DIRECT=1 timeout 12s \
  build/port/sly1_native build/port/extracted/boot/SCUS_971.98 \
  '/home/reaper/pcsx2/Games/Sly Cooper and the Thievius Raccoonus (USA).iso'
```

The command captures the early loader and native draw path. Its historical
EOF/zero-upload expectations above no longer apply; inspect the recent
`OPENRAC_TRACE_DIRECT=1` and `OPENRAC_TRACE_FONT=1` output for current texture
state and payload evidence.

## Work needed next

1. Compare PSMT4 texels and PSMCT32 CLUT entries in the live atlas against the
   PS2 GS's exact CSM1 indexing, alpha, and texture-coordinate behavior. Follow
   a known glyph from its resource bytes through the native texture and final
   framebuffer pixel; change only the port renderer/backfill layer supported
   by that comparison.
2. Trace why the native path selects a menu text object during startup while
   PCSX2 shows the opening movie and title sequence. Identify the first Sly
   state transition or resource callback that diverges, keeping functional
   replacements in `native_c/` until matching decomp C exists.
3. Reach the Sly Raccoon title screen natively, then start a new game and
   render the first level. A readable text atlas or menu fragment is not a
   playable-state milestone.
4. Continue through controller input, movement/collision, audio and saves.

## Historical implementation notes (prior to exact stream integration)

## Collider contact-table pass (2026-11-06)

`hand/native_c/collision_contact_tables.c` now supplies port-only
implementations for the collider's contact-pair flag table, paired row pool,
root retry-counter reset, and post-load compatibility scan. The functions are
registered in `hand/backfills.tsv`; none of this code changes matching
decompilation progress. The C-only launcher rebuild succeeds. A bounded
10-second native launch still reports zero submitted vertices and a black
window, so this closes a missing collision callback path without resolving the
current visual blocker. The remaining collider callback gap on this path is
`func_001B8540`.

AddressSanitizer traced the intermittent `double free or corruption (out)` to
a heap-use-after-free in the native kernel semaphore map: a guest thread was
checking a map node while another thread deleted it. Semaphore records now
use shared state with a per-semaphore mutex and condition variable; deletion
removes the map entry, marks the shared record deleted, and wakes its waiters.
A 60-second AddressSanitizer launch and a 20-second normal traced launch
survived without the abort. The renderer still reports zero vertices, so this
fix stabilizes startup but does not resolve the visual blocker.

A follow-up 12-second loader trace after the semaphore fix confirms the loader
now consistently completes the first collider callback at decoded offset
`0x1594`, consumes the following object records, then reaches the second
collider at `0x1643`. That collider's 6,626-vertex mesh still declares 65,535
faces; its face read reaches decoded-stream EOF and the callback returns with
no scene vertices submitted. This reproduces the existing resource-parser
blocker without the earlier heap corruption. Trace: `/tmp/sly1-stable-loader-trace.log`.

The current black frame is now traced one step farther. With
`OPENRAC_TRACE_DIRECT=1`, the native renderer decodes two menu draws and 372
vertices in a later frame; the legacy guest GIF counter still reports zero
vertices because it observes the separate hardware-style path. A captured
960x720 native frame contains only zero-valued pixels. The 512x512 PSMT4
upload at GS block `0x1400` is 131,072 bytes and every byte is zero. Font trace
stage 19 shows the image read beginning with the Splash reader already at
`remaining=-1`; stage 7 records the attempted `0x5F4F0`-byte payload with zero
leading words. This makes stream alignment/resource parsing the current visual
blocker, rather than title callback dispatch or missing menu vertices. The
first concrete alignment lead remains the invalid next object type `0x5300`
at decoded offset `0x1594`.

For a native renderer/frame capture, run:

```sh
OPENRAC_TRACE_DIRECT=1 OPENRAC_DUMP_NATIVE_FRAMES=1 timeout 8s \
  build/port/sly1_native build/port/extracted/boot/SCUS_971.98 \
  '/home/reaper/pcsx2/Games/Sly Cooper and the Thievius Raccoonus (USA).iso'
```

Frames are written to `/tmp/sly1-native-frame-*.ppm`. To capture the menu
packet writer snapshots too, add `OPENRAC_DUMP_FONT_PACKETS=1`; these go to
`/tmp/sly1-font-packet-*.bin`.

## Port/decomp boundary

Active functional replacements belong under `port/game/sly1-us/hand/native_c/`
and are registered in `backfills.tsv`. Superseded implementations remain under
`hand/superseded_backfills/` for comparison and are not compiled. Keep every
port-only change separate from matched decomp C; do not edit the decompilation
checkout to make the port work.
The runtime transport and renderer adapters belong under `port/runtime/` and
`port/renderer/`; document any temporary guest-device behavior as such.

## What a new contributor needs

No additional disc image, boot ELF, or other user-supplied file is blocking
the current work on this machine. Contributors elsewhere need their own
NTSC-U `SCUS_971.98` and disc image; setup and one-time asset extraction are
documented in [ASSETS.md](ASSETS.md). The local read-only decomp snapshot is
`/home/reaper/.cache/sly1-decomp-current` at `cf9ad46`; upstream master was
checked separately at `dd6f80b`. Upstream reports 4,730/4,806 matching US
functions (93.10% code) and 4,738/4,831 Europe functions (92.42% code). Its
new HUD compass function `func_001316E8` and placed-object link table do not
replace current render backfills. `func_00161B10` and `func_00184DE8` still
lack matching C in the decomp project; the latter has a separate port backfill.
The active hostgen report is
`build/port/hostgen/us/report.md`: 197 units read, 1,094 functions translated
to host C, one stub, and 3,796 without C, with seven port-owned overrides.
These measures have different denominators and do not establish gameplay
readiness. The generated source is older than upstream master.

The best immediate contribution is graphics debugging: trace one glyph from
the uploaded PSMT4 nibble and CSM1 palette entry through TEX0/UV selection,
alpha/blend state, the DirectRenderer shader and the framebuffer pixel. Record
the packet fields and image captures that justify each change. This needs
comfort with PS2 GS texture formats, GIF packets, OpenGL shaders and image
inspection. No additional game dump is needed to continue this track on this
development machine. A contributor on another machine needs a legally obtained
NTSC-U `SCUS_971.98` ELF and matching Sly 1 disc image, plus the compiler, CMake,
SDL2 and OpenGL development dependencies described in the build instructions.
Start with
`port/renderer/texture_pool.cpp`, `port/renderer/direct.cpp`,
`port/renderer/shaders/direct.frag`, and `port/game/sly1-us/hand/native_c/`.
The dump command above is reproducible; compare a fresh capture with the
existing evidence before changing renderer behavior. Game-code contributors
can take a separate track: inspect `build/port/hostgen/us/report.md`, choose a
missing function on the active boot/menu path, refresh/check the decomp snapshot
for matching C, then implement only still-missing behavior as a port-only C
backfill registered in `backfills.tsv` and hostgen inputs. Keep these functional
replacements in `port/game/sly1-us/hand/native_c/`; do not edit
`sly1-decomp`. The decomp repository has its own contribution and coordination
rules, so coordinate there before working on matching sources. When matching
C arrives, let source precedence replace the backfill and retain only
port-specific adaptation. Keep graphics fixes in `port/renderer/` and runtime
adapters in `port/runtime/` so they remain distinct from game-function
backfills.
`docs/port/ASSETS.md` covers ELF/ISO setup and one-time extraction.
After a readable menu, work remains on Sly-specific level data and scene
rendering, controller-to-pad integration, collision, audio, and saves.
To move toward playability, the project needs: (1) a verified visible menu
frame; (2) a populated title/level path with Sly-specific scene rendering;
(3) SDL controller state wired into the game's pad-library interface; and
(4) working movement/collision, audio and save behavior. OpenRAC's generic
host runtime is reusable; Ratchet-specific world records are not.

To reproduce the current window and collect renderer counters:

```sh
cmake --build build/port --target port_native -j2
OPENRAC_TRACE_DIRECT=1 timeout 12s build/port/sly1_native \
  build/port/extracted/boot/SCUS_971.98 \
  '/home/reaper/pcsx2/Games/Sly Cooper and the Thievius Raccoonus (USA).iso'
```

Set `OPENRAC_VIF_TRACE=1` only for actual VIF1 submissions. The ISO path above
is specific to this machine; another developer can supply their own NTSC-U
boot ELF and ISO. Keep functional game-code replacements in
`port/game/sly1-us/hand/native_c/` and register them in the backfill inventory,
so matching implementations in `sly1-decomp` can replace them cleanly.
Runtime/rendering changes belong in `port/runtime/` or `port/renderer/`.

## Collider world-frame backfill (2026-11-06)

`hand/native_c/collider_mesh_transform.c` now supplies port-only scalar C for
retail `func_001B8960` and the VU transform body. It updates the placed-object
world frame, transforms collision vertices, face normals, plane values, and
per-face vectors for both collider meshes, and follows the retail mesh-field
offsets. The native launcher builds and reaches the same Splash object-list
cursor without the former `func_001B8960` no-C warning. This does not resolve
the invalid-looking next row at `0x1594`, render scene geometry, or make the
port playable. Bounds update and world registration remain missing; the port now translates
`func_001B9680` to save the current world position at `+0x370`.

## Earlier native status (2026-11-07; superseded by animation-parser update)

The first hostgen build recorded here reads 199 units, translates 1,480 functions, includes
59 functions translated from selected retail assembly, has one stubbed
function, and reports 3,396 functions without C. There are 51 unreadable C
units. These numbers supersede the older 1,094-function snapshot earlier in
this handoff. The build compiles 212 generated C translation units and links
`build/port/sly1_native`; the repository check reports zero problems.

A 12-second native-only launch using the existing extracted US ELF and local
ISO initialized SDL/OpenGL and reached repeated direct-render submissions.
The direct trace saw a 240-vertex draw, but its state was untextured with
target/source/texture addresses all zero; the first vertex was black with
alpha 128. Separately, the native 2D path uploaded the 512x512 PSMT4 atlas and
its 8x2 palette. Those uploads do not prove that a glyph was selected or
rendered correctly. The launch emitted many warnings for missing game
functions and still did not demonstrate a readable menu or a playable scene.
The next graphics step remains tracing one glyph from atlas/palette data
through UV selection, GS state and the final native framebuffer. In parallel,
prioritize missing functions on the reachable startup/menu path; global
hostgen totals do not establish reachability or readiness.

Recent hostability fixes for `0018C000` memory-card return values and
`00128000` range-record argument flow are staging-only changes in the port
repository. No matching decompilation file was changed, and neither fix is a
functional game backfill. Preserve that separation when matching C becomes
available upstream.

The next rebuild translated `file_1E08F8.cpp`, including matching C for
the render-thread root `func_001E2398`. Hostgen coverage rose from 1,480 to
1,527 translated functions and unreadable units fell from 51 to 50; the native
launcher links. Its thread calls are provided by the port's native
`linux_threads.cpp` service. This still has not produced evidence of a readable
menu. At that point the movie decode root `func_0018E5E8` remained no-C because
`file_18D8E0.cpp` uses unsupported `class` and inheritance declarations.

The movie sink staging now compiles `file_18D8E0.cpp` and provides matching C
for `func_0018E5E8`, Ring::Put/Idle, and Dma::Put/Idle. Vptrs and the ring's
field offsets are preserved from the US vtable records; the local DMA object
gets its retail vptr before virtual dispatch. Current hostgen coverage is
1,572 translated functions with 49 unreadable units. The next audited missing
startup call is `func_0011A498` from `func_001B7968`; matching C is present in
`file_112698.cpp`, which is still unreadable due to C++ constructor forms.

A 12-second native launch from this earlier checkpoint started both thread
roots without no-C warnings, but its trace was capped before later textured
menu draws and therefore reported only the repeating 240-vertex untextured
draw. The menu-draw checkpoint below supersedes that partial observation.

## Earlier native launch checkpoint (2026-11-07; superseded below)

The current hostgen report has 1,832 translated functions, 91 selected from
assembly, 3 stubs, and 3,026 functions without C. `func_0013B598` is now
selected from retail assembly with its verified two-argument signature; this
preserves the reader argument forwarded to `func_001BAC30` and removes the
prior `0x0021B4D8` data-address dispatch from the trace. No decomp source was
modified.

That bounded native trace faulted during Splash object loading. It
records vertex count 0 and face count 0, then link capacity `0x3F80` at cursor
`0x700008EE`; later `func_001BAC30` writes through guest address
`0x02000000`. A null indirect call from `func_0011ECB8` also remains. The
window is still black and the game is not playable. Exact trace:
`/tmp/sly-native-reader-trace.log` on the development machine. The immediate
reader/allocator investigation and run command are in
[`PLAYABLE_TODO.md`](PLAYABLE_TODO.md).

## Menu draw checkpoint (2026-11-07)

A fresh native trace reaches the Paused menu draw callback at `0x194f80` and
emits glyph records from the loaded font. The 512x512 PSMT4 atlas and 8x2
palette upload to TBP `0x1400` and CBP `0x1600`; both resolve in the native
texture pool. A 136,512-byte frame packet decodes to three draws. The glyph
batch is draw 2: 474 vertices (79 quads), atlas texture handle `0x3`, TCC on,
modulation and blending enabled. Its first vertex is at NDC `(-0.021, 0.000)`,
UV `(0.533, 0.916)`, RGBA `(127,127,127,128)`. This shows that font geometry,
indexed texture lookup, and GIF parsing all reach the renderer. It does not
show that the pixels reach the display: the user-visible window remains black
or garbled, and existing PPM captures were taken before the 136KB packet
appeared.

The VIF trace had previously hidden worker traffic after its first two
transfers. `OPENRAC_VIF_TRACE_LIMIT` now raises that bounded limit (default 2).
The first two transfers emit 736 DIRECT bytes; transfer 1 also loads eight MPG
blocks. The later sampled worker transfers repeat the same 736-byte DIRECT
pattern with no MSCAL. `OPENRAC_DUMP_NATIVE_FRAME_SKIP=N` can skip early
renderer packets when capturing a later framebuffer. The relevant machine
log is `/tmp/sly-native-direct-textured-draws.log`; do not check binary game
data or frame dumps into the repository.

Next, capture a framebuffer after a 136KB textured packet, then compare it with
the captured vertices, scissor, depth, blend and sampled atlas texels. This is
the current visual blocker; the older null-call and invalid-write symptoms are
not present in this run.

## Textured-frame capture checkpoint (2026-11-07)

The earlier capture command wrote frames before the large menu packet, so its
all-black image did not represent the glyph draw. The native frame dumper now
accepts `OPENRAC_DUMP_NATIVE_FRAME_MIN_BYTES=N` (default 512; maximum 16 MiB).
The minimum-byte filter is applied before `OPENRAC_DUMP_NATIVE_FRAME_SKIP`, so
skip counts only eligible packets. `OPENRAC_DUMP_NATIVE_FRAME_STRIDE=N` now
spaces the dumper's eight bounded images across eligible frames after that
skip; its default of 1 captures consecutive frames. This is useful for sampling
intermittent presentation issues without writing a long video. This command
captures the actual large menu frame on this machine:

```sh
OPENRAC_TRACE_DIRECT=1 OPENRAC_DUMP_NATIVE_FRAMES=1 \
OPENRAC_DUMP_NATIVE_FRAME_MIN_BYTES=131072 timeout 8s \
  build/port/sly1_native build/port/extracted/boot/SCUS_971.98 \
  '/home/reaper/pcsx2/Games/Sly Cooper and the Thievius Raccoonus (USA).iso'
```

The resulting `/tmp/sly1-native-frame-04.png` is still effectively black, but
contains a faint dark-green block of glyphs near the center. It corresponds to
the 136,512-byte packet and draw 2's 474 vertices; the packet filter now makes
that correlation reproducible. With `OPENRAC_DEBUG_DIRECT_FLAT=1`, the same
draw produces clearly rasterized gray rectangles in the same region, so the
draw reaches the framebuffer and its geometry is not wholly off-screen. With
`OPENRAC_DEBUG_T4_INDEX=all`, sampled atlas indices are nonzero, but the glyphs
remain fragmented; replacing CLUT colors with grayscale does not fix the
layout. `OPENRAC_DEBUG_T4_UPLOAD_SWIZZLE=1` was inconclusive in the low-contrast
RGB capture. Repeating the comparison in index-grayscale mode shows that
applying the GS-local swizzle to the upload stream turns the glyph interiors
into high-frequency noise, while ordinary linear upload retains recognizable
letter fragments. The linear upload interpretation is therefore the better
current hypothesis; do not apply the swizzle path as a fix. The remaining
suspects are glyph/atlas correspondence, CLUT values and textured shader
state; these diagnostics do not yet distinguish them.

The diagnostic-only `OPENRAC_DEBUG_T4_FLIP_V=1` option reverses converted T4
rows before palette lookup. Its index-grayscale capture also turns the text
into dense noise, so the existing unflipped row order is better supported.
This rejects a simple whole-texture vertical flip as the repair; it does not
rule out an address/stride error within the upload or a different sampler
convention.

`OPENRAC_DEBUG_T4_SWAP_NIBBLES=1` reverses the low/high pixel order in each
uploaded byte. It changes the text pixels but leaves the same fragmented menu
layout in the index-grayscale capture, so nibble order alone is not the fix.
All three layout probes are opt-in renderer diagnostics and do not change the
default image path.

The raw 64-byte PSMCT32 CLUT upload at block `0x1600` is now captured as
`/tmp/sly1-gs-upload-1600.bin`. Its initial little-endian words are
`0x001b001f`, `0x001c001f`, `0x0010001d`, and `0x001c0010`; interpreted as
RGBA bytes, the entries are low-intensity green with low alpha. The payload
matches the decoded Splash resource, so the next check is whether the retail
GIF upload's CLUT format/CSM/CSA and GS alpha interpretation have been decoded
correctly, and whether the glyph UVs sample the intended atlas cells. Do not
brighten or recolor the palette as a functional fix until this is established.

The exact captured normal frame is machine-local at
`/tmp/sly1-native-frame-04.png`; flat and index diagnostics are likewise under
`/tmp` and should not be committed. Next work: correlate one known glyph's
record (atlas `u/v`, dimensions, palette entry) with the raw upload and the
texture sample coordinates after perspective division. Then isolate the
renderer with a controlled, non-game diagnostic texture to check alpha and T4
lookup independently; keep any experiment behind a debug switch. The UI is not
visible at present, so the port has not reached a playable state.

The follow-up `OPENRAC_TRACE_FONT=1` trace now provides one concrete
correlation: ASCII `P` is recorded at atlas `(u=273,v=43)`, size `16x41`, and
drawn from logical screen position about `(313.2,246.4)`. Retail C flips its V
range to GS coordinates `469..428`; the native vertex log reports normalized
UV `(0.533,0.916)` at the top-left, matching that transform. This validates
the packet's UV arithmetic against the matched decomp function, but does not
yet prove the raw atlas texels at the resulting memory addresses form the
expected glyph. The font layout helper itself matches `func_0015CD18` in the
configured decomp snapshot, so keep it unchanged unless the atlas comparison
shows a specific mismatch.

## Latest level-rendering checkpoint (2026-11-07)

The exact retail animation table loader and its missing helpers are now used
instead of the handwritten table-reader reconstruction. A bounded native run
completes both Splash and selected-level loading through stage 5; the active
world is `0x0088d530`, and animator records now have supported types and valid
objects/vtables. The level's property table parses 62 entries, with group
counts 193 and 10. These are resource-property records, not proof of scene
geometry. The object-record parser then reads a count of zero at cursor
`0x70000c9c`; the loader continues through stage 5, but the renderer still
receives zero scene vertices, and the UI trace reports zero registered fonts
and zero visible text. The immediate blocker remains finding the serialized
level geometry/object path and reaching its draw callback. Current generated
coverage is 2,045 C functions plus 108 retail-assembly translations; the
12-second diagnostic trace is `/tmp/sly-native-resource-link-trace.log`.

The port-only resource-property backfill now has capped diagnostic stages
151–154 to report its counts and first 24 returned resource pointers under
`OPENRAC_TRACE_LOADER=1`. This diagnostic does not change decompilation source
or retail behavior.

A follow-up trace walks up to eight nodes from each of those first 24 values.
The observed values begin with a type `0x1e` reference to a type `0x0f` node,
then nested tagged nodes and scalar property values. The trace exposes no
direct model or mesh key, so this generic property tree is not the current
geometry lead. The selected level's callback-record list still reads zero;
that is only one field in its loader stream, not evidence that the complete
resource contains no scene data. The next useful graphics trace is on the
actual mesh-owning object/callback path, after level activation.

## Retail world-initialization routines enabled (2026-11-07)

The hostgen build now translates exact NTSC-U assembly for `func_00151D28`,
`func_001BEB10`, and `func_001DCD28`, which the port-only level loader calls
during resource setup and world traversal. This removes three no-C stubs from
that path without changing `sly1-decomp`. A fresh 12-second direct-level run
still reaches stage 5 with world `0x0088d530`, but reports zero root children,
zero renderer vertices, zero registered fonts, and zero visible text. These
retail routines alone do not establish the level object path. Trace:
`/tmp/sly-native-retail-init-path.log`.

## Exact render-list traversal and current startup state (2026-11-07)

The trace wrapper for `func_00165728` has been removed from active overrides
and retained at `hand/diagnostics/func_00165728_trace.c`. Its retail US
assembly (`asm/us/text.s`, 0xFC bytes) confirms the same entry stride `0x70`,
subdata stride `0x28`, object callback at `entry+0x38`, nested call to
`func_001662A8`, and linked-node offsets `self+0x4C` / `node+0x80` as the
decomp C. There is no identified semantic mismatch in that traversal.
`hostgen.json` now lists the function in both `from_assembly` and `asm.only`;
hostgen's generated report confirms source `asm/us/text.s` and 223 total
functions translated from assembly.

A bounded 12-second native run with the exact assembly remains at
`sly_frame=0,3,0`, game mode 0, and zero vertices. It submits three GIF chains
and repeatedly reports guest threads `func_001DC710` and `func_0015EE80`.
This rules out the former instrumented C override as the cause of the black
screen. Capture: `/tmp/sly-native-165728-retail-asm.log`. Next, follow the
regular startup/frame path from these threads and identify why no per-frame
scene or UI geometry is submitted; keep the Splash-to-title transition as a
separate route to trace.

## Current black-frame diagnosis (2026-11-11)

The black window is not expected game output. Normal startup reaches frame
state 3 with an active world, 37 global UI objects, and registered fonts, but
reports zero visible text, vertices, image tags, and GIF submissions. Corrected
port tracing confirms text-buffer setup runs five times, while text layout,
glyph emission, and screen-element/menu draw callbacks do not. The UI frame
update and draw dispatchers do run. The retail UI pointer table and initialized
vtables are valid; no active object's draw method is dispatched. The previous
trace's apparent invalid UI pointers came from reusing trace stage 183, which
was already used by the graphics worker.

The user pressed Enter several times. The native pad bridge captured Start
(`held=0x0008`), but the game stayed at frame state 3, menu page -1, and zero
graphics. The current lead is the title/menu state transition and its input
path, not the renderer or font atlas. Pad edge tracing and function-entry
targets now cover `func_001E9108`, `func_00194D30`, and `func_001936A0`. See
`PLAYABLE_TODO.md` under “Black-window UI and input trace”.

## Fresh native client check and VU1 stop (2026-11-17)

Rebuilt `port_native_sdl_host32` from the current `wip/native-sly-port` source
and checked the output directly. The idle framebuffer captures around native
frames 300–399 show the no-card prompt steadily; the Continue highlight blinks.
That does not explain the brief black flashes in the user's 2026-11-15 desktop
recording: 8-fps samples contain seven full-black frames among 52, while the
native framebuffer readbacks in the idle run stay stable. One Spectacle
active-window capture also shows the Sly prompt correctly. A separate
post-swap `GL_FRONT` readback returned all-zero frames on Wayland, so that
readback is not reliable evidence of what the compositor displayed. Repeated
active-window sampling lost the Sly window after focus changed and was
discarded. The black flashes remain unresolved; get a recording bound to the
Sly window before attributing them to the renderer or the desktop compositor.

With one process-local Cross press at native frame 300, the prompt first
remains visible, then contracts with overlapping copies for about fifteen
frames and the output becomes black. No world meshes are submitted. This is
not a playable transition; the overlapping frames have not been confirmed
against retail. The current run therefore reproduces both the prompt and the
post-input visual artifact, rather than showing an all-black frame throughout.
It does not establish which earlier source change introduced the artifact.
Logs: `/tmp/sly-current-client-20261117.log`,
`/tmp/sly-current-idle-flicker-20261117.log`, and
`/tmp/sly-current-direct-state-20261117b.log`. Local captures:
`/tmp/sly-current-idle-montage.png` and
`/tmp/sly-current-postinput-montage.png`.

The same run catches VIF submission 328's first mesh packet. VU1's XGKICK at
PC `0x68` points to qword 469, where the GIF tag `0x0000000000039001` is
rejected for reserved bits. A later kick at PC `0x6e` points to qword 35 and is
accepted, but that is not evidence of visible geometry. A separate MSCAL entry
`0x2` then reaches micro-PC `0x52d`, where the native VU translator stops on
upper word `0x01e049fc` paired with lower no-op `0x8000033c`. The bounded
fault-context trace records the preceding 16 instruction pairs in
`/tmp/sly-upper-fault-context-20261117.log`. Do not classify the word as a
missing valid opcode or replace it with a no-op until its retail VU semantics
and the branch into `0x52d` are confirmed.

The earlier qword trace also showed `SQ VF4,1(VI5)` at PC `0x4bc` clearing the
W lane on qwords 37, 40, and subsequent stride-three entries. That trace does
not yet prove this store is wrong: VU execution reaches another stop, and the
packet writer may intentionally overwrite that lane. First establish the exact
microprogram path and instruction semantics, then capture the resulting GIF
packet and frame again. Keep this investigation in the port runtime; it has
not changed or backfilled matched decomp C.

The run was bounded and its process exited afterward. It used the native
launcher only; no emulator was used. Earlier broad packet instrumentation
produced an oversized temporary log and was stopped; use the targeted failure
context instead. The build reports 99 missing-function traps, a separate
coverage limit that is not the error that stopped this VU stream.

## VU scalar accumulator instructions (2026-11-17)

The 16-pair trace identifies upper words `0x01e049fc`, `0x01c039fc`, and
`0x01e099fc` as special selector `0x1c`: VU `MULAq` operations. The translator
had implemented neighboring `MULAx/y/z/w`, but not `MULAq` or `MULAi` (`0x1e`).
The public VU instruction manual defines these as scalar multiplies into ACC
using Q or I. The native translator now implements both under the VU runtime;
no matched Sly C or VU assembly changed. The diagnostic that skipped unknown
upper instructions was removed.

A rebuilt host32 run no longer stops at those instructions. It continues to
the existing GIF boundary: XGKICK at PC `0x68` starts at qword 469, while the
valid GIFtag is at qword 468 and qword 469 contains `0x0000000000039001` with
its A+D payload. The strict parser rejects the latter as a tag with reserved
bits. The next kick at PC `0x6e`, qword 35, is accepted. Captured output still
shows the full no-card prompt before the process-local Cross press and a black
frame afterward. This removes one interpreter stop but is not a graphics or
playability improvement yet. Run log: `/tmp/sly-mulaq-postinput-20261117.log`.

## First XGKICK pointer audit (2026-11-17)

A narrowly filtered native run traced the loaded retail program at entry
`0x5f` through its first two XGKICKs. At PC `0x63`, `ILW VI6,12(VI4)` loads
468. PC `0x67` executes `IADDIU VI6,VI6,1`, making the operand 469; PC `0x68`
then executes `XGKICK VI6`. The data at 468 is the valid EOP GIFtag
`0x100040000000806d`; 469 is its first A+D payload qword
(`value=0x39001`, `address=0x48`). PC `0x6d` decrements VI7 from 36 to 35,
and PC `0x6e` XGKICKs the independently generated valid GIFtag at qword 35.
The captured sequence matches the loaded US retail VU words. The XGKICK at
`0x68` still fails strict packet validation at 469.

The public VU User's Manual says the IALU's dummy stages are bypassed and that
XGKICK gives the external GIF the value of `VI[is]`, which GIF treats as the
leading GIFtag address (§§3.4.7 and XGKICK). That rules out explaining this
trace as a generic one-cycle VI write delay, but it does not explain why the
retail program increments past the valid tag before the first kick. The
missing fact is the transfer context that supplies or consumes the tag at
qword 468, or a different verified interpretation of the captured VU
memory/pointer state. Do not decrement the XGKICK address or relax GIF
validation: an earlier retry from 468 produced no visible frame, and the
current evidence does not establish that it is the correct packet boundary.

Run log: `/tmp/sly-xgkick-pointer-audit-20261117.log`. The run was native and
bounded; its timeout ended with exit 137 after the launcher did not exit on
SIGINT. No source code changed, and no game data was added to the repository.

## Accepted second kick packet audit (2026-11-17)

A separate targeted run dumped the naturally occurring PC `0x6e` XGKICK from
qword 35 in submission 328, immediately after the rejected PC `0x68` kick.
The packet is accepted by strict GIF validation. Its 1,760 bytes contain an
EOP PACKED GIFtag with 109 A+D writes. Decoding each destination from the low
byte of the second 64-bit half, as the native GIF renderer does, shows 104
writes to PRIM (`0x00`) and one each to TEX0_2 (`0x07`), CLAMP_2 (`0x09`),
TEX1_2 (`0x15`), and two reserved/unrecognized addresses (`0x43`, `0x48`).
There are no XYZ2 (`0x05`) or XYZF2 (`0x04`) writes, so this is not a vertex
packet. The corrected decode supersedes the earlier statement that all 109
writes targeted PRIM. The accepted kick still does not recover the mesh packet
rejected at PC `0x68`. Do not conflate this 109-write capture with the separate
entry-`0x2` qword-35 capture later in this handoff, which has a 70-loop tag and
a different payload.

Run log: `/tmp/sly-qword35-packet-audit-20261117.log`; local dump:
`/tmp/sly1-vu1-kick-00.bin`. The 10-second native run ended via timeout with
exit 137 after the launcher did not exit on SIGINT. No repository source or
retail data changed.
