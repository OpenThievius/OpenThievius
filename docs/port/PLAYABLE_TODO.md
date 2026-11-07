<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Concrete plan for a playable native Sly 1

Fresh repository review: 2026-11-07, starting at 1e17ead. A textured menu
frame capture on the same date narrowed the visual blocker further; see the
graphics handoff checkpoint below.
This is a work plan, not a claim of playable progress. Work is active on
wip/native-sly-port and stays in the port repo.

## Assessment

The project has useful extraction, host generation, platform and rendering
infrastructure. Its largest gaps are executable game-code coverage and the
boundary between Sly rendering and console hardware simulation. Repeated
startup traces have not yet established a complete path from a loaded level
to a visible, controllable player.

Evidence inspected:

- CMake's default runtime includes ee.cpp, vif1_dma.cpp and vu1_native.cpp.
  The latter maintains vector registers, flags, microcode memory and a program
  counter. Compiling this code for the host does not make hardware simulation
  satisfy the user's no-emulation requirement.
- The configured C source is /tmp/sly1-decomp-current-check; assembly comes
  from a different checkout. This is a fragile build baseline.
- The current generated report lists 1,833 translated functions, 90
  assembly-derived functions, 3 stubbed functions, 3,026 functions without C,
  and 47 callers with omitted arguments filled with zero. Matching percentage
  is therefore a poor measure of executable port coverage.
- README, ROADMAP and OPENRAC_REUSE give different snapshots and counts.
- The direct-level loader constructs objects but does not establish gameplay.
  The first eight traced default-startup objects do not prove what is wrong
  with an actual level: their identity basis and zero position/radius may be
  valid for their role. Their identity and ownership must be established first.

## Ordered TODO

### 1. Establish a reproducible baseline

- [ ] Pin the decomp revision, tool revisions, retail version and source paths
  in a build manifest; replace the temporary source checkout with a stable one.
- [ ] Record generated coverage from that exact build, including backfill and
  assembly selections. Clearly separate historical results in the roadmap.
- [ ] Document one supported developer build and launch command. Keep host32
  alternatives explicitly experimental until there is evidence to choose one.

Done when a fresh checkout with user-supplied game data reproduces the same
function report and build without undocumented files in /tmp.

### 2. Enforce the native architecture

- [ ] Audit the default target's actual runtime calls, classifying compiled
  game code, native services, asset/packet decoding and hardware interpretation.
- [ ] Put instruction execution and console device simulation behind a
  separate diagnostic target; exclude them from the playable target.
- [ ] Define native interfaces for resource reads, scheduling, input, audio
  and scene drawing. Preserve retail pointer/layout adapters where necessary.
- [ ] Retain usable OpenRAC platform, hostgen and direct-renderer infrastructure.
  Compare any proposed additional reuse against its actual source and license;
  import generic components with attribution, never RAC-specific game code.

Done when the playable target executes compiled game functions and native
services without a CPU/VU instruction interpreter. Static translation of a
known function is distinct from a runtime interpreter and must be documented.

### 3. Close the game-code coverage gap

- [ ] Generate a per-unit report explaining why matching source is excluded:
  missing metadata, C++ lowering, ABI disagreement, unsupported syntax or no C.
- [ ] Prioritize units reachable from loading, frame update, player creation,
  camera and draw submission. Fix whole-unit hostability before writing more
  one-function replacements for code already available upstream.
- [ ] Audit omitted arguments, function-pointer calls, float/vector arguments
  and pointer-global declarations on those paths against retail behavior.
- [ ] Make required missing calls produce a named diagnostic and stop that
  operation, rather than silently succeeding as no-ops. Explicitly classify
  harmless optional services.
- [ ] Give every functional backfill provenance, ABI notes, known limitations
  and an upstream replacement condition in the port's separate registry.

Done when the chosen level's load/update/draw path has no unexplained no-ops
or fabricated zero arguments. A global matching percentage is not this gate.

### 4. Prove resource and object ownership

- [ ] Use one validated extraction/cache per disc identity; check concurrent
  launches share it without duplicate extraction processes.
