<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Sly 1 native graphics handoff

Last checked: 2026-11-06. This document records the live native C-only
launcher state, not a playable milestone. The launcher opens a window and the
direct GIF renderer decodes menu text and textured quads, but the captured
frame is still black. Its menu atlas upload is zero-filled because the Splash
reader reaches EOF before the image payload is read. No emulator is used by
this path.

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

## Current resource investigation

The Splash compressed resource is read from ISO sector `820372` for `150849`
bytes. The native LZ backfill now carries back-reference bytes across its
8 KiB output windows; an offline replay matches all `430858` decoded bytes in
`resource_0002.decoded.bin`. The refill state is kept in reader field `+0x2C`
so it does not overwrite the sector reader's `+0x38`/`+0x3C` bookkeeping.

A bounded native run with `OPENRAC_DUMP_LZ_WINDOWS=1` captured the runtime LZ
windows. The first 430,858 bytes match `resource_0002.decoded.bin` byte for
byte, with no mismatch; this verifies the live decompressor output for the
Splash stream, not just the offline replay. The trace wrote 55 8 KiB windows
before timeout, so only the expected 430,858-byte prefix was compared; later
captured bytes may belong to subsequent output and are not evidence about the
Splash resource. The optional dump writes `/tmp/sly1-lz-window-*.bin` and is
enabled alongside `OPENRAC_TRACE_LOADER=1`.

The current bounded trace supersedes the earlier `0x128BC` dimension and
9,542-record diagnosis below this point in the investigation history. Retail
LZ output still matches all 430,858 bytes of the extracted Splash resource.
The live parser failure is now localized to a collision mesh in the object
list:

* The second collider starts at decoded offset `0x1643`. Its mesh has 6,626
  12-byte vertices; face count `0xFFFF` follows at `0x14CE1`. The previous
  port reader imposed a 16,384-face cap and returned early, which made later
  property IDs and nested counts unreliable.
* Retail `func_00137FB0` assembly treats that value as a positive unsigned
  16-bit count: it allocates the face arrays and enters the face loop. The
  port backfill now accepts the full 16-bit count and stops safely when the
  decoded stream reports EOF. In the latest native run, EOF occurs while
  parsing this face block, before the second mesh or common object properties
  are reached. The previous `0x1555` property at `0x54D72` was an artifact of
  the early return and is no longer current evidence.
* The callback returns at EOF and frame setup continues, but
  `func_001DBA38` requests `0x5F4F0` image bytes with the stream already
  exhausted. The destination remains zero-filled, and the renderer reports no
  submitted menu vertices in this run.

The next investigation is the stream/refill boundary and why the selected
object declares 65,535 faces when the decoded Splash resource ends before that
block can be read. Verify the retail worker's continuation and the object's
serialized range before changing texture or GS behavior. Keep the count
handling in `hand/native_c/`; it remains a port-only backfill. The earlier
9,542-record and `0x128BC` dimension claims were from a different,
misattributed parser trace and should not guide fixes.

Retail `func_00162890` still supplies the useful reference for its nested
record sizes: `c0 * 12`, `c1 * 12`, `c2 * 4`, `c3 * 8`, `c4 * 4`, followed by
`refs * c0 * 4`; optional unsigned dimensions each consume `dimension * 12`.
Those sizes describe the function, but the current trace does not reach it for
the second collider. Keep all functional reader and sentinel handling as
port-only backfills, replacing it with matching C when the decompilation covers
those functions. Do not adjust GS texture interpretation while image data
remains uninitialized.

Reproduce the focused startup trace with:

```sh
OPENRAC_TRACE_FONT=1 OPENRAC_TRACE_LOADER=1 timeout 12s \
  build/port/sly1_native build/port/extracted/boot/SCUS_971.98 \
  '/home/reaper/pcsx2/Games/Sly Cooper and the Thievius Raccoonus (USA).iso'
```

To also dump each runtime LZ output window for comparison with the extracted
resource, add `OPENRAC_DUMP_LZ_WINDOWS=1` to that command. Remove stale
`/tmp/sly1-lz-window-*.bin` files before capturing a fresh sequence.

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

An earlier bounded run on 2026-11-06 accepted sampled channel-2 DMA chains,
uploaded a 512x512 PSMT4 atlas and 16-entry palette, and decoded two menu draws
with changing font positions. That trace used the mesh backfill's 16,384-face
cap, which returned before the retail `0xFFFF` face loop; its draw counters do
not describe the corrected reader path.

The latest native run reaches frame setup but reports zero submitted vertices.
Its resource reader reaches EOF inside the second collider's 65,535-face
block, before `func_001DBA38` can fill the `0x5F4F0`-byte image buffer. The atlas
upload at block `0x1400` therefore still contains zero-filled data. The prior
dark/noisy framebuffer and texture-index captures came from the early-return
reader path and are not evidence about the current retail-aligned parse. Do not
change swizzle, CLUT, or shader behavior until the resource reader reaches and
fills the image range. After that, resume checking PSMT4 indices, CLUT
order/alpha, TEX0/UV selection, and shader output against fresh framebuffer
pixels. The trace reports
`TCC=true`, modulate texturing, blending enabled, and GS alpha scale; those
settings remain candidates for later inspection, not proven faults.

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
OPENRAC_TRACE_FONT=1 OPENRAC_DUMP_NATIVE_FRAMES=1 OPENRAC_DUMP_UPLOAD=0x1400 \
  OPENRAC_TRACE_DMA_PAYLOAD=1 \
  OPENRAC_DUMP_FONT_RESOURCE=1 \
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
`OPENRAC_TRACE_DMA_PAYLOAD=1` logs large DMA payload source addresses, QWC,
and their offset in the assembled stream to help locate the atlas's REF/REFS
source range. `OPENRAC_DUMP_FONT_RESOURCE=1` writes the current image table's
full `0x5F4F0`-byte backing buffer to `/tmp/sly1-font-resource.bin`.

