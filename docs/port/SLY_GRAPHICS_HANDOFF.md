<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Sly 1 native graphics handoff

Last checked: 2026-11-06. This document records the live native C-only
launcher state, not a playable milestone. The launcher opens a window and
reaches Sly's frame/menu work, but the rendered image is black. No emulator is
used by this path.

## What runs today

Build and launch from the repository root:

```sh
cmake --build build/port --target port_native -j2
build/port/sly1_native build/port/extracted/boot/SCUS_971.98 \
  '/home/reaper/pcsx2/Games/Sly Cooper and the Thievius Raccoonus (USA).iso'
```

The source ELF and ISO above are already present in the current development
environment. The executable is a host-native launcher linked with SDL/OpenGL
and OpenRAC renderer code; it is not PCSX2 or another emulator. The current
experimental game path still uses guest-addressed memory and small EE DMA,
VIF1, VU1 and GS compatibility services. Those services are a bridge for
diagnosing Sly's draw stream, not the intended architecture for a finished
port. The target design remains native game code, native platform services
and Sly-specific draw-data adapters; see [DESIGN.md](DESIGN.md) and
[OPENRAC_REUSE.md](OPENRAC_REUSE.md).

The display path reaches port-only `func_00202A80`. The `0x08000000` word in
the earlier chain dump begins a GIF IMAGE tag with 8,192 qwords; it is not a
VIF opcode. The port-only initializer now fills `D_00261F1C` from the channel
table at `0x00279340`; traces show GIF channel 2 at `0x1000A000` and VIF1 at
`0x10009000`.

The current failing call is the font/text packet writer. It passes a raw GIF
packet buffer beginning with a GIFtag (captured buffer length about 992 bytes),
not a DMA-tagged chain. The `func_00202A80` backfill currently passes that
pointer to `openrac_native_renderer_submit_dma_chain`, which parses the GIFtag
as a DMAtag. The runtime has `openrac_native_renderer_submit_gif_stream`, but
it currently assumes one image-style tag size, does not account for FLG/NREG,
and does not continue through EOP. It is not called here. Keep the VIF1 route
separate: VIF1 chains still go through the VIF/VU translation path before
DIRECT or XGKICK GIF data reaches the renderer.

The VIF1 runtime remains available for actual VIF1 work. It collects bounded
DMA chains, including references into the port heap at `0xE0000000`, interprets
VIF commands, runs the existing VU1 instruction translator on MSCAL, and
forwards DIRECT data or XGKICK output to the GIF/GS parser. Its runtime API is
listed in `port/game/sly1-us/hostgen.json` to avoid a colliding missing-function
stub.

## Current measured blocker

A bounded run on 2026-11-06 reaches menu/frame callbacks and produces a
135,632-byte display chain. A separate font trace records a writer buffer at
`0x002C3B00` advancing to `0x002C3CE0`, with the final tag near `0x002C3BE0`.
This establishes that the writer produced a nonempty packet buffer. The
channel trace shows the expected GIF and VIF1 addresses. Renderer counters
still report zero decoded vertices and draws. GS display-register writes and
the four earlier 144-byte startup transfers do not establish a visible frame.
The first required renderer evidence is a complete EOP-terminated packet
stream, a successful texture/image transfer, and at least one nonempty draw.

To capture the failure and first words of the stream, run:

```sh
OPENRAC_TRACE_FONT=1 OPENRAC_TRACE_DMA_CALL=1 OPENRAC_STARTUP_TRACE=1 timeout 12s \
  build/port/sly1_native build/port/extracted/boot/SCUS_971.98 \
  '/home/reaper/pcsx2/Games/Sly Cooper and the Thievius Raccoonus (USA).iso'
```

## Work needed next

1. Fix the type mismatch at `func_00202A80`: route channel 2's raw GIF stream
   to the GIF-stream API, remove the unused DMAC register writes on this
   synchronous native path, and register the helper in `hostgen.json`. Keep
   channel 1/VIF1 on the VIF runtime path. Confirm the source pointer is a
   valid guest span and trace each channel/source pair.
2. Extend GIF parsing to walk packed, reglist, image, and disabled tags through
   EOP. Bound tag count and total bytes, reject truncated or invalid spans, and
   preserve image payload bytes. Check multi-tag packet boundaries; a single
single-tag size assumption is insufficient for packed text and image uploads.
3. Run the native launcher and confirm accepted packet bytes, a texture/image
   transfer, and at least one nonempty DirectRenderer draw. Inspect a captured
   window image. If the parser accepts data but draws remain zero, inspect the
   decoded GIF/GS commands and identify the first unsupported state or record.
4. Only then continue through title and level loading, controller input,
   controllable Sly, collision, audio, and saves. A menu screenshot is useful
   progress but is not a playable-state milestone.

## Port/decomp boundary

All functional replacements used to reach this point belong under
`port/game/sly1-us/hand/native_c/` and its registration tables. They are
port-only backfills and must stay replaceable when matching C appears in
`sly1-decomp`. Do not edit the decompilation checkout to make the port work.
The runtime transport and renderer adapters belong under `port/runtime/` and
`port/renderer/`; document any temporary guest-device behavior as such.

## What a new contributor needs

No additional disc image or boot ELF is needed on the current development
machine. Contributors elsewhere need their own NTSC-U `SCUS_971.98` and disc
image; setup and one-time asset extraction are documented in
[ASSETS.md](ASSETS.md). The current refreshed read-only decomp snapshot is
`/home/reaper/.cache/sly1-decomp-current` at `cf9ad46`. Hostgen's latest
generated report is `build/port/hostgen/us/report.md`.

The most useful self-contained work is the raw GIF stream adapter and its
verification. It needs comfort with GIFtag layout, PS2 GS image uploads,
guest-memory bounds, C backfills, and the existing C++ DirectRenderer. Start
with `port/game/sly1-us/hand/native_c/func_00202A80.c`,
`port/game/sly1-us/hand/native_c/display_packets.c`,
`port/runtime/sdl_renderer.cpp`, and `port/renderer/direct.*`; keep functional
game-code replacements registered in `hand/backfills.tsv`. A second useful
track is to work through the no-C frontier in `build/port/hostgen/us/report.md`
and choose one reachable function to implement as a clearly documented
port-only C backfill. A later rendering task is deriving Sly's scene draw data
and asset formats for a Sly-specific renderer; OpenRAC's Ratchet-specific
world-render records are not reusable for that.

To reproduce the current window and collect startup/renderer counters:

```sh
cmake --build build/port --target port_native -j2
OPENRAC_STARTUP_TRACE=1 timeout 12s build/port/sly1_native \
  build/port/extracted/boot/SCUS_971.98 \
  '/home/reaper/pcsx2/Games/Sly Cooper and the Thievius Raccoonus (USA).iso'
```

Set `OPENRAC_VIF_TRACE=1` only for actual VIF1 submissions. The ISO path above
is specific to this machine; another developer can supply their own NTSC-U
boot ELF and ISO. Keep functional game-code replacements in
`port/game/sly1-us/hand/native_c/` and register them in the backfill inventory,
so matching implementations in `sly1-decomp` can replace them cleanly.
Runtime/rendering changes belong in `port/runtime/` or `port/renderer/`.
