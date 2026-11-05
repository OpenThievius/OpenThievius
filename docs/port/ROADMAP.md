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
samples. The native LZ backfill now consumes the 150,849-byte Splash stream
from the retail source fields, and its first 8 KiB begins with the same bytes
as the independent offline decoder. A bounded loader-stage trace confirms the
Splash parser reads a zero resource flag, a value of 1, and a plausible table
count of 1491, then completes its callback. The earlier `0x6876bb00` report
mistook overlapping compressed-header bytes for a parsed entry count; the
defensive size guard is not taken by this run. Splash is catalog key `0x2`,
not level resource `0x0100`. Startup reaches the title menu, but the native
window is still black: menu page population/drawing is incomplete, the world
lists are empty, and no scene geometry reaches the renderer.

## Current no-emulation build status

### Decomp synchronization (2026-11-05)

The separate working checkout remains untouched. The native build uses a
read-only worktree at `/home/reaper/.cache/sly1-decomp-latest`, refreshed from
[`OpenThievius/sly1-decomp`](https://github.com/OpenThievius/sly1-decomp)
upstream at `a0fd295` (PR #54, Nov 5). The decomp report is matching coverage,
not runtime or playable-game coverage. Host-only staging adaptations keep the
PS2 source unchanged while compiling it for i386.

Against `a0fd295`, all 84 selected Sly C++ units and 45 port-native backfills
compile. The combiner reports 3,810 native definitions for 4,806 catalogued
functions and 727/788 functions defined in the direct-call frontier (61
missing). The build includes partially covered 0x4000 windows alongside
whole-file units. Matching functions now take precedence over former
port-only shadow backfills; platform service replacements remain separate.

The latest bounded startup run passes both DMA callback waits and reaches the
startup worker in `func_0018B580`. The graphics worker submits native GIF
traffic, but the window is still black and the ISO receives no sector reads.
There is no playable scene yet.

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

The parser proceeds into the resource's camera and collision-object paths, but
the latest run stops its loader thread at missing `func_001388A0` while the
active frame workers continue submitting GIF traffic. It remains a black
window with no verified scene or gameplay. Several port-only camera/VU
boundaries (`func_00144540`, `func_00144BE8`, and `func_00125138`) are explicitly
temporary no-ops; the mesh boundary `func_001B8820` only preserves copied
geometry and does not apply the retail VU transform. These gaps cannot support
a playable scene and must be replaced with real native renderer/collision
behavior.

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
callback returns through stages 1–5. This does not establish successful
resource parsing: calls such as
`func_00151A88`, `func_001C1190`, and `func_001B3040` still resolve to no-C
stubs, so successful resource parsing is not established. The host window
remains black; there is no display framebuffer setup, blit, or verified active
scene. Treat this as loader-path diagnostic evidence, not a playable
milestone. The coordinator also omits retail progress/job bookkeeping. The
factory's required class
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
architecture. Its latest bounded run reads the cached Splash sectors and
matches the offline decoder's initial output bytes, but the native parser
stalls in a resource-skip routine. It does not configure a display framebuffer;
no scene reaches the renderer. Do not treat that run as evidence of native
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
