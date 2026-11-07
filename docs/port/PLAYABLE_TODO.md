<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Concrete plan for a playable native Sly 1

Fresh repository review: 2026-11-07. The current local decomp input is
`e1b39f1` from `/home/reaper/Code/sly1-decomp`, and CMake uses that checkout
for matched C and retail assembly. The latest hostgen run reads 185 units,
translates 2,043 C functions plus 199 from US retail assembly, and reports
zero stubs. A bounded native run completes Splash and the selected level
resource through loader stage 5, with active world `0x0088d530`. The exact
retail `func_001EE900` constructor now links the level's class-`0x86` object at
`world+0x1ec4`; its exact transform helpers are also selected. The frame still
reports zero scene vertices, frame state 3, and game mode 0. The port is not
playable.

**Retail object-reader closure (2026-11-07):** the latest US assembly
allowlist also selects reached resource and object routines
`func_001240F8`, `func_0011F850`, `func_0012BC38`, `func_00124868`,
`func_001245A8`, `func_00164868`, `func_001587C8`, `func_00124A48`,
`func_001D7040`, `func_0017FEC0`, `func_001F4500`, `func_001478F0`,
`func_001AE820`, and `func_001F6B40`. `func_001240F8` previously used a
port-only C reconstruction; its source remains guarded in `hand/native_c`,
and it is removed from the backfill registry while retail assembly is active.
These additions let default Splash startup reach the main update loop with
five valid world root objects, including classes `0x8e` and `0x7d`, without a
logged guest call fault in the last bounded run. The run reports frame state
3, game mode 0, zero scene vertices, and 416 accepted GIF chains at its final
trace; the window remains black. A direct `OPENRAC_BOOT_LEVEL=0x0100` probe
still creates a root object with type `0xffffffff` and faults on unmapped
address `0xFC000120`, so direct-level loading is not fixed by this default
boot progress. Trace: `/tmp/sly-native-frame-callbacks.log` and
`/tmp/sly-native-direct0100-reader-closure.log`. Latest hostgen coverage is
in `build/port/hostgen/us/report.json`.

**Direct-loader closure experiment (2026-11-07):** adding another 37 reached
functions with US assembly raised the allowlist to 236 and made the direct
`0x0100` probe fault earlier in `func_00147AB0` while dispatching an unset
resource callback. Adding 51 further startup functions raised it to 287 and
left boot in the early kernel wait with no active world. Both expansions
were reverted. The retained 199-function allowlist was rebuilt and a fresh
default run again reached the update loop, with frame state 3 and zero scene
vertices. Do not treat these broader static translations as safe merely
because hostgen can translate them; restore them only after tracing the
specific callback state they require. Experiment logs:
`/tmp/sly-native-direct0100-callback-closure.log`,
`/tmp/sly-native-direct0100-expanded-closure.log`, and
`/tmp/sly-native-default-199-asm.log`.

**Reader-chain checkpoint (2026-11-07):** Exact US retail assembly is now
selected for `func_001BAC30`, `func_00162890`, `func_001DCEA0`,
`func_00196410`, `func_001B9AE0`, `func_0012B870`, `func_0012B8C8`, and
`func_0013B4C0`. The first three previously had port C backfills; their source
files remain guarded in `hand/native_c`, while they are no longer registered
in `backfills.tsv`, so the replacement code stays separate from retail
behavior. Switching to exact readers moved the common object parser through a
valid short record to cursor `0x70001595`, then later to `0x70001ee8`, rather
than misreading a count and consuming the remainder of the Splash data. The
next nested child-list path still faults at a call through zero after class
`0x83`. The latest bounded run still reaches frame state 3/game mode 0 with
zero scene vertices and two accepted GIF chains; it is not playable. Rebuild
with `cmake --build build/port --target port_native -j2` after changing
hostgen configuration; building only `sly1_native` does not regenerate
translations. Trace: `/tmp/sly-native-restart-check.log`; generated coverage:
`build/port/hostgen/us/report.json`.

**VIF error-mask correction (2026-11-07):** commit `f8fa708` attempted to
continue past the startup packet's unknown VIF opcode `0x40` when ME1 was
masked. Rechecking the retail write and the EE VIF1_ERR bit definitions shows
that Sly writes `2`, which sets ME0 and does not set ME1 (bit 2). The captured
packet therefore still takes the parser's unsupported-command path; the
workaround did not activate and must not be treated as a graphics fix. The
mask-based bypass has been removed. This packet remains a separate startup
error-handling question, not evidence for the zero-vertex scene-rendering
blocker. Trace: `/tmp/sly-native-vif-mask-fix.log`.

Since the previous pushed checkpoint, the US assembly allowlist gained 27
reached routines. These remove more missing-C warnings without adding
port-only behavioral backfills. The latest native trace still shows active
threads and accepted GIF chains, but zero scene vertices; default boot remains
at frame state 3/game mode 0. The next graphics task remains identifying the
retail title transition and the owner/class callbacks that produce visible
scene submissions, rather than treating the first VIF error as the mesh path.

The rebuilt default-boot trace also reports a call to guest address zero from
retail `func_0011ECB8`. Its selected assembly unconditionally calls the
record-created object's virtual reader at vtable offset `+0x38` immediately
after the factory returns. The current trace does not identify which record or
why that slot is empty; instrument the factory result and vtable slot at this
dispatch before changing its behavior. The run still reports zero scene
vertices, frame state 3, game mode 0, and no visible graphics. Trace:
`/tmp/sly-native-vif-mask-corrected.log`.

**Earlier graphics checkpoint (superseded by the owner-list trace below):** a full object snapshot of the
direct-loaded world found two class `0x44` instances, but these are collision
objects, not visible actors. Both have null mesh data at `+0x380` and a zeroed
render mesh copy at `+0x3A4`; the world still submits zero scene vertices.
The latest run found a class-1 descriptor at `0x00217418` with a valid
`func_001833D0` record reader and populated method slots 7 and 20. Its method
slot 19 is null, but the matched `func_00182ED8` checks that slot before use.
An indirect call still reaches address zero during the record path, so its
source remains unresolved. Parsing continues through loader stage 5; this is a
confirmed defect to trace, not proof that the world is fully initialized.
The port has not reached the first actual game image. Next trace the null call
through class-1 nested record parsing, then find the title/level transition and
the record path that creates renderable objects. Trace:
`/tmp/sly-native-class1-vtable.log`.

