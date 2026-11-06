<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Sly 1 native graphics handoff

Last checked: 2026-11-06. This document records the live native C-only
launcher state, not a playable milestone. The launcher opens a window,
uploads Sly's font atlas and palette, and decodes menu geometry, but the
resulting image is black or severely garbled. No emulator is used by this
path.

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

A bounded run on 2026-11-06 accepts sampled channel-2 DMA chains, uploads a
512x512 PSMT4 font atlas at GS block `0x1400` and its 8x2 palette at `0x1600`,
and decodes two draws per menu frame. The atlas upload uses PSM `0x14` (PSMT4),
with 131,072 bytes for 512x512 4-bit pixels; the CLUT uses PSM `0x00` and
contains 16 entries. The background draw has 6 vertices; the
font draw varies from 372 to 480 vertices. Font positions change across
frames and the fade alpha rises from its initial low value. The float return
and glyph-advance path are therefore producing changing positions; the old
“zero vertices/draws” and identical-coordinate diagnoses are obsolete.

The normal renderer produces a very dark, noisy menu shape. A direct
framebuffer readback confirms the pixels are already dark/noisy before SDL
swaps the window. An opt-in flat-color pass that bypasses texture, depth,
scissor, and blend state draws the decoded menu geometry, so the vertex stream
is rasterized. The remaining failure is in texture sampling or GS pixel/state
interpretation, not in the final window presentation. The 512x512 PSMT4 upload
at `0x1400` has near-uniform index counts and looks spatially noisy when all 16
indices are visualized as grayscale. Reading the same bytes as an already
swizzled GS-memory image also does not produce a recognizable atlas. These
experiments do not establish which format/order Sly's source buffer uses; do
not apply a swizzle based on them. Trace the DMA REF source for the image data,
then validate PSMT4 indices, CLUT order/alpha, TEX0/UV selection, and shader
output against captured framebuffer pixels. The trace reports `TCC=true`,
modulate texturing, blending enabled, and GS alpha scale; those are evidence
to inspect, not proven faults.

To reproduce and inspect the texture conversion, run from the repository root:

```sh
OPENRAC_TRACE_DIRECT=1 OPENRAC_DUMP_TEXTURE=1400 timeout 12s \
  build/port/sly1_native build/port/extracted/boot/SCUS_971.98 \
  '/home/reaper/pcsx2/Games/Sly Cooper and the Thievius Raccoonus (USA).iso'
```

The dump writes `texture_1400_*.ppm` and matching `.pgm` alpha planes in the
current directory. Inspect those alongside `OPENRAC_TRACE_DIRECT` output. The
font/menu traces are in `/tmp/sly1-native-texture-trace.log` and
`/tmp/sly1-native-draw-state.log` on this development machine; those temporary
files are not repository artifacts.

Additional opt-in diagnostics for a new run:

```sh
OPENRAC_DUMP_NATIVE_FRAMES=1 OPENRAC_DUMP_UPLOAD=0x1400 \
  OPENRAC_DEBUG_T4_INDEX=all OPENRAC_DUMP_TEXTURE=1400 timeout 18s \
  build/port/sly1_native build/port/extracted/boot/SCUS_971.98 \
  '/path/to/your/Sly 1 NTSC-U.iso'
```

The frame dump writes up to eight actual draw frames to
`/tmp/sly1-native-frame-*.ppm`; the upload dump writes the assembled IMAGE
payload to `/tmp/sly1-gs-upload-1400.bin`. `OPENRAC_DEBUG_DIRECT_FLAT=1`
bypasses texture, depth, scissor and blend state to isolate geometry
rasterization. `OPENRAC_DEBUG_T4_INDEX=all` visualizes all indices as grayscale;
values `0` through `15` make one index transparent. The
`OPENRAC_DEBUG_T4_UPLOAD_SWIZZLE=1` comparison treats upload bytes as an
existing GS-local-memory image; it is not a default renderer mode.

## Work needed next

1. Trace the DMA chain's REF/REFS payload addresses that supply the `0x1400`
   IMAGE transfer. Compare the exact 131,072-byte assembled image payload to
   the referenced guest-memory range and the resource record. Verify the source
   before changing PSMT4 conversion.
2. Use `OPENRAC_DEBUG_T4_INDEX=all` to visualize indices, values `0` through
   `15` to make one index transparent, and
   `OPENRAC_DEBUG_T4_UPLOAD_SWIZZLE=1` only as a swizzled-memory comparison.
   Then verify the actual PSMT4 nibble order and 16-entry CSM1 CLUT against GS
   transfer fields and texture samples. No tested form yet yields a readable
   atlas, so preserve default conversion until evidence identifies the
   discrepancy.
