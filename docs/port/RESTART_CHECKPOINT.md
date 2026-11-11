<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Sly 1 port restart checkpoint

Updated: 2026-11-11, after re-fetching decomp master `bfc74817` (Wave 24),
regenerating US assembly from its config, and tracing the Splash path. Work in
`/home/reaper/sly/OpenThievius` on branch
`wip/native-sly-port`. The user wants frequent commits and pushes. Keep the
port-only functional C backfills under `port/game/sly1-us/hand/native_c/` or
`overrides_c/`, registered in the matching hand table; do not put those
replacements in `/home/reaper/Code/sly1-decomp`. Continue using native runs
only; no emulator is involved in the port work.

## Latest handoff (2026-11-11)

The decomp checkout was fetched again and remains clean at `bfc74817` on
`master`; no commits have landed after Wave 24 / PR #117. The US report remains
4,760/4,807 functions and 93.25% of code bytes. The port build now reports 732
C translations, 405 US retail assembly translations, and 2 stubs.

The canonical checkout had no local `asm/us` output, so an ignored temporary
copy was made at `/tmp/sly1-decomp-wave24-asm`, supplied the existing user ELF,
and ran `tools/setup_asm.py --version us`. CMake now points the assembly
source at that generated output while C still comes from the clean canonical
checkout. No decomp source or generated files were changed in the main
checkout.

The latest port change selects exact US retail assembly for
`UpdateRangeRecordBounds` (`func_001B1BF8`) and `AreFloatsNearlyEqual`
(`func_001EAC68`). The 45-second native run reaches 22.08 guest seconds, but
still reports zero vertices, image tags, GIF-chain submissions, visible text,
or queued movie. No artificial input was sent. Log:
`/tmp/sly-wave24-range-bounds-45s.log`. A later call-path trace shows object
tree updates run, but range-record setup, update and packet routines are not
reached. Splash calls text layout and notifier setup, but not the text draw or
glyph emit functions. The currently open launcher traces keyboard/controller
input as well; it remains neutral until a user presses a button. Latest log:
`/tmp/sly-current-canonical-asm-interactive.log`. Continue by checking whether
input advances the Splash graph; if not, trace why its `0x261` self-edge
remains unarmed.