The resource descriptor reader `func_0011E680` now lives only in
`hand/backfills.tsv` and `hand/native_c/resource_descriptors.c`; it is no longer
registered as a function override. This keeps the functional C backfill
separate from matched decomp code so it can be replaced cleanly when that
function is decompiled upstream. The collision-contact pointer correction is
also port-only and follows the retail load of the table handle.

The current change set selects seven additional functions from exact NTSC-U
assembly: `func_00183350`, `func_00139DE8`, `func_001249A8`, `func_00124338`,
`func_00127378`, `func_00139EC8`, and `func_001280C0`. They close reached
constructor, activation, cleanup, message, and VU0 callbacks without changing
the decompilation or adding functional replacement C.

**Fresh default-boot check (2026-11-07):** a 12-second native launch without
`OPENRAC_BOOT_LEVEL` reads only the Splash range (sector 820372, 150849 bytes).
It stays in frame state 3/game mode 0, keeps the menu page at `-1`, registers
no fonts, and reports zero visible scene vertices. This means the direct-level
probe is not the only blocker: normal startup also fails to transition from
Splash into the intro/title flow. Next, trace the retail state/movie request
that should follow Splash while separately preserving the level-resource probe
for scene-object work. Trace: `/tmp/sly-native-default-graphics-check.log`.

The first rejected VIF1 startup chain is reproducible from the US ELF itself:
the static bytes at `0x00212110` contain the 48-byte CNT payload and opcode
`0x40` at stream offset `0x14`, exactly matching the native VIF diagnostic.
This rules out ISO-sector corruption as the source of that packet. Retail
startup writes `2` to `VIF1_ERR`: that masks ME0 (DMA-tag mismatch), while ME1
(invalid-command stall) is bit 2 and remains unmasked. The native parser's
stop at opcode `0x40` is therefore consistent with an unmasked invalid VIF
command; this packet still has not been tied to the missing scene callback.
Trace: `/tmp/sly-native-default-vif-trace.log`.

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

Two more runtime-reached no-C leaf functions now use exact US retail assembly:
`func_001363D0` (the child cleanup callback reached during level initialization)
and `func_00160D98` (the retail constant-return helper used by startup code).
Their no-op warnings are gone. The rebuilt native run still reaches level
loader stage 5 with an active world and zero scene vertices, so these exact
leaf fixes remove incorrect behavior but do not yet connect level data to a
renderable object. Trace: `/tmp/sly-native-asm-cleanup-trace.log`.

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

**Restart checkpoint — frame-list corruption trace (2026-11-07):** the prior
frame-queue hypothesis was disproven. Matched decomp `func_0015EC68` already
copies and clears all three source queues correctly. Port-only trace
reconstructions of `func_00139EC8` and list append `func_00152470` show the
global frame list at `0x002622E0` has consistent head/tail and node `+0x630`
links after every append and after the `func_001243E0` callback. Later,
`func_00152600` removes the original node `0x0082f9b0` and sees corrupted
next/previous values `0xbf343386` / `0xbef492b5`, faulting at
`0xbef498e5`. The corruption therefore occurs after the transfer callbacks
and before frame-context cleanup; its writer is still unidentified. Latest
trace: `/tmp/sly-native-frame-head-chain.log` (failure near end). Current
uncommitted code puts the two C diagnostics in port-only override/backfill
files; it does not edit `sly1-decomp`. Next, inspect the calls between the
last valid frame-list trace and `func_00152600`, identify the first bad write,
then fix the underlying port fault. The latest build succeeded (2,040 C
functions translated, 206 retail assembly functions, zero stubs); a bounded
native launch still faults and shows no graphics. No emulator was used.

**Display worker correction (2026-11-07):** the frame worker was reaching
`func_0015F320`, but hostgen had no implementation for it and supplied a
no-op. Added the exact US retail assembly function to the assembly allowlist;
the old unlinked handwritten C prototype was removed because it passed the
wrong packet capacity and was not a safe substitute. Rebuilt successfully:
190 C units, 2,040 C functions, 207 retail assembly functions, zero stubs.
A 15-second native run completed its window without the previous list fault.
The instrumented run inserted and drained 40 frame-list nodes; every recorded
`+0x630` link was valid through cleanup. By the final trace, the renderer had
accepted 1,461 GIF chains and submitted 240 vertices (117 inside the viewport).
The UI trace still reports zero visible text, and this does not establish that
the game title or a playable scene is visible. Next, diagnose why ordinary
startup remains at frame state 3/game mode 0 with menu page `-1`, then confirm
visible title/scene output. Run log: `/tmp/sly-native-retail-15320-trace.log`.

**Black-frame capture and object-reader assembly (2026-11-07):** renderer
capture confirms the window is still black: all eight dumped 960x720 frames
contain one color (black). Runtime diagnostics report frame state 3, game mode
0, menu page `-1`, and zero visible text. The 240 submitted vertices are not
evidence of visible graphics. `func_0011ECB8` was another reached no-op despite
having US retail assembly; selecting that assembly removes its warning and
keeps startup running, but does not change the black result. A 15-second trace
drained all 40 queued frame nodes with valid links and no list fault. The
current hostgen selection builds 2,040 C functions and 208 US assembly
functions, zero stubs. Next trace the title/movie/state transition and identify
which reached no-op or missing resource prevents entry into the title scene;
do not treat rendering the frame-transfer rectangle as a scene. Evidence:
`/tmp/sly-native-retail-11ecb8.log`, `/tmp/sly1-native-frame-00.ppm` through
`/tmp/sly1-native-frame-07.ppm`.

**Retail state-dispatcher check (2026-11-07):** the handwritten
`func_001E9108` C backfill was moved to `hand/reference_c/` and the exact US
assembly selected. This preserves the comparison source while the runtime
executes vanilla instructions. The clean build now has 2,035 C functions,
209 retail assembly functions, and zero stubs. A fresh bounded run still
reports frame state 3/game mode 0, menu page `-1`, and zero visible text; its
eight captured frames remain black. Thus the C dispatcher was not the only
cause. The state data reaches the retail dispatcher with zero flags, and no
pending movie/resource request is visible in the trace. Next inspect who
initializes the state data and queues the initial title/intro request, plus
the reached missing callbacks in that path. Trace:
`/tmp/sly-native-retail-state-dispatch.log`.

