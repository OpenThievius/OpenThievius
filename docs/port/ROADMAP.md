<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Native port roadmap

The port and the matching decompilation can progress together. A complete
matching decompilation is not required to begin tooling, asset research or a
viewer, but native gameplay needs hostable C along its boot and frame paths.
See [DESIGN.md](DESIGN.md) for the architecture and open decisions.
The hardware-model launcher described in the implementation history below is
an experimental branch of the work, not the target port architecture. Follow
the [OpenRAC reuse plan](OPENRAC_REUSE.md): native game code, native service
replacements and Sly-specific renderers; do not extend EE/IOP/DMA/VIF/VU/GS
models as gameplay features.
The [NTSC-U catalog extractor](ASSETS.md) now locates and extracts 46 named raw
resource ranges, and the [VAG audio extractor](ASSETS.md) recovers standalone
samples. The exact decomp stream-reader unit `00134000.cpp` is now host-readable and
selected for the native build, including its LZ and buffered-byte readers. A
port-only native ISO adapter reads the 150,849-byte Splash resource from the
retail sector fields. Splash is catalog key `0x2`, not level resource `0x0100`.
Its font atlas and palette data are present and match the decoded Splash bytes,
but the submitted text remains malformed and no normal title/gameplay path is
visible in the native launcher.

## Current visual blocker (2026-11-11)

### Decomp refresh (2026-11-11)

