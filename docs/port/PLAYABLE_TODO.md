<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Concrete plan for a playable native Sly 1

Fresh repository review: 2026-11-07. The current local decomp input is
`e1b39f1` from `/home/reaper/Code/sly1-decomp`, and CMake uses that checkout
for matched C and retail assembly. The latest build translates 2,044 C
functions and 132 functions from US retail assembly, has zero stubs, and
reports 2,682 functions without C plus 12 unreadable units. A bounded native
run now completes Splash and the selected level resource through loader stage
5, with active world `0x0088d530`. This supersedes the earlier empty-reader,
heap-initialization, and child-parser faults below.

The port now uses exact retail assembly translations for the animation table
loader (`func_001B3040`), its element parser and class lookup helpers. The
previous reconstructed table parser was removed. Its remaining factory guard
is a separate port-only override that skips unsupported animation types while
consuming their fixed payload. `sly1-decomp` remains untouched. The level still
produces zero scene vertices and the window is black; reaching stage 5 proves
resource parsing/activation progress, not visible graphics or playability.

Retail-backed static translation of `func_00162890` succeeded in an isolated
hostgen experiment. Its instruction stream confirms that nested-array sizes
include the one-based entry index and that both dimension payloads multiply
the full base payload byte count by the dimension and 12. The port-only C
backfill now follows those formulas. The latest launcher run never reached the
function, so this parser correction remains unvalidated at runtime.

The resource teardown/dispatch path now uses exact US assembly for
`func_001C0808`, `func_001BEE48`, `func_001E9688`, and the missing-assembly
dependencies reached from them. A fresh 12-second direct-level run remains
stable at stage 5 but still reports zero scene vertices, so these lifecycle
callbacks were not the missing level-mesh path. Trace:
`/tmp/sly-native-lifecycle-asm-closure.log`.

The menu glyph packet is known to be valid through texture lookup, but the
displayed frame remains black/garbled. An earlier build reached child-object
dispatch and faulted at `0xFFFFAC70`; that checkpoint used a different decomp
snapshot and is superseded by the earlier-described current failure. This
document is an ordered work plan, not a claim of playable progress. Work is
active on
wip/native-sly-port and stays in the port repo.

A 2026-11-07 bounded trace widened the opt-in world-list callback sample from
8 to 64 records. It captured all 12 entries: each dispatches through
`func_00125C90`, and each sampled local basis is identity. No additional
render callback appeared; sampled frames still report zero scene vertices.
This rules out a trace cap hiding a different callback, but object ownership
and the reason these entries do not submit geometry remain unknown. The
diagnostic is enabled only with `OPENRAC_TRACE_FONT=1`.

A new bounded direct-level trace records the child list, mesh pointer, extra-data
pointer, and sphere radius for those same update-list entries. All sampled
entries have zero in all four fields. They are empty infrastructure objects,
not cullable mesh owners, so forcing visibility or synthesizing bounds is the
wrong fix. The native frame remains state 3, game mode 0, and menu page `-1`,
so this direct-load probe has not reached a playable state. The retail state
transition and resource callbacks that create renderable level objects remain
unknown. Trace that path, then verify a real mesh pointer reaches the draw
queue.

**Class ownership follow-up (2026-11-07):** the two classes in that update-list
sample are now attributable. Class `0x1e` nodes are created by the world setup
path (`func_001DCEA0`); class `0x51` nodes use descriptor `D_00217B60` and are
ConeVolume instances created through `func_001E6188`. The opt-in registration
trace records owner, parent, mesh, attachment, extra-data, and radius fields.
For the direct-loaded level, all ten class-`0x1e` nodes and both class-`0x51`
nodes are root registrations owned by world `0x0088d530`, with no parent and
zero mesh/attachment/extra-data/radius values. These are world bookkeeping and
culling-volume entries, not candidates to force through the scene renderer.
This closes the immediate question about the sampled lists but does not find
the level's renderable-object table. Continue from the level activation/resource
object path rather than expanding these update-list callbacks. Diagnostic log:
`/tmp/sly-native-class-owner-trace.log`.