**Resource-table assembly experiment (2026-11-07):** enabling exact assembly
for `func_0011D140`, `func_0011D968`, and `func_0011DB00` alone stopped startup
before an active world was created. The loader thread remained in its early
poll at `func_0011D078`. Adding exact assembly for that poll routine and its
missing `func_001F8480` dependency restores the active-world loop. The
dependency-closed set is retained. It removes those reached no-op warnings,
but the bounded run still reports frame state 3/game mode 0, zero scene
vertices/text, and all-black captured frames. Trace:
`/tmp/sly-native-resource-poll-closure.log`; the standalone failure is at
`/tmp/sly-native-resource-table-asm.log`.

The current hostgen build has 2,035 C functions and 214 US assembly functions,
with zero stubs. It drains 40 frame-list nodes with valid links; this is more
reliable startup, but still not visible graphics or playability.

**Object-key callback restoration (2026-11-07):** added exact US assembly for
the reached no-op `func_001837E0`, which performs resource-key lookup and
queues a callback. Its direct callees were already available, so this
single-function addition rebuilt cleanly and removed its no-op warning. A new
15-second run remains in state 3/game mode 0 with zero visible text. It drains
40 frame-list nodes cleanly, but both sampled native frames are still all
black. The assembly path is restored, but it has not yet produced a visible
title or scene. Trace: `/tmp/sly-native-object-key-asm.log`. Current count:
2,035 C functions / 215 US assembly functions / zero stubs.

**Class-`0x4d` display-packet path (2026-11-07):** selected exact US assembly
for `func_00139B70` and its missing packet helper `func_001612F8`. These
functions build the GS packet used by the repeated class-`0x4d` objects. The
native run remains stable and drains all 40 frame nodes with valid links, but
the capture is still all black; frame state is 3/game mode 0, visible text and
scene vertices are zero. GIF DMA chains are accepted, so remaining work is
upstream of useful scene submissions or in interpreting their packet content.
Trace: `/tmp/sly-native-class4d-gs-packet.log`. Current count: 2,035 C / 217
US assembly / zero stubs.

**Scene packet-builder translation (2026-11-07):** the runtime had also been
resolving `func_001662A8` as a no-op even though its matching source still has
`INCLUDE_ASM` and the US retail body is available. Selected that exact
assembly. The older port C prototype is retained in `hand/reference_c/` but
removed from the runtime build to avoid competing implementations. The
15-second run stays stable and drains all 40 frame nodes, but still reports
state 3/game mode 0, zero scene vertices/text, and black captured frames. The
builder's presence alone is insufficient; establish whether a drawable scene
record reaches it and whether its output is consumed. Trace:
`/tmp/sly-native-scene-packet-asm.log`. At that checkpoint: 2,035 C / 218 US
assembly / zero stubs.

**Scene packet helper closure (2026-11-07):** exact US assembly for the
scene-packet builder's no-op helpers `func_001EE3B8` and `func_001EE668` is
now selected alongside `func_001662A8`. The 15-second run remains in the
active-world loop, drains 40 frame nodes cleanly, and accepts its GIF chains,
but reports zero visible text/scene vertices and produces all-black renderer
captures. The packet-builder path is now present through its immediate helper
calls; inspect the table counts/record pointers passed into `func_001662A8` and
whether the loaded Splash data contains drawable records before adding more
assembly. Trace: `/tmp/sly-native-scene-packet-helpers.log`. Current count:
2,035 C / 220 US assembly / zero stubs.
bounded run reads Splash as two 37-sector chunks and level `0x0100` as four
599-sector chunks plus a final 3-sector chunk, then still reaches loader stage
5 with zero scene vertices. This removes duplicate full-range I/O and keeps
the retail ring state intact; it does not yet create a drawable level object.
Trace: `/tmp/sly-native-reader-ring-trace.log`; dumps:
`/tmp/sly1-level-reader-window.bin` and `/tmp/sly1-level-reader-ring.bin`.

The old port-only `func_0011ECB8` override was removed after checking the exact
US assembly. That assembly reads a counted object list: each record supplies a
class slot, key, and flag, then the factory creates the object and its virtual
reader consumes its payload. `func_0011EC40` is the separate property/callback
ID reader. The speculative object parser with its guessed alignment heuristic
remains under `hand/superseded/` for trace history only. The previously reported
6-vertex/0x880-face collision
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
`func_0011ECB8` and `func_00196410`, plus the first object's virtual reader
contract. Later exact-assembly inspection established that these are object
records, not callback records; see the correction below.
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

## Historical: mistaken callback-list interpretation (2026-11-07; superseded)

The investigation below contains the reversed interpretation: exact US
assembly proves `func_0011ECB8` is the object-list reader, while
`func_0011EC40` reads property/callback IDs until its negative sentinel. The
`func_0011ECB8` stage-23/24 cursor bracket and its factory calls describe the
object records, not a callback list. Keep the entries below as chronology only.

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

## Retail object-list and DMA trace correction (2026-11-07)

The old port-only `func_0011ECB8` parser is no longer in the active build. Exact
US assembly confirms it reads counted object records containing class slot,
key, and flag, then invokes the object's virtual reader. Its count is an object
record count. `func_0011EC40` is the separate property/callback-ID reader. The
speculative parser and its unverified alignment heuristic are retained only
under `hand/superseded/`. The native
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


## Graphics distance checkpoint (2026-11-07)

The native launcher reaches the game frame loop and can load the selected level
record, but it has not produced an actual scene image: recent traces report
zero scene vertices, no registered fonts, and no visible text. The earlier
fragmented menu glyph captures came from a forced menu diagnostic and should
not be treated as vanilla graphics progress. Exact US assembly now supplies
menu filtering and text rendering where native startup had port-only overrides;
matching decomp C is used for the function already available there.

The next meaningful graphics milestone is one retail-created level object
with valid mesh data reaching the renderer and producing nonzero vertices.
The active world currently has no root children, while sampled update objects
have no mesh pointer or radius, so adjusting texture colors or culling cannot
produce a real scene. Font-table initialization is a parallel gate for title
and menu graphics.

## Resource-reader and default-start follow-up (2026-11-07)