- [ ] Select one real level and trace its resource key through decode, object
  construction, activation and assignment to the active game world.
- [ ] Identify traced objects by class, resource record, parent and mesh owner.
  Capture local/world transforms and bounds before and after activation.
- [ ] Compare those fields with the decoded record and retail update order.
  Fix the first demonstrated mismatch, including missing parent callbacks.

Done when a known renderable object has attributable mesh data, bounds and
camera-relative placement. Never force visibility merely to pass this gate.

### 5. Build the Sly scene-rendering path

- [ ] Document Sly's draw-request and mesh layouts at the boundary before
  hardware command generation; implement a native scene submission adapter.
- [ ] Render one actual level mesh with its real transform, camera and material
  using the existing GL infrastructure. A standalone viewer is diagnostic only.
- [ ] Add the remaining required geometry, texture/material, depth, alpha,
  skinning and animation behavior from Sly's formats and retail semantics.
- [ ] Fix the independently reproducible 2D font problem by comparing decoded
  glyph rectangles, UVs, texture addressing and blend state at each boundary.

Done when the running game's active world draws recognizable level geometry
and the animated player, with legible UI and correct camera motion, without
executing VU microcode. A triangle, font atlas or vertex count is insufficient.

### 6. Connect loading to real gameplay

- [ ] Complete the normal title-to-New-Game transition and world ownership.
  Keep direct loading as a diagnostic entry point, not a substitute for this.
- [ ] Wire native input through player actions and verify movement, jump,
  attack, camera control, collision, grounding and interaction.
- [ ] Implement the sound-event bridge to native audio, pause/resume and level
  transition behavior. Verify death/restart and checkpoint state.
- [ ] Exercise a complete opening section with human input and record observed
  behavior, remaining defects and the exact build used.

Done when the user can launch, start a game, control Sly and progress through
an actual section with working camera, collisions and interactions. This is
the first playable gate, not proof that the whole game is complete.

### 7. Finish and maintain

- [ ] Expand validation across levels, enemies, abilities, saves/loads, audio
  and transitions; keep failures explicit rather than claiming full support.
- [ ] Validate supported platforms and packaging after the primary gameplay
  path works; preserve game-data ownership and extraction requirements.
- [ ] Commit each coherent fix with its evidence and push regularly. Keep the
  WIP PR and contributor documentation aligned with the actual default build.

## Earlier native checkpoint (2026-11-07; superseded below)

The US native launcher now gets beyond the earlier malformed Splash object
records. The port explicitly selects retail assembly for the loader methods
`func_0011ECB8`, `func_00158B30`, and `func_001B5F50`, and for the animation
curve factories and missing readers needed by `func_0012B8C8`. These are
static translations of known retail functions, not a runtime interpreter.
Functional C backfills remain in the port registry and decomp source is
untouched.

Current hostgen reports 1,832 translated functions, including 91 selected
from retail assembly, 3 stubs, and 3,026 functions without C. It also reports
99 calls that omit arguments and fill them with zero. These are build counts,
not runtime coverage.

Retail assembly confirms `func_0013B598` forwards incoming register `a1` as
the stream to `func_001BAC30`, while its current decompilation signature omits
that argument. The port now selects this static retail translation with
signature `void(void*, void*)`, fixing that demonstrated ABI mismatch without
editing `sly1-decomp`. The earlier dispatch through descriptor data
`0x0021B4D8` no longer appears in the follow-up trace.

The latest native launch still faults during Splash object loading. The trace
reaches `func_001BAC30` through `func_0013B598`, but reports a null indirect
call from `func_0011ECB8` and then a write through guest address `0x02000000`
in `func_001BAC30`. Collision-reader stages show vertex count 0, face count 0,
then link capacity `0x3F80` at reader cursor `0x700008EE`; this is the earliest
concrete suspicious value. The launcher is not playable and remains visually
black. Latest trace: `/tmp/sly-native-reader-trace.log` on the development
machine.

## Immediate next steps