The earlier reader-window comparison was invalid: `0x70000000` is EE
scratchpad, but the dump helper normalized it as a main-RAM alias. The helper
now reads the scratchpad mapping and captures both the cursor window and the
full 8 KiB decoder ring. The latest level capture has cursor `0x70000c9c`,
`0x1364` bytes remaining, and 1,171 nonzero bytes in the ring. No exact 8 KiB
match was found in the offline-decoded resource, but the decoder's logical
output offset and ring-history boundary have not been captured, so that result
does not yet prove a decompression mismatch.

The native level loader also no longer pre-reads the entire compressed range
or overwrites reader fields `+0x20/+0x24`. Exact retail `func_00137490` now
refills the configured ring through the port's synchronous sector adapter. A
bounded run reads Splash as two 37-sector chunks and level `0x0100` as four
599-sector chunks plus a final 3-sector chunk, then still reaches loader stage
5 with zero scene vertices. This removes duplicate full-range I/O and keeps
the retail ring state intact; it does not yet create a drawable level object.
Trace: `/tmp/sly-native-reader-ring-trace.log`; dumps:
`/tmp/sly1-level-reader-window.bin` and `/tmp/sly1-level-reader-ring.bin`.

The old port-only `func_0011ECB8` override was removed after checking the exact
US assembly. It reads per-object callback records; its count is not the scene
object or mesh count. The speculative parser remains under `hand/superseded/`
for trace history only. The previously reported 6-vertex/0x880-face collision
record came from the earlier Splash resource (sector 820372), not the selected
level resource (sector 867214); do not use it to diagnose the level mesh.

The level resource header's first word is also no longer treated as a scene
offset: `func_001DB928` passes it to `func_0018D500` as an allocation size and
stores the second word as the resource revision. For key `0x0100` these are
`0x00240cd0` and `0x3d687640`. The next format trace should follow the allocated
table at `D_00275718` through its consumers; the current object-record list is
empty, and the table's relation to scene geometry remains unproven.

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
- The current hostgen report reads 185 units, translates 2,051 C functions
  and 71 from assembly, lists 2,742 functions without C, and has 12 unreadable
  units. The build allows those units to be omitted, so it is not a sound
  gameplay baseline yet.
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

That earlier Splash fault was fixed sufficiently for the loader to reach the
frame loop and menu callback. The direct-level probe is a separate diagnostic
path: the current trace completes `func_00162890` and `func_00128C10`, then
faults during the child-object read path (`func_0011ECB8` / `func_00137D10`)
near reader cursor `0x70000641`, with guest address `0xFFFFAC70`. The full
object parser input window matches the extracted decoded resource at both
entry and exit, so that parser's stream cursor is not presently the first
demonstrated mismatch. The launcher remains black/garbled and is not playable.

The current menu evidence narrows the independent graphics issue: 79 textured
glyph quads decode with valid nonzero UVs, atlas handle `0x3`, and a resolvable
atlas/CLUT, but no recognizable pixels reach the visible window. This points
to the native draw/framebuffer path after packet decoding. Do not conflate that
with the direct-level loader fault.

## Immediate next steps

1. Identify a level object in the active world lists that owns a mesh, then
   trace its descriptor, constructor, activation callback, mesh vertex pointer
   and render callback. Current runs show populated update lists but zero scene
   vertices, so first establish which entries are actual renderables.
2. Follow that object's render callback into the native scene submission path.
   Capture the first nonzero vertex stream and its camera/material state, then
   fix the earliest missing or invalid boundary before adding renderer
   workarounds.
3. Investigate the missing font registry as a separate UI path. The direct
   level resource has no registered fonts in the latest trace; this does not
   explain why the scene has zero vertices.
4. Keep the direct-level shortcut as a diagnostic. Once geometry is visible,
   restore the normal title-to-New-Game transition and verify movement,
   collision, camera, input, audio and checkpoint behavior with human input.
5. Close the 12 unreadable build units and audit omitted call arguments, but
   prioritize functions proven reachable from the selected level's draw path.

One additional retail-assembly comparison found and fixed a port-backfill
layout error: `func_00162890` stores its component references as 32-bit values
at four-byte strides. The C backfill had been writing 16-bit values at two-byte
strides despite allocating the correct 4-byte-per-entry array. This does not
change how many bytes the reader consumes, so it is a memory/layout correction,
not yet a fix for the child-read fault. Keep it in the port backfill so it can
be replaced cleanly when the decomp supplies that function.

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