The native port now has a separate `func_001BAC30` backfill for the missing
class-4 reader, plus port-only packed-vector readers `func_00137E90` and
`func_00137F08`. A bounded run consumes that class-4 record through stage 134
and continues to loader stage 5. A malformed nested frame link (`0xffff823f`)
previously faulted object update; the existing `func_00125138` backfill now
skips links outside guest EE RAM and records the values. These are port
backfills only; replace them with matching decompilation if/when it becomes
available.

Do not read the class-`0x1e` and class-`0x51` entries from stages 55–56 as
render meshes: their constructors and descriptors identify them as collision
objects. The default-start trace also finds class-`0x4d` objects, but their
resource pointer at `+0x380` is zero when they enter the world registration
callback. During a 10-second default-start run, the frame loop stayed alive,
but `vertices=0`, `image_tags=0`, `image_qwords=0`, `visible_text=0`, and
`font_array=0`; the world root still had no renderable children. Trace:
`/tmp/sly-native-class4d-objects.log` and `/tmp/sly-native-default-visual.log`.

The next investigation should follow the selected level's factory, reader, and
owner-list path to find where its object records are lost. Separately trace why
the font registry count and IDs are initialized while its font-array and
default-font pointers remain null. Verify fixes with nonzero GS vertices or GIF
image transfers and then a visible frame. The game is not playable yet.

**Class-`0x4d` lead reassessment (2026-11-07):** the startup trace shows these
objects owned by the global resource manager at `0x006abd50`, while the
explicitly selected level (`OPENRAC_BOOT_LEVEL=0x0100`, resource index 1,
record 0) has a separate world at `0x0088d530`. The class-`0x4d` objects have
no class-specific read records in the trace; their null `+0x380` fields do not
establish that the level mesh reader failed. Do not prioritize those objects
as the level's renderables without a source-to-render ownership trace.

Retail `func_0015F710` was another no-C stub in the list/activation path. The
native host now translates its exact NTSC-U instructions. This replaces a
no-op list traversal and removes its runtime warning; it changes neither the
decomp checkout nor its C. Bounded default-start and forced-level runs both
reach the frame loop, but still report only two base world-root objects, zero
root children, and zero renderer vertices. The forced-level trace is
`/tmp/sly-native-listfix-direct0100.log`; default startup is
`/tmp/sly-native-listfix-5f710.log`. Continue from the selected level's
factory/reader/owner-list path and establish where its expected object records
are lost before changing rendering. A follow-up audit also found that the
port-only `func_0011E680` callback-value array used a 16-byte C stride, while
retail advances 8 bytes per value and places its result immediately after 32
entries. The backfill now uses the retail 8-byte layout. The forced-level trace
did not enter a descriptor callback and still has zero children/vertices, so
this corrects a latent parser bug but does not explain the empty level.

Exact NTSC-U translation is now selected for `func_001B8540`, a reached VU
bounds routine; its no-C warning is gone, but the forced-level root and vertex
counts remain unchanged. Exact NTSC-U translation is also enabled for
`func_001B4688`, the resource-key table lookup called by `func_00139F80`.
This removes another no-op stub, but the forced-level root and vertex counts
remain unchanged. The current build reports 2,050 generated C functions, 148
retail-assembly translations, and zero stubs; the decomp
checkout remains clean and current at `e1b39f1`.

The latest bounded direct-level trace is
`/tmp/sly-native-direct0100-lookup-b4688.log`. It still has two base world-root
objects, zero root children, zero vertices/image tags/image qwords, and
`scene_gate=0` / `game_mode=0`. Retail inspection found that
`func_0011B008` is a 4-byte tag initializer: its US ELF instructions set only
the first word to `-1`, then return. The port backfill previously also zeroed
three following words, clobbering the payload and potentially adjacent stack
values. It now matches the retail write while remaining isolated under
`hand/native_c`. The rebuilt native launcher ran the selected level for 12
seconds; it still reports two base world-root objects, zero root children,
zero vertices and no image transfers. The existing font count/IDs are
unchanged from previous traces. Latest run:
`/tmp/sly-native-1b008-tag-only.log`.

With `OPENRAC_TRACE_FONT=1`, the exact object-list parser enters at cursor
`0x70001854` with 1,964 bytes remaining and returns at the ring boundary
`0x70000000` / remaining `-1`. Factory trace stages 23–24 bracket that parser;
the factories called between them prove it creates 112 objects: 110 type-0
records, one type `0x86` (key 2), and one type `0x04` (key 1). The type-`0x04`
object loads collision geometry; no visual class is in this record list. Earlier
startup also creates ten type-`0x1e` and forty type-`0x4d` collision objects,
plus two type-`0x51` objects. Trace:
`/tmp/sly-native-level-factory-tuples.log`.

The class-`0x86` constructor was a reached no-C stub. The port now selects exact
US retail assembly for `func_001EE900`, which calls `func_00182C18` and stores
the object at owner `+0x1ec4`. Its transform helpers `func_001EE9D0`,
`func_001EEA40`, and `func_001EEB60` are selected from the same retail assembly.
A bounded run confirms `special_scene_object=0x00a7e540` and root classes
`0x6d,0x86,0x04`, with the callback warnings gone; it still reports zero scene
vertices and game mode 0. Trace: `/tmp/sly-native-scene-callback-cluster.log`.

Next, find what resource or state transition should create the level's visual
objects; do not infer them from collision meshes or the separate property list
handled by `func_0011EC40`. The default Splash-to-title transition also remains
stuck at frame state 3/game mode 0. All functional port-only replacements stay
under `hand/native_c`; this change uses exact retail assembly and leaves the
sibling decomp checkout unchanged.

**Restart checkpoint (2026-11-07):** the current native run still reaches the
frame loop with a black scene and zero renderer vertices. The latest investigation
traces the resource-key table populated after loader stage 146 and walks the
selected level's owner lists for classes `0x04`, `0x51`, and `0x4d`. Retail
`func_001B4688` searches 40-byte table rows by signed key at `+2`; do not treat a
zero payload at `+0x24` as proof of a missing resource, since it depends on the
record type. The class `0x04`, `0x51`, and `0x4d` reader-backed descriptors parse
a mesh header at object `+0x380`: radius, vertex count/pointer, face
count/pointer, and link capacity/pointer are at `+0x380..+0x398`. The populated
class-`0x04` data is collision geometry, not evidence of a renderable mesh;
follow each descriptor's read method before interpreting these fields.