1. Compare the class `0x3F` collision record at reader cursor `0x700008EA`
   with retail `func_00137FB0` and the serialized Splash bytes. Establish
   whether zero vertex/face counts followed by link capacity `0x3F80` are
   valid, and why `func_001BAC30` derives a write destination at
   `0x02000000`. Fix only the demonstrated reader, allocator, or record-layout
   mismatch. Keep functional replacements in the port backfill registry with
   provenance and an upstream removal condition.
2. Trace the remaining null indirect call from `func_0011ECB8` to its record,
   descriptor slot, and intended method. Do not suppress it to keep startup
   alive.
3. After Splash loading completes without a fault, follow the active world
   through update and native scene submission. Capture the first draw request's
   mesh, material, transform, and camera inputs, then render one recognizable
   level image before moving to input and player controls.

The bounded native trace command used for this checkpoint is:

```sh
OPENRAC_TRACE_LOADER=1 OPENRAC_STARTUP_TRACE=1 timeout 10s build/port/sly1_native \
  build/port/extracted/boot/SCUS_971.98 \
  '/home/reaper/pcsx2/Games/Sly Cooper and the Thievius Raccoonus (USA).iso'
```

It runs the native launcher, not an emulator. The latest captured output is
kept in `/tmp/sly-native-reader-trace.log` on the development machine
only.

Treat renderer traces and function coverage as diagnostics only; neither
proves visual correctness or playability.

## Coverage audit result (2026-11-07)

Run `python3 port/tools/audit_hostgen.py build/port/hostgen/us/report.json
--out build/port/hostgen/us/coverage-audit.json` as one command after hostgen.
The JSON retains complete compiler diagnostics and the direct-call frontier;
its categories overlap and do not establish indirect-call coverage.

The initial report at the time of this audit had 57 unreadable units: 16
reported inherited-member collisions, 15 layout assertion failures, 11
call-signature mismatches and 7 unlowered C++ members. This snapshot is
historical; later coverage results are recorded below. Core object units
00124000 and 00128000 encounter duplicate inherited `id` fields in the staged
animation header. World unit 0019C000 retains a C++ cursor constructor and
method syntax. Prioritize correct inheritance/layout and cursor lowering;
do not suppress layout assertions or rename fields without resolving accesses.
The audit command is implemented; source hostability fixes remain pending.

The staging correction preserves C++ member hiding for Animation.id
and ObjectCycler.index by renaming only the inherited anonymous-C copy.
The derived field keeps its name; base casts still address the original base
layout. The rebuilt object units no longer report those duplicate fields or
the associated layout failures. The full native build succeeds, but coverage
remains 1,108 translated functions: independent call-signature and C++ method
errors still exclude these units. Next address the static iterator helpers in
00124000 and the EnsureRangeRecord signature in 00128000, with retail ABI
verification rather than speculative casts.

The 00124000 static iterator helpers now lower to file-local C functions,
with their bodies and call sites preserved. The lowerer refuses to consume a
class unless its sole method is followed by the class terminator. A full
native rebuild succeeds; this unit now reports three errors: the sound-call
signature disagreement and a local Frame_124CC0 inheritance declaration
(with its consequent incomplete-type error). Coverage is still unchanged.
Retail inspection confirms EnsureRangeRecord consumes an object pointer;
fix its inconsistent callers/declarations together rather than adding casts.

The placed-object unit is now hostable. Its second `func_001BFCD8`
declaration and call use a distinct staged C name with the same assembler
label, allowing hostgen to retain the two source-level signatures while
mapping both to retail address `0x001BFCD8`. The local animation frame is
lowered to its zero-offset base fields plus its original tail. Rebuilding
increases translated coverage from 1,108 to 1,139 functions; unreadable units
fall from 57 to 56. The native launcher links successfully. This establishes
more real game code in the host build, but does not yet establish that these
functions are reached in a playable run.

Generalizing the conservative single-static-method helper lowering across all
source units increased hostgen translation from 1,139 to 1,284 functions and
reduced unreadable units from 56 to 53. The native launcher links and the
repository check passes. This recovered functions from several unrelated
units, but does not change the runtime playability status. The remaining
constructor/method failures are non-static helpers, including constructors
that initialize stack records. Lower those only with call-site/layout
preservation; do not erase them from records as if they were static methods.