## Historical: instrument the callback-list reader (2026-11-07; superseded)

The investigation below predates the retail-assembly correction at the end of
this document. `func_0011ECB8` was misidentified as an object-record parser;
its count and alignment traces do not describe the level's scene-object list.

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

An earlier trace of the Splash resource (sector 820372) reached the collider
mesh path with 6 vertices, `0x880` faces and 22 links, then read beyond the
valid stream. That malformed record is not evidence about the selected level
resource: its range is sector 867214, and the current trace reaches object
record count zero without entering the collision parser. Keep the Splash
collision issue separate until it is reproduced with a valid, attributable
mesh. The animator probe override remains behavior-equivalent to the decomp
and logs serialized types when `OPENRAC_TRACE_LOADER` is enabled.

Reproduce with:

```sh
OPENRAC_BOOT_LEVEL=0x0100 OPENRAC_STARTUP_TRACE=1 OPENRAC_TRACE_LOADER=1 \
timeout 12s build/port/sly1_native build/port/extracted/boot/SCUS_971.98 \
  '/home/reaper/pcsx2/Games/Sly Cooper and the Thievius Raccoonus (USA).iso'
```

The latest machine-local trace is `/tmp/sly-native-value-list-probe.log`. No
scene geometry or playable level is established by reaching this parser path.

## Stream and heap follow-up (2026-11-07)

The next blocker was earlier than the collider report above: unreadable unit
`00134000` left the core buffered-reader functions as no-ops. The port build
now translates `func_00137A60`, `func_00137B90`, `func_00137BE0`,
`func_00137C40`, `func_00137CC0`, `func_00137D10`, `func_00137D78`,
`func_00137820` and `func_001378A0` from the retail US assembly. They remain
generated assembly fallbacks in the port build; no decomp source was changed.
Hostgen now reports 2,051 C functions, 84 assembly translations, zero stubs,
2,729 functions without C, and 12 unreadable units.

After those translations, the startup reader consumed 8,184 bytes from the
Splash resource. The next failure is allocator initialization: the port-only
`func_0018D500` saw `D_002C2A1C = 0x160` and heap limit `0`, so allocations of
390,384 and 9,056 bytes failed. The run then faulted at host address `0x8`.
This confirms the no-op byte reader was one blocker, while the guest heap is
now the first demonstrated blocker. The parser-size changes above remain
unverified at runtime.

Concrete next tasks:

1. Trace the retail initialization path that should set `D_002C2A14`,
   `D_002C2A18` and `D_002C2A1C`; determine why the native start reaches
   `func_0018D500` with a bogus bump pointer before changing allocator policy.
2. Check whether the missing initializers live in one of the 12 unreadable
   units or are skipped by the native startup path. Prefer translating the
   relevant retail functions or fixing startup wiring over inventing heap
   bounds.
3. Re-run the direct-level probe and verify a nonzero guest allocation,
   advancing stream cursor, and parsed object count before returning to the
   collider mesh count mismatch.
4. Keep any required behavior backfill under `hand/native_c/` and registered
   in the port backfill/override tables. Replace it with matching decomp C
   when available; never fold it into generated or upstream decomp source.

Latest bounded run log: `/tmp/sly-native-reader-asm-current2.log`. The native
launcher remains non-playable; this run rendered no level or controllable
character.

## Heap initialization and level-resource progress (2026-11-07)

Retail `func_0018D430` was the missing heap initializer. Its US assembly aligns
the loaded-image end to get the heap base, sets the limit to `0x02000000`, and
initializes `D_002C2A14`, `D_002C2A18` and `D_002C2A1C`. The port build now uses
that assembly fallback, so allocator requests succeed and the configured
direct-level probe reaches sector 867214 (4,911,835 compressed bytes).

The current build uses 2,051 matched C functions, 105 retail assembly
translations, zero stubs, 2,708 functions without C, and 12 unreadable units.
Additional retail translations cover the missing float/resource readers and
the level callback path. The latest 12-second native run stays alive through
the timeout and advances the selected level resource through loader stages
3–5. It constructs an active world, but reports no registered fonts, no
visible text, and no rendered scene mesh. The trace also reports a null font
vtable call in `func_0016BA40`; it is nonfatal in this run. A port-only
`func_001ACDA0` guard skips its retail font callback only while font slot 0 or
its vtable is absent. With a valid font, it follows the original decomp logic.