Committed native-only diagnostics in `port/sly1/main.cpp` and
`port/game/sly1-us/hand/native/sly1_native_worker.cpp` validate descriptor size
before reading mesh fields and dump resource-key rows. Builds use
`cmake --build build/port --target port_native -j2`. Trace logs remain local in
`/tmp`; the latest traces are listed below. All compatibility/backfill C
remains under `hand/native_c`; no changes were made to the sibling decomp
checkout.

**Owner-list correction (2026-11-07):** a bounded native trace now scans all
149 class slots on the directly loaded level world, rather than sampling only
classes `0x04`, `0x51`, and `0x4d`. Non-empty owner lists are classes `0x00`,
`0x1e`, `0x04`, `0x4d`, `0x51`, `0x6c`, `0x6d`, and `0x86`. The class-`0x04`
object does contain 10,110 vertices and 13,070 faces, but its descriptor
callbacks are `func_001B8540` and `func_001B9680`; the latter is the collision
previous-transform callback in the port-only `collider_mesh_transform.c`.
Treat that geometry as collision data, not the missing visual scene. No visual
object class appears in the world's owner lists, and the frame still reports
zero submitted vertices. Trace: `/tmp/sly-native-all-owner-classes.log`.
Next, trace the level resource's object records and deferred asset requests to
find why visual-class instances are never created or attached; independently
trace the default Splash-to-title transition, which also remains at frame state
3/game mode 0.

**Earlier restart checkpoint (2026-11-07; superseded by the follow-ups below):** exact US retail assembly
has been enabled for the reached value conversion/owner-list helpers
`func_001C1190`, `func_001C13E8`, `func_0011A080`, `func_0011A358`,
`func_0011A630`, and `func_0011BA28`. Their former C backfills remain in
`hand/native_c/resource_value_parser.c` behind build guards, but were removed
from `backfills.tsv`, preserving them as port-only reference code. This
selection builds, but has not yet had a fresh launcher run. The direct-level
probe still faults in `func_0015C8B8` after `func_001AE510` requests key 2 via
`func_0015C1C0`; a small isolated diagnostic backfill now traces that table
lookup without overriding the larger font/text source file. Build and run that
probe next, inspect the key-2 entry and fallback in `/tmp`, then trace the
writer of `D_00262268` if it is still `0xFC000120`. Latest confirmed blocker
remains no visible scene geometry: default boot is black, frame state 3,
game mode 0, zero renderer vertices. Rebuild with
`cmake --build build/port --target port_native -j2`; decomp checkout untouched.

**Animator allocation root cause (2026-11-07):** the key-2 lookup trace now
shows `D_00262268[2] == 0x110`, and the fault occurs when retail
`func_0015C8B8` dereferences that value as a trail object. The owner table is
populated by retail `func_001B2DF8`, which asks `func_001FD8A8` for its backing
array. That function had no C and ran as a zero-return stub, so its array base
was null and later entries became small offsets (`0x88`, `0x110`, ...). The
retail routine's live path simply returns `func_0018D758(size)`; a port-only
backfill now implements that delegation through the native heap adapter.
Rebuild and repeat the direct-level trace to verify the table contains mapped
guest addresses and the loader advances past the former `0xFC000120` fault.
The backfill is isolated in `hand/native_c/func_001FD8A8.c` for clean
replacement when upstream C is available. Trace evidence:
`/tmp/sly-native-key2-lookup-trace.log`.

The follow-up run verifies that allocation fix: table index 2 now holds
`0x00845230`, and the direct-level load advances past the previous trail
dereference fault. It then stops in the exact retail `func_00162890` path via
`func_00165AE8` / `func_0018D568`: a 0x90-byte copy is requested from
`0x3f800000`, the IEEE-754 bits for `1.0`, which is not a mapped guest address.
The copy backfill now has an opt-in trace for out-of-range source addresses,
and its matched function is explicitly overridden so that diagnostic actually
runs. Latest trace: `/tmp/sly-native-copy-source-trace.log`. Next trace the
`func_00162890` record/flag fields that lead to this `func_00165AE8` call and
find where the float is mistaken for a source pointer. Do not add a bounds
skip or suppress the copy: that would hide the data-layout or reader error.

The focused follow-up identifies the owner record as `0x00a7f77c`, entry
index `0xf0` (the 131st copy call). Its serialized flag is `0x203`, so the
low-bit branch is genuinely selected; its corresponding 0x28-byte record has
tag `0x0c`, then `0x3f800000` and other float fields. The preceding 130 entries
share valid pointer `0x00d14ef0`. At the decision point, the reader cursor has
jumped from `0x70000d8f` to `0x70001790` and the bytes there are recorded in
`/tmp/sly-native-animation-flag-trace.log`. This points to either a valid
type-`0x0c` record that needs distinct handling or a stream/record alignment
error at the ring refill. Compare the US retail field semantics and the
decompressed bytes around that refill before changing the branch or data.

**Restart checkpoint (2026-11-07):** follow-up traces rule out a direct
out-of-bounds table lookup. At the fault, `func_00137D10` reads signed index
`0xef` (239); the owning object reports `0x7aa` (1962) entries, and the row
address resolves inside the table at `0x00cf5020`. That row and its neighbors
all have tag `0x0c`; their `+4` words are float-looking values (`0x3f800000`
and `0x3f42a305`), not mapped guest pointers. The flagged owner record is the
240th record (`index 0xf0`, flags `0x203`), and the assembly selects the copy
branch. The reader cursor crosses the scratchpad ring refill between
`0x70000d8f` and `0x70001790`; inspect how the retail loader preserves record
boundaries and handles tag `0x0c` across that refill. Do not mask the copy
fault. Latest evidence is in `/tmp/sly-native-animation-bounds-trace.log`,
`/tmp/sly-native-animation-neighbor-trace.log`, and
`/tmp/sly-native-animation-flag-trace.log` (local runtime logs are not in Git).

Current state: the animator-table allocation backfill is in
`hand/native_c/func_001FD8A8.c`, registered in `backfills.tsv`; it is port-only
and can be replaced when the decomp provides that function. Native build has
advanced past the old small-offset animator pointer fault, but still exits on
the invalid source read at `0x3f800000`. There are no visible game graphics
yet. Continue with the `0x0c` record semantics and ring-refill alignment before
attempting a workaround. Build target: `cmake --build build/port --target
port_native -j2`. Work remains on branch `wip/native-sly-port`; decomp checkout
was not modified.

