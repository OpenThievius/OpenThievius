<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Sly 1 native graphics handoff

Last checked: 2026-11-06. The native launcher is still not playable: its default
path does not reach the opening movie or title screen and its visible output is
malformed, dark text fragments against a mostly black background. The
user-provided 63-second PCSX2 recording shows the publisher credits, Paris
rooftop opening movie, and the animated Sly Raccoon title screen with Sly and
the Start/Select prompts. Its PCSX2 game list identifies the European 2.00
disc (`Sly Raccoon (Europe) (En,Fr,De,Es,It)`); current native probes use the
NTSC-U `SCUS_971.98` ELF and matching US disc. Treat the recording as a
sequence and feature reference, not a pixel-matched target. It does not show
selecting New Game or level control.

An opt-in native probe now skips movie/title startup and synchronously loads
catalog key `0x0100` (`A Stealthy Approach`) after the first startup resource.
The parser completes and installs a second world, but scene vertices remain at
zero and the window stays black. This isolates level parsing from title startup;
it does not make the game playable.

The direct-load trace now reads world pointers through both EE RAM and the
guest-port heap correctly. It sees 25 entries in the world root list and 24 in
the `0x55` family list, with class IDs `0x58` and `0x51`. Activation and
registration backfills populate 77 nodes in each of world lists `+0x54` and
`+0x60`; this is confirmed by a bounded run. The `+0x1BE8` class-0x51
ConeVolume list contains one object at `0xE20A57B0`, with flags `0x0AFFFF00`,
high flags `0x120`, mode zero at `+0x2D0`, and value zero at `+0x304`.
Sampled update callbacks are no-C `func_00125C90`; they see zero at `+0x28C`
and `+0x298` and do not queue draw requests. Scene vertices remain zero.
Retail `func_00184DE8` processes the class-0x51 ConeVolume list into VU data;
it remains assembly-only in decomp and now has a separate retail-derived port
backfill. The deferred frame dispatcher and request bucketer
(`func_0019DC98` and `func_0019D4B0`) also have port-only backfills. A bounded
native run stays active with these installed. The sole volume has mode/value
zero, so it contributes no VIF output vectors and no scene vertices; the next
work is missing `func_00125C90` object update logic and the resulting scene
population.

Upstream `sly1-decomp` master was checked at `dd6f80b` on 2026-11-06 (US
4,730/4,806 functions, 93.10% code; Europe 4,738/4,831, 92.42% code). Its
latest changes add a HUD compass drawing function and placed-object link data;
they do not match `func_00125C90` or the class-0x51 ConeVolume processor.
The generated host source is based on an older snapshot and does not yet
include those unrelated changes.

The latest native trace reaches the menu text callback and submits two draws
with 372–480 vertices. It uploads a 512x512 PSMT4 atlas at GS block `0x1400`
and an 8x2 PSMCT32 palette at `0x1600`; both uploads contain nonzero data. The
font uses 133 glyph records. The full atlas and palette upload payloads
match the decoded Splash resource byte for byte at offsets `0x6160` and
`0x5B50`, respectively, when correlated with the image-table offsets and
image-cache read start. This makes missing/zero image data an obsolete
explanation. A diagnostic run with `OPENRAC_DEBUG_T4_INDEX=all` replaced
palette colors with index greyscale; the text stayed fragmented, so palette
RGB values alone do not explain the corruption.

The live trace confirms that screen-element constructors run and put menu
object `0x26FF68` on its normal hidden page (`-1`). About five seconds later,
the frame dispatcher `func_001E9108` requests a transition from page `-1` to
page 0 in mode 0. Decomp data identifies page 0 as `Paused` with eight items.
The trace shows pad-state `+0x14 == 1`, edge flags 0, and game time about 5.0
seconds at that transition. The first menu draw then enters `func_00194F80` on
page 0. This is a startup-state/scene-flow issue in addition to the malformed
glyphs. The direct draw has a text draw with 480 vertices; its first text
vertex uses UV `(0.533, 0.916)` and RGBA `(127,127,127,17)`.

The screen-element constructor loop `func_001A9F78` exists in the current
decomp source but its US status is still `no-c`, so hostgen emitted a no-op.
The port now supplies that loop as a clearly separate backfill. Remove it when
the matching C is included in the host build. Trace stages 31–33 record the
reset condition, requested page transition, and pause-menu constructor.

Keep functional game-code replacements in
`port/game/sly1-us/hand/native_c/`, separate from matched decompilation C.
No emulator is used by the native launcher. Current build and launch commands
are below.

## What runs today

Build and launch from the repository root:

```sh
cmake --build build/port --target port_native -j2
build/port/sly1_native build/port/extracted/boot/SCUS_971.98 \
  '/home/reaper/pcsx2/Games/Sly Cooper and the Thievius Raccoonus (USA).iso'
```

To exercise the level loader before title/movie support exists:

```sh
OPENRAC_BOOT_LEVEL=0x0100 build/port/sly1_native \
  build/port/extracted/boot/SCUS_971.98 \
  '/home/reaper/pcsx2/Games/Sly Cooper and the Thievius Raccoonus (USA).iso'
```

The opt-in loader uses a reserved upper EE-RAM range starting at `0x01000000`
for resource reads larger than the renderer's shared `D_002C3B00` buffer. Treat
this fixed scratch range as development-only until broader memory use is
audited. `func_001DB838` world teardown is kept as a separate port-only
backfill in `hand/native_c/` and can be replaced by matching decomp C later.

The source ELF and ISO above are already present in the current development
environment. The host-native launcher uses SDL/OpenGL and OpenRAC renderer
code, with temporary guest-addressed EE DMA, VIF1, VU1 and GS services to run
and diagnose Sly's draw path. Those compatibility services are not an emulator.
The target design remains native game code, native platform services and
Sly-specific draw-data adapters; see [DESIGN.md](DESIGN.md) and
[OPENRAC_REUSE.md](OPENRAC_REUSE.md).

The display path reaches port-only `func_00202A80`. The `0x08000000` word in
an earlier chain dump begins a GIF IMAGE tag with 8,192 qwords; it is not a
VIF opcode. The port-only initializer fills `D_00261F1C` from the channel table
at `0x00279340`; traces show GIF channel 2 at `0x1000A000` and VIF1 at
`0x10009000`.

The renderer reports texture state `TBP=0x1400`, `TBW=8`, `PSMT4`, `TW=TH=9`,
`CBP=0x1600`, `CPSM=PSMCT32`, with `TCC=1`, `TFX=MODULATE`. Font glyph UVs and
the image/palette payloads are present. The grayscale-index diagnostic
preserves the malformed layout, narrowing the current renderer investigation
to glyph geometry/UVs and alpha/fade state, alongside the unexpected menu state.

## Historical resource-reader investigation (superseded by the live trace)

The Splash compressed resource is read from ISO sector `820372` for `150849`
bytes. The exact matched decomp LZ reader is now active; its previous port
backfill is archived. The independent offline replay matches all `430858`
decoded bytes in `resource_0002.decoded.bin`. The refill state is kept in
reader field `+0x2C`
so it does not overwrite the sector reader's `+0x38`/`+0x3C` bookkeeping.

A bounded native run with `OPENRAC_DUMP_LZ_WINDOWS=1` captured the runtime LZ
windows. The first 430,858 bytes match `resource_0002.decoded.bin` byte for
byte, with no mismatch; this verifies the live decompressor output for the
Splash stream, not just the offline replay. The trace wrote 55 8 KiB windows
before timeout, so only the expected 430,858-byte prefix was compared; later
captured bytes may belong to subsequent output and are not evidence about the
Splash resource. The optional dump writes `/tmp/sly1-lz-window-*.bin` and is
enabled alongside `OPENRAC_TRACE_LOADER=1`.

The detailed parser and collider notes below record earlier investigations.
Their claims that the image cache is zero-filled, the atlas is absent, and the
renderer has no texture state are superseded by the current measurements above.
Retain them only as historical debugging context, not as descriptions of the
current build.

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
  exhausted. The destination remains zero-filled. Separate direct-renderer
  tracing has recorded title quads entering the renderer, while the captured
  framebuffer remains black.

The next investigation is the decoded container layout and the ownership of
the suspected image block. The Splash decoded size is 430,858 bytes, the image
read requests `0x5F4F0` (390,384) bytes, and their difference is `0x9E1A`.
The 390,384 bytes beginning at decoded offset `0x9E1A` have high entropy and
look like image data. Image-A metadata has a maximum `offset + size` of exactly
`0x5F4F0`, the retail cache allocation size. This strongly supports the tail
being the serialized image block, although its start has not been verified as
the reader's position when the retail image callback runs. The object list starts at
`0x14D5`; its first type-4/key-1 row enters the retail C03 collider callback
and returns at `0x1594`. At that offset the bytes are `00 53 00 4B 05 FF`, and
the next 6,626-vertex collider begins at `0x1643`. The generic property loop
and observed scalar widths agree with current retail C. The image callback
uses a separately stored byte count, aligns the reader to 16 bytes, then reads
that count. New opt-in trace stages capture the header/cache state (25–26) and
callback cursors before alignment, after alignment, and after the read
(27–29). The reader cursor is a circular 8 KiB window address, not a
decoded-resource file offset; correlate its refill state and cumulative output
before assigning decoded offsets. Compare these values with retail cache
control flow before changing the cursor, face count, texture upload, or GS
behavior. Keep functional corrections in `hand/native_c/` as port-only
backfills. The earlier
9,542-record and `0x128BC` dimension claims were from a different,
misattributed parser trace and should not guide fixes.