For source unit 0018C000, the staged lowering now converts the simple
Pair_18CC20::Set, Key_18F198::Same and Pair_18F198::Set methods into C
helpers with explicit object pointers and rewrites their call sites. Field
accesses remain at their original offsets. The unit's diagnostics fell from
six to two; both remaining errors are return-type disagreements for the
memory-card write/close calls. Hostgen coverage has not increased yet, and the
native build still links. Resolve those function declarations from retail
call sites before claiming this unit recovered.

The retail call sites and assembly show that `func_0018D328` and
`func_0018D2B0` return the status in `v0`; their C bodies called
`FinishMemoryCardCall` but discarded its result. The staging layer now corrects
only these two signatures and returns in its disposable C view. The matching
decompilation sources remain unchanged. `0018C000` now compiles, increasing
hostgen translation from 1,284 to 1,329 functions and reducing unreadable units
from 53 to 52. The native launcher compiles 212 generated C translation units
and links. This is a host-coverage result only: it does not demonstrate that
startup reaches these routines or that a scene is visible. The next useful
coverage work is to inspect the 52-unit audit for startup/load/update/draw
callers and resolve the highest reachable whole-unit blocker with retail ABI
evidence. Keep backfills in the port registry and staging layer, with a clear
replacement condition for eventual decompiled C.

The next whole-unit blocker, 00128000, had lost the `PlacedObject*` argument
from four range-record setter signatures and had added an extra float argument
to the staged `EnsureRangeRecord` call. Retail assembly for 001292E8,
00129318, 00129348 and 00129378 passes the object in `a0`, preserves the float
in `f12`, and calls 001291D0 with the object. Staging now restores that
argument flow and the pointer-only helper signature. Its final C++ record
method, `St_12B2F8::Set`, lowers to a file-local helper with an explicit record
pointer and unchanged field offset. `00128000` now compiles, increasing
translation from 1,329 to 1,480 functions and reducing unreadable units from
52 to 51. The native launcher links, and `tests/check_repository.py` reports
zero problems. The remaining 51 units still need prioritization by actual
startup/load/update/draw reachability; this coverage gain alone does not mean
the game is playable.

Hostgen can now compile `file_1E08F8.cpp`. Its port staging lowers the small
`Ctx_1E0CD0::Pad` and `Sub_1E4EA8::SetOwner` methods to explicit-self C helpers,
and represents constructor `func_001E4E38` with its retail `self` return.
Soft-float overloads that share retail addresses are given distinct C names
with the same assembler labels, preserving their separate source call types.
The render-thread root `func_001E2398` is now translated from matching C.
Coverage is 1,527 translated functions, with the same 59 selected-assembly
translations, and unreadable units fell from 51 to 50. The three thread APIs it
calls (`func_001F6A10`, `func_001F6A50`, `func_001F6A90`) have existing native
implementations in `port/game/sly1-us/native/linux_threads.cpp`. The other
startup root, `func_0018E5E8`, was still blocked by `file_18D8E0.cpp`'s C++
movie sink classes at this stage. The build and repository check pass;
visibility/playability has not been demonstrated.

Hostgen now compiles the movie sink unit `file_18D8E0.cpp`, including matching C
for root `func_0018E5E8` and the four methods named by its retail vtables. Port
staging models the sink vptr as a 32-bit field, retains the ring fields at
their retail offsets, and initializes the ring and stack DMA vptrs from the
verified US vtable addresses. This keeps the C translation's virtual dispatch
layout intact. Translation rose from 1,527 to 1,572 functions, and unreadable
units fell from 50 to 49; the native launcher links and the repository check
passes. The startup-root list no longer contains either `func_001E2398` or
`func_0018E5E8`. The audit's next missing call is `func_0011A498` from
`func_001B7968`; its matched C is in `file_112698.cpp`, an unreadable unit.
This remains code-coverage progress, not proof of visible or playable output.