## Work needed next

1. Find the earliest stream misalignment in Splash's object list. The trace
   shows count 36 and a plausible first collider record (type 4, key 1), but
   after its reader returns at decoded offset `0x1594`, the next record reads
   type `0x5300`, outside the retail descriptor table. A later type-5 record
   reaches the collider with 6,626 vertices and an all-ones face count at
   decoded offset `0x14CE1`. The full descriptor is already preloaded and the
   retail LZ window algorithm reproduces the decoded bytes, so do not patch
   around the count yet. Compare the first object's actual stream consumption
   with its retail method chain and check preceding world sections. Keep any
   functional correction in the port-only backfill.
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
5. Continue through title and level loading, controller input,
   controllable Sly, collision, audio, and saves. A menu screenshot is useful
   progress but is not a playable-state milestone.

## Collider contact-table pass (2026-11-06)

`hand/native_c/collision_contact_tables.c` now supplies port-only
implementations for the collider's contact-pair flag table, paired row pool,
root retry-counter reset, and post-load compatibility scan. The functions are
registered in `hand/backfills.tsv`; none of this code changes matching
decompilation progress. The C-only launcher rebuild succeeds. A bounded
10-second native launch still reports zero submitted vertices and a black
window, so this closes a missing collision callback path without resolving the
current visual blocker. The remaining collider callback gap on this path is
`func_001B8540`.

AddressSanitizer traced the intermittent `double free or corruption (out)` to
a heap-use-after-free in the native kernel semaphore map: a guest thread was
checking a map node while another thread deleted it. Semaphore records now
use shared state with a per-semaphore mutex and condition variable; deletion
removes the map entry, marks the shared record deleted, and wakes its waiters.
A 60-second AddressSanitizer launch and a 20-second normal traced launch
survived without the abort. The renderer still reports zero vertices, so this
fix stabilizes startup but does not resolve the visual blocker.

A follow-up 12-second loader trace after the semaphore fix confirms the loader
now consistently completes the first collider callback at decoded offset
`0x1594`, consumes the following object records, then reaches the second
collider at `0x1643`. That collider's 6,626-vertex mesh still declares 65,535
faces; its face read reaches decoded-stream EOF and the callback returns with
no scene vertices submitted. This reproduces the existing resource-parser
blocker without the earlier heap corruption. Trace: `/tmp/sly1-stable-loader-trace.log`.

The current black frame is now traced one step farther. With
`OPENRAC_TRACE_DIRECT=1`, the native renderer decodes two menu draws and 372
vertices in a later frame; the legacy guest GIF counter still reports zero
vertices because it observes the separate hardware-style path. A captured
960x720 native frame contains only zero-valued pixels. The 512x512 PSMT4
upload at GS block `0x1400` is 131,072 bytes and every byte is zero. Font trace
stage 19 shows the image read beginning with the Splash reader already at
`remaining=-1`; stage 7 records the attempted `0x5F4F0`-byte payload with zero
leading words. This makes stream alignment/resource parsing the current visual
blocker, rather than title callback dispatch or missing menu vertices. The
first concrete alignment lead remains the invalid next object type `0x5300`
at decoded offset `0x1594`.

For a native renderer/frame capture, run:

```sh
OPENRAC_TRACE_DIRECT=1 OPENRAC_DUMP_NATIVE_FRAMES=1 timeout 8s \
  build/port/sly1_native build/port/extracted/boot/SCUS_971.98 \
  '/home/reaper/pcsx2/Games/Sly Cooper and the Thievius Raccoonus (USA).iso'
```

Frames are written to `/tmp/sly1-native-frame-*.ppm`. To capture the menu
packet writer snapshots too, add `OPENRAC_DUMP_FONT_PACKETS=1`; these go to
`/tmp/sly1-font-packet-*.bin`.

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
report is `build/port/hostgen/us/report.md`: 191 units read, 587 functions
translated to host C, one stub, and 4,270 without C, with seven port-owned
overrides of matched C. These counts use different
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

## Collider world-frame backfill (2026-11-06)

`hand/native_c/collider_mesh_transform.c` now supplies port-only scalar C for
retail `func_001B8960` and the VU transform body. It updates the placed-object
world frame, transforms collision vertices, face normals, plane values, and
per-face vectors for both collider meshes, and follows the retail mesh-field
offsets. The native launcher builds and reaches the same Splash object-list
cursor without the former `func_001B8960` no-C warning. This does not resolve
the invalid-looking next row at `0x1594`, render scene geometry, or make the
port playable. Bounds update and world registration remain missing; the port now translates
`func_001B9680` to save the current world position at `+0x370`.
