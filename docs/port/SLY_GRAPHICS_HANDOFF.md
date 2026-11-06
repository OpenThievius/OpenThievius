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

Channel 2 (`0x1000A000`) is a DMA chain and correctly goes through
`openrac_native_renderer_submit_dma_chain`; it must not be treated as a raw
GIFtag stream. In the latest bounded trace, 438 of 438 submitted chains were
accepted with zero rejects. Channel 1/VIF1 remains separate and goes through
the VIF/VU translation path before DIRECT or XGKICK GIF data reaches the
renderer. The raw GIF submission helper exists for genuinely raw streams, but
there is no current evidence that the menu callback supplies one.

The VIF1 runtime remains available for actual VIF1 work. It collects bounded
DMA chains, including references into the port heap at `0xE0000000`, interprets
VIF commands, runs the existing VU1 instruction translator on MSCAL, and
forwards DIRECT data or XGKICK output to the GIF/GS parser. Its runtime API is
listed in `port/game/sly1-us/hostgen.json` to avoid a colliding missing-function
stub.

## Current measured blocker

A bounded run on 2026-11-06 opens the window but remains black; the renderer
reports zero vertices and draws. An earlier font trace reached menu glyph
lookups, with every call receiving the same screen X/Y. This suggests zero
font scale/advance, a float-return bridge problem, or another layout issue,
but it does not yet prove which one. The latest fresh run accepted all sampled
GIF DMA chains, so packet routing itself is not the best-supported blocker.
GS display-register writes and earlier startup transfers do not establish a
visible frame. The first visual milestone remains one valid texture/image
transfer and at least one nonempty draw with distinct screen coordinates.

To capture the failure and first words of the stream, run:

```sh
OPENRAC_TRACE_FONT=1 OPENRAC_TRACE_DMA_CALL=1 OPENRAC_STARTUP_TRACE=1 timeout 12s \
  build/port/sly1_native build/port/extracted/boot/SCUS_971.98 \
  '/home/reaper/pcsx2/Games/Sly Cooper and the Thievius Raccoonus (USA).iso'
```

## Work needed next

1. Trace the font at `func_0015E318` and `func_0015CD18`: log font `sx/sy`,
   glyph width, computed advance, and the return value received from
   `sly1_native_call_f32`. Confirm whether each glyph's x position changes.
2. If scale and advance are nonzero but coordinates do not move, inspect the
   host call bridge's MIPS integer/FPU register mapping and float return path.
   If coordinates are valid, decode the submitted DMA/GIF chain and identify
   the first unsupported GS command or state. Verify tag/payload boundaries
   only when a specific malformed boundary appears in the trace.
3. Capture a window image after the fix and require at least one decoded
   nondegenerate draw; then verify texture uploads and the complete menu frame.
4. Continue through title and level loading, controller input,
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

No additional disc image, boot ELF, or other user-supplied file is blocking
the current work on this machine. Contributors elsewhere need their own
NTSC-U `SCUS_971.98` and disc image; setup and one-time asset extraction are documented in
[ASSETS.md](ASSETS.md). The current refreshed read-only decomp snapshot is
`/home/reaper/.cache/sly1-decomp-current` at `cf9ad46`. Hostgen's latest
generated report is `build/port/hostgen/us/report.md`.

The best immediate contribution is the font scale/advance and host-call
bridge trace described above. It needs comfort with MIPS o32 calling
conventions, float arguments/returns, the V24A2 font layout, and native C
backfills. Start with `port/game/sly1-us/hand/native_c/font_text.c`,
`port/game/sly1-us/hand/native_c/menu_text.c`,
`port/game/sly1-us/hand/native/sly1_native_worker.cpp`,
`port/sly1/main.cpp`, and `port/renderer/direct.*`. A second useful track is
to inspect `build/port/hostgen/us/report.md`, trace a function on the actual
menu/boot/level path, and implement one missing function as a port-only C
backfill. After the menu draws, contributors can work on Sly's scene records
and level assets, controller-to-pad integration, collision, audio, and saves.
To move toward playability, the project needs: (1) a verified visible menu
frame; (2) a populated title/level path with Sly-specific scene rendering;
(3) SDL controller state wired into the game's pad-library interface; and
(4) working movement/collision, audio and save behavior. OpenRAC's generic
host runtime is reusable; Ratchet-specific world records are not.

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