**Input-window comparison (2026-11-07):** the apparent cursor jump is not
proof of ring misalignment. The 44-byte snapshot at reader cursor
`0x70000d8f` exactly matches `build/port/extracted/decoded/resource_0100.bin`
at file offset `0x192d8f`; the snapshot at `0x70001790` matches the same file
at `0x193790`. Both file and guest cursor deltas are `0xa01`, so this is
consistent with consuming the intervening serialized payload. The raw bytes
are genuine extracted resource data, and the native reader reaches the
matching sequence. Stop investigating this as a ring-refill fault. Next
later row-lifecycle and renderer-write probes supersede the proposed ring
refill and owner-row checks below. Latest runtime trace remains
`/tmp/sly-native-animation-bounds-trace.log`.

**Row lifecycle and renderer-write trace (2026-11-07):** the
`0x13290`-byte table starts with row 239 zeroed; nested parsing stores a valid
auxiliary pointer (`0x00d18060`) at row `+4`. A fresh exact guest-store watch
then confirmed the later writes to row 239 `+0/+4`: `func_00162890` stores the
type `0x0c` and pointer `0x00d18060`, and `func_00164868` subsequently writes
float `1.0` at `0x00cf4ff8` and `0x00cf4ffc`. The latter store occurs after a
call to `openrac_ee_swc1` in the translated `func_00164868`; it is not an
inference from the helper's entry snapshot. At entry, its writer object
`0x0064c090` has `begin=cursor` and `end` in the native temporary range
`0xe041...`; the path that changes the eventual store destination still needs
to be traced within the routine/cursor helpers. Do not suppress the row copy
or infer the final cause from the entry snapshot alone.

`func_00164868` remains unmatched in sly1-decomp, so any eventual functional
replacement C must stay port-only and replaceable. The port backfill for
`func_0018D4B0` matches the now-available decomp C (16-byte aligned bump from
`D_002C2A14`, mirrored to `D_002C2A18` and `D_002C2A1C`), and the native temp
arena uses `0xe0410000–0xe0420000` for guest thread 1; neither explains the
table word. Next, instrument the exact stores to `0x00cf4ffc` and the
`func_00164868` output cursor within the same run, with the guest PC and
writer fields captured together. Reconcile the cursor path before changing
parser or renderer behavior. Evidence: earlier
`/tmp/sly-native-row-write-watch.log`; earlier writer snapshots in
`/tmp/sly-native-writer-fields2.log` and arena trace in
`/tmp/sly-native-temp-arena-overlap.log` (local logs are not tracked). The
native launcher still faults on source address `0x3f800000` and shows no game
graphics.

**Packet helper ABI follow-up (2026-11-07):** decomp C for `func_001EE5C8`
calls `func_001EE560` with only the writer argument, while the retail assembly
leaves `a1-a3` live as hidden inputs and uses them to construct a packet before
returning size/cursor data. Selecting exact assembly for both routines through
ordinary translated C produced a bad table value and an invalid source
address, consistent with the hidden-register ABI not surviving that call
boundary. A port-only C backfill for `func_001EE5C8` now explicitly forwards
all four arguments into selected exact retail assembly `func_001EE560`, then
copies and advances the writer cursor. Hostgen accepted and generated this
backfill after removing unsupported debug includes. A fresh run confirms the
backfill is called with plausible packet inputs and advances its temporary
writer cursor, but startup still faults later; the backfill does not by itself
fix graphics. Keep this C under `hand/native_c` and out of sly1-decomp.

**Object-reader follow-up (2026-11-07):** the direct exact guest-store trace
confirmed the row-239 writes described above. The new packet-helper ABI
backfill was then exercised with bounded argument traces: `func_001EE5C8`
passes type/count/flags to selected retail `func_001EE560`, and writer cursors
advance through `0xe041...` with sizes consistent with the retail size table.
The launcher still faulted at `0x6859E4DC`, so this repaired the call boundary
but not the malformed object-reader path. A warning immediately before that
fault showed `func_001E4C48` was still a no-op. Selecting its exact US assembly
removed that fault and advanced level parsing, but exposed a later invalid
list pointer at `0x40000004`.

`func_001289C0` had also been running the port reconstruction in
`hand/native_c/resource_object_common.c`; exact US assembly is now selected
instead, with the reconstruction retained behind
`SLY1_RETAIL_ASM_FUNC_001289C0`. A temporary trace of `func_001C1390` recorded
lookups with IDs `0x3842`, `0x0000000a`, and `0xffffa162`, returning
`0x301`, `0x9e9a54`, and null respectively. A separate store trace showed the
next call to `func_0011A338` had `list == 0` and would write to
`0x40000004`. The port now overrides that list helper to ignore a null list,
while keeping the decomp implementation unchanged. This gets past that crash;
the next clean run faults while traversing a linked-list node at
`0x08000000`, through `func_0011A4D8` from the object-reader path. Trace the
node-list owner and creation site next. No scene vertices or graphics are
visible.

Two existing port-only collision callback translations are now registered as
matched-C overrides in `hand/overrides.tsv`: `func_001B89B8` and
`func_001B7C40`. The retail C directly calls a nullable world method at vtable
offset `+0xd8`; the overrides preserve their other state updates and skip that
call when the method is absent. This removes one call-to-zero diagnostic and
lets the loader continue further. They are isolated
port C and do not alter sly1-decomp. The clean native build succeeds with 208
retail assembly translations and zero hostgen stubs. The window remains black,
with no visible scene vertices; next trace the `0x08000000` node through its
owner and creator before another graphics change.

## Restart handoff (2026-11-07)

Current branch: `wip/native-sly-port`; the original handoff checkpoint was
`310594e` and the latest pushed fix is `68424ac`. The working tree was clean
when this handoff was written. To resume, first inspect
`func_0011A6C0` and its caller `func_0011A4D8` in the decomp, then find where
the relevant keyed-tree global (candidate `D_00259F38`) is initialized and
populated. The last native run faults while `func_0011A4D8` follows a child
pointer to `0x08000000`; verify the tree and child pointers at the call site
before changing runtime behavior. Preserve any replacement C only in the port
repo's `hand/native_c` and register it through `hand/overrides.tsv` or
`hand/backfills.tsv`, as appropriate. Do not modify sly1-decomp.

