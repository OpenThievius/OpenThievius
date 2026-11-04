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
The parser now consumes IMAGE2 payload framing correctly. After fixing the
VIF1 OFFSET buffer rebase below, a bounded run publishes 120 vertices; all are
outside the display vertically, so the window remains black. Texture sampling,
scene streaming and gamepad mapping are also missing.

VIF1 channel 1 was holding its source-chain transfer active at CHCR `0x105`,
which prevented later display-list work from completing. The native VIF1 path
now walks source-chain tags, handles transfer-tag data, uploads MPG payloads
to mapped VU1 micro memory, unpacks the observed vector formats to VU1 data
memory, and sends DIRECT packets to the GIF reader. It also completes the DMA
status and interrupt boundary. The port-side VU1 translator now executes a
partial microinstruction set, including integer loads/stores, and the VIF path
tracks VIF1 TOP/TOPS/ITOP state across MSCAL calls. A native run showed that
OFFSET must clear DBF and rebase BASE to the current TOPS; the prior port
service reset TOPS to the old BASE. After correcting that behavior, the same
bounded startup run publishes 120 vertices instead of none, though their
positions remain outside the screen. A later XGKICK still encounters a
non-tag, so the renderer has not produced a visible scene.

A VIF trace of the first populated display-list transfer sees 19 MPG uploads,
25 MSCAL/MSCALF calls, V2/V3/V4 UNPACKs and two DIRECT packets. Before the
OFFSET fix, a 15-second run showed accepted XGKICKs carrying primitive-state
writes without coordinates; a later kick started at zero-filled VU data and
encountered a value rejected as a GIF tag. After the fix, 120 vertices are
published but remain offscreen, and a later kick still encounters an invalid
tag. The game also advances large IMAGE payloads, which the host currently
counts and discards. No visible title scene or playable renderer has been
verified. Set `OPENRAC_VIF_TRACE=1` to capture the first two populated
transfers.

Translating `func_001B93D8` from retail assembly fixed a zero-argument C
placeholder that discarded callback registers. A separate native MMIO handler
accepts GIF channel 2 transfers and queues their source-chain payloads. These
port-side services do not modify the vanilla functions in `sly1-decomp`.
`func_001E1FD0` and
`func_001E20B8` remain unsupported varargs stubs. Only the ordinary 2048-byte
sector mode is supported.
The source bridge also has unreadable source files and C++ constructs that need
preserving; continue along the loader and boot call graph, grounding host
behavior in retail code or public hardware documentation.

The order can change as source research reveals dependencies. Keep each phase
grounded in evidence from Sly's retail code. Continue function matching in
`sly1-decomp`; prioritize functions and data on the loader, boot, frame and
rendering paths once those paths are identified. Generated code and game
assets stay in ignored local build/data directories.