The port-only animation/table reader now emits opt-in stage 20–22 records for
the image-B, image-A, and font table counts, including the circular reader
cursor and its remaining-byte field immediately after each count. These probes
are intended to correlate the table boundaries with cumulative LZ output; the
cursor itself is not a decoded-file offset. Source comparison against the
refreshed `sly1-decomp` snapshot confirms retail `func_001B3040` calls
`func_001B2BA8`, `func_001B2CF8`, reads animation and animator counts, then
parses the font table. The port follows these table widths: image-B records are
12 bytes, image-A records are 0x14 bytes, animator type zero has no payload,
nonzero animator types consume 0x1C bytes, and font headers are 0x10 bytes
followed by `glyph_count * 8` bytes. No table-width mismatch is established;
the animator payload's type-specific fields remain simplified in the port but
do not affect stream alignment. Keep any behavioral completion as a port-only
backfill until matching C is available in the decomp project.

A bounded native trace with `OPENRAC_TRACE_FONT=1 OPENRAC_TRACE_LOADER=1`
measured image-B count 63 at cursor `0x7000008D`, image-A count 61 at
`0x70000383`, and font count 4 at `0x70000EDD`; all remaining-byte fields were
positive. The reader reaches `0x700014D5` immediately after `func_001B3040`,
where the object-list count 36 begins. Thus the preceding table reader reaches
the expected serialized boundary; the first invalid-looking row still occurs
inside object-list parsing, after the first collider callback returns at
`0x1594`. This trace does not explain why that callback boundary disagrees with
the following bytes.

Retail `func_00162890` still supplies the useful reference for its nested
record sizes: `c0 * 12`, `c1 * 12`, `c2 * 4`, `c3 * 8`, `c4 * 4`, followed by
`refs * c0 * 4`; optional unsigned dimensions each consume `dimension * 12`.
Those sizes describe the function, but the current trace does not reach it for
the second collider. Keep all functional reader and sentinel handling as
port-only backfills, replacing it with matching C when the decompilation covers
those functions. Do not adjust GS texture interpretation while image data
remains uninitialized.

The US boot ELF's retail `func_00137FB0` disassembly confirms the mesh stream
order used by the port backfill: float radius, unsigned 16-bit vertex count,
12-byte input vertices, unsigned 16-bit face count, link-capacity count, then
per-face three unsigned indices, float plus 12-byte face payload, unsigned
byte edge count, and three unsigned 16-bit fields per edge. The mesh loop calls
`func_00137B90`, whose instruction is `lbu`; the adjacent signed-byte helper
`func_00137CC0` is not called there. The first Splash collider trace follows
this layout through mesh/tree cursors `0x14DE`→`0x14E8`→`0x14EC`, then its
second mesh/tree at `0x152A`→`0x1534`→`0x1538`. Its property terminator,
nested zero-count record list, and callback return land at `0x1587`, `0x1594`,
and `0x1594`, respectively. This validates the reader widths for the first
collider; it does not explain why the following top-level row begins with
`00 53 00 4B 05 FF`.

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

## Prior measured graphics evidence (archived tail experiment)

Before the stream-unit staging fix, the port-only `func_001378A0.c`
experiment copied decoded bytes `0x9E1A..0x6930A` into the image cache for the
observed Splash revision and cache size. Stage 88 confirmed the `0x5F4F0`-byte
copy, and a captured frame showed nonzero PSMT4/CLUT uploads with faint
lettering. This did not establish the retail image block's decoded offset. The
experiment is archived under `hand/superseded_backfills/`; matched
`func_001378A0` from the decomp is active now. Those atlas and menu captures
are historical and do not describe the current build.

## Decomp refresh and matched-C check (historical snapshot)

The checkout has since advanced beyond the snapshot described here. See
[ROADMAP.md](ROADMAP.md) for the current `dd6f80b` upstream status. Relevant matching
source at the time included
`func_001BAC30`, `func_00138278`, `func_0011ECB8`, and the animation/image/font
table readers. The low-level mesh decoder `func_00137FB0` is still classified
as assembly-only (`handwritten VU0 macro instruction`) in the source
catalogue.