After diagnosis, rebuild with
`cmake --build build/port --target port_native -j2` and run the native launcher
with a bounded timeout. The native run is authorized; do not use an emulator.
Update this handoff and commit/push a focused checkpoint after a confirmed
change. Current visible result remains a black window with zero scene vertices,
so the project is not yet at a graphics-ready or playable state.

**Tree-pointer and record-loop follow-up (2026-11-07):** the collision
constructor backfill at `func_001B7968` was present in the hand-backfill list,
but hostgen kept its matched decomp C because this function has a decomp
definition. It is now registered as a port-only matched-C override in
`hand/overrides.tsv`, with its source under `hand/overrides_c/` so hostgen does
not ingest it twice. A bounded direct-level trace shows the collider's
`+0x2c` tree field is `0x18`; its owner is `0x0088d530`, parent is
`0x010c7850`, and descriptor is `0x0021e180`. The tree walk then treated low
RAM as a recursive tree and faulted at `0x08000000`. The override now skips
the optional lookup when that field is outside mapped EE RAM. This is isolated
port behavior; the decomp source remains unchanged. The rebuilt launcher
passes that fault in a 15-second native run.

The next reached defect is in the exact retail `func_0011ECB8` record loop:
`func_0011E580` receives class `0x683e`, which is outside its valid descriptor
table (`0..0x94`) and returns null; retail assembly then calls the object's
vtable reader at `+0x38` without checking it. This indicates a malformed or
misaligned record stream before the null dispatch. Trace the reader cursor and
the record bytes at the first invalid class before adding a skip or changing
payload consumption. Latest run: `/tmp/sly-native-invalid-tree-guard.log`.
Build command: `cmake --build build/port --target port_native -j2` (succeeded;
201 generated C units, 208 selected retail assembly functions, zero stubs).
The window is still black and no graphics-ready state has been demonstrated.

**Object-record cursor probe (2026-11-07):** `func_0011ECB8` is now a
port-only C backfill selected in place of its retail assembly translation.
It follows the retail count, three 16-bit fields, factory call, virtual reader,
slot registration, and activation sequence, while opt-in tracing records the
reader cursor around each reader callback. On the direct `0x0100` probe, a
class-`0x53` reader at `func_00196410` advances the cursor from
`0x70000eb3` to `0x700016bf`; the next object record is class `0x683e` at
index 1 of 37. The 48-byte window at that point contains a mix of values and
float-looking data, not another valid class record. The first invalid factory
result still follows retail's no-op missing-call behavior so the trace can
show subsequent reads; this does not repair the stream. Later activation
faults at `0x1689c3e0` through `func_00183350` and
`func_00182ED8`, called from nested `func_0011ECB8` under
`func_00196410`/`func_001DCEA0`. Reproduced in
`/tmp/sly-native-reader-callback-sequence.log`; the byte capture is in that
same log. Reader-address tags show the same reader (`0x0064c4c0`) reaching a
class-`0x53` callback at cursor `0x70001341`, then a nested class-`0x53`
callback at `0x70000eb3`; the latter advances to `0x700016bf` immediately
before the invalid record. This narrows the next trace to how
`func_00196410`'s nested record path returns to its parent, but does not yet
identify the missing or misread payload. The build succeeds with 207 retail
assembly translations and zero stubs. No visible graphics are produced.

**Decomp remote check (2026-11-07):** the decomp checkout and
`origin/master` are still at `e1b39f1`; no new master commit is available.
The unmerged `origin/codex/us-small-functions` branch is at `8bf37ac` and
contains 51 additional matched definitions across six source files. The other
checked follow-up/wave branches have no changes ahead of master. None of the
51 additions defines the current reader/activation path (`func_0011ECB8`,
`func_00196410`, `func_00183350`, `func_00182ED8`, `func_001DCEA0`,
`func_001B7968`, or `func_001B7A98`), so there is no upstream C replacement
for this blocker yet. The decomp checkout remains untouched.

**Restart checkpoint (2026-11-07):** This is the current handoff and
supersedes the earlier `func_0011A6C0` tree-pointer handoff above. The working
tree was clean at checkpoint. Branch `wip/native-sly-port` was pushed through
`059a53e`. The `func_001B7968` tree-pointer fault is fixed by a port-only
override, and the record loop is now a port-only C backfill at
`port/game/sly1-us/hand/native_c/resource_object_record_loop.c`; neither change
touches the decomp repo. The next unresolved issue is the parent reader path:
`func_00196410` invokes nested record loading, then the shared reader cursor
appears to land inside float-like payload before `func_0011ECB8` reads class
`0x683e` at object index 1/37. Follow the nested calls and cursor ownership
through `func_00196410`, `func_0011ECB8`, `func_001DCEA0`, `func_00183350`, and
`func_00182ED8`; establish which call is expected to consume the bytes before
changing record handling. Relevant trace: `/tmp/sly-native-reader-callback-sequence.log`.
Latest build succeeded with 202 generated C units linked, 207 retail assembly
translations, and zero hostgen stubs. The bounded native direct-level run
still exits with SIGSEGV at guest `0x1689c3e0`; no graphics are visible and
the game is not playable. Decomp remains at `e1b39f1` on master; the 51 extra
definitions in `origin/codex/us-small-functions` do not cover this call path.
Resume by checking the trace and exact translated/decomp assembly for the
listed readers, then make one evidence-backed port-only correction, rebuild,
and run the native launcher with a bounded timeout. Native launcher runs are
authorized; do not use an emulator or edit `/home/reaper/Code/sly1-decomp`.

**Frame-list fault follow-up (2026-11-07):** Exact NTSC-U assembly for
`func_0011ECB8` calls `func_00182ED8` only when `func_0011ECA0(mode)` returns a
nonnegative slot. The port backfill had called it for every record, so the
activation call is now inside that condition. The same function now records
the start cursor and three decoded 16-bit fields for any invalid descriptor;
the previous byte window was post-read and could not prove what the factory
received.