3. Compare normal framebuffer captures with
   `OPENRAC_DEBUG_DIRECT_FLAT=1`. Trace glyph UVs, positions, scissor, depth,
   blend and alpha-test state through the DirectRenderer shader. Change the
   smallest state that explains the captured pixels, then capture a readable
   menu frame with the default renderer.
4. Once the menu is readable, trace the title-to-level transition and identify
   the missing Sly-specific scene/resource decode and draw path. OpenRAC's
   renderer and host runtime are reusable; Ratchet world records and game
   logic are not.
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
NTSC-U `SCUS_971.98` and disc image; setup and one-time asset extraction are
documented in [ASSETS.md](ASSETS.md). The current refreshed read-only decomp snapshot is
`/home/reaper/.cache/sly1-decomp-current` at `cf9ad46` (PR #62; checked
2026-11-06). `python3 port/tools/sly1.py status --source
/home/reaper/.cache/sly1-decomp-current` reports 4,720/4,806 matching US
functions, 92.17% code bytes, and 86 without matching C. The latest hostgen
report is `build/port/hostgen/us/report.md`: 189 units read, 573 functions
translated to host C, one stub, and 4,278 without C. These counts use different
denominators and answer different questions; neither means the same amount of
gameplay is ready. The snapshot is read-only and may lag GitHub if new decomp
commits land, so refresh it and rerun the status command before starting work
on missing functions.

The best immediate contribution is graphics debugging: trace one glyph from
the uploaded PSMT4 nibble and CSM1 palette entry through TEX0/UV selection,
alpha/blend state, the DirectRenderer shader and the framebuffer pixel. Record
the packet fields and image captures that justify each change. This needs
comfort with PS2 GS texture formats, GIF packets, OpenGL shaders and image
inspection. No additional game dump is needed to continue this track on this
development machine. A contributor on another machine needs a legally obtained
NTSC-U `SCUS_971.98` ELF and matching Sly 1 disc image, plus the compiler, CMake,
SDL2 and OpenGL development dependencies described in the build instructions.
Start with
`port/renderer/texture_pool.cpp`, `port/renderer/direct.cpp`,
`port/renderer/shaders/direct.frag`, and `port/game/sly1-us/hand/native_c/`.
The dump command above is reproducible; compare a fresh capture with the
existing evidence before changing renderer behavior. Game-code contributors
can take a separate track: inspect `build/port/hostgen/us/report.md`, choose a
missing function on the active boot/menu path, refresh/check the decomp snapshot
for matching C, then implement only still-missing behavior as a port-only C
backfill registered in `backfills.tsv` and hostgen inputs. Keep these functional
replacements in `port/game/sly1-us/hand/native_c/`; do not edit
`sly1-decomp`. The decomp repository has its own contribution and coordination
rules, so coordinate there before working on matching sources. When matching
C arrives, let source precedence replace the backfill and retain only
port-specific adaptation. Keep graphics fixes in `port/renderer/` and runtime
adapters in `port/runtime/` so they remain distinct from game-function
backfills.
`docs/port/ASSETS.md` covers ELF/ISO setup and one-time extraction.
After a readable menu, work remains on Sly-specific level data and scene
rendering, controller-to-pad integration, collision, audio, and saves.
To move toward playability, the project needs: (1) a verified visible menu
frame; (2) a populated title/level path with Sly-specific scene rendering;
(3) SDL controller state wired into the game's pad-library interface; and
(4) working movement/collision, audio and save behavior. OpenRAC's generic
host runtime is reusable; Ratchet-specific world records are not.

To reproduce the current window and collect renderer counters:

```sh
cmake --build build/port --target port_native -j2
OPENRAC_TRACE_DIRECT=1 timeout 12s build/port/sly1_native \
  build/port/extracted/boot/SCUS_971.98 \
  '/home/reaper/pcsx2/Games/Sly Cooper and the Thievius Raccoonus (USA).iso'
```

Set `OPENRAC_VIF_TRACE=1` only for actual VIF1 submissions. The ISO path above
is specific to this machine; another developer can supply their own NTSC-U
boot ELF and ISO. Keep functional game-code replacements in
`port/game/sly1-us/hand/native_c/` and register them in the backfill inventory,
so matching implementations in `sly1-decomp` can replace them cleanly.
Runtime/rendering changes belong in `port/runtime/` or `port/renderer/`.
