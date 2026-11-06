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

## Current visual blocker (2026-11-06)

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
level.

The latest bounded native level probe (2026-11-06) confirms that placed-object
registration now populates 77 nodes in each of world lists `+0x54` and `+0x60`.
The `+0x1BE8` class-0x51 scene list has one object at `0xE20A57B0`; its retail
fields are flags `0x0AFFFF00`, flags-high `0x120`, and zero at `+0x2D0`,
`+0x304`, `+0x308`, `+0x28C`, `+0x298`, and `+0x370`. Sampled update entries
call no-C `func_00125C90` and likewise have zero `+0x28C`/`+0x298`, so they do
not enqueue deferred draw requests. The frame still submits zero scene
vertices. Retail disassembly shows `func_00125C90` is a substantial camera,
object-transform, and culling callback; its missing body is why sampled update
nodes retain zero transform outputs. The deferred frame dispatcher and request
bucketer (`func_0019DC98` and `func_0019D4B0`) have separate port-only C
backfills. `func_00184DE8`, the assembly-only class-0x51 scene batch preparer,
now has a retail-derived port backfill and survives the native frame path. Its
single current scene object contributes zero VIF vectors, so the frame still
submits zero scene vertices. The next gameplay/rendering target is the missing
`func_00125C90` body. Keep all functional replacements under `hand/native_c/`
until matching decomp C is available to the host build.

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
The generated host report reads 196 units and translates 1,091 functions
(zero assembly translations, one stub, 3,799 functions without C), plus seven
port-owned overrides of matched C. Its source snapshot is `cf9ad46`.

The newest decomp source contains the matched screen-fade updater
`func_001EFD10` with its mode, timing and opacity logic. The current native
hostgen path still reports this function as no-C because its containing
`001EC000.cpp` unit has unrelated C++ helper constructs that staging cannot yet
lower. This source is useful for a later hostgen staging fix, but it is not
active in the launcher today. The port-only frame callback remains separate.

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

The latest direct upstream check is master `dd6f80b`; the generated host source
still comes from `cf9ad46`. Current source coverage is US 4,730/4,806 functions
(93.10% code) and Europe 4,738/4,831 (92.42% code). The latest merge adds
matching HUD compass drawing (`func_001316E8`) and placed-object link data, but
no matching C for current render blocker `func_00125C90`; `func_00184DE8` now
has a separate port-only backfill. The active hostgen report reads 196 units,
translates 1,091 functions, stubs one function, lists 3,799 without C, and
applies seven port-owned overrides. These are separate measures from decomp
coverage and do not establish gameplay readiness.

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