A bounded 12-second native launch after this build no longer reports the two
startup roots as no-C stubs, but its direct-render trace is still the same
untextured 240-vertex draw with zero target/source/texture addresses and black
vertex color. It uploads the font atlas and palette repeatedly without proving
glyph selection. The immediate visual blocker remains the draw-state path;
the next game-code blocker is still `func_0011A498` through the unreadable
`file_112698.cpp` unit.

## Latest build and startup failure (2026-11-07)

Port staging now lowers the `Gc_259F38` constructor/destructor and `RetValue`
aggregate returns in `file_112698.cpp`. It also corrects the hidden `a0`
argument on the 0011C3C0 pool helper and its 0011C3C8 caller, based on US
retail assembly. The 0011FFA8 staging wrapper calls its void worker and returns
the verified terminal `v0=0`. No decomp source was modified. The native target
builds 212 generated C translation units and links. Coverage is 1,834
translated functions, 59 assembly-derived, 3 stubbed, 3,056 without C, and 47
unreadable units.

The next native launch faults during object setup instead of reaching the
previous black render window. Temporary traces show `func_0011ECB8` parsing a
three-record list from EE scratchpad data. It accepts the first record
(`type=0x7A`, `key=0x25D`) and then reads `type=0` where a second record begins.
The bytes in the decompressed chunk match the decoded Splash resource exactly,
so the unpacker and its input are not the discrepancy. Investigate the parser
cursor and caller that supplies this list, focusing on the 0%-matched
`func_0011ECB8` and `func_00196410`, plus the first record's callback contract.
Do not suppress class zero or patch the descriptor table: that would hide a
cursor or call-path error. The runtime later faults at `0xDBBBC9E2`; startup
and the previous black/garbled rendering remain unresolved.

The `func_0011E580` object-factory backfill existed in the port tree but was
not selected: hostgen compiles the staged C files, then keeps the decomp
definition for duplicate symbols unless the port lists a matched function in
`hand/overrides.tsv`. It is now explicitly listed there. This makes the
port-owned implementation active while preserving a one-row removal path when
matching C becomes hostable. Verification: hostgen reports eight overrides
used, the generated definition contains the port factory body, and the
generated registry points `func_0011E580` at it. The next bounded native launch
still faults on this malformed-record path, with a call through address zero
from `func_0011ECB8` followed by an unmapped access at `0xAB1BABFD`.

## Latest native checkpoint (2026-11-07, supersedes the preceding fault details)

The launcher no longer runs the Splash loader into an out-of-range write.
Crash-register and reader diagnostics showed retail `func_001BAC30` receiving
`0x3F800000` (the bit pattern for `1.0f`) as an array count, then attempting an
allocation whose signed size wrapped. The checked heap backfill correctly
rejected that request, but retail assembly continued its count-controlled
loop and wrote through null. This is a stream-layout/cursor defect, not heap
exhaustion.

The port now has a temporary, explicitly registered `func_00137C40` safety
backfill: normal reads remain unchanged, and values above `0x100000` are
reported at loader stage 161 and returned as zero. In a 12-second native
launch it contained four invalid values (`0x0FFFFFFF` at cursor `0x70000A16`,
`0x0FFFFFFF` at `0x7000057A`, then `0x3F800000` and `0xC0000000` at
`0x70000FF0` and `0x70000FF4`),
prevented the prior guest-RAM fault, and let the loader continue into later
objects. The run timed out while active; it did not reach a playable scene.
The trace also reports an unresolved indirect call to `0x0021B4D8`. Treat the
guard as containment only; fix the parser/call path and remove the override
when matching `func_00137C40` is hostable. Trace: `/tmp/sly-native-guard-run.log`.

Current generated hostgen summary is 1,833 translated functions and 3,028
functions without C. The report includes assembly-selected functions among
the translated total; see `build/port/hostgen/us/report.json` for per-function
selection details. The native window remains black, so the playable gate is
not met.

## Follow-up: descriptor override and world-load completion (2026-11-07)