Hostgen staging now reads both `00138000.cpp` and `001B8000.cpp`, and the native
build uses their matching `func_00138278` and `func_001BAC30` bodies. The
callback row parser `func_0011ECB8` remains a port-only backfill; the separate
mesh decoder `func_00137FB0` is still assembly-only, and `func_001289C0`
remains a port backfill. Exact matched `func_00196410` now handles the nested
group callback. A current bounded run advances past the old `0x5300` read,
dispatches type `0x53` / key `0x054b`, and then finds a zero cursor in the
nested reader; Splash image and CLUT data are still zero.

## Exact stream-reader integration (historical startup state)

`port/tools/sly1.py` now lowers the local static helper methods in
`00134000.cpp` to file-local C during disposable host staging, and drops one
unused inline setter while preserving its record layout. The upstream decomp
checkout remains read-only. This makes the entire matched stream-reader unit
readable to hostgen: it currently contributes 24 exact functions, including
`func_001378A0`, `func_00137A60`, `func_001377C0`, `func_00137820`,
`func_00137B60`, and the scalar readers. The previous port implementations
are retained in `hand/superseded_backfills/` and unregistered in
`backfills.tsv`; hostgen reports no unreadable `00134000` unit.

Two separate host adapters are active: `func_001F5A58` maps the still
assembly-only byte-copy operation to `memmove`; `func_00140C28` and
`func_00140CC8` synchronously read through the native ISO service. The latter
functions have matching source in `00140000.cpp`, but that whole unit still
fails C staging on C++ constructors/iterators. A traced native run confirms a
successful read of sector 820372 (74 sectors) into `0x002C3B00`.

At the time this trace was captured, the image callback ran at EOF and left the
cache zero-filled. This is superseded: a later live trace reaches the font
callback with populated image and palette data, and the two payloads match
their expected decoded Splash ranges. Do not use the old zero-upload finding
as the current rendering diagnosis.

To reproduce the current exact-stream startup trace, run from the repository root:

```sh
OPENRAC_TRACE_LOADER=1 OPENRAC_TRACE_FONT=1 OPENRAC_TRACE_CD=1 \
  OPENRAC_TRACE_DIRECT=1 timeout 12s \
  build/port/sly1_native build/port/extracted/boot/SCUS_971.98 \
  '/home/reaper/pcsx2/Games/Sly Cooper and the Thievius Raccoonus (USA).iso'
```

The command captures the early loader and native draw path. Its historical
EOF/zero-upload expectations above no longer apply; inspect the recent
`OPENRAC_TRACE_DIRECT=1` and `OPENRAC_TRACE_FONT=1` output for current texture
state and payload evidence.

## Work needed next

1. Compare PSMT4 texels and PSMCT32 CLUT entries in the live atlas against the
   PS2 GS's exact CSM1 indexing, alpha, and texture-coordinate behavior. Follow
   a known glyph from its resource bytes through the native texture and final
   framebuffer pixel; change only the port renderer/backfill layer supported
   by that comparison.
2. Trace why the native path selects a menu text object during startup while
   PCSX2 shows the opening movie and title sequence. Identify the first Sly
   state transition or resource callback that diverges, keeping functional
   replacements in `native_c/` until matching decomp C exists.
3. Reach the Sly Raccoon title screen natively, then start a new game and
   render the first level. A readable text atlas or menu fragment is not a
   playable-state milestone.
4. Continue through controller input, movement/collision, audio and saves.

## Historical implementation notes (prior to exact stream integration)

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

Active functional replacements belong under `port/game/sly1-us/hand/native_c/`
and are registered in `backfills.tsv`. Superseded implementations remain under
`hand/superseded_backfills/` for comparison and are not compiled. Keep every
port-only change separate from matched decomp C; do not edit the decompilation
checkout to make the port work.
The runtime transport and renderer adapters belong under `port/runtime/` and
`port/renderer/`; document any temporary guest-device behavior as such.

## What a new contributor needs

No additional disc image, boot ELF, or other user-supplied file is blocking
the current work on this machine. Contributors elsewhere need their own
NTSC-U `SCUS_971.98` and disc image; setup and one-time asset extraction are
documented in [ASSETS.md](ASSETS.md). The local read-only decomp snapshot is
`/home/reaper/.cache/sly1-decomp-current` at `cf9ad46`; upstream master was
checked separately at `dd6f80b`. Upstream reports 4,730/4,806 matching US
functions (93.10% code) and 4,738/4,831 Europe functions (92.42% code). Its
new HUD compass function `func_001316E8` and placed-object link table do not
replace current render backfills. `func_00161B10` and `func_00184DE8` still
lack matching C in the decomp project; the latter has a separate port backfill.
The active hostgen report is
`build/port/hostgen/us/report.md`: 196 units read, 1,091 functions translated
to host C, one stub, and 3,799 without C, with seven port-owned overrides.
These measures have different denominators and do not establish gameplay
readiness. The generated source is older than upstream master.

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