An experiment enabling retail assembly `func_001B3510` faults on guest address
`0x0C1AE80F` from the level callback path. That function remains unselected
until its invalid input is explained. The complex animation renderer is not
considered working. The complete direct-level trace is
`/tmp/sly-native-stable-probe.log`; the launcher command is:

```sh
OPENRAC_BOOT_LEVEL=0x0100 OPENRAC_STARTUP_TRACE=1 OPENRAC_TRACE_LOADER=1 \
timeout 12s build/port/sly1_native build/port/extracted/boot/SCUS_971.98 \
  '/home/reaper/pcsx2/Games/Sly Cooper and the Thievius Raccoonus (USA).iso'
```

Next tasks:

1. Trace the loader path that populates `D_00262268[]` and `D_0026227C` from
   the Splash/font resource. The UI font callback sees an empty registry even
   after the resource reader advances; find the missing initializer or the
   earlier parser field that prevents registration.
2. Confirm the Splash and level readers consume the correct byte ranges. The
   trace shows small remaining counts (`268` and `5168`) and large heap growth;
   compare each cursor and allocation against the retail routine before
   treating stage 5 as a complete parse.
3. Follow the first level mesh allocation into the native packet path. Require
   nonzero decoded vertices and a nonempty scene draw before claiming a visual
   level.
4. Diagnose why translated `func_001B3510` receives invalid guest data. Check
   its caller inputs and all resource records before re-enabling that
   animation path.
5. After a scene mesh renders, verify Sly spawn, collision, movement, camera,
   controller input, and level transitions. A level-resource load is not a
   playable milestone.

All C compatibility code stays in `hand/native_c/` and the override table;
retail assembly remains a configured native fallback. Neither changes
`sly1-decomp`. The launcher is still not playable.

## Retail animation-parser checkpoint (2026-11-07)

The handwritten `animation_table_loader.c` guess was removed from the port
backfill registry. Hostgen now translates the exact US retail assembly for
`func_001B3040`, `func_001B2F00`, `func_001B4920` and `func_001B4980`; its table
helpers `func_001B2BA8` and `func_001B2CF8` already have matching C. The
port-only `animator_loader_probe.c` factory guard consumes the fixed 0x1C-byte
record payload and skips unsupported types rather than calling a null vtable.
All of this is port-side; the decomp checkout is unchanged.

The latest 12-second native run reaches Splash stage 14 and the selected level
stage 14, and keeps the level world active at `0x0088d530`. Animator records
now show valid retail types 1, 4, 6, 7 and 8 with nonzero objects and vtables.
The runtime remains black: the UI reports 37 global UI objects but zero fonts
and zero visible text, while the renderer reports zero scene vertices. The
next graphics milestone is the first attributable mesh callback producing a
nonzero vertex stream, not merely an active world or populated update lists.
Hostgen reports 2,043 C functions, 109 retail assembly translations, zero
stubs, 2,705 functions without C and 12 unreadable units. The bounded trace is
`/tmp/sly-native-retail-anim-loader4.log`.

## Retail callback and DMA trace correction (2026-11-07)

The old port-only `func_0011ECB8` parser is no longer in the active build. Exact
US assembly confirms it reads per-object callback records; its zero count was
not a scene-object or mesh count. The speculative parser and its unverified
alignment heuristic are retained only under `hand/superseded/`. The native
build now selects exact retail assembly for `func_0011ECB8`, `func_001AD970`,
and `func_00202C38`; the last two were active runtime functions. Hostgen reports
2,044 C translations, 111 assembly translations, zero stubs, 2,703 functions
without C, and 12 unreadable units.

A 10-second direct-level run reaches loader stage 5 and keeps world
`0x0088d530` active, but still reports frame state 3, game mode 0, zero scene
vertices, no registered fonts, and no visible text. The active world list
counts are `1,c,c,a`; the root object has no children. This remains a black,
non-playable frame.