Inspection of generated host code found that the port already contained a
`func_0011E680` resource-descriptor backfill, but hostgen was still using the
decomp definition because the function was absent from `overrides.tsv`. Adding
the explicit port override activates the code and its scoped diagnostics. A
new 12-second NTSC-U run no longer reports the earlier indirect call to
`0x0021B4D8`; it reaches `func_001DCEA0` stage 5 after object initialization
and then continues through frame setup. The invalid-count guard still contains
malformed values. This establishes that world loading can complete under the
current port substitutions, not that all objects were parsed correctly.

The remaining black display is now downstream of world-load completion. The
run still reports many missing-C functions in post-load and frame setup, and
only submits the small initialization GIF chains in the observed window. Next
trace the first active frame callback and its draw-list dispatch, then fill
the highest-level missing callback with port-only C that matches retail call
semantics. Do not treat successful stage 5 as a playable level. Latest trace:
`/tmp/sly-native-dispatch-active.log`.

## Follow-up: preserve raw reader values and inspect first frames (2026-11-07)

After enabling the `func_0011E680` backfill, later runs no longer observed the
`0x3F800000` and `0xC0000000` values that had driven the original runaway
array loop. The broad `func_00137C40` guard was therefore removed: it had also
rewritten unrelated high-bit words, including `0x80000000`, and could hide
valid retail data. The `func_0018D500` heap-bounds check remains as a memory
safety measure, but no generic reader now substitutes values.

A 20-second traced run reached the active frame loop with the world present,
four registered fonts, and menu object `0x26FF68` in transition. At 5.1 game
seconds the trace counted 240 submitted vertices (120 visible) and over 1,400
GIF-chain submissions, yet the window remained black; the samples had
degenerate `0x80000000` vertex words. The live frame callback was
`func_001AD970`, the display worker was `func_00202C38`, and the game held at
frame state 5 with no visible text. This moves the next investigation to the
actual frame draw data, display chain, and the numerous missing-C callbacks
around `func_001AD970`, rather than loader completion. Latest trace:
`/tmp/sly-native-postload-trace.log`.

## Follow-up: activate the object-record parser backfill (2026-11-07)

The unguarded launch confirms the reader override is no longer needed: without
it, Splash initialization still reaches `func_001DCEA0` stage 5 and the active
frame loop, with no `0x02000000` fault. It still reports a null indirect call
from `func_0011ECB8` while parsing the first Splash object list. Inspection
shows the port's instrumented `func_0011ECB8` parser was also only in
`backfills.tsv`, while hostgen selected the decomp definition. The port now
explicitly overrides it, and its forced `from_assembly` / `asm.only` selection
has been removed from `hostgen.json`. The new build reports 1,834 translated
functions and 90 assembly-selected. A 12-second launch now emits the port
stages 21–35 with plausible first records (count 36, slot 4, key 1, flag -1),
reaches world-loader stage 5, and reports no null or `0x0021B4D8` indirect
call. The broad reader guard is absent; no guest-RAM fault occurs. Later
serialized values still include out-of-range slots, so the record stream
needs validation before treating its objects as correct. The window is still
not a playable scene. Trace: `/tmp/sly-native-object-records-active.log`.

## Updated priorities (2026-11-07)

The following results supersede the older startup-fault and “next loader
fix” statements above. The latest hostgen report reads 199 units and translates
1,834 functions, including 90 static assembly translations; it reports 3
stubs and 3,030 functions without C. These are generation counts, not reachable
gameplay coverage.

A normal native launch reaches the active frame loop without the former null
indirect-call and `0x02000000` write errors. A fresh traced launch reaches the
Paused menu page, calls its draw callback (`0x194f80`), emits glyph records,
and uploads the 512x512 PSMT4 atlas plus its 8x2 palette. This proves menu text
generation and texture upload, not a visible menu: the user-visible output
remains black or garbled. The existing PPM captures were taken before the
136KB textured menu packets begin. The renderer also receives repeating
912-byte packets,
each decoding to a 240-vertex untextured draw whose first vertex has zero UVs
and black RGBA; target, source and texture are all zero. Separate 144-byte
packets decode to no vertices.

The next tasks are:

1. Isolate why the decoded textured glyph draw leaves no visible pixels.
   `OPENRAC_TRACE_DIRECT=1` now shows a 136,512-byte frame packet with three
   draws: the first glyph batch is 474 vertices (79 quads), textured with
   atlas handle `0x3`, TCC enabled, modulation, and blending. Its first vertex
   is at `(-0.021, 0.000)`, samples `(0.533, 0.916)`, and has RGBA
   `(127,127,127,128)`. The atlas and CLUT uploads reach TBP `0x1400` and CBP
   `0x1600`; both resolve successfully. Next compare draw/scissor/depth/blend
   state with the resulting framebuffer and test the indexed atlas values at
   these UVs. Do not replace the working path until a specific fault is
   demonstrated.
2. Correlate that glyph batch with the source DMA chain and the other two
   draws in the same frame. The first two VIF1 transfers are 736-byte DIRECT
   streams (transfer 1 also loads eight MPG blocks); the repeating worker
   transfers likewise report 736 DIRECT bytes and no MSCAL. `OPENRAC_VIF_TRACE`
   now accepts `OPENRAC_VIF_TRACE_LIMIT` to see later transfers. The renderer
   accepts channel-2 chains and VIF-produced GIF bytes; do not conflate the
   small repeating black packets with the later 136KB textured frame packet.
3. Trace the first scene object with real mesh data from its resource record
   through parent/world transform, camera culling and draw callback. Sampled
   update entries currently have zero translation and radius; prove their
   identity and role before changing culling.
4. Refresh the decomp hostability audit and prioritize unreadable units only
   when their functions are on these observed paths. Check call signatures and
   omitted register arguments against retail assembly.
5. Reproduce the intermittent DMA allocator abort before fixing it. One
   font-traced run aborted during vector reallocation in
   `openrac_native_renderer_submit_dma_chain`; a later font/DMA trace and a
   20-second `MALLOC_CHECK_=3` run did not reproduce it. If it recurs, capture
   DMA source ranges and run a sanitizer build. Do not apply an allocator
   workaround without evidence.

For a later framebuffer capture, `OPENRAC_DUMP_NATIVE_FRAME_SKIP=N` skips the
first N eligible renderer packets before writing up to eight PPMs. The default
is zero. The draw-state trace for this update is
`/tmp/sly-native-direct-textured-draws.log`. Existing machine-local PPMs were
captured before the glyph batch. No game data was committed.

All functional game replacements remain in
`port/game/sly1-us/hand/native_c/`, registered as backfills and removable when
matching C becomes hostable. Runtime and renderer corrections remain under
their respective port directories. The next progress gate is a verified
visible menu or scene image, not more global function coverage.

## Decomp snapshot check (2026-11-07)