Canonical `/home/reaper/sly/sly1-decomp` is clean on `master` at `bfc74817`
(Wave 24, PR #117), tracking `origin/master`. Wave 24 adds 410 readable names
and project-owned assembly macros; the US match report is unchanged at
4,760/4,807 functions and 93.25% of code bytes. No new matching function body
was available to replace a port backfill. A rebuild against the refreshed
checkout succeeds: 732 C translations, 388 assembly translations, and 2
stubs, across 269 generated C units.

Port-side input fix: native pad reports now put the button high byte at
`report[2]` and low byte at `report[3]`, matching retail's
`(buf[2] << 8) | buf[3]` decode. The previous reversed order changed button
identities (including Start to R1). `OPENRAC_TRACE_PAD=1` traces transitions
in this decoded order. The build succeeds. A 90-second run reached 44.7 guest
seconds but had no key input, movie request, or graphics; the transition after
a real button press remains to be checked. Log:
`/tmp/sly-pad-input-interactive-90s.log`.

World/animation recovery: selected exact US retail assembly for eight reached
no-op callbacks—`UpdateObjectTree`, `SeekAnimationEventCursor`,
`RunAnimationEventsToTime`, `ApplyAnimationLoopMode`, `ApplyListedPoseSources`,
`AddAnimationPlayerCallback`, `UpdateMatchingListedObjects`, and
`SetRangeRecordPosition`. The build now has 732 C translations, 396 assembly
translations, and 2 stubs. A 45-second native run reached 22.08 guest seconds,
with no movie/world request, vertices, image tags, or GIF submissions and all
37 UI elements inactive. Its pad trace stayed neutral. This confirms elapsed
time alone does not trigger the opening scene. Log:
`/tmp/sly-update-object-tree-summary-45s.log`.

Exact retail assembly now covers 15 additional reached GIF, text-panel, frame
transform, camera, and world-list callbacks. Build coverage is 732 C
translations, 388 assembly translations, and 2 stubs. A 65-second native run
reaches 32.15 guest seconds but remains in global element-stack state 3 (its
normal state), with no pad input, no
movie request, no world load, and zero graphics. Trace:
`/tmp/sly-long-startup-65s.log`. PCSX2 reference video becomes visible around
22 seconds; whether it received a controller input at that point is still
being confirmed. Wave 24 names this object `g_elementStack` and its update
callback `UpdateElementStack`, correcting the former app-level input-wait
interpretation.

The next trace found 11 further reached UI state callbacks running as no-ops;
exact US retail assembly is selected for text-panel/timer state, element
hiding, cursor-frame selection, texture-register lookup, and near-camera group
updates. Build: 732 C translations, 373 assembly translations, 2 stubs. Their
no-op warnings are gone, but all 37 screen elements remain inactive and all
renderer counters remain at zero. Trace: `/tmp/sly-ui-state-retail-asm-20s.log`.

Fresh native evidence: the build now selects exact US assembly for 22 more
reached visual/UI functions (text and arc/item setup, UI state helpers, packet
buffers, masked-image submission, resident-image transfer, texture-register
setup, and frame animator step). Build: 732 C translations, 362 assembly
translations, and 2 stubs. The 20-second trace has no warnings for these
functions and shows 37 initialized fonts, but all 37 elements remain mode 0;
vertices, visible text, image tags, and GIF submissions remain zero. The graph
player advances to 9.48 seconds on the `0x261` `2 -> 2` self-edge; its notifier
gate is still null. Log: `/tmp/sly-renderer-retail-asm-20s.log`.

The latest native build selects exact US assembly for the reached frame setup
helpers `func_00193210`, `func_00193270`, `func_00193288`, and the reached
font/layout helpers `func_001E2268` and `func_001E22C0`. The build has 732 C
translations, 340 assembly translations, and 2 generated stubs. A
15-second native trace confirms the functions no longer warn as no-ops, but
still records zero vertices, visible text, and GIF submissions. It also shows
Splash graph object ID `0x25f` present on state 2 / event `0x261` / self-edge
`2 -> 2`; do not use the earlier missing-graph-link diagnosis for this run.
Log: `/tmp/sly-frame-init-asm-15s.log`.

The build now also selects exact US assembly for `func_001E2268` and
`func_001E22C0`, reached font/resource layout helpers that were no-op stubs.
Coverage is 732 C translations, 340 assembly translations, and 2 stubs. A
second 15-second run still shows all 37 UI objects inactive and no graphics.
The class-`0x57` CountdownNotifier stays at state 0 with a null `+0x338` timer
gate and zero timer; its graph remains on the unarmed `0x261` self-edge. Log:
`/tmp/sly-font-layout-init-asm-15s.log`.

Canonical `/home/reaper/sly/sly1-decomp` is clean on `master` at `d56e797c`
(Wave 23, PR #116), tracking `origin/master`. Wave 23 adds no matched
functions: the US report remains 4,760/4,807 functions and 93.25% matched code.
It adds compiler-investigation notes and near-match re-scores; notably,
`func_00109488` remains unmatched and its current role as a port-only backfill
is unchanged. The native build was rebuilt successfully against this checkout:
732 C translations, 340 US retail assembly translations, and 2 generated
stubs. No port code change was needed from this decomp update.

The current runtime-driven assembly selection also restores the reached UI
constructors `InitTimerElement`, `InitGauge`, `InitFramedElement`,
`InitPointerTextCaption`, `InitLetterboxElement`, and `CreateArcSprite`, plus
their exact US assembly dependencies. The text glyph/layout measurement
helpers called in the same setup chain are restored. Build: 732 C translations,
335 assembly translations, 2 stubs, and 3,841 without C across 269 generated
units. The 10-second trace confirms these functions no longer warn as no-ops,
but all 37 UI objects remain mode 0 and there are still zero vertices, image
tags, or submitted GIF chains. Log:
`/tmp/sly-wave22-screen-init-asm-10s.log`.

Canonical `/home/reaper/sly/sly1-decomp` was fetched and fast-forwarded to
`4c574846` (Wave 22, PR #115); it is clean and tracks `origin/master`. The US
report lists 4,760/4,807 matched functions and 93.25% matched code. Wave 22
matches triangle animator callback `func_001B55C0` and active-set helper
`func_001536C8`, and expands state-walker code. The source units for both
functions remain unhostable in the current C frontend due to header-level
layout errors and C++-only constructs. Neither newly matched function was
reached during the latest startup trace. Rebuilding
the port against the updated master succeeds; generated coverage remains 732
C translations, 308 US assembly translations, 2 stubs, and 3,868 functions
without C across 269 generated units.

The current build now registers `func_001E1FD0` and `func_001E20B8` as
standalone port-only formatter backfills. Exact US retail assembly is selected
for screen glyph substitution `func_001AA5A0` and string copy `func_001E2138`.
This removes the formatter's generated no-op stub and restores the reached
screen-text callbacks. Build: 732 C translations, 308 assembly translations,
2 stubs, and 3,868 functions without C across 269 generated units. A fresh
10-second native run still shows UI objects at mode 0 and zero vertices/GIF
submissions, so the screen remains black. Log:
`/tmp/sly-aa5a0-exactasm-10s.log`. Keep the two formatter replacements in the
port's `hand/native_c` directory until matching upstream C is hostable.

Decomp checkouts: `/home/reaper/sly/sly1-decomp` is clean on `master` at
`55d39f28`; public `origin/night21` (`4097b806`) is open as PR #114. The other local clone,
`/home/reaper/Code/sly1-decomp`, is a fork at `e1b39f16` from wave 16 and is
older overall; it was useful only to read the C definitions of three small
packet writers. None of these decomp checkouts was modified.

The current `build/port` cache uses canonical `master` C and separate US
retail assembly. It succeeds with 730 C translations, 305 assembly
translations, 2 stubs, 3,872 without C, and 267 generated units. The three
reached packet-word writers (`func_001EE3F0`, `func_001EE420`,
`func_001EE498`) now have port-only C backfills in
`hand/native_c/display_packets.c`. The `func_001EE5C8` backfill skips negative
counts to prevent a malformed strip from wrapping its packet cursor into
unmapped memory. This safety behavior is isolated from the decomp and should
be revisited once the model-data pointer is understood.

The earlier “reversed strip range” diagnosis was incorrect: the probe read two
halfwords from `strip->unpack` without confirming that it pointed at the
`StripUnpack` descriptor. `func_00165588` expects that pointer to address
`off[0]` and `off[1]`; in a suspect strip it instead appeared to point at a VIF
command. A follow-up confirmed a valid case where `unpack=packet+4` and the
payload starts with descriptor offsets `0x24, 0x1e0`. Another earlier sample
begins `04 01 00 01 24 c0 54 68`; there `packet+4` is a command word, so the
offset/packet-prefix path still needs tracing. Do not clamp or rewrite the
descriptor values based on the old probe. Keep the negative-count guard as a
temporary port safety measure. With it, an 8-second launcher run remains alive
and a 5-second state trace reaches the frame loop with an active world and
eight 144-byte GIF setup DMAs. It still shows zero scene vertices, image
tags/submissions, and visible text; the window remains black. Logs:
`/tmp/sly-night21-4097b806-negative-vif-guard-8s.log` and
`/tmp/sly-night21-4097b806-vif-guard-state-5s.log`.

After removing the invalid probe, a clean `port_native` rebuild succeeded
(730 C, 287 assembly translations, 2 stubs). A fresh 5-second direct-level
run completed its timeout without a crash and emitted the same eight 144-byte
GIF setup DMAs; it still has no visible scene. Log:
`/tmp/sly-night21-4097b806-decomp-refresh-clean-5s.log`.

A follow-up inserted temporary probes before/after strip packet detachment and
at `func_00165588`. A 15-second direct-level launch produced no strip events
at any probe, despite collision face-tree stages 190–192; the fresh 5-second
run likewise produced only the eight setup DMAs. The pointer arithmetic is
therefore not the current reproducible blocker on this launch path. Reproduce
the conditions for the older strip samples before changing their producer;
first identify why this boot parses collision meshes but does not reach the
strip builder.
The probes were removed and the normal `port_native` target rebuilt. Log:
`/tmp/sly-night21-4097b806-strip-pointer-stages-15s.log`.

The source shows `BuildStripMeshPacket` and `BuildLitStripMeshPacket` set
`strip->unpack` through `ReserveVifUnpack(..., &hdr->p)`, write the descriptor
offsets through that pointer, and later adjust it by
`strip->addr[0] - starts[0]`. If the path is reached again, compare the
original descriptor pointer and packet begin with the copied addresses for
both valid and suspect prefixes. Verify a nonzero scene vertex count and
visible graphics after fixing that producer path. Do not claim playability yet.

## Decomp rework check (2026-11-11)

Fetched the public repository again. `master` is `55d39f28` (4,758/4,807 US
functions, 93.108% code); open `night21` is `4097b806` (4,759/4,807, 93.125%
code, PR #114). Its 82-file delta is mostly type/signature corrections and
report/source updates, plus an exact active-set collision helper. The broad
master rework adds substantial
collision, placed-object, and player code, but no newly matched function on the
current strip-render path. `file_15F678.cpp` clarifies the descriptor layout
(`off[4]`, `strip[4]`, `f10`, `f12`) and confirms the builder/relocation flow;
it does not explain the inconsistent packet prefix. Neither decomp checkout
was modified. The current `build/port` cache uses canonical `master` C and
separate US retail assembly. Current hostgen coverage is 730 C translations,
305 retail assembly translations, 2 stubs, and 3,872 functions without C
across 267 generated C units.

## Frame-system initializer follow-up (2026-11-11)

The port-only initializer table in `func_00185B38` called 24 functions that
had exact US retail assembly but were still no-op stubs. `hostgen.json` now
selects those 24 assembly bodies, including display setup `func_001688B8` and
render-frame setup `func_0015E810`. A 12-second default native run and a
12-second direct-level run both reach the frame loop; the direct-level run
reaches loader stages 13–14. Both remain black, with zero vertices, visible
text, and GIF-chain submissions. Logs:
`/tmp/sly-canonical-55d39f28-init-callback-asm-12s.log` and
`/tmp/sly-canonical-55d39f28-init-callback-asm-direct0100-12s.log`.

The follow-up also selects exact assembly for the reached animator helpers
`func_001B4AA0` and `func_001B4AD0`, UI updater `func_001AB058`, and commentary
updater `func_001E00B0`. They no longer emit no-op warnings, but the 10-second
native trace still has zero visible text, vertices, or GIF submissions. All
37 UI objects remain at mode 0. The next trace target is the producer that
should activate a drawable UI object or scene callback; do not force object
modes or the Splash graph. Log:
`/tmp/sly-canonical-55d39f28-reached-anim-ui-asm-10s.log`.

## Current follow-up (2026-11-11)

The latest native build adds exact US retail assembly for the reached
`ClaimAnimatorStep` (`0x001B4B08`), `func_001B4B68`, and `func_001B4BC8`
callbacks. This follows the prior restoration of `FlushCursors` and
`StepAnimators`; all five no longer produce no-C warnings in the bounded
startup run. Build report: 727 C translations, 278 retail assembly
translations, 2 stubs, and 3,902 functions without C across 267 generated C
units. The six-second run reaches the regular frame loop without a crash, but
still has a black window, zero vertices, zero visible text, and zero GIF chain
submissions. The Splash graph remains unarmed at state key `0x261`, edge
`2 -> 2`, while the animation player's time advances. Trace:
`/tmp/sly-canonical-55d39f28-animation-callbacks-6s.log`.

The decomp checkout was fetched again and remains clean at `55d39f28` (PR
#113), with no newer upstream commits. Its newly matched collision, placed
object, and player functions remain outside this early startup/rendering
frontier and their refreshed C++ units still do not pass the port's C hostgen
frontend. Keep those sources untouched; consume them after hostgen can compile
them or their startup-relevant retail assembly is identified.

Fetched the public decomp again; `master` remains `55d39f28` (PR #113), with
no new commits. The broad rework changed 126 files and added/expanded matched
code in collision, placed-object interpolation, and player request/target
paths. It is useful future gameplay coverage, but those new C++ units still
fail the current port C hostgen frontend and the functions are not on the
present startup/rendering frontier.

An opt-in repeated snapshot in `port/sly1/main.cpp` now records the Splash
StateGraph and StepSequence runtime fields. I selected exact US retail
assembly for `CreateAnimationPlayer` (`0x0012C288`), its four reached helper
functions (`0x0012D3F8`, `0x0012F6B0`, `0x00128998`, `0x001299E0`), and the
per-frame player updater `func_0012EBD8`. The player is now allocated and its
time advances each frame. I also restored `StartObjectSubGroup` (`0x001F4660`);
the Splash's only subgroup has zero children.

The graph `0x25f` still has four states, seven edges, and `armed=0`; its sole
walker remains at state key `0x261` on edge `2 -> 2`, even while the animation
player advances. The class-`0x57` CountdownNotifier remains state 0,
`+0x338` is null, and its 4–6 second timer stays at zero. The world has no
class-`0x65` StepSequence. There are still no vertices, visible text, or GIF
submissions. The canonical notifier update treats `+0x338` as an optional gate
reference; determine whether Splash intentionally leaves it null and what
normal transition should leave the idle self-loop. Do not set it or force the
graph. Latest traces:
`/tmp/sly-canonical-55d39f28-animation-player-update-10s.log`,
`/tmp/sly-canonical-55d39f28-subgroup-contents-8s.log`, and
`/tmp/sly-canonical-55d39f28-direct-level-after-animation-10s.log`.

The separate direct-level probe remains the route for investigating the first
actual gameplay graphics: world loading succeeds, but the GS writer stays at
its initial DMA tag and reports zero geometry. Resume with the producer for
the Splash gate/edge and then the direct-level GS geometry path; do not treat
the Splash state trace as evidence that graphics are fixed.

Follow-up on the class-`0x53` parser trace: canonical
`src/main/file_15F678.cpp` contains the full `ReadPlacedObjectModel`
implementation behind `func_00162890`, but hostgen cannot compile that C++
unit. The port's compact C reader was still selected and misaligned Splash's
stream. `hostgen.json` now selects exact US retail assembly for
`func_00162890`; the compact C implementation remains guarded in
`hand/native_c/resource_object_common.c` as a separate replacement candidate.
An eight-second native run passes the old invalid nested count at
`0x70001d1f`, consumes valid records past `0x70001f00`, then reaches type
`0xffffda00` at `0x70001f06`. Selecting exact retail assembly for the small
`func_00182C58` wrapper removes its no-op; its target `func_001837E0` is already
translated from assembly. Exact assembly is also restored for collision reader
`func_00137FB0`, leaving its port C backfill registered but superseded by
`asm.only`. These changes let the resource load continue into world setup and
the frame loop. The subsequent exact `ReadAxisRotationCurve`,
`ReadTangentWeightCurve`, and `GetTangentWeightCurveTimes` selections fix the
state-graph cursor and let the first graph consume five animation children;
the top-level reader reaches completion. Graphics remain zero. Latest loader
log: `/tmp/sly-canonical-55d39f28-tangent-readers-retail-8s.log`.

## Render queue follow-up (2026-11-11)

The refreshed `file_167D68.cpp` contains the mesh queue implementation but is
not hostgen-readable. Exact US retail assembly is now selected under `asm.only`
for `SubmitMeshQueueEntry` (`0x001694A0`), `QueueMeshStrip` (`0x001694F8`),
`QueueNodeLink` (`0x00169608`), `RemapMeshQueueValues` (`0x001696B8`), and
`FlushMeshQueue` (`0x00169718`). The remapper was reached as a no-op. Hostgen
now links 257 assembly translations (725 C translations, 2 stubs, 3,925
without C). An eight-second direct-level native run has no missing-function
warning for this cluster, but still records zero vertices and zero GIF
submissions. Log: `/tmp/sly-canonical-55d39f28-mesh-queue-cluster-8s.log`.

This is a correctness fix for the animation mesh-copy queue, not a graphics
milestone. The five direct-level outer groups remain marked to skip geometry,
so do not change their flags or force their draw path. Continue with the normal
Splash-to-title/state transition and trace which class draw callbacks run.

## Canonical decomp refresh checkpoint (2026-11-11)

Canonical `OpenThievius/sly1-decomp` master is `55d39f28` (PR #113), fast-forwarded
2026-11-11 in `/home/reaper/sly/sly1-decomp`; that checkout is clean and at
`55d39f28`. Upstream reports 4,758/4,807 NTSC-U functions matched (98.98% by
function count, 93.11% of code, and 94.56% of data). Since `f36f43ee`, nine
functions were matched in collision, object interpolation, and player physics
code. A detached source worktree is `/tmp/sly1-decomp-55d39f28`. The isolated
build at `build/port/canonical-55d39f28` uses this source and the older
`/home/reaper/Code/sly1-decomp` checkout only for US retail assembly. The
hostgen C frontend cannot parse several refreshed C++ units and falls back to
assembly for their new definitions; at that first refresh point,
linked coverage is 725 translated functions, 257 selected assembly functions,
2 stubs, and 3,925
functions without C across 265 generated C units. The build succeeds.

## State-graph startup follow-up (2026-11-11)

The earlier Splash lookup diagnosis was wrong: a later trace shows the real
class-`0x88` StateGraph with ID `0x25f` at `0x00847800`. Exact US retail
assembly is now selected for `StartStateGraph` (`0x001B61A8`),
`SpawnInitialStateWalker` (`0x001B61F0`), `CreateStateWalker`
(`0x001B6290`), `FindStateEntryIndexOrZero` (`0x001B63F8`),
`FindStateEntryIndex` (`0x001B63C0`), `PopZeroedFreeSlot` (`0x001B5EE0`),
`StartNextStateWalkerEdge` (`0x001B66A0`), `NotifyStateWalkerChange`
(`0x001B6E28`), and `CreateAnimationPlayerWithoutPresetTargets`
(`0x0012C7B0`). `PopFreeSlot` (`0x001B5EA0`) and
`CallStateWalkerListeners` (`0x001B6D78`) are separate port-only C backfills
under `hand/native_c/`, registered in `backfills.tsv`.

The isolated build succeeds with 727 C translations, 266 retail assembly
translations, 2 stubs, and 3,914 functions without C across 267 generated C
units. A 10-second native run has no no-op warnings in the reached state-graph
path, but still reports frame state `0,3,0`, zero scene vertices, no visible
text, and no GIF submissions. The state-graph path is no longer stubbed, but
the trace does not confirm a title transition; the black screen remains. Trace:
`/tmp/sly-canonical-55d39f28-state-animation-10s.log`.

Port overrides without a host-readable matching C definition are staged
ahead of the canonical units as separate port-owned source files. This lets
address-based functions remain callable when the reworked decomp renames
their C symbols or a canonical unit cannot be read. Implementations stay in
the port under `hand/native_c/`, `hand/overrides_c/`, and
`hand/superseded_backfills/`; the decomp checkout is clean, with no port edits.

Exact retail assembly is selected for `ReadAxisRotationCurve`
(`0x0011FFD8`), `ReadTangentWeightCurve` (`0x0011EFC8`), and
`GetTangentWeightCurveTimes` (`0x0011F180`). The first state graph now consumes
its five animation children and the top-level reader reaches completion before
frame processing. A direct-level native probe loads populated resource/world
lists and runs the render thread. Frame tracing still shows no GS geometry: the
GS writer cursor stays at its initial tag, the VIF writer advances about
`0x2e0` bytes, and vertex/GIF submission counters remain zero. Logs are
`/tmp/sly-canonical-55d39f28-direct-level-packets-8s.log` and
`/tmp/sly-canonical-55d39f28-frame-packet-trace-8s.log`. The canonical notifier
lookup now resolves Splash's class-`0x75` child
(`0x846cb0`, id `0x22a`) as the CountdownNotifier owner. The window still has
zero vertices, no visible text, no GIF image tags, and no DMA submissions; it
remains black. This is a lookup fix, not a visual or playable milestone. The
earlier run is logged at `/tmp/sly-canonical-f36f43ee-owner-resolved.log`.

The port now separates the two lookup paths made clear by the reworked source:
`func_0015A198` is the object-ID hash gatherer and `func_0015A4F0` is the
class-kind gatherer. The updated `func_0015A418` wrapper uses the former, as
canonical `FindFirstObjectById` does. This fixes the stale port lookup's
incompatible semantics and compiles as an isolated override. The older Splash
run reported the state-notifier link search for ID `0x25f` as zero and the
graph pointer at `+0x2e4` as null; the later state-graph follow-up above
supersedes that result. Its `-1` to `0` transition requested event `0x261`,
but `CreateStateWalker` (`0x001b6290`) was then a no-C stub. State was 0, the
timer gate was 0, and graphics/text/DMAs were absent.
The state-setter backfill currently covers modes 0, 2, 3, 6, and 7; modes 4
and 5 still need the camera math. Trace:
`/tmp/sly-canonical-f36f43ee-mode-lookup-next.log`. This earlier trace
predates the state-graph follow-up above. Keep the implementation under
`hand/overrides_c/` so matching C can replace it later.

The first refreshed native run against `55d39f28` showed a class-`0x53`
ObjectGroup stream fault: after an initial list count of 36, a nested count
`0x1ddd` was read at `0x70001d1f`, and the parent later saw `0xffffcccc` at
`0x70001df5`. The exact-model-reader follow-up above supersedes that cursor
diagnosis. Subsequent exact reader selections pass that earlier cursor frontier.
The current parser frontier is the top-level reader exhaustion at
`0x70000000` (`remaining=-1`) during `func_001DCEA0` setup.

A port-only state graph loader retains the retail field layout and stream
reads while guarding absent class stream callbacks. The list append override
rejects invalid guest pointers and repairs an invalid tail before writing.
Those fallbacks got Splash past its immediate `0xFFFFFFFF` crash; replace them
with canonical C when those units are host-readable.

Earlier startup trace after matching the ObjectGroup lifecycle callbacks:
`/tmp/sly-canonical-9eb87599-countdown-state.log`. The Splash group's sole
member is a class-`0x57` CountdownNotifier (`id=0x54b`). It remains at state
`-1`, with its timer disabled and `timeLeft=0` (configured min/max 4–6 sec).
The frame loop runs for about seven guest seconds, but no movie is queued and
no screen element is visible; vertices, GIF image tags, and DMA submissions
remain zero. Exact US retail assembly was selected for the title/prompt
callbacks, removing those no-C stubs without changing the outcome. Next inspect
the notifier start/arming path and the Splash transition producer to find why
screen elements remain hidden. The decomp checkout is unchanged.

## State

- The port-only `func_001E4EA8` now mirrors the
  retail per-frame service tail even when no resource request is pending,
  and after completing a resource request. The tail includes camera/world
  updates and `func_001C06D8`; the latter and `func_001C0070` are selected from
  exact US assembly because they have no matching C yet. The C backfill stays
  isolated under `hand/native_c/`.
- Added the missing `func_00160CE8` counter reset as a port-only backfill:
  decomp C has the body, but hostgen still classifies it no-C. It is listed in
  `hand/backfills.tsv` only (not `overrides.tsv`, which is for replacing
  matched definitions).
- Build succeeded with `cmake --build build/port --target port_native -j2`.
  The ordinary US ELF/ISO native run completed its 8-second trace window with
  no `func_00160CE8` warning, but remains at `sly_frame=0,3,0`, game mode 0,
  and `visible_text=0`. This rules out the missing counter reset as the full
  startup fix. Log: `/tmp/sly-native-counter-reset-verified.log`.
- The default-startup warning trace also showed five reached animation/update
  functions had no C. Exact US assembly is now selected for
  `func_001E77C0`, `func_001E9C78`, `func_001E9EE8`, `func_001EA0A8`, and
  `func_001EAAE0`. Hostgen reports 230 assembly translations, zero stubs.
  A fresh native run no longer treats these as no-ops, but stays at
  `sly_frame=0,3,0`, `visible_text=0`; they are not sufficient to advance the
  title transition. Log: `/tmp/sly-native-ui-update-asm.log`.
- Exact assembly comparison remains: the assembly has a pre-reader
  `func_001430B8` timer call; the current backfill still has an older
  `func_00142ED0(1.0f)` after parsing. Recheck this sequencing against the
  retail body before treating this patch as complete.

- Previous pushed baseline: `67afef0 fix(sly1): restore retail resource service tail`.
- Decomp remote check: fetching `/home/reaper/Code/sly1-decomp` found no new
  commits; local `master` and `origin/master` remain `e1b39f1`.
- A 35-second native-only launch advances guest time to about 17 seconds but
  remains at frame state `0,3,0`, with menu page `-1` and no visible text.
  This is not just a short startup delay. The trace's old `movie` fields were
  mislabeled: `0x00269A04..0x00269A10` belong to the worker-dispatch structure
  used by `func_0018E4F0`, not confirmed movie status. The trace now labels
  them `thread_dispatch`.
- Next: trace the normal Splash-to-title transition/request producer and its
  state selector callbacks, and separately identify whether a retail PSS
  playback call exists. Keep the direct-level probe diagnostic-only.

## Previous state

- Latest pushed commit at this update: `a115f82 docs(sly1): preserve resource watchpoint findings`.
- The checkout was clean and matched `origin/wip/native-sly-port` when this
  update was written.
- The local decomp checkout `/home/reaper/Code/sly1-decomp` was fetched and
  checked: clean `master` at `e1b39f1 feat(us): match small function batch`;
  the routines relevant to the current object/render investigation were still
  in progress there.
- Build succeeded with 2,043 translated C functions, 222 assembly
  translations, and zero stubs. Rebuild generated code with
  `cmake --build build/port --target port_native -j2` after hostgen changes.

## Actual graphics blocker

The native window starts, but the current level probe produces no scene
vertices or visible text. The direct `OPENRAC_BOOT_LEVEL=0x0100` probe reaches
loader stage 5 but does not connect that loaded world to gameplay. Five outer
groups (`0x00fe903c`, `0x00fe942c`, `0x00fe981c`, `0x00fe9c0c`,
`0x00fe9ffc`) remain skipped by their resource flag; preserve that retail
behavior. A separate render list has an unskipped entry at `0x00847ce0` with
three primary records. Their descriptor at `0x0083f4b0` points to three
512-byte GS image payloads at `0x0064d6e0`, `0x0064d8e0`, and `0x0064dae0`.
Retail `func_001662A8` still outputs command tags and zero scene geometry
addresses. The exact pointer chain and transfer evidence are written in
`docs/port/SLY_GRAPHICS_HANDOFF.md`, so `/tmp` captures are not required to
resume. Exact US assembly is also enabled for `func_001DC9A0`, the 800-byte
level effect initializer that had been an empty no-C stub. The latest bounded
run remains frame state 3/game mode 0 with zero scene vertices.

### Latest producer-side evidence (2026-11-07)

A native GDB hardware watchpoint was armed on guest EE address `0x0064d6e0`
before direct-level startup (`OPENRAC_BOOT_LEVEL=0x0100`). It fired during
`func_001DBA38`'s reader copy, through `func_001DCEA0` and
`func_001DBA38 -> func_00137A60 -> func_0018D800`. At the copy routine,
registers were `rdi=0x0064cae0`, `rsi=0x0c00`, `rdx=host-mapped guest buffer`,
`rcx=0x0064d6e0`; the destination is exactly `0x0c00` bytes after the copy
start. This ties the watched image payload to the direct-level resource
reader; the later DMA trace identifies its format, but not the render-list
consumer. The
retail C for `func_001DBA38` is in
`/home/reaper/Code/sly1-decomp/src/main/001D8000.cpp` around line 206: it reads
`D_00275720` bytes into `D_00275718` after advancing the reader by `0x10`.
All three 512-byte payloads were confirmed in the decoded resources: level
`0x0100` at `0x4d0470`, `0x4d0670`, `0x4d0870`, and Splash `0x0002` at
`0xac00`, `0xae00`, `0xb000`. Each sequence has a `0x200` stride. The image
payloads are shared across the two resources.

Filtered native traces confirm all three are PSMCT32 image uploads in GIF DMA
chain `0xe0418ac0`: REFS tags at stream offsets 704 (`0x64d6e0`, tag
`0xe0418bb0`), 1312 (`0x64d8e0`, tag `0xe0418c30`), and 1920 (`0x64dae0`, tag
`0xe0418cb0`), each QWC 32. Each preceding GIF setup sets `TRXREG=(16,8)` and
`TRXDIR=0` (host-to-local), then uses a GIF IMAGE tag for 32 qwords. These are
16x8 image uploads, not mesh geometry. A conditional GDB breakpoint at the
matching chain root confirmed the retail
call stack `func_001DCEA0 -> func_001B3450 -> func_001692B0 ->
func_001686E8 -> func_00202A80 -> openrac_native_renderer_submit_dma_chain`.
This is resource initialization; it does not explain the zero scene vertices.
An entry-filtered breakpoint at `func_001662A8` for `0x00847ce0` also showed
`func_00165728 -> func_00124A48 -> func_00146470 -> func_001DCEA0` as its
caller chain. The command-buffer capture therefore belongs to resource
initialization, not a per-frame scene draw. See
`/tmp/sly-dump-render-entry.gdb` and `/tmp/sly-dump-render-entry.log`.

The write-watch GDB script was `/tmp/sly-watch-mesh-first-write.gdb`; the read
watch script and output are `/tmp/sly-watch-mesh-read.gdb` and
`/tmp/sly-watch-mesh-read.log`. Both derive guest base from native symbol
address `0x724e90`. The DMA target traces are
`/tmp/sly-native-dma-target-context.log`,
`/tmp/sly-native-dma-target-64d8e0.log`, and
`/tmp/sly-native-dma-target-64dae0.log`; the conditional stack trace is
`/tmp/sly-watch-mesh-chain.gdb` and `/tmp/sly-watch-mesh-chain.log`. The
source-level reader and current packet evidence are now
preserved here and in `docs/port/SLY_GRAPHICS_HANDOFF.md`.

Useful captures, if `/tmp` survives restart:
`/tmp/sly-native-direct0100-mesh-sections.log`,
`/tmp/sly-native-direct0100-packet-dump.log`,
`/tmp/sly-native-direct0100-dc9a0-asm.log`,
`/tmp/sly-native-direct0100-render-groups-final.log`, and
`/tmp/sly-watch-entry-flag-detail.log`. Read
`docs/port/SLY_GRAPHICS_HANDOFF.md` and
`docs/port/PLAYABLE_TODO.md` before repeating older probes; they contain prior
startup, rendering, and decomp coverage findings.

## Resume here

Trace the renderable-object list and retail draw callback that should populate
the scene geometry address fields; the three image transfers above are not
the missing mesh path. Do not force the skipped groups or synthesize packet
data. Separately trace the normal
Splash-to-title/New Game transition; normal startup still stays on Splash
(frame state 3/game mode 0) and does not reach the opening sequence or title.
Keep the direct-level probe diagnostic-only. Keep all functional C replacements
as clearly labeled port-only backfills, separate from matching decompilation.

Native launcher command used for the direct-level probe:

```sh
OPENRAC_BOOT_LEVEL=0x0100 build/port/sly1_native \
  build/port/extracted/boot/SCUS_971.98 \
  '/home/reaper/pcsx2/Games/Sly Cooper and the Thievius Raccoonus (USA).iso'
```

The native window opening successfully is not a playable or visible milestone.

### Restart note (2026-11-07)

The latest corrected guest-memory dump is `/tmp/sly-dump-render-entry-guest.log`;
the earlier `/tmp/sly-dump-render-entry.log` used host addresses incorrectly
and should not be used. The corrected dump shows the real entry at
`0x00847ce0`, nested records at `0x00848070` (stride `0x50`), and shared
payload pointer `0x00840730` in each of its three nested records. The observed
call remains in initialization:
`func_001662A8 <- func_00165728 <- func_00124A48 <- func_00146470 <- func_001DCEA0`.

That suspected override mismatch has now been checked. The full retail US
assembly body (`asm/us/text.s`, around line 125800) uses the same entry stride
(`0x70`), subdata stride (`0x28`), object callback at entry `+0x38`, nested
call to `func_001662A8`, and linked-node offsets (`+0x4C`, `+0x80`) as the
decomp C. No semantic discrepancy was found in the traversal. The former
trace-heavy port wrapper has been moved to
`hand/diagnostics/func_00165728_trace.c`; it is no longer registered as a live
override. `hostgen.json` explicitly selects `func_00165728` from US retail
assembly through `from_assembly` and `asm.only`. The generated report confirms
`asm/us/text.s` as its source (223 functions selected from assembly).

A 12-second native run using that exact assembly still stays at
`sly_frame=0,3,0`, `game_mode=0`, and `vertices=0`, with three GIF chain
submissions and two active guest threads (`func_001DC710` and
`func_0015EE80`). This confirms the C wrapper was not the cause of the black
screen. The bounded run was stopped by `timeout` (exit 124); there was no
launcher crash. Its full trace is `/tmp/sly-native-165728-retail-asm.log`.
Continue by tracing why the normal startup draw/frame path does not advance
past this state and why it submits no per-frame scene or UI geometry. Keep any
functional C repair in the port-only backfill area, never in `sly1-decomp`.

Corrected dump highlights: entry `0x00847ce0` has three nested records at
`0x00848070`, `0x008480c0`, and `0x00848110`; each record points at
`0x00840730`. The entry's `+0x28` flag is zero, so retail
`func_001662A8` proceeds for this entry. These records still have not been
connected to per-frame geometry. The `func_00165728` selection mismatch is
resolved; resume tracing the actual retail render-list producer and why the
regular startup draw path does not submit geometry. The normal Splash-to-title
transition is a separate unresolved route; direct-level mode remains
diagnostic only.

No repository changes were pending at the restart checkpoint before this
note. Rebuild with `cmake --build build/port --target port_native -j2`; native
launcher runs are authorized. Do not run PCSX2 or another emulator.

## Latest restart handoff (2026-11-07)

This section supersedes earlier "resume here" notes where they conflict. The
active branch is `wip/native-sly-port`, currently at pushed commit `dfd8d48`
(`debug(sly1): correct startup thread trace labels`); the checkout was clean
and no build or launcher process was active when this checkpoint was written.
The decomp checkout `/home/reaper/Code/sly1-decomp` was fetched and remains at
`e1b39f1`; it has no new code to consume. User asked for frequent commits and
pushes. The game still opens to a black screen and is not playable.

Recent runs have established that the black output is the intentional retail
screen clear. Default startup reads only the Splash resource, sits at frame
state `0,3,0` / game mode 0, never requests a level, and produces no visible
text or scene vertices. Waiting longer does not advance it. Generic resource
queue tracing found no enqueue/read request after Splash. The activated async
worker path now enters exact retail `func_0018D0A0` and polls its semaphore;
that generic callback is not the missing movie or scene request. The old
`0x00269A04..0x00269A10` trace fields are worker-dispatch state, not proof of
movie playback. The direct-level shortcut remains diagnostic only; its
resource groups are legitimately skipped and it has not produced a playable
scene. Do not force those flags or synthesize geometry.

The former import-table lead was incorrect. This ELF has no `.lib.stub` or
other import-table section; `PsIIlibmpeg 2420` / `PsIIlibipu 2400` are data
strings adjacent to codec tables, not import records. Sly's own retail code
does contain the statically linked MPEG player: `func_0018F448` sets it up via
`func_0020ADA0` and `func_0020AE40`, reached from the request processor
`func_0018F198`. `func_0018F148` drains the request queue and `func_0018F0E8`
enqueues requests. A current trace sees none of those queue/player calls after
the one Splash read. Continue with the producer/state transition that should
request media; use Sly retail assembly as evidence and do not transfer
addresses or names from another game. If the evidence identifies a missing
retail function requiring functional C, keep it under
`port/game/sly1-us/hand/native_c/` and register it in `hand/backfills.tsv`;
never change `sly1-decomp` for the port backfill.

The global element stack is in state `3`, its normal state. The game clock
advances, and the selected pad is in mode/state `3/4` with no pressed buttons.
That is not evidence of an app-level input wait; the missing startup/title
transition and the absence of drawing remain unresolved. The native output
remains only the black clear.

### Latest UI and vertex trace (2026-11-07)

The current diagnostic build adds an opt-in `OPENRAC_TRACE_UI_CALLBACKS=1`
snapshot in `port/sly1/main.cpp`. It lists all 37 retail UI objects once their
world and font registry are initialized, including vtable update/draw targets,
mode, and page. The latest native-only 12-second run reaches the UI loop and
registers 30 fonts (four font registry entries), but reports `visible_text=0`;
all listed UI objects have mode 0. The menu object at index 25 has page `-1`.
Several text objects use update callback `func_001AB058`; other currently
inactive callbacks include `func_001AE178` and `func_001E00B0`, which still
show no-C warnings. This is a focused lead for checking retail callback
activation and title setup, not evidence that the callbacks should be forced
active.

The same run briefly reports 240 vertices (117 classified visible) with
bounds approximately `(0,-0.5)` to `(641,224.5)`, then later snapshots report
zero vertices. The captured application window remained black, with no visible
text. Treat this as transient full-screen-sized geometry, not confirmed Sly
artwork; trace its producer and GS/framebuffer presentation before calling it
a first graphics milestone. Run log:
`/tmp/sly-native-ui-callbacks-ready.log` (may disappear on restart). The
diagnostic flag is opt-in; regular launches do not print the inventory.

### Canonical decomp recheck (superseded 2026-11-11)

The previous check used an outdated fork at `/home/reaper/Code/sly1-decomp`
and missed the active sibling checkout. The default sibling at
`/home/reaper/sly/sly1-decomp` has now been fast-forwarded, without source
edits, from PR #81 to canonical `d534006` (PR #102, Nov 9). It is clean on
`master`. Its current report has matching C for 4,667/4,807 NTSC-U functions
(85.76% of code bytes). Note: `build/port/CMakeCache.txt` remains configured
to `/home/reaper/Code/sly1-decomp` at `e1b39f1`; the latest successful native
build therefore still uses that older input until hostgen can consume the
reworked units.

The canonical source contains `src/main/movie_player.cpp`, including
`func_0018F198` (`PlayMovie`), `func_0018F448` (`StartMovieStream`),
sector-stream lookup, MPEG/IPU setup, and movie decode callbacks. Its
`src/main/main_loop.cpp` drains queued movies with `PlayQueuedMovies` before
world-load processing and again afterward; the startup initializer table also
calls `CreateMovieDecodeThread`. In `src/main/file_14EE40.cpp`,
`HandleStepSequenceMessage` sends message type 7 to
`QueueMovie(g_moviePlayer, msg->f4.i)`. The concrete producer-side trace lead
is to determine whether/where the startup step sequence posts type 7 and why
the native run has no queued movie after Splash. These source files are now in
the configured checkout. A C-only hostgen run against `d534006` read 238
units, translated 563 functions, stubbed 2, and left 4,323 without C. Several
important C++ units fail the current C staging because headers contain
methods/templates and inheritance layouts that the stager cannot lower. This
is a hostgen compatibility gap, not a loss of matching progress. Keep
generated code out of the decomp repository.

The same source reclassifies the words previously logged as `thread_dispatch`:
`D_00269A00` is `MoviePlayer`; offsets `+4`, `+8`, and `+0xC` are queued key,
successor key, and stream reader. Offset `+0x134` is the decoder thread step.
`func_0018E4F0` is `QueueMovieWithSuccessor`, which sets the successor. The
diagnostic now prints these fields under movie-player names. This corrects
the earlier interpretation that they were worker-dispatch fields. A fresh
12-second native run (using the existing CMake input) showed queued key,
successor key, and reader all zero from 0.42 through 5.45 guest seconds;
`threadStep` remained 2. The frame stayed `0,3,0`. It reported 240 vertices
for two intermediate snapshots, but the window remained black, so these are
not confirmed game graphics. Log: `/tmp/sly-native-decomp-refresh-queue.log`.

The user-provided 62-second PCSX2 capture shows the game go from launch black
to the SCEE Presents card, animated opening, gameplay, and title prompt. The
native state-object diagnostic narrows UI index 17 (`func_001E00B0` update /
`func_001E0678` draw, object `0x0026e9f8`) to `+0x250=0`, `+0x260=0`,
`+0x264=0`, `+0x268=0`, `+0x26c=0`, `+0x270=0`, `+0x274=0`,
`+0x278=0x00218330`. At state 0, retail `func_001E00B0` returns when `+0x26c`
is zero. This is a useful state lead, not proof this UI object itself starts
the movie. Native log: `/tmp/sly-native-intro-state.log` (transient). The
diagnostic build succeeded and the 12-second native run reached timeout
without crashing; no emulator was used.

### Canonical source integration update (2026-11-11)

The latest upstream Sly decomp master is `9eb87599` (PR #110), fetched from
`OpenThievius/sly1-decomp` after the local `/home/reaper/sly/sly1-decomp`
checkout was found 317 commits behind. A detached snapshot is available at
`/tmp/sly1-decomp-9eb87599`; the decomp checkout was not edited. Its README
reports 4,747 of 4,807 US functions matched (98.75% by function count; 92.49%
of code), and the rework adds typed mesh and buffered-stream layouts plus
functional `func_00137FB0` and `func_00138278` bodies in
`src/main/file_137290.cpp`.

The native port was rebuilt against that snapshot. Hostgen still reports 247
units, 713 translated functions, 238 assembly translations, 2 stubbed, and
3,951 without C. It cannot compile `file_137290.cpp` as one unit because
included aggregate headers have duplicate inherited member names and resulting
layout assertions (including `WorldListedObject::next` and collider
`contacts`). The new collision reader therefore informs the port-only
`hand/native_c/collision_mesh_reader.c`; it is not copied into the decomp or
counted as matching work. `func_00137FB0` remains excluded from hostgen's
assembly-only list while the backfill is active. The canonical face-tree C is
also still not hostable, so its separate port backfill remains registered.

Comparing the reworked `LoadCollider` with the port callback found the actual
second-mesh misalignment: the callback stored the face-reference pointer where
`refCount` belongs, omitted the `refs` pointer field, then used the first
mesh's `faceCount` as the condition for reading `faceStarts`. It therefore
consumed thousands of extra halfwords before the second mesh. The port now
stores `refCount` at `+0x408`, the refs pointer at `+0x40c`, and reads
`faceCount + 1` signed halfwords only when `refCount > 0`, matching upstream.

The second mesh now parses with 3,604 vertices, 6,403 faces, and 6,115 edge
records; its face tree has 16,113 nodes. The callback returns with positive
stream bytes remaining and the level parser proceeds into placed-object
records. A 10-second bounded native run reaches the frame loop without a
process crash. It still reports zero rendered vertices and a black window, so
the remaining work is in world-object activation or scene draw submission,
not the second collision-mesh read. Latest native log:
`/tmp/sly-canonical-9eb87599-collider-offset-fix.log`. Build:

```sh
cmake -S port -B build/port/canonical-fresh \
  -DOPENTHIEVIUS_SOURCE_SLY1=/tmp/sly1-decomp-9eb87599 \
  -DOPENTHIEVIUS_ASM_SOURCE_SLY1=/home/reaper/Code/sly1-decomp \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build/port/canonical-fresh --target port_native -j2
```

The reworked packet-writer source also defines `func_00152CA0`
(`AddDmaReturnTag`), which closes a DMA tag and appends a `RET` tag. Its
containing unit is not hostable yet, and runtime diagnostics showed this
function being called as a no-C stub. A port-only equivalent was added to
`hand/native_c/display_packets.c` and `hand/backfills.tsv`. The rebuilt launcher
no longer reports that missing-function warning, but still reaches the frame
loop with zero vertices, image tags, and submitted chains. This is a repaired
packet primitive, not a visual milestone. Run log:
`/tmp/sly-canonical-9eb87599-return-tag.log`.

### Active work after resume (2026-11-11)

1. Keep work on `wip/native-sly-port`; keep the canonical decomp checkout
   read-only.
2. Trace the loaded level's placed objects from `func_001289C0` through world
   list activation and the frame draw callbacks. The loader now completes both
   meshes and face trees, but the frame trace still reports zero vertices.
   The `AddDmaReturnTag` no-op has been repaired and did not by itself produce
   render submissions; keep tracing the object-to-draw path.
3. Keep `func_00137FB0`, `func_00138278`, and the class-4 callback as clearly
   marked port-only code. Replace them only when the corresponding canonical C
   can be hosted or when the backfill is deliberately verified against it.
4. Re-run bounded native launches after a reader fix and track scene vertices.
   Do not claim graphics from resource parsing or a non-black window.

Primary durable notes are this file and `docs/port/PLAYABLE_TODO.md`; the
latest queue and worker evidence is near the end of the latter. `/tmp` logs
may disappear on restart. Build generated translations with:

```sh
cmake --build build/port --target port_native -j2
```

The native launcher accepts the extracted US ELF and disc image; the known
paths are `build/port/extracted/boot/SCUS_971.98` and
`/home/reaper/pcsx2/Games/Sly Cooper and the Thievius Raccoonus (USA).iso`.
Native launches are authorized. Do not use PCSX2 or another emulator.

## Notifier lookup follow-up (2026-11-11)

The current branch is `wip/native-sly-port`. Canonical `sly1-decomp` remains
read-only at `f36f43ee4236753a04d73881b82d16ca0100bcea`; the old sibling
checkout is not used for C. The canonical source defines
`FindFirstObjectByIdGathered` at `0x0015A838` as a wrapper around
`GatherObjectsById` at `0x0015A4F0`. Since `file_1549C8.cpp` is still unreadable
to hostgen, both functions are staged as separate port-only overrides in
`hand/overrides_c/` and registered in `hand/overrides.tsv`. The adjacent
`0x0015A198` is a different traversal and must not be conflated with either.

The 7-second native trace proves mode-1 lookup returns the expected child
(`0x846cb0`, class `0x75`, id `0x22a`, parent `0x846910`) and stores it at
notifier offset `+0x2ec`. A prior trace incorrectly read `+0x2e8`, which is
the state-walker handle. The notifier remains at state `-1` with timer gate
off. `func_00147AB0` still reports no C in the current host build, but its
same-state `-1` call is not yet shown to be the graphics blocker. No movie,
visible text, vertices, GIF image tags, or DMA submissions appear. Log:
`/tmp/sly-canonical-f36f43ee-owner-resolved.log`.

Latest successful build:
`cmake --build build/port/canonical-f36f43ee --target port_native -j2` — 263 C
translation units compiled, 722 functions translated, 247 retail assembly
translations, 2 unsupported C stubs, and 3,936 functions without C. The
window remains black; this is not a visual milestone.
