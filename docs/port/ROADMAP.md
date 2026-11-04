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
samples. The LZ decoder matches the captured retail output window and reaches
a structured Sly reader. Level/model formats and a native scene renderer still
block the first playable scene.

## Current no-emulation build status

The no-emulation host32 path directly compiles all 68 Sly C++ source units and
eleven separate port-native backfill translation units, then combines them by
retail address.
The current combined object contains 2,341 native function entries out of
4,766 catalogued addresses and still has 607 unresolved function addresses.
Recent catalog backfills implement the resource-key lookup used by compiled C
and a catalog flag counter whose caller remains assembly-only. This is an
object build only: there is no linked native game executable or native service
entry yet. The older C-only hostgen launcher still depends on the experimental
guest runtime and does not count as the no-emulation gameplay path. The retail
MIPS output remains isolated in optional `us-asm-research` output. Build
artifacts live under ignored `build/port/`; port-native backfills stay outside
the matching decompilation in `port/game/sly1-us/native/`.

The separate direct-C++ path compiles all 68 source units and three port-only
frame backfills as i386 objects, then combines them with retail-address aliases.
It has no executable or native service entry. A Fedora 44 i386 build probe also
showed that the reused shared guest runtime cannot simply be linked into that
process: it relies on `__int128`, which the i386 compiler does not provide.
Keep the useful OpenRAC infrastructure, but build a native-only service layer
instead of pulling the guest instruction/device runtime into gameplay.

These are build milestones only. The next step is to replace missing calls on
the boot and frame paths with accurate matching source or isolated port
backfills, then connect the frame's game-produced drawing packets to a native
renderer. Continue through scene loading, input, audio and saves. Do not count
assembly translation, a link check, or a black window as playable progress.

The host32 combine step now also writes a direct-relocation frontier rooted at
the port's native frame-loop backfill, `func_00185758`. In the current build,
46 of 75 functions reached through function-address relocations have compiled
definitions; 29 are missing. This is narrower than the 607-address whole-game
frontier and gives the next backfills a concrete starting list. It does not
follow indirect calls through retail vtables or callback tables, so it is not
the complete runtime closure. The machine-readable report is in
`build/port/host32/link/link-frontier.json`.

The native frame path supplies `func_001430F8`, the Sly Count read, with a
Linux host monotonic clock scaled to 294.912 MHz, and `func_001FB6B0`, the
64-bit tick conversion used by Sly's timers. Native backfills also implement
the frame-flag clear at `func_0016E118` and the reference-count transitions at
`func_001DDA20` and `func_001DDA50`. These resolve five functions in that
direct frontier without executing console instructions. They remain
port-only backfills, separate from the matching decompilation.

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

The current host32 combine artifact records 1,316 function-reference aliases,
379 retail data aliases, 2,322 native function addresses, and 616 unresolved
function addresses across the compiled decompilation plus these backfills. The
backfills expose additional unresolved calls; this is a whole-program static
frontier, not a count of functions needed by the boot path. Rebuild it with
`cmake --build build/port --target port_combine_decomp_host32`; its report is
kept in the ignored `build/port/host32/link/link-frontier.json`.

The standalone C++ catalog reader reproduces all 46 NTSC-U decoded ranges
byte-for-byte. Resource `0x0100` is “A Stealthy Approach,” but its scene records
and renderer inputs are still not identified. Keep the old guest executor
isolated as research; it is not a gameplay dependency.