The current bounded direct-level run fails earlier in the frame cleanup path,
at `func_00152600` called by `func_0015EFE8`. Its context list is
`0x00262340`, link offset `0x630`, and its head is node `0x0082f9b0` (class
`0x4d`, owner `0x006abd50`). A bounds-checked trace read found that node's
embedded links at `+0x630` are `next=0xbf343386` and
`previous=0xbef492b5`; the fault is while following this invalid list link at
`0xbef498e5`. These values look like payload/float words, but the first write
that corrupts them is not yet identified. Trace the node through resource
loading, `func_00139EC8`, the `func_001243E0` callbacks, and the transfer from
`D_002622E0` into the frame context before changing list semantics. The old
reader trace at `/tmp/sly-native-reader-callback-sequence.log` remains the
record-path evidence; the latest list trace is
`/tmp/sly-native-list-link-full.log`. Build succeeds (202 generated C units,
207 retail assembly translations, zero stubs). The window remains black and
the port is not playable. All code changes remain in this port repo; the
decomp repo is untouched.

**Restart checkpoint (2026-11-07, scene packet path):** The active branch is
`wip/native-sly-port`, clean and pushed at `3962c6c` (`translate retail scene
packet helpers`). The contradictory checkpoint above is historical; current
work has progressed past its reader and frame-list faults. The native launcher
now reaches the active world (`0x6abdd0`, state `3`, game mode `0`) without
fault for a bounded 15-second run. Frame-list inserts/removals are balanced
(40 each), all links are valid, and the frame worker sends the display
transfer packet. Exact US assembly has replaced several reached no-op
translations, including scene packet builder `func_001662A8` and helpers
`func_001EE3B8` / `func_001EE668`; these are selected retail functions, not
port backfills. The current build succeeds with 2,035 C translations, 220
assembly translations, zero stubs, and 202 generated C units linked.

Captured renderer frames remain fully black (one color, mean zero), the scene
builder emits zero vertices, and no visible text or playable state has been
reached. Latest bounded run: `/tmp/sly-native-scene-packet-helpers.log`; frame
captures: `/tmp/sly1-native-frame-00.ppm` and siblings. The next focused lead
is caller `func_00165728` in `src/main/00164000.cpp`: inspect its record count,
entry pointers, and indirect callback path at runtime, then determine why its
calls into `func_001662A8` produce no geometry. Decomp checkout remains at
`e1b39f1` on master; the 51 definitions on
`origin/codex/us-small-functions` do not cover the current scene path. Keep
functional replacements in the port's `hand/native_c/` and retain displaced
prototypes/backfills under `hand/reference_c/`. Native launcher runs are
authorized; do not use an emulator or modify
`/home/reaper/Code/sly1-decomp`.

**Direct-load SIF continuation (2026-11-07):** The first fresh
`OPENRAC_BOOT_LEVEL=0x0100` run reached resource sector 867214, then stalled
in `func_0011D078` on sound-service RPC 52 (`send=0`, `recv=12`). Retail
wrapper `func_0011E010` issues command `0x34` and discards its result. The
native SIF boundary now completes that request with the existing idle sentinel.
The next observed request was RPC 54 (`send=4`, word `1`, `recv=12`), issued
by void wrapper `func_0011E110`; it now receives the same idle completion.
This is limited to two sound commands with no consumed output. The successful
bounded run then reads sector 867214 and reaches the selected level's object
parser before faulting at guest address `0x08000000`. The symbolized path is
`func_001DCEA0 -> func_0011ECB8 -> func_00196410 -> func_0011A4D8`, called
from `func_001837E0` during owner registration. The next investigation is the
keyed-tree root and child slots read by `func_0011A4D8`; trace its pointer,
count, child array and list head, then identify the initializer that should
populate them. Build succeeded; direct-run log:
`/tmp/sly-native-sound-rpc34-36.log`. The selected-level parser does not
complete, no graphics are visible, and the game is not playable.

**Restart checkpoint (2026-11-07, direct-load scene entries):** The keyed-tree
fault was from a collider initializer reading `+0x2c = 0x18`; the existing
port-only `func_001B7968` guard rejects that non-pointer, so it is not the
current stop. A port-only `func_0011ECB8` diagnostic backfill now records each
record cursor and returns at the first impossible type instead of dispatching
through a null descriptor. In the direct `0x0100` run, it sees count `0x25`,
class `0x53` at cursor `0x70000ead`, and the next record becomes invalid type
`0x683e` at index 1/cursor `0x700016bf` after the class callback consumes
`0x80c` bytes. The malformed remainder is not yet explained. This guard lets
startup continue to the active world; it is diagnostic and does not establish
that all level objects loaded.

The port-only tracing wrapper for matched `func_00165728` follows the decomp
layout (entry stride `0x70`) and captures each list's entry count, nested
element count/pointer, and callback. The bounded 10-second run remains active
in world `0x0088d5b0`, frame state 3/game mode 0. The first lists have one
entry; later entries have valid nested pointers and some use callback
`0x001b4ad0`. Still, the renderer reports zero vertices, zero image tags,
zero image qwords, three accepted GIF chains, and the screen remains black
with no visible text. The immediate graphics lead remains the data passed by
`func_00165728` into retail `func_001662A8`, especially the nested count/pointer
at entry offsets `+0x58/+0x5c`, then the scene draw path that should submit
actual primitives. Latest log: `/tmp/sly-native-scene-entry-64.log`.

The font-trace helper now bounds-checks its stage-limit table and accepts the
additional trace IDs through 98. Latest build succeeds with 2,037 C
translations, 218 US assembly translations, zero stubs, and 202 generated C
units linked. The work is on branch `wip/native-sly-port`. All backfills and
diagnostic replacements are port-only; `/home/reaper/Code/sly1-decomp` is
untouched. This is still not playable; continue with the scene-data/render
submission path and preserve the default Splash-to-title transition as a
separate blocker.

**Scene element follow-up:** The group at `0x00847acc` has six entries. Its
first non-skipped entry (`0x00847ce0`) has three nested elements at
`0x00848070`; each element has zero at `+0x24` and `+0x28`, so it passes the
retail `func_001662A8` early-skip checks. Other entries are generally skipped
by the flag at `entry+0x28`. The group still produces no measured vertices.
The next trace should capture the full nested element records and all
`0x28`-byte subdata consumed by `func_001662A8`, then follow its generated GIF
packet to learn why it carries no geometry. Latest log:
`/tmp/sly-native-scene-nested-items.log`.