The canonical decomp checkout is current at `bfc74817` (Wave 24, PR #117),
with 4,760/4,807 US functions matched and 93.25% of code bytes matched. Wave
24 adds 410 readable names and project-owned assembly macros, but no newly
matched functions or code bytes. The names improve navigation and the macros
make retail assembly easier to read; neither exposes new matching C for the
port. Wave 22's `func_001B55C0` and `func_001536C8` remain the latest newly
matched functions; their source units still cannot pass the port's C frontend
because shared headers contain duplicate collider members, failed layout
assertions, and C++-only declarations. Neither was reached in the latest
startup trace. Rebuilding against Wave 24 succeeds with 732 C translations,
388 retail assembly translations, and 2 stubs. A fresh trace identified 22
reached visual/UI no-ops and they now run from exact US retail assembly. The
initialized font count rose from 35 to 37, but all screen elements remain
inactive and renderer counters remain at zero. The player advances along the
Splash graph’s state-2 self-edge. A 65-second native run reaches 32.15 guest
seconds but remains in global element-stack state 3, which is its normal
state, with no movie queued and no graphics submissions. Wave 24 names
`0x275BC0` as `g_elementStack` and `func_001E9108` as `UpdateElementStack`;
this corrects the earlier interpretation of state 3 as an app-level input
wait. The startup transition and missing graphic output remain unresolved.

### Frame setup and Splash graph trace (2026-11-11)

The startup frame initializer called `func_00193210`, `func_00193270`, and
`func_00193288` as no-op stubs. Exact US retail assembly is now selected for
all three; that earlier build linked with 338 assembly translations. The
current build has 340 after restoring two reached font/layout helpers. A fresh 15-second
native trace confirms those no-op warnings are gone but still reports zero
vertices, visible text, and GIF submissions. It also confirms the Splash
StateGraph object exists at ID `0x25f`, remaining on state 2 / event `0x261` /
self-edge `2 -> 2`; the missing-link theory from earlier traces is no longer
supported. Log: `/tmp/sly-frame-init-asm-15s.log`.

Exact US assembly was then selected for the reached font/resource layout
helpers `func_001E2268` and `func_001E22C0`. A second 15-second trace confirms
these no-op warnings are gone, but still reports 37 inactive UI objects and
zero vertices, visible text, or GIF submissions. The CountdownNotifier is
still at state 0 with timer gate `+0x338` null and timer value 0 (range 4–6);
the `0x261` graph edge remains an unarmed `2 -> 2` self-loop. This narrows the
next investigation to the producer that should arm the notifier or activate
Splash UI. Log: `/tmp/sly-font-layout-init-asm-15s.log`.

### Direct-level packet update (2026-11-11, latest)

Earlier checkpoint: the public decomp was at `55d39f28` (4,758/4,807 US
functions; 93.108% code) and `night21` was `4097b806` (4,759/4,807; 93.125%).
The broad rework adds collision,
placed-object, and player code, while night21's 82-file delta mostly corrects
types/signatures and reports and matches one active-set collision helper. The
changed `file_15F678.cpp` confirms the strip descriptor and packet relocation
flow, but adds no newly matched function on this render path. The port-hostgen
frontend still cannot compile the relevant full C++ unit. A separate local
fork checkout (`/home/reaper/Code/sly1-decomp`, `e1b39f16`, wave 16) contains C
definitions for the small packet writers `func_001EE3F0`, `func_001EE420`, and
`func_001EE498`; their behavior remains backfilled only in the port's
`native_c/display_packets.c`.

The direct-level launcher had been faulting because these reached packet-word
writers were no-op stubs. A prior probe called the first two halfwords at
`strip->unpack` a reversed range (`0x4e3c..0x3eb1`); this was not a verified
descriptor read and must not guide a fix. `StripUnpack` begins with four
16-bit `off` values, followed by four `strip` values. A valid sample has
`unpack=packet+4` with descriptor offsets `0x24` and `0x1e0`. A suspect sample
has `packet` bytes `04 01 00 01 24 c0 54 68 ...` and the same `+4` pointer
lands on a command word. Trace the producer pointer, packet begin, and copy
delta for both cases. Keep the port-only negative-count guard in
`func_001EE5C8` until this path is understood; do not clamp descriptor values.
The 8-second direct-level run stays alive, but still produces no scene
vertices, GIF image tags/submissions, or visible text.

Build `build/port/night21-4097b806` succeeds with 730 C translations, 287
selected retail assembly translations, 2 stubs, and 3,890 functions without
C across 267 generated units. The 5-second trace reaches the frame loop with
an active world and resource lists and emits eight 144-byte GIF setup DMAs.
It still reports zero scene vertices, zero GIF image tags/submissions, and no
visible text. The window remains black; this is not a visual milestone.
Trace: `/tmp/sly-night21-4097b806-vif-guard-state-5s.log`.

The `night21` model source narrows the next probe: `BuildStripMeshPacket` and
`BuildLitStripMeshPacket` assign `strip->unpack` through
`ReserveVifUnpack(..., &hdr->p)`, initialize the descriptor at that address,
and after packet-buffer detachment the loader adjusts it by
`strip->addr[0] - starts[0]`. The source flow alone does not explain why some
copied packets start with a prefix that puts the pointer on a command word.
Keep the negative-count guard and three helper implementations in the
port-only backfill layer until the parser/relocation path is understood.

A fresh 15-second direct-level probe with temporary hooks before/after detach
and at `func_00165588` produced no strip events. It did reach the collision
face-tree parser (loader stages 190–192) and emitted the eight 144-byte setup DMAs.
The strip pointer problem from older logs is not reproduced by the current
launch path. Next trace the object/model handoff into `ReadPlacedObjectModel`
and determine why it does not reach the strip builder before changing packet
arithmetic.
Probe log: `/tmp/sly-night21-4097b806-strip-pointer-stages-15s.log`.

An earlier native startup checkpoint used the `f36f43ee` source snapshot of
`sly1-decomp` and hostgen translated 722 functions and 247 selected retail
assembly functions across 263 generated C units. The current build uses
`night21` (`4097b806`) and its coverage is recorded above. These are
build-compatibility counts, not a playable milestone.

Splash's `StartStateNotifier` lookup now resolves the class-`0x75` child at
`0x846cb0` as the CountdownNotifier's owner. Both lookup functions are
port-only overrides because `file_1549C8.cpp` is not hostable yet. The
7-second native run still shows notifier state `-1`, timer gate off, no visible
text, zero scene vertices, and no GIF image tags or DMA submissions. The
window remains black. Next trace the Splash transition producer and the first
actual screen, movie, or scene draw request. Details and the reproducible log
are in [`PLAYABLE_TODO.md`](PLAYABLE_TODO.md).

### Historical visual investigation (2026-11-06)

The user-provided PCSX2 reference screencast
`Screencast_20261106_183929.webm` (2026-11-06) shows the European release
booting through the credits, the Paris rooftop opening sequence, and the title
screen. From about 48 seconds onward, the title logo and Start/Select prompts
overlay an animated rooftop scene; Sly and the camera continue moving. The
recording does not show selecting New Game or controlling Sly, so it confirms
the title/attract presentation but not playable progress. Current native probes
use the NTSC-U `SCUS_971.98` ELF and matching US disc; use the video for
sequence and feature reference rather than pixel comparison. The native target
sequence remains to reach that title scene, then transition to a controllable
level. Earlier native captures showed malformed text fragments; the latest
default-startup capture appears entirely black.

The latest native callback trace sharpens the blocker. Translating the
`func_00143648` camera constructor and its reachable assembly-only dependency
closure makes sampled retail `func_00125C90` calls pass the first VU0 MAC gate
with `0xF0`. The port-only `func_001453B0` sphere/frustum adapter then rejects
the sampled point: node position `+0x140..+0x148` and radius `+0x2A8` are zero;
camera `+0x40` is `0x4500D8D8` and `+0x44` is zero; the test point x is
`0xC500D8D8`, y/z are zero. The next step is to establish why these update-list
entries have zero placement/radius data and whether they are valid scene
objects. Do not alter the culling comparison or force objects through it.
The camera updater now runs from selected retail assembly as well. A port C
declaration bug had passed the camera global's storage address instead of its
stored object pointer; the corrected call sees one valid controller-stack
entry and no longer reaches the former invalid callbacks. This still does not
populate the sampled scene-object position/radius fields.

The newest host build adds port-only screen-fade update/draw backfills
(`func_001EFD10`, `func_001EFEE0`) based on current decomp C, and the
assembly-only queued-fade helper `func_001F00F0`. Hostgen cannot compile their
containing decomp units yet. The launcher still captures an all-black frame;
its renderer repeatedly receives a single untextured 240-vertex black draw per
packet and no other visible title geometry. Thus the screen-fade code is restored, but there
is no evidence yet that it explains the missing title or level scene.

The latest bounded native level probe (2026-11-06) confirms that placed-object
registration now populates 77 nodes in each of world lists `+0x54` and `+0x60`.
The `+0x1BE8` class-0x51 ConeVolume list has one object at `0xE20A57B0`; its
retail fields are flags `0x0AFFFF00`, flags-high `0x120`, mode `+0x2D0 == 0`,
and value `+0x304 == 0`. Sampled update entries
call translated retail `func_00125C90` and have zero `+0x28C`/`+0x298`, so they
do not enqueue deferred draw requests. The frame still submits zero scene
vertices. Its initial VU gate now passes (`MAC=0xF0`), but its separate
sphere/frustum check rejects sampled objects with zero position and radius.
The deferred frame dispatcher and request bucketer (`func_0019DC98` and
`func_0019D4B0`) have separate port-only C
backfills. `func_00184DE8`, the assembly-only class-0x51 ConeVolume/VU
preparer, now has a retail-derived port backfill and survives the native frame
path. The current volume has mode/value zero and contributes no VIF vectors;
this routine does not submit scene geometry. The frame still submits zero
scene vertices. The next gameplay/rendering target is valid scene/camera state
and the next callback after the sphere/frustum check. Keep all functional
replacements under `hand/native_c/` until matching decomp C is available to
the host build.

Upstream `sly1-decomp` was checked directly on 2026-11-06 at master
`dd6f80b` (US: 4,730/4,806 functions and 93.10% code; Europe: 4,738/4,831
functions and 92.42% code). The latest merge adds exact HUD compass drawing
code in `func_001316E8` and a placed-object link table; neither is on the
current level-render path. It does not provide C for `func_00184DE8`,
`func_0019DC98`, or `func_00125C90`. The active generated host snapshot remains
older than this upstream master, so the new unrelated match is documented but
not claimed as compiled into the launcher.

A hostgen staging path now lowers small C++ constructs in matched decomp units
into temporary C without modifying the read-only decomp checkout. It exposes
exact stream-reader and other matched code to the native build. Port-only
compatibility functions remain registered separately in `hand/backfills.tsv`.
The latest generated host report reads 197 units and translates 1,094 functions
(zero assembly translations, one stub, 3,796 functions without C), plus seven
port-owned overrides of matched C. Its source snapshot is `cf9ad46`.

The newest decomp source contains the matched screen-fade updater
`func_001EFD10` with its mode, timing and opacity logic. The current native
hostgen path still reports this function as no-C because its containing
`001EC000.cpp` unit has unrelated C++ helper constructs that staging cannot yet
lower. Until hostgen can compile it, the port builds separate backfills for
`func_001EFD10` and `func_001EFEE0`; both remain replaceable by matching C.

The staging pass now also makes `00138000.cpp` and `001B8000.cpp` readable;
the native build uses matching decomp C for `func_00138278` and
`func_001BAC30`. It also compiles matching `func_00196410` from
`file_1963D8.cpp`. The corresponding port backfills are not active; the
assembly-only `func_00137FB0` mesh decoder and `func_001289C0` resource helper
remain port-owned.

The native sector adapter successfully reads the Splash resource from ISO
sector 820372 (74 sectors). The first confirmed mismatch was property ID
`0x5B`: the table was never initialized in the active 64-bit hostgen path, so
`func_0011E680` returned without consuming the signed 16-bit payload. The next
ID consequently appeared as `0x7300` instead of `0x73`.

A port-only C backfill now initializes all 1,179 property descriptors from the
sly1-decomp near-match, kept separately under `hand/native_c`; the initializer
is called once from the shared property-stream reader. The port reader uses the
retail callback field at descriptor `+0x18` for flag `0x100`. A focused native
trace verified entry 91 as type `0x101F`, flags `0x120`, callback `0x0012A9C8`;
property `0x5B` advances two bytes and the next property is `0x73`.

A 10-second native run then stayed active and repeatedly submitted two draws
with 372 decoded vertices, rather than faulting at the earlier null callback.
This advances startup, but does not establish a visible image or playable game;
the window still needs visual confirmation and the title/level, controller,
collision, audio, and save paths remain incomplete. The next task is to trace
this new post-property record path and determine why the submitted textures and
frame are not yet confirmed visible.

### Direct level-load probe (2026-11-06)

The native resource loader now performs the retail `func_001DB838` world reset
before replacing an existing world. This C implementation is a port-only
backfill, registered in `hand/backfills.tsv`, and must yield to matching C when
that function is available in the host build. With `OPENRAC_BOOT_LEVEL=0x0100`,
the native loader bypasses the missing movie/title flow after Splash and reads
the catalogued `A Stealthy Approach` range (sector 867214, 4,911,835 bytes).
The resource parser reaches stage 13 and installs a second world
(`resource_level_index=1`, world `0xe103cf80`). The large synchronous sector
read uses reserved upper EE RAM at `0x01000000` so it does not overwrite
`D_002C3B00`, which also backs render-frame buffers. This scratch range is a
development adapter and needs review before general multi-level use.

The probe still produces zero scene vertices and the window remains black.
Several level construction/update functions are still no-C stubs, so parser
completion is not evidence of a valid rendered or controllable level. The
direct-load option is disabled by default; unset `OPENRAC_BOOT_LEVEL` for the
normal startup path. The next gameplay milestone is to populate/render the
loaded level and bring up Sly's player/input/collision path.

**Follow-up, 2026-11-07:** a fresh generated-code audit found that the
`func_001E4E80` / `func_001E4EA8` native loader path above was not selected:
those port C definitions were only fallback backfills, and hostgen used the
matching C definitions instead. They are now registered as explicit
port-owned overrides, preserving the decomp checkout unchanged. With the
override active, the native probe does read the stated ISO range and enter the
level parser, but faults before the previously reported stage-13 completion.
The current verified state is therefore earlier than the old paragraph
claims. A port-only `func_001EE930` value-list reader now replaces the native
no-op selected for that assembly-classified address, allowing the parser to
advance into collision records. The current fault is in collider mesh parsing:
the stream reports 6 vertices, `0x880` faces, and 22 links with about 5.8 KB
remaining before reading beyond valid guest memory. See
[`PLAYABLE_TODO.md`](PLAYABLE_TODO.md#direct-level-probe-follow-up-2026-11-07)
for the trace and investigation tasks. The level is not yet loaded or playable.

The latest world trace correctly resolves guest-port heap pointers: it counts
25 entries at the world root and 24 entries in the `0x55` family list (classes
`0x58` and `0x51`). Port-only backfills for `func_00124050` and
`func_001240F8` now walk object activation and register placed objects. A
bounded native run then counted 77 entries in both world lists `+0x54` and
`+0x60`, where both were empty before. The renderer still reports zero
vertices, so this fixed list population but has not yet produced a visible
scene. The frame trace now samples the 77-entry `+0x60` update list: each
sample dispatches to `func_00125C90`, which remains a no-C host stub. This
callback must run before the draw-request queue is populated. A port-only
`func_0019D410` now appends retail-sized 0x80-byte requests, but the queue
remains empty until the missing object-update callback is implemented.
`func_0019DC98` is the deferred draw pass; its current replacement is a
port-only backfill, as is `func_0019D4B0`. Keep both separate and remove them
when matching decomp C is available to the host build.

The older `af52bbb` snapshot added `func_001B1438`, outside the Splash setup
path. `func_00109488` remains ASM in `src/main/file_1000C8.cpp`;
its `nonmatching/func_00109488.cpp` near-match documents the descriptor values
used for the port-only backfill. Hostgen drops discarded post-increment
expressions, so backfill pointer steps must be assigned. Keep this functional C
separate so it can be removed when matching decomp C becomes available.

The port-only byte-copy helper `func_001F5A58` currently maps to host
`memmove`, and `func_00140C28` / `func_00140CC8` map disc reads to the native
ISO service. These are separate backfills because the first function remains
assembly-only and the matching CD routines are in an unreadable `00140000.cpp`
unit. Earlier
second-collider offsets came from the prior port reader and need a fresh
interpretation.
The earlier semaphore use-after-free was fixed and bounded AddressSanitizer and
normal traced runs survived without that abort.

The generated host source snapshot is `cf9ad46`; its current report reads 196
units and translates 1,091 functions, with one C stub, 3,799 functions without
C, and seven port-owned overrides of matched C. Upstream master is now
`dd6f80b` (US: 4,730/4,806 functions and 93.10% code; Europe: 4,738/4,831 and
92.42% code). These host-generation and decompilation counts use different
denominators; neither establishes gameplay coverage. Use
[SLY_GRAPHICS_HANDOFF.md](SLY_GRAPHICS_HANDOFF.md) for reproducible commands,
current evidence, contributor setup, and prioritized tasks. Older entries
below are dated research history and may describe earlier renderer paths.

The next milestone is a visible frame from Sly's real draw stream. Playability
still requires a working title/level path, controller input, controllable Sly,
collision, audio, and saves.

## Current no-emulation build status

### Decomp synchronization (2026-11-06)

Upstream master `dd6f80b` is now the matched-C input for the native host
build. Current source coverage is US 4,730/4,806 functions (93.10% code) and
Europe 4,738/4,831 (92.42% code). The latest merge adds matching HUD compass
drawing (`func_001316E8`) and placed-object link data, but no matching C for
the object update/draw chain. The hostgen report reads 199 units, translates
1,105 C functions plus three selected US retail assembly routines, stubs one C
function, and lists 3,785 functions without C. The selected
assembly translations are `func_00125C90`, `func_00126270`, and
`func_00166690`; they are generated from `asm/us/text.s` without modifying
the decomp checkout. The direct-level probe parses key `0x0100` and installs
its world, but does not enter gameplay or produce scene geometry. The native
pad adapter now reports an active pressure-mode pad, preventing the startup
dispatcher from forcing the pause menu at five seconds; the title scene still
produces no sustained geometry. The default startup trace reaches retail
`func_00125C90` for placed-object updates, but its VU0 MAC check rejects the
sampled transforms before the mesh request builder or packet callback. These
counts are separate measures from decomp coverage and do not establish
readiness.

### Earlier bounded native run (2026-11-06)

This run predates the descriptor-table initialization fix below. It saw property
ID `0x5B` at cursor `0x1619`, followed by `0x7300` at `0x161C`; the decoded
retail bytes are `5B 00 06 00 73 00 10 00`, so the next ID should be `0x73`.
That caused an implausible record count, a null callback, and a guest call-through
fault. The later focused trace below verifies this specific parser failure is
fixed. The black window remains unresolved.

At the time of this earlier probe, decomp master `af52bbb` had 4,721/4,806 US
functions with C (92.20% code bytes); Europe had 4,730/4,831 (91.72%).
`func_001B1438` did not touch Splash startup. `func_00109488` remains ASM;
its near-match documents the 1,179-entry property table. The port has an existing
native-only registry in `port/game/sly1-us/native/func_00109488.cpp`; a duplicate
hand/native C experiment was removed. The active `func_0011E680` handler and
its retail descriptor dispatch remain the next targets. Generated-host coverage
is not gameplay coverage.

The updates since the previous `a0fd295` snapshot include extensive
name/header refactors and documentation; matching coverage is unchanged. The
name and header improvements helped make the `00134000.cpp` stream unit
host-readable through port-side staging. Host staging remains in ignored
`build/port/` paths and does not modify the decomp checkout. Generated-host
function counts are separate from decompilation matching coverage and are not
gameplay verification.

The dated notes that follow are retained as implementation history; use the
current visual blocker above for the live launcher state.

An earlier bounded startup run, before exact `00134000.cpp` stream code was
selected, read Splash from sector 820372 (74 sectors), decompressed it, and
reached parser stages 1–5. Port-only frame-context and
packet-buffer backfills now initialize and retire empty frames; intrusive-list
helpers also match the decomp's append, reset, offset and empty-check behavior.
The native semaphore bridge wakes the display workers. A five-second launch
survives without the previous packet-vector corruption, but the window remains
black: the scene lists and registry are empty, and the renderer reports zero
vertices. This is startup progress, not a playable state.

### Latest bounded native run (2026-11-05)

The synchronous CD bridge now reports its completed read through the native
`func_00203D90` backfill, while the matching IOP-backed body remains available
in staged source as `decomp_func_00203D90`. This stopped the stream worker from
reissuing the same Splash request: the latest 5-second run read sector 820372
for 74 sectors exactly once. The host staging for the display worker also keeps
its current frame pointer local across the main thread's shared-pointer clear,
avoiding the earlier null dereference in `func_0015EE80`.

The combiner now excludes local GCC cold clones from callable retail entries.
That exposed compiled functions such as `func_00151A88` that were previously
misreported as ambiguous; the direct-call frontier rose from 729/788 to
743/799 with 56 missing. The current build compiles 84/84 Sly C++ units and
60/60 port-native backfills, and maps 3,834/4,806 catalogued functions plus one
non-catalogued backfill address.

The parser reaches stage 5 for the startup Splash object. The first missing
work is now scene population and rendering: multiple object construction and
resource callbacks remain no-C stubs, and the active frame trace still has no
world lists or geometry. Several port-only camera/VU boundaries
(`func_00144540`, `func_00144BE8`, and `func_00125138`) remain temporary no-ops;
the mesh boundary `func_001B8820` only preserves copied geometry and does not
apply the retail VU transform. These gaps cannot support a playable scene.

### New decomp work found (2026-11-05)

Upstream `master` is `a0fd295` (PR #54, Nov 5), fetched into the read-only
worktree. PR #54 adds project-authored readable names and documentation for
the button-code list (`func_0016F188`, `func_0016F1F8`, `func_0016F238`,
`func_0016F2E0`, and `func_0016F588`), plus a renamed record header. It does
not change the US function catalog or add matching function bodies. The
button-code notes will help when wiring controller input, but they do not
resolve the current startup stall.

The source refresh exposed a host staging issue: `host_abi.h` predefines the
shared `common.h` guard, so the newly added `names.h` was skipped. The host
compiler now includes `names.h` explicitly after the ABI header. With that
change, all 84/84 C++ units and 45/45 native backfills compile (127 compiler
warnings). The address combiner reports the same 3,810/4,806 functions and
727/788 direct-call frontier as before.

The SDL3 and OpenRAC DirectRenderer runtime compiles as 32-bit i386 with the
matching i686 headers and SSE2 flags. The dynamic SDL host32 launcher links and
opens its window, runs the decompiled game on a worker thread, and submits GS
GIF chains through port-only DMA services. Port-only IOP/SIF and interrupt
critical-section replacements now carry startup past earlier waits. The latest
trace reports missing `func_00109488` and `func_0015F320`, then remains active in
`func_001F6B60`; the window is still black, CD reads remain at zero, and there
is no playable scene yet.

The no-emulation host32 path directly compiles the Sly C++ source units and
remaining port-native backfills, then combines them by retail address. Against
the refreshed `3ef6157` upstream snapshot, 84 Sly units and 32 port-native
backfill units compile. The combined object records 3,802 native function
definitions for 4,806 catalogued addresses. Its direct-call frontier is
719/787 functions defined (68 missing); this does not
follow indirect callbacks, function tables, constructors or vtables. The SDL
launcher opens a user-supplied cooked ISO or raw BIN and provides native
synchronous sector-read, wait, and error services at Sly's CD boundary. A
trace-enabled launch of this snapshot reports missing functions
`func_00109488` and `func_0015F320`, then remains in `func_001F6B60` without CD
reads. No scene is visible yet.

The C-only hostgen path now has port-only startup loader initialization,
message dispatch, stream range setup, and object factory backfills in
`game/sly1-us/hand/native_c/`. The factory now compiles as native C and
dispatches descriptor constructors through the host-generated C function
map. A bounded native run reaches the persistent frame loop and submits the
four initial display packets. The port-only `func_001E4EA8` coordinator
decodes the Splash range and constructs the type `0x6C` resource object. A
partial, port-only `func_001DCEA0` parser backfill now compiles and reaches the
native disc reader: a bounded run read sector 820372 (74 sectors). Port-only
overrides for `func_0018D608`, `func_0018D658` and `func_0018D6F0` now provide a
thread-local packet arena in reserved guest-port memory, preserving the game
packet-building C while replacing its platform allocation boundary. The latest
12-second run avoided the invalid allocation fault and recorded 984 accepted
GIF-chain submits. The newest run confirms the reader serves the cached sector
file and produces the offline decoder's verified first 8 KiB. A port-only
guard rejects the resulting impossible resource-entry count, after which the
callback returns through stages 1–5. The port-only resource value and animation
table parsers now have C backfills, and the resource factory initializes its
owner lists, descriptor methods, aligned free-list pools, and listener records
through the resource context. A bounded native run completes the Splash
callback without fault. The resource state backfill resolves Splash to
resource ID 2 and initializes the visual
state table; the opt-in trace reports the expected level and record indices.
The trace shows no active or pending movie keys. It does not establish whether
the game has queued another resource request.
This does not establish a usable scene: several resource and frame callbacks
still resolve to no-C stubs. The host window remains black. The trace now shows
the GS display registers being written (`PMODE=0x8066`, `DISPFB1=0x1446`), but
zero submitted vertices, image tags, blits, or local-to-local transfers. The
next blocker is producing scene or Splash draw commands through the frame and
resource callbacks, not opening a display mode. Treat this as loader/frame-loop
diagnostic evidence, not a playable milestone. The coordinator also omits
retail progress/job bookkeeping. Port-only backfills `func_001526D0`,
`func_00182FC8`, and `func_00182C80` now
restore object-list membership and activation from the retail C path. In a
bounded run the world registry becomes nonempty, but the renderer still sees
zero vertices; frame traversal and draw submission remain incomplete.
The resource-property activation callback `func_0011EDA8` is now backfilled
too. Its latest bounded run completes Splash parsing without the former no-C
warning, but still produces no draw geometry. The deferred world-job drain
`func_001DBE48` now dispatches queued callbacks and returns their records to the
world pool; a bounded run completes without its former no-C warning or fault,
but vertices remain at zero.
The factory's required class
constructor `func_001DB4C8` now has a port-only native C backfill; its vector
helper `func_001DBA90` and pool initializer `func_001DC740` are backfilled
alongside it. They are registered through hostgen and link as native functions. The
buffered byte reader `func_00137A60` is also backfilled and linked. The direct
host32 build compiles all 68 Sly C++ units and 37 port-native backfills, then
reaches frame setup before stopping at missing graphics setup function
`func_00201D60`. Its direct-call frontier and indirect graphics callbacks
remain incomplete, and it has no verified title or playable scene. Neither
launcher currently demonstrates native gameplay.

The separate C-only hostgen launcher compiles Sly C without translating
assembly, and links OpenRAC's renderer, but still uses the experimental
guest-memory/runtime services rather than the target native-service
architecture. Its latest bounded run reads the cached Splash sectors, matches
the offline decoder's initial output bytes, and returns through the Splash
resource callback. It does not configure a display framebuffer; no scene
vertices reach the renderer. Do not treat that run as evidence of native
gameplay.
Retail MIPS translation
remains optional research output under `us-asm-research`. Build artifacts live
under ignored `build/port/`; port-native host32 backfills stay outside the
matching decompilation in `port/game/sly1-us/native/`.

The separate direct-C++ path compiles all 68 source units and three port-only
frame backfills as i386 objects, then combines them with retail-address aliases.
It has no executable or native service entry. A Fedora 44 i386 build probe also
showed that the reused shared guest runtime cannot simply be linked into that
process: it relies on `__int128`, which the i386 compiler does not provide.
Keep the useful OpenRAC infrastructure, but build a native-only service layer
instead of pulling the guest instruction/device runtime into gameplay.

These are build milestones only. The next concrete milestone is a host-native
executable entry and service layer for compiled Sly code, followed by loading
a retail scene and feeding its draw data to a native renderer. Continue through
the title, level loading, input, audio and saves. Do not count assembly
translation, object combination, a link check, or a black window as playable
progress.

The host32 combine step now also writes a direct-relocation frontier rooted at
the port's native frame-loop backfill, `func_00185758`. In the current build,
57 of 89 functions reached through function-address relocations have compiled
definitions; 32 are missing. This is narrower than the whole-game unresolved
frontier and gives the next backfills a concrete starting list. It does not
follow indirect calls through retail vtables or callback tables, so it is not
the complete runtime closure. The machine-readable report is in
`build/port/host32/link/link-frontier.json`.

The no-emulation startup backfill now reads the NTSC-U initializer table and
calls its 18 callbacks in reverse order through the native function map. Five
callbacks are interior entries absent from the catalog; the combine step now
includes separately backfilled addresses so those entries resolve as native
code. All 18 table entries currently have native definitions, including
port-only wrappers and two Sly configuration initializers. This establishes
constructor coverage for that table; the host32 objects still do not link into
a launcher.

The host32 backfill for `func_00185B38` reproduces the frame callback setup
already present in the separate hostgen backfill and invokes callback targets
through the native address map. The current NTSC-U callback table has 29
entries; 23 have native targets and six are skipped until their code is
available. `func_001430C8` now has a host32 snapshot replacement that stores
the full monotonic 64-bit count into Sly's extended timer globals.

The native frame path supplies `func_001430F8`, the Sly Count read, with a
Linux host monotonic clock scaled to 294.912 MHz, and `func_001FB6B0`, the
64-bit tick conversion used by Sly's timers. Native backfills also implement
the frame-flag clear at `func_0016E118` and the reference-count transitions at
`func_001DDA20` and `func_001DDA50`. These resolve five functions in that
direct frontier without executing console instructions. They remain
port-only backfills, separate from the matching decompilation.

The host32 backfill for `func_00128018` performs the retail copy cases and
three-axis local-coordinate transform with host float operations. It resolves
one additional catalogued address. Retail also leaves intermediate values in
VU0 state, which this native backfill does not model; the reachable direct-call
frontier then stood at 56 of 89 functions defined, with 33 missing.

The native backfills for `func_001EACA0` and `func_001BC260` implement the
quadratic-root and coordinate-normalization math used by Sly's movement path.
The combined host32 object now resolves 1,339 function references, though it
still is not a linked game executable.

The port-only `func_001F6C20` DMA reset wrapper completes as a no-op on the
host32 path because native submissions are synchronous. Its addition resolves
one more function in the direct frame frontier, now 57/89 defined with 32
missing.

| Phase | Work | Milestone |
|---|---|---|
| 0 | Decide source-generation boundary and host memory model; inspect reusable OpenRAC tooling and license | A small Sly source subset can be prepared without modifying sly1-decomp |
| 1 | Read-only source integration, build scaffolding and frontier reporting | The port can build a host test program from generated Sly code |
| 2 | Research the retail loader/index; implement disc validation, extraction and asset format notes | NTSC-U catalog ranges and standalone media can be extracted from a user's own disc |
| 3 | Derive asset readers/converters from Sly's retail code | Port-ready level and texture assets from a user's disc |
| 4 | Native viewer and renderer prototypes for Sly's actual rendering systems | An extracted Sly scene draws without running game code |
| 5 | Replace required platform APIs and host the boot/title path | Native program reaches a useful in-game state |
| 6 | Complete frame path, renderer integration, audio, input and saves | NTSC-U is playable on desktop platforms |
| 7 | PAL support, parity fixes, packaging and launcher | Both retail versions are supported |

**Experimental implementation history (not a playable-port milestone):** the hostgen tool and CMake entry point are in
place. With local retail-backed assembly available, the latest build translated
1,499 functions from C and 3,178 from assembly; all 81 generated C units
compile. The fresh hostgen report also lists 84 functions without C or a usable
assembly translation. `port_native` reads a user's extracted boot ELF and optional ISO,
maps the ELF segments, and calls the translated entry. The launcher accepts
Sly's `SetupThread` stack range, registers five constructor-table interior
callbacks, and runs guest worker threads on host threads with separate guest
stacks. A native ISO 9660 reader accepts cooked images and raw 2352-byte BINs,
locates paths, and reads file ranges. The IOP bootstrap constructor has a
native CD-ready hook, and file open/poll/close use ISO lookup. The native DMA
helper performs validated EE RAM copies synchronously; interrupt handlers,
semaphores and CP0 timer reads have basic host implementations. The missing
`func_001430C8` timer snapshot now has a native replacement that follows its
retail sequence and stores Sly's extended timer state. SIF SetReg and
GetReg (syscalls 0x79 and 0x7A) use a host-side register map. The SIF RPC bind
loop is bypassed for native startup. Native startup publishes SIF BOOTEND and
CMDINIT at register 4 (value 0x60000), with an opaque subaddress, when the
disc-backed IOP replacement starts. Native SIF command handling completes
command-init and binds both guest RPC clients. In the latest bounded run,
startup passed the first sound service request using an empty completion reply.
The native disc readiness hook also passes the subsequent startup poll. The
native fallback responses report sound initialization and bank loading as
unavailable. A native memory-card RPC boundary answers the observed
extended-server probe (`0x80000400`, RPC `0xFE`) with a zero result structure.
Retail's `func_002093F0` rejects its version field at offset +4 and returns
`-0x78`; this reports the unavailable service through the game's existing
error path, without implementing card detection or saves. The native sound
boundary now completes RPC 77 as idle for bounded
queued audio submissions. A 12-second native run passed these requests, read
74 ISO sectors and continued issuing GIF work across guest threads. This is a
silent fallback, not audio playback. The run still times out without a visible
title screen: the host GIF FIFO has no renderer consumer, and the frame loop,
input, audio, saves and other device services are incomplete. SIF DMA command
handling and other IOP RPCs remain unfinished.

The NTSC-U graphics wait now paces the two installed display interrupt
callbacks, and the retail `func_001F6B50` iSignalSema wrapper is serviced by the
host semaphore implementation. A bounded run confirmed the display worker
wakes instead of remaining blocked on every frame. It still reads only the
initial 74-sector range; DMA handler setup and the missing renderer consumer
remain blockers, so this is synchronization progress rather than a visible
scene.

The observed SIF setup also registers and enables DMA channel 5. Its retail
AddDmacHandler and EnableDmac boundaries are now separate host services, and
completed native SIF commands dispatch that registered callback. The latest
12-second run no longer reports those functions as missing, but still reads
only the initial sectors and remains headless; renderer work is still required.

The GIF source-chain reader now takes ADDR and SPR from bits 32-63 of the
first DMAtag word. A bounded run accepts the observed 704-byte packed A+D
stream and subsequent image-data chunks without rejecting channel 2 transfers.
The FIFO still has no consumer, so these validated commands are not yet drawn.
The host parser consumes the full GIF stream synchronously; its unused FIFO
history is capped at a rolling 4 MiB so startup cannot grow it without bound.

A stateful GIF decoder now tracks packed writes, register-list writes, image
payload spans and EOP markers across DMA chunks. The bounded startup trace
reads FRAME_1 `0xa0046` and SCISSOR `0x027f0000`, identifying a 640x224 display
target. This is renderer input state only; it does not rasterize or present
pixels yet.

The trace also reads the directly mapped GS privileged registers. During the
bounded run PMODE stays at `0x8067`, DISPLAY1 at `0x1be9ff0183327c`, and
DISPFB1 flips between `0x1400` and `0x1446`, confirming that Sly alternates
display buffers while the GIF packet stream advances. SDL presents decoded
host geometry in the native window; a GS pixel store is still absent.

An SDL3 window consumes host geometry snapshots on the main thread at the
game's 640x224 logical resolution. Guest execution runs on a separate host
thread. Initial counts of up to 12,336 vertices were artifacts: the GIF parser
treated IMAGE2 payload as tags, so image bytes looked like register writes.
The parser now consumes IMAGE2 payload framing correctly. A bounded native
run after the VIF1 OFFSET and PACKED XYZ fixes publishes 120 vertices, with 58
inside the 640x224 viewport. This confirms screen-space geometry reaches SDL,
but does not verify a recognizable scene. Texture sampling, scene streaming
and gamepad mapping are also missing.

VIF1 channel 1 was holding its source-chain transfer active at CHCR `0x105`,
which prevented later display-list work from completing. The native VIF1 path
now walks source-chain tags, handles transfer-tag data, uploads MPG payloads
to mapped VU1 micro memory, unpacks the observed vector formats to VU1 data
memory, and sends DIRECT packets to the GIF reader. It also completes the DMA
status and interrupt boundary. The port-side VU1 translator now executes a
partial microinstruction set, including integer loads/stores, and the VIF path
tracks VIF1 TOP/TOPS/ITOP state across MSCAL calls. A native run showed that
OFFSET must clear DBF and rebase BASE to the current TOPS; the prior port
service reset TOPS to the old BASE. Correcting this produced 120 vertices. A
second renderer fix decodes PACKED XYZ's Y field from its 128-bit position
instead of the native 64-bit register layout; the latest trace places 58
vertices inside the display viewport. The parser also normalizes PACKED RGBA
and UV fields. A later XGKICK still encounters a non-tag, and no recognizable
scene has been verified. The latest trace has no BITBLTBUF or TRX register
writes, so IMAGE payloads cannot yet be mapped to texture destinations.

A VIF trace of the first populated display-list transfer sees 19 MPG uploads,
25 MSCAL/MSCALF calls, V2/V3/V4 UNPACKs and two DIRECT packets. Before the
OFFSET fix, a 15-second run showed accepted XGKICKs carrying primitive-state
writes without coordinates; a later kick started at zero-filled VU data and
encountered a value rejected as a GIF tag. After the fixes, 120 vertices are
published and 58 are within the viewport, but a later kick still encounters
an invalid tag. The game also advances large IMAGE payloads, which the host
currently counts and discards. No visible title scene or playable renderer
has been verified. Set `OPENRAC_VIF_TRACE=1` to capture the first two
populated transfers.

Translating `func_001B93D8` from retail assembly fixed a zero-argument C
placeholder that discarded callback registers. A separate native MMIO handler
accepts GIF channel 2 transfers and queues their source-chain payloads. The
port now also supplies functional replacements for the matched but
hostgen-incompatible varargs bridges `func_001E1FD0` and `func_001E20B8`.
Their formatter writes into checked guest memory and handles the common integer,
string, pointer and floating-point conversions. These replacements live only
under `port/game/sly1-us/hand/`; they do not change matching code or counts in
`sly1-decomp`.

The refreshed hostgen report now has zero matched-C stubs and a 12-function
reachable frontier. It still reports 16 source units that its C frontend
cannot read directly; their retail assembly provides translated fallbacks for
the current build. A bounded native run after the formatting backfills stayed
at the semaphore wait (`func_001F6B60`) with zero rendered vertices, so the
backfills have not yet produced a visible scene or playable state. Only the
ordinary 2048-byte sector mode is supported. Continue along the loader and
frame/render paths, using OpenRAC's host-native workflow and grounding Sly
behavior in its retail code.

The port also enables syscall 0x30 and returns a `ReferThreadStatus` record;
this remains inside the experimental MIPS assembly/guest-scheduler executor.
It builds, but a fresh bounded trace still ends at `func_001F6B60` with zero
rendered vertices. It does not demonstrate progress toward a native playable
path. The report has 11 reachable functions without C, most of them Sly's
handwritten syscall/interrupt path. That frontier is not the main architectural
blocker: the current launcher executes translated retail instructions and
simulated kernel calls, while the target architecture is compiled decompiled
C++ plus native services. Stop extending the device-model and syscall shims
as gameplay features. The launcher may be used for time-bounded research only.

The next implementation milestone is to replace the MIPS assembly execution
path with compiled Sly C++ and native library boundaries, reusing OpenRAC's
generic runtime/platform pieces where their licenses and dependencies permit.
The first task is a source-build audit: compile a representative constructor,
utility, loader and frame unit directly as C++ against an address-preserving
host ABI; inventory failures and library calls from the decomp sources; and
confirm hostgen has not made the translated retail instruction executor a
required dependency. Then connect the Sly frame data to a native renderer.
See [`OPENRAC_REUSE.md`](OPENRAC_REUSE.md) for the reuse inventory and revised
sequence. The current port is not playable.

The existing hostgen report says its Sly staging is not complete: 17 of 68
source units are unreadable. The logged causes include conflicting per-file
prototypes for shared functions, C++ constructs rejected by the C frontend,
and includes that the generated compile command cannot resolve. The other 51
units compile as generated C; this is only a partial link check, not a working
C++ build. The next adapter pass must compile a representative set of source
units directly as C++ and retain hostgen only for the guest-address mapping,
function table and any compatible translations that are still useful.

The order can change as source research reveals dependencies. Keep each phase
grounded in evidence from Sly's retail code. Continue function matching in
`sly1-decomp`; prioritize functions and data on the loader, boot, frame and
rendering paths once those paths are identified. Generated code and game
assets stay in ignored local build/data directories.

## Native C++ path update

The port now compiles all 68 current US decompilation source units directly as
32-bit host C++ through `port_compile_decomp_host32`. A standalone native i386
probe also called the decompiled `func_00137820` and observed its expected state
update. This keeps the source checkout untouched and avoids hostgen's C
lowering for game logic. The host ABI header restores 32-bit pointers and
64-bit Sly integer types; the staging pass widens the two PS2 `UL` literals
that would otherwise truncate on i386. This is an object-compile milestone,
not a linked game or a playable build.

The native combine step aliases differing C++ prototypes by their retail
function address and maps `D_XXXXXXXX` globals to their encoded retail address.
It combines the 68 objects into one relocatable i386 object, resolving all 373
data references and 1,310 function references. Its generated map has native
addresses for 2,319 catalogued functions. The remaining link frontier is 603
distinct function addresses (876 mangled symbol names); these functions do not
have compiled definitions in the current source units. The next native work is
to resolve the function closure needed for startup and level loading, then
replace that path's platform calls with native services. Do not stub this
entire unresolved set and call the result a game.

`RetailDataMap` maps the ELF load segments into a 32-bit process as non-
executable data and can patch static data words that match known function
addresses. A local native process mapped the user's ELF and read the catalog
key `0x2` at `0x00247AF0`; the pointer patcher also replaced the 145 repeated
static entries for one known retail function. These are address-space
primitives only; the native map has not yet been connected to a game entry
point.

The first native control-flow target is now identified. The retail boot stub
`func_00100008` performs PS2 setup and transfers to `func_00185758`. That
assembly-only routine initializes persistent game state, then runs the frame
loop; each iteration calls `func_001E9108` for the current state and
`func_001607D0` for frame timing/update work. `func_001E9108` is also
assembly-only and dispatches on a state field through a nine-entry table.
Neither function has a compiled definition in the 68 current C++ units, so
the native launcher could not enter the game's frame path. Port-only C++
translations of `func_00185758`, `func_001E9108` and the state-transition
helper `func_001E96F8` now live under `port/game/sly1-us/native/`. The native
compiler keeps them separate from `sly1-decomp` and the combine step rejects
duplicate retail addresses if matching C/C++ later becomes available. These
backfills compile, but the launcher still lacks a native entry and the loop's
initialization, resource-loading and platform dependencies are unresolved.
Implement those specific boundaries next; do not generate generic no-op
definitions for the full unresolved frontier.

The current host32 combine artifact records 1,350 function-reference aliases,
379 retail data aliases, 2,351 native function addresses, and 599 unresolved
function addresses across 68 decompilation units and 27 port-native backfill
objects. The direct frame-call frontier remains 57 of 89 functions defined;
the report does not follow indirect renderer callbacks. New backfills for
`func_00185D20`, `func_001860A8`, `func_001278A8` and `func_001280C0` prepare
Sly's draw entries, transform their basis data and dispatch object callbacks.
No native renderer consumes these entries yet. These are port-only
implementations, not matching decompilation. The unresolved count is a
whole-program static frontier, not a count of functions needed by the boot
path. Rebuild it with
`cmake --build build/port --target port_combine_decomp_host32`; its report is
kept in the ignored `build/port/host32/link/link-frontier.json`.

The standalone C++ catalog reader reproduces all 46 NTSC-U decoded ranges
byte-for-byte. Resource `0x0100` is “A Stealthy Approach,” but its scene records
and renderer inputs are still not identified. Keep the old guest executor
isolated as research; it is not a gameplay dependency.

The SDL frontend's pressure-mode pad report now feeds port-only Sly controller
setup and button-edge polling backfills. The native frame clock uses a 60 Hz
host cadence, and the native synchronous resource path retires its pending-load
gate. Startup initializes the frame-state object and global game-manager
vtables. Recent `hand/native_c` backfills initialize the title/menu object,
request its first page, push it onto the overlay stack, and open it. The trace
now advances the frame-state object from state 3 to state 5 and keeps its overlay
stack stable at two entries; this fixed repeated menu insertion and frame-state
corruption. These remain port-only implementations, apart from matching
decompilation code. Port-only UI frame dispatch now walks the 37 retail UI
objects and invokes their update and draw callbacks. Port-only world-update
dispatch in `func_0019D6C0` now walks its three retail object lists with
guest-address scratch frames. The frame-state backfill now clears the retail
input flags and pushes the selected map node with the correct two-argument
overlay call. The latest trace reports all three world lists empty (`0,0,0`)
and the map selection null; the current world object has not received level
content. The current hostgen build compiles and links 146 C units (182
translated functions). The native display worker submits hundreds
of initialization GIF chains, but the window is still black and no scene
geometry reaches the renderer. The title menu's `func_001936A0` page population
and `func_00194F80` drawing callbacks still have no C implementation in the
port. It is not playable; scene activation, resource-to-scene decoding, title
page population, rendering, audio and further platform services remain.

The 2026-11-05 loader trace clarifies that the Splash read is not a failed
level load: key `0x2` reaches parser stages 1–5 with decoded cursor `0x15af`.
Its first records are flag `0`, value `1`, and table count `1491`; the parser's
oversized-count escape path is not taken. The earlier `0x6876bb00` diagnosis
came from interpreting bytes overlapping the compressed header as a parser
count. The next visible milestone is a rendered title menu, then identifying
and loading the distinct level resource.

The 2026-11-06 reader review corrects the next Splash lead. The native
coordinator has already read the complete 150,849-byte descriptor into
`D_002C3B00`; the missing port definition of `func_00137490` is the retail
asynchronous sector-ring worker, and the current `sly1-decomp` checkout has
matching C for it. The no-C stub is reached only after the synchronous source
is consumed, so implementing that worker alone will not resolve the EOF. The
native trace reaches a `0xFFFF` face count in the second collider and then
exhausts the decoded stream. The earlier object-list transition is now the
first suspicious point: a plausible type-4/key-1 collider returns at decoded
offset `0x1594`, then the next record reads type `0x5300`, outside the retail
descriptor table. A later type-5 record reaches the 6,626-vertex collider;
its `0xFFFF` face count is present at decoded offset `0x14CE1`. A separate
windowed translation of retail's LZ routine matches the offline decoder, so
the bytes are not explained by a decoder mismatch. Compare the first object's
stream consumption with its retail method chain and check the world sections
before it. The offline decoder emits 430,858 bytes while the resource header
begins with `0x5F4F0` (390,384); establish whether that header size limits a
subresource or the whole decoded payload. Any resulting functional backfill
stays in the port repository. The port remains not playable.

The menu drawing boundary is deeper than `func_00194F80`: its text path calls
`func_0015C8B8`, `func_0015DB28`, `func_0015DFF8`, `func_0015E1B0`,
`func_0015E318`, and `func_0015C900`, all currently unresolved assembly in the
64-bit hostgen build. Hostgen also cannot read `file_193210.cpp` as C because
the unit uses C++ inheritance and scoped helper methods. The separate direct
C++ build does compile that unit, but its SDL launcher requires a 32-bit SDL3
runtime that is not installed in this environment. A useful menu milestone
therefore needs a native font/glyph rendering path and working menu callbacks,
not only a backfill for the high-level draw function.

The 2026-11-06 cursor follow-up adds stages after the optional world blocks,
buffered string, and animation tables. It confirms the pre-list cursors at
`0x49`, `0x69`, `0x6B`, `0x8B`, and `0x14D5`; the animation table ends where
the 36-row object list begins. `resource_readers.c` and the table read widths
were checked against current decomp C and match at this level. Next inspect
`func_001C1190` script values and `func_0011EC40` property-specific reads in
`resource_value_parser.c`; do not infer that valid table boundaries imply
valid world-property interpretation. This diagnostic narrows the next review
target but does not advance visual output or playability.

The latest source review compared the Splash pre-list readers against current
decomp `cf9ad464696a197803f584980bff43ed62b6bf31`. Script-value/property
parsing, object-list headers, collider prefix reads, and placed-object fields
match their retail serialized widths in the captured path. For the first
collider, `func_001289C0` starts at decoded `0x1538`, reaches
`func_00162890` at `0x1587`, and that nested reader consumes six bytes to
`0x158D` with zero records. The zero transform flags, empty group count, and
empty child list then end at `0x1594`. Its actual descriptor dispatch reaches
the retail C03 reader `func_001BAC30`; that callback parses a zero-vertex,
zero-face first mesh before returning at `0x1594`. The next bytes decode as
`00 53 00 4B 05 FF`, and a later C03 starts at `0x1643` with 6,626 vertices.
These facts alone do not establish a row-boundary error. The retail image
callback reads a separately stored byte count after aligning the stream to 16
bytes. The Splash decoded size is 430,858 bytes and the image request is
`0x5F4F0` (390,384), leaving `0x9E1A`; this arithmetic is a useful lead but does
not prove that the callback's cursor is `0x9E1A`. The previous trace did not
capture the image header revision/count or cached revision. Add trace points at
`func_001DB928` and before/after alignment in `func_001DBA38`, then compare the
native state to current retail cache control flow before changing reader or
geometry behavior. Keep the `func_001B89B8` translation and other functional
backfills port-only. The frame remains black and the port is not playable.

The 2026-11-06 native collider pass adds a port-only scalar translation of
`func_001B8960` and its retail VU mesh transform. It runs the existing
`func_00125138` world-frame update, transforms vertex and face-normal arrays
for both collider meshes, recomputes each face plane against the transformed
vertex, and updates the per-face vectors. The mesh view follows the serialized
`Mesh03` offsets (radius, vertex count/pointer, face count/pointer, link
capacity/pointer, face vectors). The native build succeeds and the bounded
Splash run no longer reports `func_001B8960` as a no-C stub; the cursor still
reaches `0x1594`, and no geometry reaches the renderer. This is a collision
state improvement, not visual or playable progress. The remaining collider gap
in this path is the handwritten bounds callback `func_001B8540`. A separate
port-only backfill now covers root registration and contact-pair bookkeeping
(`func_00129DF8`, `func_001385A0`, `func_00138738`, `func_00138788`, and
`func_00138F28`). The port also translates `func_001B9680`, which copies the
world position at `+0x140` to the previous-position field at `+0x370`.

## Current visual checkpoint (2026-11-07)

The object-record override advances through initialization to the active frame
loop without the earlier null indirect-call or `0x02000000` write errors. This
does not establish that every Splash record is correct or that the world has
renderable objects. A fresh native trace reaches the Paused menu page and its
draw callback (`0x194f80`), emits glyph records, and uploads the font atlas and
palette. The user-visible output remains black or garbled, so callback
execution is not visual success.

`OPENRAC_TRACE_DIRECT=1` shows a repeating 912-byte packet that decodes to one
240-vertex, untextured draw. The first vertex has zero UVs and black RGBA; its
target, source and texture are all zero. A separate 144-byte packet decodes to
no vertices. A new trace confirms menu text generation and atlas/palette
upload, then decodes a 136KB frame packet with three draws. The glyph draw has
474 vertices (79 quads), uses atlas texture handle `0x3`, and carries valid
nonzero UVs and near-white vertex color. The atlas (TBP `0x1400`) and CLUT
(CBP `0x1600`) both resolve. Existing PPM captures were taken before the
136KB textured menu packets begin, so they do not show the glyph draws. The
first two VIF1 transfers emit 736 DIRECT bytes each; transfer 1 also loads
eight MPG blocks, and neither transfer contains MSCAL. Later worker transfers repeat
the same DIRECT-only pattern. The next diagnosis is why a decoded textured
glyph batch produces no visible pixels, comparing its atlas samples and
scissor/depth/blend state with the framebuffer.

The active investigation is now the final draw and framebuffer result for the
decoded textured menu glyph batch, the first scene object with attributable
mesh data and a valid world transform, and the meaning of the repeated black
packet. The sampled update-list entries remain zero-position, zero-radius
objects; establish their class and role before changing culling.
See the ordered tasks in
[`PLAYABLE_TODO.md`](PLAYABLE_TODO.md). The function report is 1,834 translated
functions (90 static assembly translations), 3 stubs, and 3,030 without C;
coverage does not indicate which functions execute in the active draw path.

One font-traced launch aborted during DMA-chain vector reallocation. Its
backtrace identifies where allocator corruption was detected, not its source.
A later font/DMA trace and a 20-second `MALLOC_CHECK_=3` run did not reproduce
the abort, so no allocator workaround is justified yet.

The configured decomp source is `dd6f80b`; upstream master has advanced to
`04f7f272` (149 commits ahead). Three newly exact matches in its US report are
`func_0013E650` (4 bytes), `func_0018B670` (memory-card-only), and
`func_001FFC08` (library code). None has been shown to unblock the active draw
path. See [`PLAYABLE_TODO.md`](PLAYABLE_TODO.md) for the source comparison and
the ordered next tasks.

## Direct-level follow-up (2026-11-07)

The native direct-level probe now completes the port backfill for
`func_00162890` and the following `func_00128C10` read, then faults in the
child-object path around `func_0011ECB8` / `func_00137D10` at guest address
`0xFFFFAC70`. It has not reached gameplay. Captured entry/exit windows for the
large object parser match the decoded level resource, so that parser's stream
cursor is not yet the demonstrated fault source.

Comparing the port backfill for `func_00162890` with retail MIPS exposed one
memory-layout mismatch: component references are 32-bit entries, not packed
16-bit entries. The backfill now writes at four-byte strides. A native rebuild
succeeded; a direct-level launch still exits with the child-object fault, as
expected since the correction does not alter stream consumption. The fix stays
in the port repository and can be replaced when the decomp supplies this
function.

The black/garbled menu remains a separate graphics blocker. The next tasks are
to trace the first child-object read and its callback metadata, and to follow
valid glyph vertices through GL upload, raster state and final framebuffer
pixels. See [`PLAYABLE_TODO.md`](PLAYABLE_TODO.md) for the ordered plan and
acceptance gates.

## Current source/build checkpoint (2026-11-07)

The native build uses the local `sly1-decomp` checkout at `e1b39f1` for both C
and retail assembly. Hostgen accommodates the current staging shapes and skips
configured assembly fallbacks when matching C is available. The latest run
generated 2,051 C functions and 84 retail-assembly functions, with zero stubs,
2,729 functions without C and 12 unreadable units. The build still uses
`--allow-unreadable`; those units are omitted from the executable.

The port build now translates seven core stream readers and two refill helpers
from unreadable unit `00134000` using the retail US assembly. A bounded native
launch consumed 8,184 bytes from the Splash resource, then failed two
allocations because the port-only allocator saw current guest pointer `0x160`
and limit `0`. It faulted at host address `0x8`. The reader stall has
advanced, but heap initialization is now the earliest demonstrated runtime
blocker. See [`PLAYABLE_TODO.md`](PLAYABLE_TODO.md) for trace and the ordered
next tasks.

The isolated retail-ELF analysis also corrected array multipliers in the
port-only `func_00162890` backfill. Those parser sizes still need runtime
validation because the active run has not reached that parser. All backfills
remain separate from matched/generated decomp C. No level scene or
controllable gameplay is established.

## Native level-resource follow-up (2026-11-07)

The missing allocator setup was retail `func_0018D430`, which now runs from
US assembly. It initializes the heap at the aligned loaded-image end through
`0x02000000`; guest allocations now succeed. The latest hostgen report has
2,051 C functions, 105 assembly translations, zero stubs, 2,708 without C, and
12 unreadable units.

The bounded direct-level launch reaches sector 867214 and advances through
loader stages 3–5 with active world `0x0088d530`. It remains a black, non-
playable frame: the UI reports zero registered fonts and no visible text, and
no scene mesh reaches the renderer. `func_0016BA40` reports a null font-vtable
callback. A port-only `func_001ACDA0` override skips that callback only while
font slot 0 is absent. Enabling retail assembly `func_001B3510` instead causes
an invalid guest-address fault (`0x0C1AE80F`) and remains disabled pending
input-data analysis. Latest bounded trace:
`/tmp/sly-native-stable-probe.log`.

The immediate work is to trace font registry population and validate the
resource cursor/allocation sizes against retail, then carry a real level mesh
through native GS/OpenGL submission. See [`PLAYABLE_TODO.md`](PLAYABLE_TODO.md)
for the ordered tasks and reproducible command. These runtime findings do not
establish a playable level.

## Retail animation-parser update (2026-11-07)

The exact US retail animation table loader, element parser and class lookup
helpers are now selected from assembly: `func_001B3040`, `func_001B2F00`,
`func_001B4920` and `func_001B4980`. This replaces the earlier reconstructed
table parser; matching `func_001B2BA8` and `func_001B2CF8` C remains in use. A
port-only `func_001B4A10` factory guard skips unsupported type IDs while
consuming their fixed 0x1C-byte payload. No decomp source was edited.

The current build reports 2,043 C functions, 109 retail assembly functions,
zero stubs, 2,705 functions without C and 12 unreadable units across 185
units. A 12-second native run reaches loader stage 5 for the direct level and
keeps active world `0x0088d530` alive. Animator records now have supported
retail types and valid objects/vtables. The frame still has zero scene
vertices, no registered fonts and no visible text. The next visual milestone
is identifying and submitting the first actual level mesh; see
[`PLAYABLE_TODO.md`](PLAYABLE_TODO.md).

## Retail object-list and VIF trace correction (2026-11-07)

The old port-only `func_0011ECB8` parser was removed from the active build and
retained under `hand/superseded/`. Retail assembly proves it reads counted
object records (class slot, key, and flag) and invokes each object's virtual
reader. `func_0011EC40` is the separate property/callback-ID reader. The
native target now uses exact retail assembly for `func_0011ECB8`,
`func_001AD970`, and `func_00202C38`. Hostgen reports 2,044 C translations,
111 assembly translations, zero stubs, 2,703 functions without C, and 12
unreadable units.

A 10-second direct-level run still reaches stage 5 with active world
`0x0088d530`, but reports zero scene vertices, zero registered fonts, zero
visible text, frame state 3, and game mode 0. The startup trace also captures a
48-byte inline VIF1 chain from `0x00212110`; opcode `0x40` at stream offset
`0x14` is absent from the [PS2SDK VIF command table](https://ps2dev.github.io/ps2sdk/vif__codes_8h.html).
Retail startup writes `VIF1_ERR=2`, so the invalid-command mask appears unset.
Confirm whether this packet intentionally exercises VIF error handling or is
malformed before changing the native interpreter. This startup packet does not
yet identify the missing level mesh producer. Traces:
`/tmp/sly-native-vif-failure-context.log` and
`/tmp/sly-native-dmawait-retail.log`.

## Level object-list and scene callback follow-up (2026-11-07)

The exact US assembly for `func_0011ECB8` reads a signed record count, then
class slot, key, and flag for each entry, creates it through `func_0011E580`,
and calls the object's virtual reader. A fresh direct-level trace brackets this
list at cursor `0x70001854` / 1,964 bytes remaining and reaches the ring boundary
on return. It creates 112 records: 110 type-0 entries, one type `0x86` (key 2),
and one type `0x04` (key 1). The type-`0x04` instance loads collision geometry;
the list contains no evident visual-object class. `func_0011EC40` is the
separate property/callback-ID reader.

The class-`0x86` constructor `func_001EE900` was a no-C stub despite being
reached. The port now selects its exact NTSC-U instructions, which call
`func_00182C18` and store the object at owner `+0x1EC4`. Exact assembly is also
selected for its transform helpers `func_001EE9D0`, `func_001EEA40`, and
`func_001EEB60`. A bounded native run confirms the special pointer is
`0x00a7e540` and root classes include `0x86`; those no-C warnings are gone, but
the frame still has zero scene vertices and game mode 0. Build and trace:
`/tmp/sly-native-scene-callback-cluster.log`.

The active gap is now upstream of visual object creation or the Splash-to-title
state transition. Do not count collision mesh data as rendered geometry.