The configured source checkout, `/tmp/sly1-decomp-current-check`, is at
`dd6f80b` (2026-11-06). Upstream `master` is now
[`04f7f272`](https://github.com/OpenThievius/sly1-decomp/commit/04f7f272552f4853fa251d9a12a75701546a2bd3),
149 commits ahead of that source snapshot. Its NTSC-U report lists 4,723 of
4,807 functions matched, 92.81% matched code bytes, and 92.24% matched data
bytes. Those totals use a changed denominator from the earlier report and
should not be compared as a simple regression/improvement.

Comparing the upstream function reports identifies three newly exact matches
since `dd6f80b`: `func_0013E650` (4 bytes), `func_0018B670` (176 bytes), and
`func_001FFC08` (120 bytes). The decomp's name notes identify `func_0018B670`
as a memory-card-only background job. `func_0013E650` is only a four-byte
function. `func_001FFC08` is a library function, and its relevance to the
active native frame path is not established. None currently displaces an
active Sly backfill or explains the black frame. The port build still uses the
older `dd6f80b` source snapshot; no decomp checkout was changed.

## Graphics investigation update (2026-11-07)

The frame dumper gained `OPENRAC_DUMP_NATIVE_FRAME_MIN_BYTES=N`, allowing a
capture to select the actual 136,512-byte glyph packet instead of earlier
small packets. The normal capture is still nearly black but contains faint
dark-green glyph blocks. Flat debug mode displays gray rectangles in the same
area, confirming rasterized geometry; `OPENRAC_DEBUG_T4_INDEX=all` makes the
sampled indices visible but leaves the glyph layout fragmented. The T4 upload
swizzle experiment and a diagnostic whole-texture V flip both produce
high-frequency noise in index-grayscale mode, while reversing the low/high
nibble order changes pixels but does not repair the fragmented menu. Preserve
the unflipped linear path while investigating within-image addressing. The raw
64-byte CLUT at `0x1600` contains low-green/low-alpha
RGBA words and matches the decoded Splash bytes. Verify upload CSM/CSA and
alpha interpretation against the retail packet before considering palette
changes. Evidence, command and limits are in
[`SLY_GRAPHICS_HANDOFF.md`](SLY_GRAPHICS_HANDOFF.md).

The latest font trace anchors ASCII `P` at `(u=273,v=43)`, size `16x41`,
logical screen `(313.2,246.4)`. Its emitted normalized top-left UV is
`(0.533,0.916)`, exactly the retail helper's V flip. This supports leaving the
matched font-layout behavior alone while atlas addressing/CLUT interpretation
is investigated.

Immediate graphics tasks:

1. Compare the known `P` glyph record and screen quad to the raw T4 pixels at
   its transformed UV rectangle. Explain the text fragments that remain in
   the linear index capture before changing atlas addressing.
2. Decode the font CLUT upload tag and its TEX0/TEXCLUT state, proving the
   16-entry ordering and alpha scaling independently of the image dump.
3. Add a renderer-only synthetic T4 texture/CLUT case behind a debug switch
   (solid opaque entries plus a simple glyph mask) to distinguish conversion
   errors from Sly's packet state; remove or retain it strictly as a diagnostic.
4. Once text is legible, return to the gameplay path: identify a renderable
   level object with real mesh ownership and transform, then fix the first
   demonstrated missing callback or scene adapter. Do not treat the font fix
   as a playable milestone.

## Direct-level probe follow-up (2026-11-07)

The `OPENRAC_BOOT_LEVEL` path had not been running: its port C definitions for
`func_001E4E80` and `func_001E4EA8` were listed as backfills, but hostgen chose
the matching decomp definitions. They are now also in the explicit override
registry, so the generated host file contains the native synchronous loader
and catalog lookup. The build succeeds and the generated report records both
overrides. This changes only the opt-in probe; ordinary startup remains on the
retail request path.

A fresh bounded native run with `OPENRAC_BOOT_LEVEL=0x0100` reaches the level
range at sector 867214 (4,911,835 bytes) and begins parsing it. The trace then
showed that `func_001EE930`, a value-list reader present in the decomp, was
selected as a no-op because retail metadata classifies its address as
assembly. A port-only backfill now implements its decomp-documented reads so
the object stream advances. This is deliberately kept outside matched C.

With that reader enabled, parsing advances into collider records and faults in
the collision mesh path. The latest trace reports a mesh with 6 vertices,
`0x880` faces and 22 links, with only about 5.8 KB left in the current stream;
the face loop then reads beyond valid guest memory. Determine whether those
counts come from a bad stream position/decode or a defect in the port-only mesh
reader before changing counts or skipping geometry. Earlier animator and
collider callback failures were observed in a run where the value-list reader
was still a no-op; they are not the current first failure. The animator probe
override remains behavior-equivalent to the decomp and logs serialized types
when `OPENRAC_TRACE_LOADER` is enabled.

Reproduce with:

```sh
OPENRAC_BOOT_LEVEL=0x0100 OPENRAC_STARTUP_TRACE=1 OPENRAC_TRACE_LOADER=1 \
timeout 12s build/port/sly1_native build/port/extracted/boot/SCUS_971.98 \
  '/home/reaper/pcsx2/Games/Sly Cooper and the Thievius Raccoonus (USA).iso'
```

The latest machine-local trace is `/tmp/sly-native-value-list-probe.log`. No
scene geometry or playable level is established by reaching this parser path.