`OPENRAC_VIF_TRACE=1` captures a 48-byte VIF1 chain sourced from `0x00212110`.
Its first DMA segment is an inline CNT tag with QWC 3, followed by END. The VIF
stream contains opcode `0x40` at offset `0x14`. The [PS2SDK VIF command table](https://ps2dev.github.io/ps2sdk/vif__codes_8h.html)
does not define `0x40` as a command. Retail `func_002018E8` writes
`VIF1_ERR=2`, leaving the invalid-command error unmasked according to the
[EE User's Manual, VIF1_ERR](https://usermanual.wiki/Pdf/EEUsersManual.1748563585.pdf). Determine
whether this is an intentional error probe or a malformed startup packet
before changing native VIF behavior. It is a graphics-initialization lead, not
proof that the level mesh path has been reached.

Latest traces: `/tmp/sly-native-vif-failure-context.log` and
`/tmp/sly-native-dmawait-retail.log`.

## Retail world initialization follow-up (2026-11-07)

Selected exact NTSC-U assembly for `func_00151D28`, `func_001BEB10`, and
`func_001DCD28`, which the port-only level loader calls during resource setup
and world traversal. A fresh 12-second direct-level run still reaches stage 5
with world `0x0088d530`, but produces zero root children and zero renderer
vertices, with no fonts or visible text. These translations remove no-C stubs
but do not yet reach the scene mesh path. The trace is
`/tmp/sly-native-retail-init-path.log`; current hostgen coverage is 2,044 C
functions, 114 retail assembly translations, 2,700 functions without C, and
12 unreadable units. Decomp checkout remains unchanged at `e1b39f1`.

## Native font-initialization follow-up (2026-11-07)

The normal-startup trace showed 37 global UI objects but an empty font
registry. Retail assembly for `func_001B40C8` clears and initializes the font
and image registries and their supporting pools; hostgen had left this
function as a no-op. The native build now selects this exact US retail
translation. The same trace exposed `func_001B2DF8` as another reached no-C
stub. Its retail routine reads the font-table count and calls `func_0015D8A8`
for each font record, which writes entries into the font registry. Both are
now selected from exact US retail assembly, along with `func_001B40C8`. The
build reports 2,044 C functions, 117 assembly translations, zero stubs, 2,697
functions without C, and 12 unreadable units.

A fresh 12-second native startup run no longer reports `func_001B2DF8` as a
no-op, confirming it runs, but still ends with font count 0, zero visible
text, and zero scene vertices. The remaining font issue is in the table input
or its count, or a later reset; trace that boundary before changing rendering.
Trace: `/tmp/sly-native-font-parser-assembly.log`. The decomp checkout remains
unchanged; all C compatibility code stays in the port backfill directory.

**Follow-up trace correction (2026-11-07):** `0x70000000` is the port's mapped
resource window, not an invalid pointer. In a fresh normal start, the Splash
resource enters `func_001B3040` at cursor `0x70000089` with 8,055 bytes left;
its font-table reader is reached after the stream is exhausted. This does not
establish a broken font pointer or prove that this Splash resource contains a
font table. A direct level `0x0100` run has 4,964 bytes left after
`func_001B3040` and 1,680 bytes after world setup, yet still reaches the frame
loop with zero vertices. Treat missing Splash fonts as a separate UI issue;
the first scene-graphics blocker remains ownership and activation of a
renderable level object.

That direct-level run also reached three resource animation callbacks that
were still no-C stubs: `func_001B5060`, `func_001B5200`, and `func_001B5398`.
The native host build now translates their exact NTSC-U retail assembly; the
decomp checkout is unchanged. The callbacks no longer emit stub warnings, but
the level root still has no children and the renderer still reports zero
vertices. Trace: `/tmp/sly-native-direct-level-anim-callbacks.log`.

**Graphics-callback follow-up (2026-11-07):** exact US assembly is now also
selected for `func_001B4330`, a color conversion helper reached by the level
resource path. A 12-second direct-level run survives and remains at zero scene
vertices. I also tried selecting `func_001B3510`, a text/packet routine, but
the normal Splash load faults in its `func_001B29E0` / `func_001B2988` table
lookup before the level loads. That selection has been removed. The missing
prerequisite is the table state expected by this retail routine; do not enable
it again until its initialization path is established. Trace:
`/tmp/sly-native-direct-level-graphics-callbacks.log` (faulting attempt) and
`/tmp/sly-native-direct-level-color-callback.log` (current successful run).
The current hostgen report has 2,044 C translations, 121 assembly
translations, zero C stubs, and 2,693 functions without C.
