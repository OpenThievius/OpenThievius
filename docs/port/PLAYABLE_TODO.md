<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Concrete plan for a playable native Sly 1

## Mesh packet provenance correction (2026-11-17, 13:40 CET)

The earlier broad trace followed target address `0x016d5890` across all 773
segments in the frame DMA. The mesh packet run begins at DMA segment 286, stream range
`[177312,182688)`: segment 287 is tag `0x003c8560`, data `0x016d5890`, QWC
141. A narrow trace found UNPACKs in this range but no VIF DIRECT command.

Port-only instrumentation in the generated host32 staging source brackets
`DrawModelPart` and each mesh/strip's `g_vifPacketWriter` range. It maps
`0x003c8560` to request `0x005e5400`, part `0x016d4460`, mesh 0 of 38. The part
is lit and skinned. The mesh-item range is `[0x003c8380,0x003c85d0)`; it
contains the DMA tag, while the payload is the target address above. This is
the direct source-to-DMA association needed for the next renderer trace.

The broad range did contain a 44-QWC DIRECT packet at segment 612 (40 XYZ2
writes using `PRIM=6`) and a two-QWC packet at segment 771. Those belong to a
later, unrelated part: request `0x005e5480`, part `0x016d44d0`, an unlit part
with 47 strips. They are not evidence of the selected lit mesh's output. The
broad VU XYZ writes at segments 587 and 723 are likewise outside its DMA
range. Do not treat those packets or their coordinates as the selected mesh.

The matched-range VIF DIRECT origin label remains an opt-in diagnostic. Build
passed with 194/194 staged decomp units, 85/85 port backfills, and 99
missing-function traps. The only staged-source changes were diagnostic
instrumentation; no files in the sibling decomp checkout were changed. The
bounded offscreen run ended at timeout (137) with no launcher left. Relevant
logs: `/tmp/sly-target-direct-xyz-20261117/run.log`,
`/tmp/sly-focused-model-direct-20261117/run.log`,
`/tmp/sly-model-part-range-20261117/run.log`,
`/tmp/sly-model-part-target-map-20261117/run.log`, and
`/tmp/sly-model-part-item-map-20261117/run.log`.

**Next:** trace the selected mesh's own VU output from its MSCAL calls and
correlate that output with the packet captured by the optional native adapter.
Keep each trace tied to the selected submission and command offsets; wide
frame-chain matches include unrelated draws.

## Rebuilt default-client visual recheck (2026-11-17, 13:14 CET)

I rebuilt `wave55-sdl32` from the current source and checked the default
renderer (`OPENRAC_NATIVE_SLY_WORLD_RENDERER=OFF`). In the delayed offscreen
capture, eight sampled frames show the complete no-card prompt; the Continue
highlight animates between selected and unselected. The first eight eligible
captures before that delay are black, consistent with startup before the first
UI draw. A process-local Cross at frame 300 reproduces the known transition:
the last prompt samples are followed by a black frame. This does not establish
whether the user's desktop-window flicker is present; offscreen captures do
not test the desktop compositor. The default client still does not reach
playable world graphics.

The build completed with 194/194 staged decomp units and 85/85 port backfills;
it reports 99 missing-function traps. Both bounded launches ended at timeout
(exit 137), and no launcher remains. No matched decomp source changed. Local
captures and logs are under `/tmp/sly-current-audit-20261117/`.

**Next:** continue from the confirmed prompt-to-black boundary and identify
the retail world-vertex GIF output. Do not treat the opt-in distorted world
adapter as the default result.

## Lit-packet and rejected-kick correction (2026-11-17, 13:05 CET)

The three sampled addresses are `ModelStrip`'s detached lit VIF buffers
(QWC 141, 87, 77), not three `FrameSource` records. Their windows converge on
the same finish call. Its rejected XGKICK at 461 points one qword past a valid
71-qword A+D state packet at 460; the accepted packet at 35 is also A+D state.
Neither packet contains XYZ2/XYZF2 writes, so shifting or retrying the rejected
kick will not restore geometry. The selected VIF path runs MSCAL entries
`0x401`, `0x411`, `0x242`, `0x356`, then `0x2`; only the final call kicks in
that range.

**Next:** trace `DrawModelPart`'s separate finish/direct packets and their VIF
DIRECT/GIF submissions until a drawable XYZ packet is tied to the queued mesh.
Keep the optional source-position adapter off by default. Details and exact
capture limits are in `SLY_GRAPHICS_HANDOFF.md`.

## Targeted world VU/GIF trace (2026-11-17, 12:56 CET)

The current opt-in client reproducibly draws a gray horizon and two stretched
structures after Continue; the default client still has no level draw. A
trace of the VIF1 chain containing `0x016d5890` found VU transform work in
entries `0x401`, `0x411`, `0x242`, and `0x356`, followed by entry `0x2` emitting
a valid 70-loop A+D packet. That packet writes only PRIM (`0x00`) and address
`0x80`; it has no XYZ2/XYZF2 destinations, so it is not the world-vertex
packet. This is one accepted kick in one selected chain, not proof that no
other chain submits geometry. The native adapter still draws from source
positions and is not a faithful rendering of retail VU output.

**Next:** enumerate accepted kicks in the same selected chain and adjacent
world submissions, decode GS register destinations, and locate the actual
XYZ output packet. Then compare that packet with the native renderer input.
Do not change the VU translator or enable the adapter by default until that
boundary is established. Full trace limits and local captures are in
`SLY_GRAPHICS_HANDOFF.md`.

## Per-present texture correction (2026-11-17, 12:45 CET)

A fresh opt-in renderer run shows the texture-placeholder count from the prior
note was not a stable diagnosis. The first per-present sample resolved all 69
unique material addresses to uploaded textures; later stable samples resolved
all 54 with zero missing. The screen stayed gray and distorted. The earlier
66-of-69 placeholder count was only a first-lookup snapshot and is superseded.
Texture decoding/state may still be wrong, but missing uploads were not the
blocker in this fresh run.

The default client still renders the no-card prompt and turns black after
Continue because its level mesh adapter is OFF. With the opt-in adapter ON, the
game queues and projects the world mesh, but its source-position / triangle-strip
approximation does not match retail VU output. The current frame remains
unrecognizable.

**Next:** capture the final VU1 output for one representative mesh, compare its
vertex positions and order with the adapter input, and update the port-only
adapter to match. Keep it OFF by default until the result shows a recognizable
scene. Detailed evidence and the texture diagnostic are in
`SLY_GRAPHICS_HANDOFF.md`.

## Post-Continue trace update (2026-11-17, 12:32 CET)

The current default client still renders the no-card prompt, then produces a
black frame after Cross. A fresh offscreen capture reproduces that result. The
world path is active after input: the trace loaded the world object, read 2,426
disc sectors, and queued 23 model parts per sampled `QueueModelPartDraws`
call. Do not spend more time proving that the world path starts; the missing
milestone is getting its draw output through the default renderer.

A targeted VU trace of submission 327 follows the dynamic source
`0x016d5890` through entries `0x401`, `0x411`, `0x242`, `0x356`, and `0x2`.
Entry `0x2` updates an A+D state packet and clears qwords 37–85. The nearby
qword-469 packet is still rejected for invalid GIFtag bits, while qword 35 is
an accepted state-only packet.

The DirectRenderer path handles UI packets. After Continue, it gets a
943,776-byte transfer with no draws, then only small 6- and 36-vertex corner
elements. Level meshes go through a separate optional native adapter, which is
OFF in the default client. With that adapter ON, 234 meshes / 8,955 vertices
are projected mostly inside the view, but the captured scene is still
distorted. The adapter uses base mesh positions and one approximate triangle
strip, so it does not reproduce the retail VU output yet. The per-present
texture check above supersedes the earlier placeholder count.

**Next:** compare one mesh's source vertices with its retail VU1 output. Keep
the adapter off by default until it produces a recognizable scene. See
`SLY_GRAPHICS_HANDOFF.md` for captures and trace limits. No matched decomp
source changed.

## Fresh default-client audit (2026-11-17, 12:03 CET)

I rebuilt the current default host32 client and captured the framebuffer with
the world renderer option confirmed OFF. Eight steady no-input frames show the
complete no-card prompt and Continue highlight. The small white dash at the
left edge is still present. A separate bounded run injected one process-local
Cross at native frame 300; eight sampled frames after frame 350 are fully
black. Its trace again records the rejected XGKICK at qword 469 and an accepted
state packet from qword 35, but this does not prove which packet causes the
black output. This matches the documented post-Continue failure; the latest
code did not regress the startup prompt, and the world is still not playable.
Both runs used SDL's offscreen driver, ended at the 10/12-second timeout
(exit 137), and left no launcher process. The offscreen captures do not assess
desktop-present flicker. The default build compiled 194/194 decomp units and
85/85 port backfills, with 790/791 reachable direct calls defined. No matched
decompilation source changed.

Captures and logs:
`/tmp/sly-default-fresh-noinput-contact-20261117.png`,
`/tmp/sly-default-fresh-check-20261117/`,
`/tmp/sly-default-fresh-check-20261117.log`, and
`/tmp/sly-default-fresh-postcross-20261117.log`.

## Latest native checkpoint (2026-11-17, 11:54 CET)

The strip-layout follow-up corrects the 11:30 working theory. A bounded trace
after Cross at frame 300 shows a representative kind-0 mesh using one shared
31-vertex `VertexRefs` list and three `FrameSource` packets. Each packet is an
8-qword GIF A+D state block. The material passes have different
`TEX0_1` values (`0x2003e5c5d940aa80`, `0x2003e5e5d940aa80`, and
`0x2003e605d940aa80`). A second representative mesh confirms the packets vary
`TEST1` and `ALPHA1` state as well. These are material/state passes over shared
geometry, not three independent geometry spans. A separate 83-vertex mesh's
completed header recorded source offsets `451`, `459`, and `467`. The earlier
suggestion to reconstruct one geometry boundary per source was wrong and is
superseded.

The port-only opt-in adapter now reads every valid source packet and submits
the shared mesh once per source. Its world submission count rose from about 80
to 234–276 per sampled frame. The settled capture changed from purple to gray
distorted surfaces, still with no recognizable scene. That change confirms
the passes affect output; it does not validate the pass order, alpha/blend
semantics, vertex topology, or transforms. The experiment remains opt-in and
the default renderer remains OFF. Build succeeded with 194/194 decomp units
and 85/85 separate port backfills; the bounded 12-second launch ended at the
timeout (exit 137), and no launcher remained. No matched decomp source changed.

The adapter still approximates each shared `VertexRefs` list as one GL
triangle strip. Before further visual changes, correlate each source offset
with its actual VU output and GS `PRIM`/`ALPHA`/`TEST` state, then preserve the
retail draw semantics in the port adapter. Captures and logs:
`/tmp/sly-multipass-capture-20261117/contact.png`,
`/tmp/sly-multipass-visual-20261117.log`,
`/tmp/sly-multipass-run-20261117.log`, and
`/tmp/sly-strip-register-audit-20261117.log`.

I reviewed fresh framebuffer captures from the opt-in world-renderer build
after a process-local Cross at native frame 300. The prompt visibly overlaps
itself as it contracts; later samples go black, then show a gray horizon and
distorted purple surfaces. This confirms the transition artifact in the native
capture and confirms that the optional adapter eventually draws something. It
does not establish whether the contraction matches PCSX2 or make the world
renderer playable. These images came from `wave55-sdl32-release`, whose
`OPENRAC_NATIVE_SLY_WORLD_RENDERER` option is ON. The default build keeps that
option OFF and its last settled post-Continue capture is still black.

Do not conflate the two rendering experiments: the normal DirectRenderer trace
decodes a 7.38 MB post-Continue packet into 76,053 vertices and 3,157 draws,
but only 889 vertices land inside clip space. The purple surfaces are from the
separate opt-in adapter. Its current conversion takes a whole `VertexRefs`
array, reads material state from only the first `FrameSource`, and submits the
array as one GL triangle strip. The matched decomp's lit and unlit strip
builders loop over every `nstrips` source and queue each at a separate VU offset. The
11:54 follow-up shows those packets hold material state for shared geometry;
they do not divide the vertex list into separate spans. Preserving all passes
is now an opt-in experiment, but its current image remains distorted and does
not explain the normal renderer's black output.

The all-path GS XYZ trace was not a valid absence-of-geometry check: its shared
64-write cap filled with startup VIF DIRECT sprite writes before the injected
Cross. A filtered VU XGKICK trace captured only five XYZ writes and is not a
complete view of VIF DIRECT or PATH1. Keep those limitations attached to any
claims based on these logs.

Captures and logs: `/tmp/sly-actual-postcontinue-20261117/`,
`/tmp/sly-actual-postcontinue-capture-20261117.log`,
`/tmp/sly-actual-settled-postcontinue-20261117/`,
`/tmp/sly-actual-settled-postcontinue-20261117.log`,
`/tmp/sly-all-input-path-draw-audit-20261117.log`, and
`/tmp/sly-vu-only-world-xyz-audit-20261117.log`. The decomp checkout was
checked at `8f7a85a3`; it had no uncommitted changes or newer fetched commits.
No matched decomp source changed.

Next: correlate one mesh's `StripHeader` source offsets with the resulting VU
output and GS draw state. Keep the default renderer configuration unchanged
until the port adapter matches those semantics.

### Previous checkpoint (2026-11-17, 11:22 CET)

The default client remains at the same milestone: it can render the memory-card
prompt, but after Continue it still has no recognizable world graphics. The
latest bounded native run built successfully and captured the qword before the
kind-5 VU path's rejected XGKICK. Qword 468 is a valid EOP GIFtag with 109 A+D
writes; the payload contains 104 PRIM writes, three texture/clamp state writes,
and two reserved/unrecognized register destinations. It contains no XYZ2 or
XYZF2 writes. So starting the kick at 468 would submit state, not geometry,
and would not fix the black post-Continue image. The independently accepted
qword-35 packet is also state-only. Preserve strict GIF parsing and keep
matched decompilation untouched.

The new host-only diagnostic is opt-in via
`OPENRAC_DUMP_REJECTED_XGKICK_PREV_PACKET=1`; it writes at most four captures
under `/tmp` without changing guest memory, the kick address, or renderer
input. Build passed; the bounded run ended on timeout (exit 137), and the
launcher was no longer running afterward. Full packet/register findings and
capture paths are in `SLY_GRAPHICS_HANDOFF.md`.

## Fresh visual review (2026-11-17, 10:46 CET)

The current default host32 launcher was rebuilt with diagnostics and inspected
from captured framebuffer output. Eight frames sampled after a process-local
Cross at native frame 300 show the no-card prompt shrinking toward the center;
this capture stops before the settled state, and it is not yet clear whether
the shrink is an intended transition. Prior settled post-Continue captures
are black. This needs a frame-by-frame comparison with the supplied PCSX2
recording before calling the effect a regression or expected animation.

The fresh trace selected the VIF DMA chain containing address `0x016d5890`,
then followed all 773 segments. It recorded three XGKICK-origin XYZ2/XYZF2
writes at offsets `0x57920` and `0x71220`, plus one at `0x712f0` in the next
submission. That wide range includes unrelated draw packets: the writes at
`0x57920` and `0x71220` map to segments 587 and 723, while the selected mesh
buffers are at segment 287. They cannot be attributed to that mesh. The trace
does not cover all GS input paths or independently observe PATH1 state.
Capture details are in `SLY_GRAPHICS_HANDOFF.md`; full trace:
`/tmp/sly-whole-chain-xyzstate-20261117.log`.

Next: map each `DrawModelPart` writer range to its DMA segments, then trace
one mesh through all GS input paths and correlate its writes with the
framebuffer. Preserve the default startup prompt; keep renderer reconstruction
in the port-only backfill layer.

### Latest opt-in world-renderer capture (2026-11-17)

I inspected the
world-only image from `/tmp/sly-one-slice-20261117/sly1-native-world-07.ppm`.
It is not a black regression in the default client: the isolated renderer draws
large distorted purple surfaces over gray, with no recognizable Sly scene.
The default renderer remains off (`OPENRAC_NATIVE_SLY_WORLD_RENDERER=OFF`),
and the earlier rebuilt default capture still shows the settled memory-card
prompt before input. After Continue, that default path is still black as
already recorded above.

The opt-in pass queues about 80 meshes per stable frame and resolves multiple
textures, but that is not evidence of a correct scene. Its computed bounds
remain broad and implausible (approximately NDC x `-2.3..1.8`, y
`-1.45..1.9`); one startup sample is much worse. The captured 68/89/106
vertex kind-6 sources belong to class `0x54`, while `DrawModelPart` also sees
many class-4 parts, so rendering only the kind-6 subset is not a valid level
renderer. The adapter still sends expanded `VertexRefs` as one GL triangle
strip and reconstructs a row-major `model * view_projection` matrix. Those
approximations are not yet justified by the decomp's `StripHeader` or the
retail VU output. Keep this experiment isolated and disabled; do not change
matrix order or add more renderer behavior without a bounded retail-to-native
draw conversion. Capture/log: `/tmp/sly-one-slice-20261117/` and
`/tmp/sly-one-slice-20261117.log`.

## Current visual and packet audit (2026-11-17)

**Rebuilt default-client check (2026-11-17, 10:02 CET):** the default
`wave55-sdl32` executable on disk predated today's source, and the newer
Release build had `OPENRAC_NATIVE_SLY_WORLD_RENDERER=ON`. I rebuilt
`port_native_sdl_host32` in the default cache and verified that option is OFF.
Eight consecutive settled frames before input show the complete no-card prompt
and animated Continue, with the known white dash at the left edge. One
process-local Cross at native frame 300 yields eight identical black frames.
That reproduces the existing post-Continue failure; it is not a new regression
in the startup prompt. The captured renderer buffer cannot confirm or rule out
flicker in the presented desktop window. The unfiltered post-input log also
contains a qword-469 invalid-tag rejection, but this run does not tie that
record to the target kind-0 Sly mesh, so use the graphics handoff for that
investigation. Images and logs:
`/home/reaper/sly/sly-client-check-20261117b/contact.png`,
`/home/reaper/sly/sly-client-postcontinue-check-20261117b/contact.png`, and
`/tmp/sly-client-postcontinue-check-20261117b-2.log`.

**Fresh default-client verification (2026-11-17, 08:21 CET; supersedes the
07:44-only visual checkpoint):** I rebuilt the default `wave55-sdl32` launcher
from the current source with `OPENRAC_NATIVE_SLY_WORLD_RENDERER=OFF`, then
captured eight later hidden frames after native frame 500. They all show the
complete no-memory-card prompt. The short white dash at the left edge remains
visible. The sampled frames do not show a whole-screen flicker; the Continue
highlight changes slightly between samples. Capture contact sheet:
`/home/reaper/sly/sly-default-client-contact-20261117.png`; final frame:
`/home/reaper/sly/sly-default-client-check-frame-20261117.png`. A separate
default-build run with one process-local Cross at frame 300 still shows a
black post-Continue frame:
`/home/reaper/sly/sly-default-postcontinue-frame-20261117.png`. This is the
known post-Continue failure, not a regression in the startup prompt. The first
eight frames immediately after process launch were black before the later
prompt frames; do not use those early frames as the settled startup result.

**Isolated world-renderer check (2026-11-17, 08:17 CET):** the opt-in
`wave55-sdl32-release` build remains separate from the default launcher. Its
mesh capture now keys by strip header and binds to the detached packet address;
the previous source-mesh-only key conflated different strip instances. The
bounded trace records 32 successful packet-address submissions with no lookup
miss in that logged sample. The resulting post-Continue frame is still only a
flat gray ground and horizon, without a recognizable level:
`/home/reaper/sly/sly-packet-key-check-frame-20261117.png`; trace:
`/home/reaper/sly/sly-packet-key-check-20261117.log`. Solid-color mode shows a
single teal full-cover mesh; skipping that mesh leaves only a teal horizon
against black sky. Captures:
`/home/reaper/sly/sly-packet-key-solid-check-frame-20261117.png` and
`/home/reaper/sly/sly-packet-key-skip-check-frame-20261117.png`. This confirms
the key collision was one adapter error, but the renderer still does not
produce the other recognizable world geometry. The default option stays OFF,
and no matched decompilation source was changed.

**Fresh client audit (2026-11-17):** I rebuilt and opened the current host32
client in the foreground, sent one process-local Cross at frame 300, and
captured `/tmp/sly-current-visible-postcross-20261117.png`. The visible frame
is black after Continue; there is no new playable output. A fresh foreground
check at 07:44 CET again showed the complete startup prompt, but also showed
the short white dash at the left edge (about 12 by 3 pixels in the captured
window): `/home/reaper/sly/sly-fresh-client-check-20261117.png`. This is the
known unresolved artifact, so the earlier statement that there was no visual
regression was too broad. A new process-local Cross run again produced a black
post-Continue frame: `/home/reaper/sly/sly-fresh-postcontinue-check-20261117.png`.
These still images confirm the prompt-to-black transition and the dash, but do
not establish whether the user's reported flicker occurs during the run. The
launcher was stopped after each bounded check.

**Post-Continue pacing check (2026-11-17):** the render thread still signals
frame-done semaphore 5; the observed signal gaps are about 0.7–0.8 seconds.
The corresponding VIF1 DMA submissions each process roughly 470–498 KB across
700–775 MSCAL calls and take 0.77–0.85 seconds, with DMA and VIF completion
successful. A targeted VU timing run covered only nine calls and 15–19 ms of
each submission, so it does not identify which of the remaining calls consume
the time. This is a real pacing problem in the post-Continue path, but it does
not explain the black image: the direct renderer still receives no scene
vertices. Keep the next visual milestone focused on one real world object;
avoid a broad VU profiler pass until a specific draw-boundary question needs it.
Traces: `/tmp/sly-frame-done-timing-20261117.log`,
`/tmp/sly-vif-frame-done-timing-20261117.log`, and
`/tmp/sly-target-vu-call-timing-20261117.log`.

**Follow-up visual check and full lit-mesh call sequence (2026-11-17):** a
fresh no-input run shows the complete memory-card warning at 3 and 6 seconds
(`/tmp/sly-noinput-visual-check-3s-20261117.png` and
`/tmp/sly-noinput-visual-check-6s-20261117.png`). The process-local Cross run
is black after the input (`/tmp/sly-target-mscal-frame-after-20261117.png`).
This confirms the same prompt-to-black transition; it does not show a startup
regression. In the dynamically matched DMA stream for target address
`0x016d5890`, the first lit mesh runs entries `0x318`, `0x401`, `0x411`,
`0x51`, `0x242`, `0x356`, then finish entry `0x2`. The first five entries
after `0x318` are present in the bounded VIF range; the next meshes repeat the
same pattern, with some setup calls omitted by caching. Exact sequence:
`/tmp/sly-target-mscal-20261117.log`.

Offset-filtered XGKICK traces show no kick from entries `0x51` or `0x242`;
entry `0x356` also completes without a pending kick. Entry `0x2` kicks an
invalid payload from qword 461 and then a valid A+D state packet from qword
35. These are preparation and state stages, not the missing scene-vertex
submission. Keep following the mesh strip data after entry `0x356` and the
later VIF/GIF handoff. Logs:
`/tmp/sly-litbatch-middle-xgkick-20261117.log`,
`/tmp/sly-focused-litbatch-finish-20261117.log`, and
`/tmp/sly-focused-litbatch-20261117.log`.

**Target VIF payload trace (2026-11-17):** the new
`OPENRAC_TRACE_VIF_TARGET_COMMANDS=1` filter logs UNPACK and DIRECT commands
only inside the dynamically selected DMA range. In submission 328,
`BuildLitVertexPacket`'s later sequence is `0x51`, `0x242`, `0x356`, then
finish `0x2`. After `0x356`, three V4-32 UNPACKs write 8 vectors at qwords
460, 468, and 476. The first vector at each range is a GIFtag; at qword 460 it
is `0x1000400000008046`. The next vector at qword 461 is A+D payload, but
finish entry `0x2` starts XGKICK there, so strict parsing treats that payload
as a malformed GIFtag and rejects it. The program then copies qword 460 to
qword 35 and emits an A+D state packet. No VIF DIRECT command appears in the
selected range. This pinpoints a malformed state kick, but it does not prove
why scene vertices never reach GIF. Trace:
`/tmp/sly-target-strip-qwords-20261117.log`.

**Fresh client and queue check (2026-11-17):** a foreground launch of the
current host32 binary visibly rendered the full no-memory-card prompt and
Continue button before input. The screenshot is
`/tmp/sly-live-client-check-20261117.png`. A later idle trace captured 894
native presents in 15 seconds. GIF data averaged 208,935 bytes per present,
peaked at 416,256 bytes, and produced no queue overflow warnings. A
process-local Cross at frame 300 then produced 718 presents in 12 seconds
with no overflow; the largest single frame was 2,098,896 bytes. The screenshot
after Cross is black: `/tmp/sly-postcross-client-20261117.png`. Direct decode
tracing shows the prompt frames decoding 1,254 vertices and 5 draws. After
the transition, large packet batches at Direct frames 317-323 decode zero
vertices and zero draws, followed only by occasional tiny draws. There are no
GIF parse errors. The blocker remains that the post-transition stream
supplies no drawable scene geometry, not a persistent queue leak. Traces:
`/tmp/sly-queue-idle-15s-20261117.log`,
`/tmp/sly-postcross-present-20261117.log`, and
`/tmp/sly-postcross-direct-decode-20261117.log`. An earlier run did emit a
real 16 MiB queue overflow warning (`queued=16776752`, `incoming=704`), but it
has not recurred; keep the queue cap unchanged and correlate present and
append traces if it returns.

**Trace correction and new lead (2026-11-17):** the previous reader probe was
filtered to hard-coded submission 327. Submission IDs change across launches,
so that filter could miss the target altogether; I should not have treated it
as stable. The host-only trace now discovers the submission by the known DMA
target address `0x016d5890` and bounds VU reads to that submission's adjacent
DMA stream range. In the fresh capture it found target segment 287 in
submission 328, stream offsets `[177312, 193088)`. Within that submission,
VU entry `0x2` at stream offset `0x2c990` XGKICKs from qword 461, where the
packet parser sees payload `0x00000000000797eb:0x47` and rejects it as an
invalid GIFtag. The preceding qword 460 is a valid GIFtag
`0x1000400000008046`; the same VU entry then XGKICKs from qword 35 and the
parser accepts that tag. In a prior run, a VIF UNPACK at the corresponding
stream offset wrote the tag to qword 460 and its next payload to qword 461.
The accepted tag describes an A+D register packet, not scene vertices. This
is a concrete VIF/VU/GIF boundary lead and is consistent with the host seeing
GS state but no useful geometry. It is not yet proven to be the sole cause of
the black frame: the next step is to trace how the entry-`0x2` packet is built
and locate the VU/DMA operation that should emit scene vertices. Keep strict
GIF validation and do not change matched decompilation.

The corrected dynamic-target instrumentation is in
`port/runtime/vif1_dma.cpp` and `port/runtime/vu1_native.cpp`. Evidence:
`/tmp/sly-kind0-xgkicks-follow16-20261117.log`,
`/tmp/sly-kind0-allstores-20261117.log`, and
`/tmp/sly-current-visible-postcross-20261117.png`.

**Entry-`0x2` dataflow confirmation (2026-11-17):** the narrower runtime
trace now confirms the packet construction in submission 328 at MSCAL offset
`0x2c990`: entry `0x2` loads qword 460, whose value is the valid tag
`0x1000400000008046`; it attempts XGKICK from qword 461 and strict parsing
rejects the payload; then the VU stores qword 460 into qword 35 and XGKICKs
from qword 35 successfully. The accepted GIFtag has NLOOP 70, one register
descriptor (A+D), and no vertex register descriptor. This matches the
renderer audit, which found only A+D packets in the selected post-transition
queue. The invalid kick is real, but this entry's accepted packet is GS
register state; neither fact identifies the missing scene-vertex producer.
Evidence: `/tmp/sly-kind0-filtered-20261117.log`.

The accepted kick was also dumped and decoded from
`/tmp/sly1-vu1-kick-00.bin` (1,136 bytes: GIFtag plus 70 payload qwords).
All 70 payloads use A+D as specified by the tag. Their address bytes are
`0x00` 53 times and `0x80` 17 times; none address XYZ2/XYZF2/XYZ3/XYZF3.
Address `0x80` is outside the GS register address range. This packet therefore
contains no vertex writes, and part of its A+D stream is invalid. That makes it
a concrete malformed state packet, but does not establish that it alone causes
the black frame. The bounded capture is
`/tmp/sly-kind0-2c990-packet-20261117.log`.

All `OPENRAC_TRACE_VU1_XGKICK_DATA` diagnostics honor the DMA and MSCAL context
filters. The target chain has now been traced through entries `0x51`, `0x242`,
`0x356`, and finish entry `0x2`; see the fresh sequence below. The remaining
work is to follow the strip UNPACKs after `0x356` to the later GIF submission.

**Earlier draw-list trace (2026-11-17, 06:05 CET):** the foreground capture
`/tmp/sly-current-visible-postcross-20261117.png` followed a process-local
Cross at native frame 300 and showed a fully black frame after Continue. The
later 07:44 CET check is recorded at the top of this section; this 06:05 trace
remains useful for its game-side and DirectRenderer details below.

The deeper trace corrects our previous diagnosis: the periodic startup sampler
still reports Splash world `0x6abdd0`, frame stage 2 and 2,426 sectors read,
but post-input draw-list traces repeatedly report level world `0x00870cf0`.
That world has 1,910 model parts, 23 queued by `QueueModelPartDraws`, and 39–48
deferred draw requests. Model draw index 21 is Sly. The startup sampler is
therefore incomplete or sampling a different state; it is not proof that the
level transition failed. These game-side request counts also do not mean that
the optional host GL mesh renderer received or displayed those models; that
renderer is disabled in the active build.

The presenter's ordinary DirectRenderer trace is the stronger rendering
boundary. From direct frames 330–716, 373 of 387 GIF batches decode with zero
vertices and zero draws, without a parser error. The other 14 batches have at
most 36 vertices; they are tiny corner batches, or lie wholly outside clip
space (for example frame 359 and frame 418). The first rejected VU1 XGKICK at
qword 469 and later accepted kick at qword 35 remain in the same run. This
places the blocker after level draw-list construction and before useful scene
geometry reaches the host renderer. It does not yet prove which VU/GIF transfer
detail is wrong. Evidence: `/tmp/sly-direct-postcontinue-20261117.log`,
`/tmp/sly-level-packets-after-continue-20261117.log`, and
`/tmp/sly1-native-frame-00.png`.

The 16 raw queued-packet dumps at eligible frames 330–345 were also walked as
GIF packets. Every tag uses a single A+D register descriptor; none uses an
XYZ2/XYZF2/XYZ3/XYZF3 descriptor. This confirms those queued packets contain
GS register state, not the scene vertex stream. Packet dump paths are
`/tmp/sly1-native-render-packets-00.bin` through `-15.bin` and are local
diagnostics only.

The qword-469 trace is for kind-5 mesh source `0x00a113c0`, which goes through
`BuildStripMeshPacket`; it is not the Sly model draw below. The latter is a
class-2 draw with 38 kind-0 lit meshes (`index=21`). Do not treat the kind-5
XGKICK mismatch as the proven cause of missing Sly geometry. The VU setup trace
for that kind-5 path shows the guest setting VI6 to 468 at PC `0x63`,
incrementing it to 469 at PC `0x67`, then issuing XGKICK VI6 at PC `0x68`.
Later it changes VI7 from 36 to 35 before the XGKICK at PC `0x6e`. The
[PlayStation 2 VU instruction manual](https://www.ps2dev.com/files/vu-instruction-manual.pdf)
defines XGKICK as starting PATH1 with the value of VI[is]. So 469 is the guest
microcode's explicit operand, not a host address calculation error. A prior
diagnostic backstep to 468 was accepted by the parser but still produced a
black frame.

The saved first Sly mesh packet (`/tmp/sly-world-part-0.bin`, 141 qwords)
decodes as VIF commands, not GIF output: it UNPACKs a three-vector header,
then 68 V4-32 vectors to VU1 address `0x027`, calls `MSCAL 0x318`, writes
another 68 V4-32 vectors at address `0x188`, and calls `MSCAL 0x401`. This
is tied to DMA segment 287 at guest address `0x016d5890` in the current host32
run. MSCAL calls at VIF offsets 178904 (`0x318`), 180020 (`0x401`), and 181408
(`0x411`) were observed in that stream. An XGKICK trace filtered to those
offsets recorded no kick, while the qword-469 rejection belongs to a separate
kind-5 entry `0x5f` path at offset `0x1790`. This removes the rejected kick as
an established explanation for Sly's missing geometry; the output handoff
from the kind-0 VU programs is still unknown. Evidence:
`/tmp/sly-kind0-offset-xgkick-20261117.log`,
`/tmp/sly-kind0-xgkick-20261117.log`,
`/tmp/sly-detach-mesh-trace-cross-20261117.log`, and
`/tmp/sly-world-mesh-capture-20261117.log`.

**Fresh kind-0 VU output check (2026-11-17):** a bounded rerun recorded normal
E-bit completion for all three calls in submission 327: `0x318` stopped at
micro-PC `0x31f` after 30 cycles, `0x401` at `0x411` after 1,803 cycles, and
`0x411` at `0x45c` after 1,800 cycles. None returned with a pending XGKICK.
The calls wrote respectively four setup qwords (14–17), 68 vectors at
qwords 39–307 with stride four, and 68 vectors at qwords 36–304 with stride
four. Those 136 vector writes are real VU data-memory output; they were missed
by the earlier qword 136–142-only watch. They are still not proof that the
normal GIF renderer receives scene vertices. The active question is which
later VU/VIF operation consumes these buffers, and where it should submit
geometry. The three selected calls produce no XGKICK themselves. Run log:
`/tmp/sly-kind0-allstores-20261117.log`; the host-only bounded trace was added
under `port/runtime/vu1_native.cpp` and leaves matched decompilation untouched.

The earlier no-input cadence capture remains stable on the complete no-card
warning in eight samples: `/tmp/sly-current-cadence-montage.png` and
`/tmp/sly-current-cadence-capture-20261117.log`. It does not reproduce the
black intervals visible in the user's Nov 15 screen recording (17–116 ms in
the cropped game window); whether those gaps are guest rendering or desktop
presentation remains unresolved.

**Next graphics task:** trace the `MeshFrameData` blocks copied after entry
`0x356` through `BuildLitVertexPacket`'s finish stage. Find why finish entry
`0x2` starts at qword 461 while the matching tag is at qword 460, and determine
where the transformed position vectors become XYZ GIF writes. Keep strict GIF
validation. Keep the separately traced kind-5 qword-469 mismatch as an
anomaly; it is not connected to Sly's kind-0 calls. Do not treat
`QueueModelPartDraws` counts as rendered geometry, and do not
enable the optional kind-6 host mesh adapter as a fix for the retail packet
path. Keep any host-only renderer experiment opt-in and separate from matched
decompilation. Start from `/tmp/sly-direct-postcontinue-20261117.log` and
`/tmp/sly-level-packets-after-continue-20261117.log`; use
`/tmp/sly-world-part-0.bin` through `-2.bin` only as local diagnostics, never
repository inputs.

**Earlier packet-boundary trace (2026-11-17):** a foreground Wayland run showed
the no-card prompt before one process-local Cross press and black output after
it. The launcher was stopped at the eight-second diagnostic timeout (exit
137), and `ps` confirmed no process remained. In VIF submission 328, the
UNPACK at stream offset 5916 writes qword 468 with GIFtag
`0x100040000000806d`; qword 469 is its first A+D payload. Entry `0x5f` then
XGKICKs from qword 469 and the strict GIF parser rejects that payload as a
tag. A later XGKICK from qword 35 succeeds. A source-address trace maps the
UNPACK data at stream offset 5920 to REF segment data `0x018d1270`, guest
source `0x018d2650`, within the 326-qword segment. Later UNPACKs overwrite
qword 468 in that same VIF submission only after the first rejected kick;
those writes cannot explain the first kick's contents. The kick starts at
qword 469, which contains the first A+D payload; its GIFtag is at qword 468.
The retail packet writer is identified below. Preserve GIF validation while
tracing why the VU kick starts one qword after that tag. Evidence:
`/tmp/sly-vif-model-packet-source-cross-20261117.log` and
`/tmp/sly-vif-gif-range-cross-20261117.log`.

**Earlier client and renderer check (2026-11-17):** I relaunched the current
host32 target and captured the actual Wayland window. Before input it shows the
full no-card warning (`/tmp/sly-current-client-20261117.png`); one in-process
Cross produces a black window (`/tmp/sly-current-postcontinue-client-20261117.png`).
This matches the previously recorded prompt-to-black result. A fresh XGKICK
trace confirms the first kick (entry `0x5f`, PC `0x68`, qword 469) is rejected
and the second (PC `0x6e`, qword 35) is accepted; see
`/tmp/sly-current-xgkick-recheck-20261117.log`. The renderer keeps presenting.
That run lacked enough world draw tracing to distinguish the retail game's
draw-request lists from the optional host mesh adapter. Its generic direct
parser does decode some batches without errors; one 24-vertex batch at direct
frame 233 lies wholly outside clip space, at `(-7.657,-22.815)..(-4.468,4.482)`.
The 1,254-vertex/five-draw batches appear before input too, so they do not by
themselves prove world geometry. Full trace:
`/tmp/sly-current-render-boundary-20261117.log`. That older run did not have
enough world draw tracing to establish the level transition or classify the
post-transition GIF stream; the newer trace above supersedes those conclusions.

**Writer attribution is now resolved:** host-only source probes show that
`ReadPlacedObjectModel` dispatches this kind-5 mesh to matched
`BuildStripMeshPacket` (`0x00163ED8`). Its trailing strip loop calls
`ReserveVifUnpack(pkt, 0xC, s->stride, off, ...)`; the live call has stride 7,
VU offset `0x1D4`, and writes 112 bytes. The packet cursor was `0x01FFC880`
and detached packet start `0x01FFB4A0`, a difference of 5,088 bytes. Adding
that offset to REF data `0x018D1270` gives `0x018D2650`, exactly the source
for qword 468; the preceding command is at `0x018D264C`. This closes the C
writer question. The VU1 `0x5f` XGKICK from qword 469 despite the valid tag at
468 is a confirmed packet-boundary anomaly, but has not been proven to be the
sole cause of black output. The latest direct trace above is the stronger
failure boundary. Keep the separate entry `0x411` float correction in mind;
do not patch either path without tracing its data contract.
Trace: `/tmp/sly-strip-target-reserve-cross2-20261117.log`.

**VU exceptional-float correction (2026-11-17):** the native VU translator
previously used host IEEE arithmetic for VU vector operations and `ERLENG`.
On the reproduced entry `0x411` path, a subnormal vector squared to zero and
host division produced infinity; `MFP` then copied infinity into a transform,
and later multiply operations could produce NaNs. The port runtime now flushes
exponent-zero operands/results to signed zero and clamps unsupported exponent-
255 inputs/results and divide-by-zero to finite signed max. The corrected
trace shows `ERLENG` returning `0x7f7fffff`, and no longer propagating an IEEE
infinity into `MFP`. This is a targeted host model in
`port/runtime/vu1_native.cpp`; matched decompilation and VU assembly remain
untouched. The post-Cross capture is still black because the independent
qword-469 XGKICK rejection persists. Build and trace:
`/tmp/sly-vu-float-model-20261117.log`.

The DMA ownership trace now ties that REF segment to the frame submission:
`TADR=0x003c3b00` is repeatedly submitted from retail `RunRenderThread`
(`0x0015EE80`), and the chain contains REF tag `0x003c3d60`, QWC 326, data
`0x018d1270`. Earlier model traces report that exact pointer and QWC as mesh 0
of the first skinned model draw (Sly; source mesh `0x00a113c0`, four bones).
This identifies the owning packet/model. The exact writer is now identified
as `BuildStripMeshPacket` by the live stride/offset/cursor trace above; it is
not the nearby `BuildLitStripMeshPacket` skin-weight stream.
Fresh DMA trace: `/tmp/sly-qword468-chain-visible-20261117.log`; caller trace:
`/tmp/sly-qword468-dma-caller-cross-20261117.log`; model pointer match:
`/tmp/sly-object-68-final-pose-20261116.log`.

**Earlier visible client check:** the native Wayland window was launched in the
foreground. The no-input frame shows the complete retail no-card warning
(`/tmp/sly1-native-visible-before-cross-20261117.png`). A process-local Cross
at frame 300 dismisses it; a later captured frame is fully black
(`/tmp/sly1-native-visible-after-cross-20261117.png`). The run log is
`/tmp/sly-vif-unpack-writer-cross-20261117.log`; it ends at the 8-second
diagnostic timeout (137), and no launcher process remains. This confirms the
known prompt-to-black failure, not a new boot regression.

An earlier trace resolves `entry 0x411`'s MSCAL caller to matched retail
function `0x00164868` (`BuildLitStripMeshPacket`). Its source calls
`g_litSkinWeightProgram` from `pkt[1]` when `hdr->count > 0`; the nearby
`0xC/count 8/address 0x1CB` reservations are in the strip loop using `pkt[2]`.
They are separate packet writers, so their shared function is not proof that
the strip bytes are the skin program's input. A targeted trace found no
`ReserveVifUnpack` record for format `0xC`, count 7, address `0x1D4`, matching
the command that wrote qword 468. The source-level `WriteVifUnpack` probe did
not identify that command's producer. The newer targeted trace above closes
that attribution as `BuildStripMeshPacket`; this historical probe did not
observe the trailing strip reservation because it had not yet been instrumented.
Do not reorder writers or alter the VU input based on proximity alone.

A fresh no-input run shows the complete retail no-card prompt in all eight
sampled frames (`/tmp/sly1-native-frame-{00..07}.ppm`). A separate run injected
one Cross press at frame 120; all eight post-input 960x720 captures are exactly
black (one color, zero mean). That run reproduces the known black world state,
not a new regression. Both runs ended at their 12- or 14-second timeout
(exit 137), with no launcher left running. Logs:
`/tmp/sly-fresh-noinput-audit-20261117.log` and
`/tmp/sly-fresh-client-audit-20261117.log`.

**Fresh VU stop context (2026-11-17):** rebuilt the current host32 target and
repeated the bounded Cross run. The frame remained black. The VIF trace still
shows the rejected qword-469 tag and accepted qword-35 tag; this run also
records a separate MSCAL entry `0x2` stopping at micro-PC `0x52d` on upper word
`0x01e049fc` paired with lower no-op `0x8000033c`. The 16 preceding instruction
pairs are in `/tmp/sly-upper-fault-context-20261117.log`. The trace-only change
in `vu1_native.cpp` records this context on unsupported upper operations; it
does not change emulation. Do not label the word a valid-but-missing operation
or treat it as a no-op until its retail encoding and control-flow path are
verified. The source qword trace's `SQ VF4,1(VI5)` writes are still ambiguous.
This is the latest current-client result; it confirms black output, but does
not prove which earlier change introduced it.

Two independent graphics-path failures are now more specific:

1. At VU1 entry `0x5f`, PC `0x68` XGKICK reads `VI6=469`. The VIF UNPACK at
   stream offset 5916 wrote qword 468 as GIFtag `0x100040000000806d` with A+D
   descriptor `0xE`; qword 469 begins its A+D payload (`value=0x39001`,
   register `0x48`). The kick from 469 is rejected because it interprets that
   payload as a GIFtag. The same valid tag is copied to qword 35 and a later
   XGKICK from 35 is accepted. Do not subtract one from XGKICK addresses or
   relax validation without explaining this two-kick sequence and checking
   transfer ordering.
2. VU1 entry `0x411` reads sequential qwords starting at 392. The new
   submission-correlated trace confirms VIF submission 144 writes qword 460
   as `0x0005b001, 0, 0x47, 0` at command offset 124968, then runs this VU
   entry at stream offset `0x2c4a0`; the entry reads qwords 460, 461, and 462
   at PC `0x448`. Its loop uses `VI6=316` as the target and exits after
   consuming qword 462. This is within one submission, so the earlier
   ordinary-float writes to qword 460 are overwritten before the VU reads it.
   The exact producer/layout contract is still unknown; compare the packet
   writer and VU input layout before changing either side. At PC `0x43b`,
   `MULA` multiplies finite
   `VF20` by `VF3.x`; this subnormal bit pattern makes ACC subnormal. Later
   `ERLENG` at PC `0x44a` squares a subnormal vector to zero and sets P to
   positive infinity. That value flows through `MFP` at PC `0x436` into
   `VF24.x`, then zero-times-infinity creates non-finite transform values.
   The first bad store is `SQ` at PC `0x445` from **VF27** (not VF11); its
   qword is later loaded into `VF2`. Do not clamp output vertices as a
   workaround. Next, identify which packet builder emitted the UNPACK at
   offset 124968 and verify its expected data layout against the retail VU
   program before changing the port's VIF/VU behavior.

The final same-build visual check is `/tmp/sly-final-visual-audit-20261117.log`
and `/tmp/sly-final-continue-audit-20261117.log`; the first confirms the
prompt and the second has two prompt frames followed by six black frames.
Trace logs include `/tmp/sly-xgkick-address-audit-20261117.log`,
`/tmp/sly-gif-origin-audit-20261117.log`,
`/tmp/sly-vu-store-q300-320-20261117.log`, and
`/tmp/sly-transform-correlated-20261117.log`. Detailed VU instrumentation uses
`OPENRAC_TRACE_VU1_XGKICK_DATA=1`; ordinary captures used
`OPENRAC_TEST_HIDDEN_WINDOW=1`, and only the post-Continue run used
`OPENRAC_TEST_PRESS_CROSS_AT_FRAMES=120`. Next, identify the producer of the
UNPACK at offset 124968 and compare its payload with the entry `0x411` input
layout, while keeping the qword-469 kick failure as a separate issue.
No matched decompilation source was changed.

## Previous bounded task (2026-11-16, paced client audit)

Fresh hidden captures confirm the standard client still has two distinct
visual states. With no input, the retail memory-card prompt is stable across
the sampled frames (`/tmp/sly-current-noinput-contact.png`). A process-local
Cross press begins dismissing that prompt; its panel shrinks during the
transition and the subsequent world display is black
(`/tmp/sly-current-16k-contact.png`). The shrinking prompt in that capture is
the dismissal transition, not the steady no-input screen. Neither state shows
a playable level.

The VU1 limit issue is now fixed narrowly in the port runtime. A fresh hidden
run with a 16,384-instruction override completed entry `0x26d` without a VU
budget warning or a VIF stream-stop warning; at the old 4,096 limit it stopped
at PC `0x2aa` and aborted the remaining VIF commands. The default for entry
`0x26d` is now 16,384; other VU entries retain the 4,096 default and the
existing environment overrides remain available. This does not fix graphics:
the first world XGKICK at PC `0x68` still rejects qword 469 because qword 468
contains the GIFtag and qword 469 contains payload. The hidden run still ends
at its 12-second timeout (exit 137), not a confirmed crash, and the post-input
frame is black. Logs: `/tmp/sly-clean-client-audit-20261116.log`,
`/tmp/sly-vu16k-client-audit-20261116.log`, and the rebuilt-default check at
`/tmp/sly-vu16k-default-client-audit-20261116.log`.

A later synchronized post-Continue capture shows why decoded draw counts were
misleading: the standard DirectRenderer receives large packets and decodes up
to 5,064 vertices / 844 draws, but one consecutive set of world frames puts
every decoded vertex at approximately NDC `(-7.4, 19.3)`, outside the clip
cube. Bypassing alpha/depth tests and masks in the opt-in flat diagnostic still
produces black captures. The next task is to trace those vertex coordinates
back to their queued GIF/VIF producer and fix the stream or decode state with
evidence. Keep the independent qword-469 XGKICK fault on the list; render draw
counts alone do not prove that packet supplied those vertices. See
`docs/port/SLY_GRAPHICS_HANDOFF.md` and
`/tmp/sly-direct-bounds-20261116.log`.

The short startup capture was misleading: it saved only the first eight
packets, which are black while the native render state is initialized. A
tracing-disabled 12-second hidden Wayland run sampled 703 presents. Its first
27 pixel samples were black; the full retail no-card prompt appeared by sample
35 and remained visible through sample 703. Text and border colors animate.
See `/tmp/sly-flicker-perframe-audit-20261116.log` and
`/tmp/sly-flicker-perframe-contact.png`.

The normal host32 frame loop was also unpaced because its 16 ms SDL wait had
only been injected by trace builds. The host staging tool now inserts that
wait for every build. A bounded hidden run reached 428 guest frames with
1,293 accepted DMA submissions and zero queue rejections before its timeout.
A separate tracing-disabled run captured 703 presents without a crash. This
stopped the renderer queue flood and produced a stable prompt after startup;
the overflow-safe queue limit remains a last-resort guard.

After Continue, the framebuffer still goes black. Entry `0x26d` now completes
with its scoped 16,384-instruction budget, so later commands in that VIF stream
are no longer skipped for this reason. The normal DirectRenderer does decode
large post-Continue batches, but a synchronized capture found all vertices in
several frames collapsed outside clip space; the producer and stream identity
still need to be established. The first world XGKICK still rejects qword 469,
so keep tracing that boundary separately through VIF, VU1 and GIF. Do not
relax validation or claim that optional renderer experiments demonstrate a
working game renderer.

### Render one recognizable world object from Sly's draw inputs

The steady no-input standard-client result is the retail memory-card prompt.
A process-local Cross input still produces a black post-input frame. The
prompt and world failures are separate states. The clean no-input run used no
optional Sly world renderer or solid-color diagnostic. Log:
`/tmp/sly-flicker-perframe-audit-20261116.log`; captures:
`/tmp/sly-flicker-perframe-contact.png`.

The green-line capture seen in the previous audit had
`OPENRAC_DEBUG_SLY_WORLD_SOLID_COLORS` enabled, so it was diagnostic output,
not the normal game renderer. Separate gray-horizon and stretched-purple
captures also came from the optional `OPENRAC_NATIVE_SLY_WORLD_RENDERER` path.
Neither is evidence that the ordinary VIF/VU/GIF renderer draws the world.

The earlier 4,096-limit run rejected the first world XGKICK at PC `0x68`,
address 469: qword 468 is the GIFtag and qword 469 is its payload. It also
exhausted entry `0x26d` at PC `0x2aa`, truncating its VIF stream before later
`MSCAL`, `BASE`, `OFFSET`, and `UNPACK` commands. The scoped 16,384 limit
removes that stop, but the standard-client frame still goes black and XGKICK
still rejects qword 469. Keep tracing the packet from the retail VIF/VU data
through GIF decode and normal renderer intake. Do not backstep addresses or
relax packet validation.

The optional geometry experiments found hundreds of repeated request
identities queued between presents; that renderer keeps the latest mesh per
identity, and real uploaded textures resolve. A port-only fix to
`func_001453B0` now uses its distance argument; `QueueModelPartDraws` moves
from zero queued parts to 23 on the first pass and as many as 78 later. The
DMA9 scratchpad copy is byte-for-byte verified for the first 12 part/state
blocks. These findings are useful for a later comparison, but they do not
change the standard-client result.

An opt-in solid-color capture found one full-view, depth-always mesh. Skipping
it reveals only a black sky and flat ground, so that mesh is not the only
blocker. The next task is to derive the real draw and material boundaries from
Sly's VIF/VU packets and get one normal packet to the framebuffer. The
projected-bounds trace is only a vertex estimate, not proof of visible
triangles. Do not change matched decompilation files or further raise VU
instruction limits to hide packet errors.

A separate host scene-graph fault is now fixed in the staged ABI adapter.
`GetTrackPosition` and `GetTrackRotation` were cast into attachment vtable
slots whose host signatures pass time before output pointers, while the
definitions put time last. That argument shift made the mode-3 animation
callback return NaN matrices. The adapter reorders those two staged function
signatures; matched decomp source remains unchanged. A fresh trace shows
finite callback matrices and no invalid model ancestor for object 68. This
removes one regression but does not produce visible world pixels: the fresh
post-continue framebuffer capture is still black. Keep the unresolved normal
VIF/VU/GIF packet failure at qword 469 as the immediate graphics blocker.
Trace controls are `OPENRAC_NATIVE_OBJECT_LOAD_TRACE` and
`OPENRAC_TRACE_PLACED_OBJECT_LOAD=1`; captures are
`/tmp/sly-object-68-pose-source-20261116.log`,
`/tmp/sly-object-68-abi-verify-20261116.log`, and
`/tmp/sly-native-frame-fixed-abi-20261116.log`.

The optional native world renderer still submits mesh approximations, and the
retail VU path still rejects the GIF kick at qword 469. Keep that issue
separate from the newly observed short-lived scene draws; neither result alone
proves a playable level.

### Native GIF queue allocation guard (2026-11-16)

A fresh Release build exposed a separate native-host failure: the XGKICK
append path could grow the shared GIF packet vector without a limit. That
append ran before the DMA-chain and GIF-stream checks, so later 16 MiB checks
could underflow `maximum - queued` after the queue had already exceeded its
cap, leading to `std::bad_alloc`. The port renderer now applies one overflow-
safe 16 MiB bound to all three append paths and rejects packets when the queue
is already full. The hidden Release client reproduced the queue reaching the
limit and emitted capped rejection warnings; it no longer threw `bad_alloc`
during the 10-second run. It was stopped by the timeout (exit 137), and
recognizable graphics remain unresolved. This is a host stability guard, not
a rendering fix; dropped GIF bytes can still leave the screen black. Log:
`/tmp/sly-release-queue-guard-20261116.log`. No matched decompilation files
changed.

The decomp's `BuildStripMeshPacket` gives the port an ordered `VertexRefs`
sequence, positions, UVs, tints, and per-object matrices. `FrameSource` does
not give vertex strip lengths: its stride is measured in qwords, and
`QueueMeshStrip` copies those frame-strided payloads into VIF memory. Do not
use those values to split the host vertex array. The current prototype instead
submits the entire array as one GL triangle strip, so its primitive topology
is still an approximation. The row-major `model * view_projection` transform
matches the translation in the final matrix row; existing captures show no
basis for changing matrix order.

A targeted runtime dump confirms the loaded VU1 code at PC 842 matches the
retail `.vutext` after parsing its interleaved MPG headers. The upload is not
shifted. The first world XGKICK at PC `0x68` starts at qword 469. Qword 468 is
the valid EOP GIFtag `0x100040000000806d` with 109 A+D writes; qword 469 is its
first payload item (`value=0x39001`, `address=0x48`), not a GIFtag. The retail
VU instruction at PC `0x67` increments VI6 from 468 to 469 immediately before
XGKICK. The runtime GIF parser is idle at that kick, so it is not continuing
an unfinished VIF DIRECT packet. This boundary mismatch precedes GL mesh
conversion. Reconcile the retail VU program's explicit pointer and the VIF
producer's packet layout before changing the translator, kick framing, or
validation. The following XGKICK at PC `0x6e` starts at qword 35 and is
accepted. The selected-program trace is enabled with
`OPENRAC_TRACE_VU1_PROGRAM_WORDS=842`.

Next, establish a valid Sly world GIF packet and derive one mesh's primitive
sequence from Sly's retail VU program and captured inputs. Then implement only
that mesh's draw conversion and material handling in the port. Keep the
prototype disabled by default and keep strict GIF validation. Any game-function
backfill stays in `port/game/sly1-us/hand/native/`; do not change matched
decompilation files.

Evidence: `/tmp/sly-vu65k-world-draw-trace-20261116.log`,
`/tmp/sly-gif-origin-capture-20261116.log`,
`/tmp/sly-xgkick-outcomes-capture-20261116.log`,
`/tmp/sly1-native-world-after-hud-vu65k.png`, and the source definitions in
`../sly1-decomp/src/main/placed_object_model.cpp` and
`../sly1-decomp/include/cursor_animator.h`.

### Reduce the no-card prompt flicker

The Nov 15 recording showed 49 black game-area frames out of 393. Draw-state
traces then found a full-screen, untextured black fill after the prompt on
some frames. The renderer already omits replacement fills on 448-line output,
because the native window clear supplies that background, but its 400-line
threshold missed Sly's 224-line scissor (`0..223`). `port/renderer/direct.cpp`
now recognizes full-height scissors from 200 through 240 lines as the same
low-resolution case. No matched game source changed.

A fresh five-second hidden-window capture built from the change saved eight
consecutive prompt frames, all visible. After startup, intermittent black
frames remained. A later detailed capture showed four black samples out of 227
post-startup frames (`/tmp/sly-flicker-copyfix-capture-20261116.log`). Each
black sample has one 120-vertex, full-screen textured draw at texture handle 1,
with `fbmsk=0xffffffff`, no prompt geometry, and black readback across the
whole game area. The immediately adjacent frames redraw the prompt. This is the
renderer suppressing a draw from an unavailable/placeholder texture while the
native present path clears the window to black each frame. It is not a missing
ISO read or an XGKICK/world packet failure.

The attempted extension of the low-resolution full-frame filter to textured
draw-buffer copies increased the observed black samples from two to four, and
did not target these draws (their TBP0/source is zero, not the draw buffer).
That change was discarded. The black frames instead correspond to the GS
framebuffer's contents persisting through a draw that writes no pixels. The
native present path now keeps a copy of the last frame and restores it after a
frame containing no visible draws; it still honors explicit full-height fills
as clears. It also binds the default framebuffer and resets write masks before
clearing. A six-second hidden capture recorded ten restored no-op frames and
zero fully black pixel samples after the prompt first appeared across 312
subsequent samples (`/tmp/sly-flicker-final-capture-20261116.log`). A captured
frame confirms the prompt stays visible; the short white dash at the left edge
remains unexplained.

A fresh offscreen capture with DMA-call tracing disabled and 100 eligible
frames skipped then saved eight consecutive samples; all eight show the full
prompt (`/tmp/sly-no-card-framecapture-skip100-20261116.log`). This short
sample does not rule out the intermittent flicker from the longer recording.
The frame dumper now accepts `OPENRAC_DUMP_NATIVE_FRAME_STRIDE=N` to space
its eight bounded samples across later eligible packets; the default remains
1 (consecutive frames). Use a larger stride to sample intermittent presentation
issues without generating a long video or large trace.
An offscreen per-present pixel trace then sampled 698 frames over 12 seconds.
Both text points were black only in frames 1–26, during initial screen
construction, and settled to the prompt color by frame 35. The panel border was
black through frame 34 and stayed blue from frame 35 through frame 698. No
black sample recurred after the full prompt appeared. This suggests the tested
offscreen build no longer has the post-startup black flicker; it does not prove
the user's visible-window flicker is gone. Trace:
`/tmp/sly-flicker-pixels-20261116.log`.

A follow-up hidden-window capture used SDL's actual Wayland driver and its
double-buffered OpenGL surface. The prompt settled by frame 35; all 427 later
pre-swap samples showed both text points and the blue border, with no all-black
sample. This covers the renderer's Wayland backbuffer path, but does not read
the compositor's displayed surface. Trace:
`/tmp/sly-hidden-wayland-present-20261116.log`.

The Codex-started Wayland run that was stopped for runaway logging later
reported world `0x870cf0` and advanced past game frame 1,500. It had no pad
trace, so it does not prove which key caused the transition or show that the
world rendered correctly. Its enabled DMA-call trace had emitted over 10 GB
of stderr; that diagnostic is now capped, but its effect on flicker was not
measured.

### The world draw path reaches a mismatched VU packet pointer

The host32 prompt regression came from an ABI mismatch in the staged copy:
matched callers declared `AddGifRegisterWrite(PacketWriter*, u64, u64)`, but
the port host ABI uses a 32-bit register address followed by a 64-bit value.
The source adapter normalized definitions but left copied headers unchanged.
On i386 this shifted the value in the call frame, producing the collapsed
scissor and texture registers. The adapter now normalizes both staged headers
and sources. No matching decomp file was changed.

The rebuilt launcher and the user's recording show the complete no-card prompt.
The scissor values are now `0x00de0001027f0000` and `0x00df0000027f0000`;
the renderer decodes them as the full 640x224 display area. DirectRenderer
decodes the panel into 1,254–1,494 vertices and 5–7 draws, with live texture
handles for its content. A separate `gs_stream` diagnostic still reports
`TBP0=0` and the placeholder, but that does not match the handles used by the
visible DirectRenderer draws and is not the active blocker.

The image still flickers: 49 of 393 game-area video frames decode as black,
with the longest black run lasting 7 frames (about 117 ms). A delayed native
capture also had one black frame among eight. Simultaneous samples of body text
and panel border go black together, even when packets arrive and DirectRenderer
decodes 5–7 draws. Only 2 of 10,388 present calls lacked packets, so packet
starvation alone does not explain the flashes. Skipping the per-present clear
in an opt-in diagnostic still produced about 9% black samples, so that clear
alone is not the cause. The short white dash at the left edge remains
unexplained.

The user's prior traced Space press dismissed the card prompt and loaded world
`0x870cf0`. The current long-running foreground process remained on the no-card
page after another Space press; because it started without `OPENRAC_TRACE_PAD`,
event delivery is unconfirmed. A separate offscreen run injected Cross at
native frame 120 and observed the pad transition `held=0x4000`, then release.
This reached the world draw path without taking focus from the visible window.

That run localized the first packet boundary. VIF UNPACK `0x6c`, immediate
`0xc1d4`, count 7, writes qword 468 as
`0x000000000000000e:0x100040000000806d`, a valid GIFtag with EOP and 109
one-register loops. The following vector writes qword 469 as
`0x0000000000000048:0x0000000000039001`, which is payload. VU1 loads VI6=`468`,
increments it at PC `0x67`, and reaches XGKICK VI6 at PC `0x68` with VI6=`469`;
the strict parser correctly rejects that payload as a new GIFtag. The preceding
captured DIRECT packets end with EOP, so they do not leave the GIF parser inside
this packet.

The follow-up trace records the VIF source offsets in that same DMA stream:
the UNPACK command is at byte `5916`, the tag vector at `5920`, and the payload
vector at `5936`. Separate UNPACKs write qwords 36 and 38 from byte offsets 868
and 2120, and qword 37 from offset 5496; the captured stream does not write
qwords 34–35. In the earlier diagnostic retry, the later XGKICK at VI7=`35`
still found qwords 34–35 zero. This locates the records, but does not yet
explain why the VU program advances to qword 469 or expects another packet at
qword 35. A segment trace ties the command and both vectors in transfer 196 to
DMA segment 4 (`data=0x018d1270`, stream bytes 832–6047); their source addresses
are `0x018d264c`, `0x018d2650`, and `0x018d2660`. No segment boundary falls
between this UNPACK command and its payload. The failure snapshot shows the
following SQ targets qword 627, not qword 35. The qword-35 tag therefore has
to come from another part of the
VIF/VU data flow or from a different effective XGKICK address.

The microcode is identified: the US ELF overlay table names program `437275`
at VU address 0 (EE address `0x00212168`), and its instruction words at PCs
`0x5f`–`0x6e` exactly match the runtime trace. The failed XGKICK snapshot at
PC `0x68` has VI3=`628` and VF7=`{0x4149d0e5, 0xc0d92618, 0x42877fa4,
0x4b008958}`. The following retail instruction stores VF7 at `-1(VI3)`, which
targets qword 627, while the later XGKICK uses VI7=`35`. This separates that
store from the missing qword-35 tag. The retail code increments VI6 at PC
`0x67` immediately before XGKICK; do not change that game code. Public PS2Tek
hardware notes describe VIF UNPACK bit 15 as adding TOPS. With the traced
TOPS, BASE, and OFFSET all zero, this UNPACK writes the tag at qword 468. The
Sony VU User's Manual is explicit that integer calculations have one-clock
latency with no calculation data hazards, and that XGKICK provides its VI
operand to GIF, which treats that value as the leading GIFtag address. Thus
VI6=469 is consistent with the instruction sequence, while qword 468 contains
the valid EOP tag and qword 469 its payload. This rules out assuming XGKICK
simply sees the old VI6. See the manual's [IALU pipeline section
(3.4.7)](https://docs.alexrp.com/mips/ee_vu.pdf) and XGKICK instruction
reference (4.6.24).

The earlier trace saw no VIF or VU store to qwords 34–35 before the failed
first kick, and its qword-35 packet appeared only in a removed backstep retry.
A fresh targeted native capture now sees the unmodified retail program reach
both instructions naturally: PC `0x68` rejects the payload kick; PC `0x6e`
then accepts the A+D packet at qword 35. The 1,760-byte packet dump contains
109 A+D writes, all to PRIM (`0x00`) under the same address decoding used by
the renderer. It contains no XYZ2 or XYZF2 writes, so this accepted packet
does not submit screen vertices. This corrects the earlier
backstep-only observation without explaining the first kick's missing tag.
Keep VIF/VU instruction semantics unchanged until the source of the skipped
tag is accounted for. Log and first packet:
`/tmp/sly-qword35-packet-audit-20261117.log`,
`/tmp/sly1-vu1-kick-00.bin`.

The follow-up hidden capture `/tmp/sly-vif-state-sequence-20261116.log` now
records that state through the failing call. For the three earlier MSCAL calls,
DBF alternates `0->1`, `1->0`, then `0->1`; the MSCALF for the `0x5f` program
switches it back `1->0`. BASE and OFFSET remain zero, so TOPS remains zero at
each switch. In the same stream, the `0xc1d4` UNPACK writes qwords 468–474
while DBF is 1, before that MSCALF. The first two vectors remain the valid EOP
tag at 468 and its payload at 469. The VU then increments VI6 and XGKICKs from
469, and the parser rejects the payload as a GIFtag on every repeated call.
This confirms the observed native double-buffer transitions and shows they do
not account for the one-qword tag/payload mismatch when BASE/OFFSET are zero;
it does not establish whether the native DBF transitions match retail hardware
or explain the separate qword-35 expectation.

An opt-in retry from qword 468 submitted the 110-qword packet, but captured
frames remained black. Execution then reached a second XGKICK at PC `0x6e`
from VI7=`35`, which also fails validation. The retry has been removed. Do not
backstep pointers or weaken GIF validation. The immediate next task is to
account for the complete VIF/VU data flow and double-buffer state against the
manual and retail command sequence, then find independent evidence for the
intended packet address. Keep masked UNPACK `0x78` support limited to the
observed selectors; column-fill and write-protect remain unsupported. The
no-card prompt's flicker remains a separate known issue.

An opt-in VU trace records the last instruction pairs and VI5/VI6/VI9/VI15
when a call hits its cycle budget (`OPENRAC_TRACE_VU1_LIMIT=1`). The default
budget remains 4096. With the diagnostic override
`OPENRAC_VU1_CYCLE_LIMIT=400000`, the captured calls no longer reported budget
exhaustion, but the same malformed XGKICK data was rejected and the game
stopped advancing at frame 142 (frame stage 2). DMA totals froze at 465
submissions; this did not produce a usable frame. Do not raise the default
budget based on this run: the XGKICK/VIF data flow is still unresolved, and the
longer VU work did not clear the visible stall.

The public [PS2Tek hardware notes](https://israpps.github.io/ps2tek/) document
the VIF TOPS-relative UNPACK rule. The linked [Sony VU User's
Manual](https://docs.alexrp.com/mips/ee_vu.pdf) documents XGKICK and the IALU
pipeline.
The sibling decomp's `PORTING.md` classifies `.vutext` as original assembly;
the 100% matching C result does not decompile these VU1 programs. This renderer
work stays port-only, with no changes to matched game functions.

For a future visible input attempt, set `OPENRAC_TRACE_PAD=1`. It records SDL
window focus, key down/up events, and pad reports so a missed Space press can be
distinguished from a prompt-state or game-input regression. Automated input
tests can stay offscreen using `OPENRAC_TEST_PRESS_CROSS_AT_FRAMES=N`.

### One deliverable: resolve the VU-to-GIF address boundary

**2026-11-16 continuation capture:** the port now follows OpenRAC's documented
one-instruction-pair XGKICK delay and rejects a malformed GIF packet without
aborting the rest of the VU program. The hidden run confirmed the first kick at
PC `0x68` is still rejected at address 469. Execution then reaches the second
kick at PC `0x6e`, address 35, but that packet is also rejected: qword 35 is
zero and qword 36 is vector data. The native frame after the injected Cross is
only a gray background, with no accepted native world meshes. This is progress
in the VU trace, not a playable or visual pass. Capture:
`/tmp/sly-vu-continuation-capture-20261116.log` and
`/tmp/sly1-native-world-00.ppm`.

**JALR ordering trial:** changing the aliased `JALR VI15,VI15` to preserve the
pre-link target sent execution to the helper at PC 1193, where the VU program
ended. The capture then had no VU XGKICK output and a black frame after input.
That candidate was reverted. The existing path still reaches both kicks, but
qword 35 remains unexplained. Treat the JALR ordering and helper path as open;
do not adopt the target-before-link change without evidence that it preserves
the retail draw path. Capture: `/tmp/sly-vu-jalr-capture-20261116.log` and
`/tmp/sly-vu-jalr-frame-07-20261116.ppm`.

Correlate the retail program's VI instructions, the VIF destination, and the
GIF leading-tag address using hardware-level semantics. Keep matching decomp
source intact, and keep any Sly function replacements in
`port/game/sly1-us/native/`. Strict GIF validation stays enabled. The previous
qword-backstep experiment stayed black and is not a candidate fix.

**Pass:** the first world packet is accepted from the correct VU address and a
native capture shows geometry from loaded Sly data. The no-card prompt flicker
and short left-edge dash remain separately open. Do not tune shaders, rank more
model candidates, or relax GIF validation without evidence.

A successful build, frame rate, callback count, texture upload, DMA count, or
decomp percentage is not visual progress. Keep each commit to one verified
boundary and update this handoff when the evidence changes.

The sibling checkout remains at `1cbb33fd`; public master is `8f7a85a3` and
retains 100% code matching. Hostgen reports the same 489 translated functions
and 12-function reachable no-C frontier on both revisions. The host32 source
adapter now handles the newer declarations and linkage in its generated copy.
A fresh build against `8f7a85a3` compiles all 194 matched units plus 85
separate port-only backfills and links with 99 missing-function traps. This is
compile/link evidence; matching source remains unchanged. The first runtime
attempt found an old port adapter changing the newer float-first ramp calls
back to the legacy register order. Gating those rewrites on the shared helper
declaration removed that camera fault.

A 15-second hidden run after the decomp refresh read six ISO spans (2,426
sectors), accepted 47,832 DMA submissions, and rejected three. Its sparse PC
samples vary across `QueryPlaneTreePoint` and other functions. The zero frame
and world counters were not enabled in that build; this is not evidence of a
stuck query or missing world. Later focused traces confirm the loaded world
advances at about 56 frames per second and reaches kind-5 and kind-6 draw
callbacks. The last confirmed visible result remains the no-card screen; the
gray kind-6 capture remains a failed experiment.

## Earlier renderer experiment notes (not the current blocker)

Do not add more startup shims or general renderer features. `FrameSource` is
an 8-byte stride/base descriptor; the selected payload can be a packed GIF
A+D state packet. The active cursor selects its frame offset. The renderer
validates the kind-6 payload and resolves its GS register values, but a bounded
run still produced only a gray horizon and ground. Submitting all three
sources of a kind-0 mesh as duplicate GL draws also failed to improve it.

The VIF trace supersedes the earlier uncertainty about the first lost boundary.
Masked UNPACK `0x78` now passes for the observed cases; the remaining question
is how VU1 execution at PC `0x68` produces the invalid XGKICK tag.

Recent NaN transforms are on nested objects 1483/1486 under object 80. Other
draws have finite matrices, so this trace does not yet show that those objects
block the first visible scene. Keep the trace as a lead, not a new general
object-transform workstream.

**Pass condition:** before implementing another render feature, document the
same target in the native and PCSX2 captures, including camera state, mesh,
frame-source/cursor state, UV/color inputs, VU output topology, texture state,
and expected silhouette. A rendered frame passes only when its silhouette
matches the reference in shape and
position. Any functional replacement stays under
`port/game/sly1-us/native/`, separate from matching decomp source.

## Retained kind-6 experiment notes

The source's input W is `fogBias + 28672` or `fogBias + 32768`. The
kind-6 VU transform builds clip W from its matrix and homogeneous constant,
using the source XYZ for position. The native `vec4(x,y,z,1)` therefore has
the right clip-space W; do not feed the raw fog value as OpenGL W. Fog remains
unimplemented in this flat-color pass.

The owner-class trace now separates three different draws. The three kind-6
meshes belong to class `0x54` (`CameraPositionedObject`), which moves with the
camera; they are a backdrop candidate, not confirmed foreground. The 30
kind-5 meshes belong to class `0x61` (`TextureRenderObject`) and are rendered
into a texture. Do not treat either as ordinary screen geometry.

The first measured owner class `0x02` (`PlacedObject`) part has 38 kind-0 lit
meshes and a finite transform. Decoding the first mesh's position UNPACK from
its draw-boundary packet shows that its 68 vertices cover only about 11 by 2
pixels at 960x720 under the captured matrices. This is an upper bound before
strip filtering, and it is too small to be a useful first-graphics target.
This measurement rejects that particular mesh as a first target. It does not
identify the next one; the bounded task above replaces the old candidate
ranking plan. Evidence: `/tmp/sly-targeted-world-part.log`, its
`/tmp/sly-world-part-{0,1,2}.bin` captures, and the [graphics handoff](SLY_GRAPHICS_HANDOFF.md).

## Historical host32 and kind-6 renderer experiment (2026-11-15)

At the time of this capture, the staged source was `1cbb33fd`: 4,807/4,807 US
functions and 100% of code matched. The current sibling checkout and active
staging snapshot have since advanced to `8f7a85a3`. The host32 build recorded
here compiled 194 matched units and 85 separate port-only backfills, mapped
4,400 catalogued entries, and linked 99 fallback traps. Those totals describe
build coverage, not game readiness.

The corrected focused trace changes the diagnosis. After one process-local
Cross press, the no-card page clears, the active world changes from the Splash
world (`0x6abdd0`) to `0x870cf0`, and the main loop advances at about 56 frames
per second. The earlier Wave 492 report of zero frames and no world was invalid:
those counters were only updated in the optional function-trace build, which
was off. Do not use that run as evidence of a hang.

The loaded-world path now has an opt-in native kind-6 prototype. It copies the
expanded `VertexRefs` positions and tint colors from `BuildStripMeshPacket`,
matches them to `PartMesh.kind` in `DrawModelPart`, and submits them with the
part and camera matrices to a native OpenGL triangle-strip draw. A hidden run
registered and drew the three 68, 89, and 106 vertex meshes. Its captured
frame was a uniform dark gray with no recognizable scene. This fails the
visible-geometry gate; do not describe it as graphics working. The prototype
is disabled by default with `OPENRAC_NATIVE_SLY_WORLD_RENDERER=OFF` until its
output matches Sly's VU clip and primitive behavior. Evidence:
`/tmp/sly-kind6-world-renderer-trace.log`,
`/tmp/sly1-native-frame-07.ppm`, `port/runtime/sdl_renderer.cpp`, and the
decomp's `src/main/placed_object_model.cpp`.

The eight-quadword `FrameSource` is a packed GIF A+D material-state packet,
not a topology/index list. `BuildStripMeshPacket` expands positions in
`VertexRefs` order, and the kind-6 VU program consumes those vertices in order.
The remaining unknown is the VU's clipping and GS output behavior, including
how this ordered stream becomes bounded visible primitives. The previous plan
to decode indices from `FrameSource` was incorrect.

### Graphics reset (2026-11-15; experiment record)

A direct-host probe first drew packet positions as a triangle strip and
produced no useful image. The current opt-in source-level adapter reached the
native renderer, but its frame is still a uniform dark gray. The implementation
is retained as an isolated experiment and disabled by default; it does not
replace matched code or satisfy the first-graphics gate.

The decompilation's 100% code match includes the 0x5550-byte `.vutext` section
as original assembly; those VU1 programs are not matched C. This is why 100%
does not mean every graphics operation is ready to compile for the host.

The initial experiment targeted one unskinned kind-6 mesh. Its output did not
pass the visual gate. The current task above replaces this target until a
specific captured draw can be paired with a reference frame. Do not decode
`FrameSource` as topology or add a general VIF/VU emulator.

### Previous candidate-ranking plan (superseded)

The prior Wave 503 conclusion that no model draw was reached was invalid: each
trace helper independently guessed the first world from its first model
callback, so it suppressed the target world when that callback was the first
one observed. Wave 512 fixes world selection by observing the transition in
the main loop. It confirms a deferred `DrawPlacedObjectParts` request for an
object with one part, then `DrawModelPart` receives 30 meshes of kind 5, with
the first mesh carrying 326 quadwords of packet data. Other parts include kind
0 meshes and unskinned kind-6 meshes. Evidence:
`/tmp/sly-wave512-model-correct-filter.log` and the 2026-11-15 hidden capture.

This plan is superseded by “One graphics slice” at the top of this file. The
draw capture and gray-frame evidence remain useful; further candidate ranking
is paused until a specific finite-transform draw can be compared with a
reference frame.

Keep this at the Sly draw boundary. Do not build a general VU interpreter or
extend the device models as the gameplay renderer. A successful build, active
game loop or HUD draw does not satisfy the gate: loaded-world geometry must be
visible and correctly shaped.

Keep any functional replacements under `port/game/sly1-us/native/` so they can
be replaced cleanly when the corresponding function is decompiled. No title,
controllable level, or playable build is confirmed.

No title, controllable level, or playable build is confirmed. The no-card panel
is the last confirmed visible milestone. This is a blocked graphics vertical
slice, not a backlog of unrelated small fixes.
Playable milestone gates: (1) first advancing game frame with a valid scene
draw; (2) title/menu with responsive input; (3) controllable level with camera,
collision, and stable rendering; (4) save/load and audio sufficient for a
release candidate. The no-card screen is a startup milestone, not a playable
build.

## Native loading checkpoint (2026-11-14, Wave 128)

The hidden host32 launcher compiles 194 matched source units and 68 isolated
port backfills. It maps 4,384/4,807 catalogued functions, and the direct-call
frontier resolves 825/831. One in-process Cross press dismisses the retail
no-card warning and resumes world loading. No title, gameplay scene, or visible
graphics are confirmed.

Waves 119–127 fixed several native-host ABI gaps in the generated staging copy:
the `CopyEffectSpawner` source and shared record are forwarded, placed-object
load streams are forwarded through base loaders, and the missing source for
`CopyObjectData` no longer arrives as a float bit pattern. A separate native
`func_001B1AC8` backfill initializes `RangeRecord` from decomp notes and layout;
the assembly remains unchanged in `sly1-decomp`. Staging also makes the
EE-carried arguments explicit for animation marker lookup, blend-entry lookup,
keyed rotation lookup, and canonicalizes call-site argument order for matched
`EvaluateHermiteVectorSegment` C.

The current run passes those paths, then faults in matched
`BuildMatrixFromRotationAndTranslation` (`func_00188B08`, linked EIP
`0x080ab719`) while `BindAttachmentTargets` builds a relative transform. The
source pointer observed by the faulting load is `0x49`. Trace the receiver and
the three matrix arguments at the call boundary; do not guard or substitute
the invalid pointer. Latest run: `/tmp/sly-wave128-clean-run.log`; cleaned
build: `/tmp/sly-wave127-clean-build.log`.

Runs keep the SDL window hidden and inject Cross inside the game process, so
they do not take desktop focus or send operating-system input. No card-format
action was taken. Keep functional replacements in `port/game/sly1-us/native/`;
when equivalent hostable matching C becomes available, remove the port copy.

## Resolved RPC wait diagnosis (2026-11-14)

The user pressed Space once on retail no-card page 3. The trace confirms Cross
was observed and dismissed the warning. `RunPendingWorldLoad` decoded the
queued world key and opened the sector stream (`stream_open=1`), but game
frames stopped at 112 before `ReadBufferedBlockHeader` returned. The active
thread repeatedly formatted the retail `Sif says RPC isn't busy, but we still
don't have returns from the IOP!` warning from `IsIopReplyReady`.

The first port-only completion attempt wrote reply sentinels only through the
legacy `func_001F8290` address and did not resolve this call path. Current
decomp callers reach the source-level `CallRemoteService` body, so the isolated
port backfill covers that name and the legacy address. Link disassembly showed
those source callers pass raw client address `0x0027b4c8`, not the mapped
`0x0827b4c8`; the backfill now accepts both. The rebuilt service still needs a
post-Continue runtime confirmation. Keep this implementation in `native/`; do
not edit matching source in `sly1-decomp`. The failing trace is
`/tmp/sly-wave60-rpc-retest.log`.

## Resolved native pad boundary (2026-11-14)

The raw-address RPC correction was exercised with the user's Space press. The
trace now passes `ReadBufferedBlockHeader` (markers `0x1e4ea805` through
`0x1e4ea807`) and returns from that stage, resolving the previous IOP reply
spin. It then faults at `0x0805af6a` inside the matched `SetPadActuatorBytes`
body (`func_002049E8`): `GetNewerPadFrame` returned null because the SDL pad
adapter does not allocate a PS2 pad DMA slot. The null slot is a hardware
rumble path, not game input. A separate native backfill now returns the
retail “pad frame not ready” result (0) for rumble requests while preserving
the upstream function body for future replacement. Wave 62 rebuilt and
confirmed the null rumble fault is gone. Evidence:
`/tmp/sly-wave61-raw-rpc.log`, `/tmp/sly-wave62-rumble-guard.log`.

## Resolved fog property callback ABI (2026-11-14)

Wave 62 passed the block-header read and the pad rumble call, then faulted at
`0x080768e8` in `SetCameraFogColor` (`func_00143A08`) on the value pointer
`0x20`. Retail `ApplyPropertyRecord` calls this type-13 property callback with
`(target, color_pointer)`. The matching `SetCurrentCameraFogColor` source is
declared with no parameters and drops the color pointer in its host C call;
the retail MIPS wrapper replaces the first argument with `g_camera` while
preserving the second. A port-only two-argument wrapper now mirrors that ABI
and forwards the pointer to the matched setter. The host build and linked-code
disassembly confirm this route; the next runtime must confirm the callback
succeeds and identify any further world-construction faults.
Evidence: `/tmp/sly-wave62-rumble-guard.log`.

## Latest Continue trace: empty attachment notification (2026-11-14)

Wave 63 passed the fog callback and the block-header stage, then faulted at
`0x08048f7f` in matched `HandleAttachedSetNotification`
(`func_001EF3D8`). The callback received a kind-2 detach for a target absent
from the attached set; the matching C loop ended with a null link and read
`n->target` before its null check. The first port guard handled an empty list
but Wave 64 showed the same case with a nonempty list and no matching target.
The port wrapper now searches the full owner-link chain, but Waves 65 and 66
still fault at the same matched-body null dereference. The descriptor points at
the wrapper, yet conditional trace values in the wrapper were absent, so the
runtime callback route and arguments remain unresolved. Do not count the guard
as effective until a trace proves it intercepted the failing notification.
Evidence: `/tmp/sly-wave63-fog-callback.log`,
`/tmp/sly-wave64-empty-attach.log`, `/tmp/sly-wave65-attach-list-guard.log`,
and `/tmp/sly-wave66-detach-trace.log`.

Wave 67 verified that the process-local Cross input works: it reached the same
fault without sending a desktop key event or taking keyboard focus. Waves 68
and 69 emitted no entry trace from the port wrapper or guest callback thunk.
Wave 68's bounded counter advanced even before trace output was enabled, so its
empty trace is inconclusive; Wave 69 only excludes that guest callback-thunk
route. Wave 70's crash snapshot recorded EBX `0x00a808a8`, word 0 `0x01000021`, and an empty
attachment head at `+0x550`; treat EBX as a candidate object pointer until the
callback ABI is established. Evidence:
`/tmp/sly-wave67-local-input.log`, `/tmp/sly-wave68-wrapper-entry.log`,
`/tmp/sly-wave69-thunk-map.log`, and `/tmp/sly-wave70-crash-object.log`.

The SDL launcher supports a process-local, one-frame Cross input for
non-interfering tests: set `OPENRAC_TEST_PRESS_SPACE_AFTER_FRAMES=N` to inject
the press after N native input polls. It does not send a desktop key event.
Pair it with `OPENRAC_TEST_HIDDEN_WINDOW=1` to keep the test window hidden and
prevent the window manager from activating it. Wave 71 verifies the hidden
launcher still submits native DMA and the local press reaches the same fault.

**Next:** trace the actual callback entry and argument layout for the matched
detach body, correct the port-only shim or dispatch route, then continue
through world construction and confirm visible title or level graphics.

**Current evidence (2026-11-14, Wave 71):** the SDL host32 launcher reaches
retail no-card page 3. Its process-local one-frame Cross test input is observed
by the native pad adapter and advances world loading past the block-header and
rumble calls. The hidden-window run still submits native graphics but then
faults at `0x08048f7f` in the matched attached-set notification body before
world load returns. No Sly window appeared on the desktop during this test; no
title or gameplay scene has been demonstrated and no card-format action was
taken. Latest log: `/tmp/sly-wave71-hidden-input.log`.

The current decomp header defines `ElementStack` as 0x34 bytes, but
`warp_menu.cpp` still
declares its global backing array as four bytes. Wave 47 register capture
confirmed the active stack pointer was `g_elementStack` and its `+0x2C` count
had been overwritten by adjacent host BSS. The port staging adapter now sizes
the host-only definition as `sizeof(ElementStack)`. Wave 48 then ran for over
3,100 frames without crashing and stayed on retail no-card page 3 with two
stack entries. A fresh frame capture confirms the correctly proportioned
warning panel and selected Continue; no title or gameplay scene is confirmed
and no card-format action was taken. Image: `/tmp/sly1-wave48-current.png`.
Run log: `/tmp/sly1-wave48-run.log`; fault registers:
`/tmp/sly1-wave47-regtrace.log`.

Retail `PollPadInput` and `UpdatePauseMenu` expose the next issue: the game
forms its button mask as `(buf[2] << 8) | buf[3]`, then uses D-pad masks
`0x1000/0x4000` and Cross mask `0x0040` for menu navigation/accept. The shared
OpenRAC SDL report serializes the standard DualShock word high-byte first, so
Cross and directions were landing in the wrong Sly masks. The Sly-only
`func_002042C8` libpad backfill now swaps the two button bytes at this boundary;
the generic OpenRAC pad serializer and decomp source remain unchanged. Wave 49
compiles and links this change. It still needs one runtime input check before
we can claim Continue advances.

The prior build's post-input trace showed that page-3 `UpdatePauseMenu`
activates Continue on `flags & 0x0040`; `ActivatePauseMenuSelection` closes the
page and sets card display mode to 2. Then `UpdateElementStack` state 0 calls
`StartObjectSubGroupFinish` when `D_002701B8` is clear. That post-input evidence
used the split Wave 57 clock layout and must be repeated before relying on the
following subgroup timer result. Do not skip directly to forcing a title or
level.

**Build baseline:** public `sly1-decomp` master is `83f2b50d` (2026-11-14),
with 4,807/4,807 US functions and 100% of code matched (4,498 exact C and 311
original assembly), plus 99.9675% of data (140 bytes remain unmatched). Recent
upstream commits name the five world bosses and reorganize their matched source
files; they add no matched code. The host32 build compiles against those paths.
The latest Wave 70 build compiles 194 Sly units and 64 port-only backfills. Its
combined object defines 4,380 catalogued function entries and resolves
825/831 direct-call frontier functions; indirect calls are not included. These
are build counts, not runtime coverage. The native trace and exact diagnostics
are documented in the port roadmap.

**Immediate tasks:**

1. Trace how the matched attached-set notification body is entered during
   `ReadBufferedBlockHeader`; Wave 68's missing wrapper trace is inconclusive
   because its counter could advance before logging was enabled. Verify the
   callback ABI and object descriptor before changing the backfill or dispatch
   route. Do not edit matching source.
2. The Wave 50 port-only C backfill for `FilterParticleVerticesVu0` is an
   approximation: its expiry, compaction, bounds and spin-angle behavior come
   from the matched caller, while its particle transform/integration is based
   on the sibling effect path because the VU microprogram's exact arithmetic
   is unknown. Validate it only once particles are reached; replace it with
   exact recovered behavior when available. Wave 55 adds a port-only CPU
   implementation of `CollectLightsForSphere` for point-light ranges and
   six-plane volumes. Its VU0 boundary and rounding behavior is not runtime
   verified; exercise it when normal startup reaches rendered scene objects.
   The latest
   [`sly1-decomp porting guide`](https://github.com/OpenThievius/sly1-decomp/blob/master/docs/PORTING.md)
   explains those two VU0 microprogram call sites and says to reimplement
   their work there, not add a VU0 execution model. Keep replacements under
   `port/game/sly1-us/native/`; the direct host32 path now routes this call
   site to its backfill, while the legacy C-only hostgen route still uses
   no-op adapters.
3. Replace remaining missing-function traps along the observed boot, frame
   and render path with narrow port-only C backfills. Retire each one when
   matching decomp C becomes available.

Do not format a card, force UI visibility, or alter matching source to
fabricate a scene. Keep saves and rendering work in the port repository.

The visible unformatted-memory-card prompt came from the separate experimental
guest-runtime launcher. Its memory-card probe reports no card; the user pressed
Enter several times and saw no response. No format was selected. Do not extend
the SIF/device model to suppress this prompt or claim gameplay from it. Keep
that runtime's graphics traces as research only. Continue on the compiled
host32 C/C++ path, keep functional missing-function replacements in the port
tree, and replace them with matching decomp C when available.

## Experimental guest-runtime trace history

The following dated run notes describe the old guest-runtime experiment. They
are not results from the no-emulation host32 launcher and are not playable
milestones.

**Latest startup callback recovery (2026-11-12):** a bounded guest-runtime trace
identified four reached assembly-only functions that had been falling through
as no-op host stubs: `func_00138828`, `func_001DC7D0`, `func_0019E168`, and
`func_001BB580`. Exact US retail assembly is now selected for them in
`port/game/sly1-us/hostgen.json`; this is build configuration, not replacement
C. The rebuild reports 527 assembly translations (up from 515), and a 15-second
run has no more warnings for those four functions. It still reaches Splash at
frame state 3 with 37 inactive UI objects, no image uploads, and the same
black output. The callback queue helpers `func_001B5EA0`, `func_001B5EC0`, and
`func_001DBBD8` were then selected from exact US retail assembly. The follow-up
trace confirms the callback now dispatches (stages 209, 211, 207, 206), but the
walker remains on state index 2 / event `0x261` / self-edge `2 -> 2`; no target
setter call occurs. A separate screen-element trace sees mode resets and Hide
calls, but no Show call. The trace does not implicate the renderer's window
presentation. Keep `func_0019E418` unselected: its exact retail body is just
`jr $ra; nop`, so its no-op warning is expected. Follow-up log:
`/tmp/sly-wip-followup-state-ui-12s-20261112.log`.

**CountdownNotifier callback recovery (2026-11-12):** the fresh warning trace
showed `func_00148E40` (CountdownNotifier message handler) and
`func_001DBCC8` (generic deferred-call append) still executing as no-op stubs.
Exact US assembly is now selected for both, plus `func_00148E18` (completion),
`func_00148EF8` (timer), and `func_001B6598` (`GetStateWalkerCurrentKey`).
The native build reports 527 assembly functions and links successfully. A
12-second startup trace removes the warnings for this cluster and confirms the
animation callback stages dispatch, but the graph remains on its state-2
self-edge; sampled frames show zero vertices and image uploads. Log:
`/tmp/sly-wip-notifier-final-12s-20261112.log`.

**First visible experimental startup prompt (2026-11-12):** the user confirmed
that the guest-runtime window displays the unformatted-memory-card prompt with
readable message text. The choice labels appear as button/placeholder glyphs.
The current memory-card RPC probe returns “unavailable”, matching the game’s
normal no-card flow; formatting was not selected. The framebuffer dump remains
black even though the window was visibly rendering, so do not rely on that
capture path as evidence that the UI is absent. Next: inspect why the Yes/No
labels become glyphs, verify arrow/Cross input through this prompt without
automating game input, and implement a safe native save-card backend before
claiming saves work. Upstream decomp remains at Wave 32 (`86682dd`).

**Animation and pose-path assembly recovery (2026-11-12):** selected exact
US retail assembly for the reached linked-animation, pose-source, tangent
curve, matrix/trig, animation-event, pad-select, track-pose release, world-list
release, deferred-call cleanup, and list helpers. Hostgen now reports 798
translated functions, 546 from assembly, and 2 stubs. The 12-second native
trace advances global element-stack state to 5 and submits 240 vertices, but
the sampled RGBA values are alpha-only black (`0x80000000`) and there are no
image transfers. The Splash graph stays on state-2 event `0x261`; the black
frame is now localized to UI/render resource data rather than missing startup
dispatch alone. Trace: `/tmp/sly-wip-animation-cluster-12s-20261112.log`.

**Latest graphics-path correction (2026-11-12):** the current port-only
`func_0019D6C0` follows Wave 32's `RunWorldFrameLists`: it reads slot 40 from
each object's descriptor (`descriptor + 0xA0`) and dispatches it with the
camera, then updates color fades. That callback reaches the class-4 object's
retail `ApplyPlacedObjectTransform`, which exits on its special bit 32 before
its draw/queue methods. The previous note that the generic dispatcher skipped
the drawable object was incorrect. See
`/tmp/sly-wave32-direct0100-render-12s-20261112.log`; do not bypass the retail
flag to make this diagnostic level appear to render.

**Historical decomp refresh (Night 26, 2026-11-11):** fetched `origin/master` through
`1bd176fe` (Night 26, PR #119), 14 commits ahead of the clean local checkout
at `bfc74817` (Wave 24, PR #117). The newer changes mainly improve names,
static-data and PAL tracking, and reports; they add no matching C for the
current startup/UI draw path. To keep the upstream checkout untouched, the
native build now reads the temporary snapshot `/tmp/sly1-decomp-night26-source`
for C and the generated US assembly copy `/tmp/sly1-decomp-wave24-asm` for
retail assembly. Hostgen still translates 732 C functions, 405 US assembly
functions, and 2 stubs. Keep functional fixes port-only.

**Range-bound recovery (2026-11-11):** added exact US retail assembly for
`UpdateRangeRecordBounds` and `AreFloatsNearlyEqual`, which the decomp's
range-record source identifies as the sphere-bound calculation and its
comparison helper. This addresses a plausible culling/packet-queue blocker.
A fresh 45-second native run still reaches 22.08 guest seconds with zero
vertices, image tags, GIF-chain submissions, visible text, or queued movie;
the new functions emitted no missing-function warnings. This rules out only
the missing implementations themselves, not the wider culling path. Log:
`/tmp/sly-wave24-range-bounds-45s.log`.

**Current black-window diagnosis (2026-11-11):** normal startup reaches frame
state 3, constructs 37 UI objects and fonts, and runs the UI update/draw
dispatchers. The static UI pointers and vtables are valid, but no active draw
method is called. Text-buffer setup `func_0015DB28` runs five times; glyph/text
draw and screen-element callbacks do not. Menu page stays `-1`, there are zero
vertices/image transfers/GIF submissions, and `func_00194F80` is not reached.
The user's Enter taps were captured as Start (`held=0x0008`) but do not change
the state or show graphics. Retail `UpdateElementStack` confirms this is
expected: in state 3, it opens the pause layer only for R1 (`0x0800`) and opens
the map for L2 (`0x0100`) when a map page exists. It does not consume Start.
The keyboard bridge maps Q to R1 and `1` to L2. This gives us a non-invasive
manual input check when the user is available, but does not explain or repair
the missing automatic Splash/title transition. Latest trace:
`/tmp/sly-title-transition-night26.log`.

**Historical native input correction (2026-11-11; superseded 2026-11-14):**
the initial source read concluded that the host adapter should emit the button
word high byte first. Cross-checking the retail masks against standard
DualShock button values showed that conclusion was reversed. The current
Sly-only boundary swap is described in the current evidence above; it keeps
the shared OpenRAC serializer intact. Earlier Enter traces remain valid: Enter
is Start, and Start does not advance the no-card Continue page.

**Reached world-update recovery (2026-11-11):** exact US retail assembly now
provides eight reached object/animation callbacks that were empty stubs:
`UpdateObjectTree`, `SeekAnimationEventCursor`, `RunAnimationEventsToTime`,
`ApplyAnimationLoopMode`, `ApplyListedPoseSources`,
`AddAnimationPlayerCallback`, `UpdateMatchingListedObjects`, and
`SetRangeRecordPosition`. The rebuild has 732 C translations, 396 assembly
translations, and 2 stubs. A 45-second native run reached game time 22.08
seconds but still had no queued movie, world-load request, vertices, image
tags, or GIF submits; all 37 UI elements remained inactive. The neutral pad
trace means an input transition was not tested. More startup time does not
advance this path. Log: `/tmp/sly-update-object-tree-summary-45s.log`.

**Renderer/UI stub recovery (2026-11-11):** a fresh native trace exposed 22
reached visual routines still running as empty stubs. Exact US assembly is now
selected for the text panel and arc/item initializers, UI state helpers,
packet-buffer helpers, masked-image packet construction/submission, resident
image transfer, texture-register setup, and frame animator step. Build coverage
is 732 C translations, 362 assembly translations, and 2 stubs. The 20-second
follow-up has no warnings for this cluster and raises the initialized font
count from 35 to 37, but all 37 screen elements remain mode 0 and renderer
counters remain zero (vertices, visible text, image tags, and GIF chains). The
Splash graph's player advances to 9.48 seconds while its `0x261` edge remains
`2 -> 2`; its gate is null and the countdown remains at zero. Log:
`/tmp/sly-renderer-retail-asm-20s.log`.

**UI state callback recovery (2026-11-11):** the follow-up trace found another
11 reached no-op callbacks for text-panel and timer state, screen-element
hiding, cursor-frame selection, texture-register lookup, and near-camera group
updates. They now use exact US retail assembly; the launcher rebuilds with 373
assembly translations. The next bounded native run has no warnings for this
cluster, but still shows 37 inactive screen elements and zero vertices, text,
image tags, or GIF submissions. The player remains on event `0x261`, state
`2 -> 2`. Log: `/tmp/sly-ui-state-retail-asm-20s.log`.

**Extended startup trace (2026-11-11):** restored exact US assembly for 15 more
reached callbacks covering GIF ST entry creation, text-panel text/subject,
object-frame transforms, and camera/world-list updates. Build coverage is now
732 C translations, 388 assembly translations, and 2 stubs. A 65-second native
run reaches 32.15 guest seconds but still shows global element-stack state 3
(normal state), no pad buttons, no movie queued, no world-load request, all 37
elements inactive, and zero vertices, visible text, image tags, or GIF
submissions. Wave 24 identifies `0x275BC0` as `g_elementStack` and
`func_001E9108` as `UpdateElementStack`; state 3 is not an app-level input
wait. The game is not merely taking longer than the earlier short traces to
reach graphics. Log:
`/tmp/sly-long-startup-65s.log`.

**Frame initializer recovery (2026-11-11):** selected exact US assembly for
the reached `func_00193210`, `func_00193270`, and `func_00193288` helpers after
the runtime trace showed they were called as no-op stubs by the frame setup
initializer. The rebuilt launcher links with 340 assembly translations. A
fresh 15-second trace has no warnings for those three, but still shows zero
vertices, visible text, and GIF submissions. It also confirms Splash graph
object ID `0x25f` is present and stays on its state-2 self-edge for event
`0x261`; the missing-link diagnosis is obsolete. Log:
`/tmp/sly-frame-init-asm-15s.log`.

**Font/layout helper recovery (2026-11-11):** exact US assembly is now selected
for reached no-op helpers `func_001E2268` and `func_001E22C0`. Build coverage
is 732 C translations, 340 assembly translations, and 2 stubs. The next
15-second native trace confirms both no-op warnings are gone, but all 37 UI
objects remain inactive and vertices, visible text, and GIF submissions stay
at zero. The CountdownNotifier remains state 0 with a null `+0x338` gate and
zero timer (range 4–6); its graph remains on an unarmed state-2 self-edge for
event `0x261`. Log: `/tmp/sly-font-layout-init-asm-15s.log`.

## Current status (2026-11-11)

**Contact-update bypass and renderer re-entry (2026-11-11):** a fresh run
confirmed `func_00138C00` faults at guest address `0xBF800228` while walking
the active world's collider list. Eleven class-`0x1e`/EffectOwner records had
null contact-table pointers; temporarily unlinking those records did not fix
the fault, so that attempted list mutation was discarded. The port now
temporarily bypasses contact updates at the load, resource-service, and frame
update call sites. This is an explicitly temporary `hand/native_c/` backfill:
it disables collision/contact simulation and must be removed when collider
registration is corrected or upstream code can be hosted. With the bypass,
the launcher stays alive through a 12-second run and reaches regular graphics
submission. `OPENRAC_TRACE_DIRECT=1` shows large scene packets decoding to zero
vertices and a repeated 240-vertex untextured UI draw with zero STQ and black
RGBA. The screen remains black. The large packets are image-transfer streams,
and the isolated 48-byte VIF rejection occurs before steady frame output; do
not treat either as the main geometry lead. Trace the producer and activation
state for the static 2,128-byte UI packet, including why its vertex attributes
are black/zero and its first position is outside the viewport. Logs:
`/tmp/sly-contact-bypass-20261111.log` and
`/tmp/sly-contact-bypass-render-20261111.log`. A packet capture confirms the
2,128-byte draw packet is identical across captured frames (SHA-256
`5e3373eaa5afba0e7f8eeb570de46d011e2aa4684ee80bc7ed8d2bc3109c1321`); the
960x720 host frame capture is fully black. Capture log:
`/tmp/sly-frame-packet-dump-20261111.log`; packet binaries and PPM frames remain
in `/tmp/sly1-native-render-packets-*.bin` and `/tmp/sly1-native-frame-*.ppm`.
The added bounded `OPENRAC_TRACE_DMA_CALL=1` trace maps the steady 2,128-byte
frame packet to a 32-byte GIF chain from `0x00343b00` and a 1,216-byte chain
from `0x00625030`; both have two DMA tags. The latter begins with GIF tag
`0x1000400000000009`, followed by the 14-register descriptor `0xE`. The VIF1
submission at `0x003c3b00` contributes a 736-byte DIRECT payload. This isolates
the packet components but has not yet identified which component contains the
invalid vertex data. The same trace sees channel-2 sends with source zero; the
native walker currently treats address zero as EE RAM and reads 265,472 bytes.
Retail `func_00202A80` also starts DMA with a zero source, so do not add a null
short-circuit without confirming the caller's intended behavior. Trace:
`/tmp/sly-dma-chain-headers-20261111.log`.

**Trace output cap (2026-11-16):** a long-running diagnostic launcher produced
over 10 GB of stderr because the host32 DMA hook printed every transfer. The
process has been stopped. `OPENRAC_TRACE_DMA_CALL=1` now prints the first 128
calls, then one sample every 256 calls up to call 4,097, followed by a single
cap notice. This keeps startup detail while bounding output. The heavy trace
may have worsened frame pacing, but it has not been established as the source
of the user's visible flicker.

**Reachable screen setup callbacks (2026-11-11):** the runtime trace showed
`InitScreenElements` calling no-op stubs for timer, gauge, framed text,
pointer-caption, letterbox, and arc-sprite initialization. Exact US retail
assembly is now selected for those callbacks and their reached small helpers;
the preceding text prep/measurement cluster is restored too. The build
succeeds with 732 C translations, 335 assembly translations, 2 stubs, and
3,841 functions without C. A fresh 10-second native trace has no no-op warnings
for either restored callback cluster, but all 37 UI objects remain at mode 0
and it still reports zero vertices, image tags, and GIF-chain submissions.
This removes missing initialization behavior without producing visible
graphics. Log: `/tmp/sly-wave22-screen-init-asm-10s.log`.

**Decomp refresh (2026-11-11):** canonical `sly1-decomp` master advanced from
`55d39f28` to `4c574846` (Wave 22, merged as PR #115); the clean local
checkout at `/home/reaper/sly/sly1-decomp` is fast-forwarded to this commit.
The US report now lists 4,760/4,807 matched functions and 93.25% of code
bytes. Wave 22 newly matches `func_001B55C0` (triangle animator slot 4) and
`func_001536C8` (`AddActiveSetItem`), and expands state-walker C. Hostgen
still cannot stage `src/main/state_walker.cpp` as C because shared headers
contain duplicate `contacts` members, failed layout assertions, and C++-only
declarations. `src/main/file_14EE40.cpp`, which contains `AddActiveSetItem`,
also fails C staging on C++-only methods and incompatible layout assertions.
Neither function was reached in the latest runtime trace, so this rework does
not yet move the visual frontier. Rebuilding against
`4c574846` succeeds with the same 732 C translations, 308 assembly
translations, and 2 stubs.

**Text preparation callback recovery (2026-11-11):** hostgen was emitting a
no-op for `func_001E1FD0` even though the port already had a functional
varargs formatter replacement, because its source unit cannot be staged by the
native C frontend. It is now registered as a standalone port-only backfill,
alongside `func_001E20B8`. Exact US retail assembly is also selected for
`func_001AA5A0` (screen text glyph substitution) and `func_001E2138` (string
copy). The native build succeeds with 732 C translations, 308 assembly
translations, 2 stubs, and 3,868 functions without C. A 10-second run confirms
the callbacks no longer emit no-op warnings, but UI objects remain in mode 0
and the renderer still submits zero vertices/GIF chains; no graphics milestone
yet. Log: `/tmp/sly-aa5a0-exactasm-10s.log`.

**Initializer callback recovery (2026-11-11):** after re-fetching the public
decomp, `master` remains `55d39f28` and open Wave 21 is `4097b806` (PR #114).
Wave 21 changes 82 files, mostly declarations, signatures, and generated
reports; it adds no new function on the current graphics path. The build cache
uses canonical `master` C from `/home/reaper/sly/sly1-decomp` and US retail
assembly from the separate wave-16 checkout.

The initialization callback table in port-only `func_00185B38` was invoking
24 functions that had exact US retail assembly available but were still
no-op stubs in the native build. The port now selects those function bodies
from assembly in `hostgen.json`; this includes display setup `func_001688B8`,
render-frame setup `func_0015E810`, and their sibling initializers. Build:
730 C translations, 301 assembly translations, 2 stubs, 3,876 functions
without C across 267 generated units. A 12-second default launch has a live
world and initialized UI/font records but still reports zero vertices, visible
text, and GIF-chain submissions. A 12-second direct-level launch reaches
loader stages 13–14 and likewise reports zero vertices/submissions. The
window remains black; this restores reached retail initializers but is not a
visual milestone. Logs: `/tmp/sly-canonical-55d39f28-init-callback-asm-12s.log`
and `/tmp/sly-canonical-55d39f28-init-callback-asm-direct0100-12s.log`.

The next trace showed four more reached no-op dependencies: the animator
helpers `func_001B4AA0` and `func_001B4AD0`, UI updater `func_001AB058`, and
commentary updater `func_001E00B0`. Their exact US assembly is now selected as
well. The 10-second follow-up has no no-op warnings for these four, but still
reports `visible_text=0`, `vertices=0`, and zero GIF-chain submissions; all 37
UI objects remain in mode 0. The trace now has 155 remaining no-op warnings,
so the next step is to locate a live drawable object's update/draw callback
and the producer that should change its mode. Do not force UI modes or graph
state. Build: 730 C translations, 305 assembly translations, 2 stubs, 3,872
functions without C. Log: `/tmp/sly-canonical-55d39f28-reached-anim-ui-asm-10s.log`.

**Canonical packet-writer registration (2026-11-11):** the refreshed upstream
checkout remains at `55d39f28`; fetching `origin` found no newer `master`
commit. The three port-only packet-word implementations already existed in
`hand/native_c/display_packets.c`, but were missing from `hand/backfills.tsv`,
so the canonical build emitted no-op warnings for `func_001EE3F0`,
`func_001EE420`, and `func_001EE498`. They are now registered as port
backfills. The canonical `55d39f28` native build succeeds, and an eight-second
direct-level run reaches loader stages 13–14 without those warnings or a
fault. It still emits only eight setup DMAs and reports zero scene vertices
and visible text. This closes a packet-writer no-op gap; it does not resolve
the scene-object/render path or make the port playable. Build and trace:
`build/port/canonical-55d39f28` and
`/tmp/sly-canonical-master-55d39f28-packet-backfills-8s.log`.

**Latest direct-level packet result:** public decomp `master` is `55d39f28`
(4,758/4,807 US functions, 93.108% code); `night21` is `4097b806`
(4,759/4,807, 93.125%). The ordinary `build/port` cache now uses canonical
master for matched C and the separate local fork for retail assembly. The
recent rework adds useful collision, placed-object, and player code; night21
mainly corrects types/signatures and matches one more helper. It adds no newly
matched function on the current strip-render path. A separate local fork checkout at
`/home/reaper/Code/sly1-decomp` is older (`e1b39f16`, wave 16) but has useful C
definitions for three reached VIF/GIF packet-word writers. Their port-only
backfills replaced no-op stubs.

The prior “negative unpack range” (`0x4e3c..0x3eb1`) was not a verified range:
the probe interpreted raw packet halfwords without confirming the descriptor
pointer. `StripUnpack` begins with `off[4]`, then `strip[4]`. A valid sample has
`unpack=packet+4` and begins with descriptor offsets `0x24, 0x1e0`; a suspect
sample's packet starts `04 01 00 01 24 c0 54 68 ...`, so the same pointer lands
on a command word. The negative-count guard in `func_001EE5C8.c` remains a
temporary port safety measure; do not clamp the values. An 8-second
direct-level run stays alive and reaches rendering setup, but still has zero
scene vertices, image tags/submissions, or visible text. Eight small GIF setup
DMAs are emitted; the window remains black. The `night21` source shows the
pointer is produced by `ReserveVifUnpack(..., &hdr->p)` in the strip mesh
builders, then adjusted by `strip->addr[0] - starts[0]` after packet
detachment. Capture and compare those pointers if the current route reaches the
builder again; do not synthesize geometry. Logs:
`/tmp/sly-night21-4097b806-negative-vif-guard-8s.log` and
`/tmp/sly-night21-4097b806-vif-guard-state-5s.log`.

**Latest reproduction attempt:** temporary probes at the pre-detach,
post-detach, and consumer points produced no strip events in a 15-second run.
The loader reached collision face-tree stages 190–192, but submitted only the
eight setup DMAs. The previous strip pointer issue is not currently reproduced
on this route. Trace the model/object handoff into `ReadPlacedObjectModel` and
reproduce the builder call before changing pointer arithmetic. Probe log:
`/tmp/sly-night21-4097b806-strip-pointer-stages-15s.log`.

**Current decomp/use assessment:** the source unit clarifies the strip
descriptor and builder/relocation flow but does not identify the packet-prefix
discrepancy. The broad rework's new matches are useful for later gameplay and
collision work; the current black screen remains earlier in startup/rendering.
Relevant C++ still does not pass the port's C hostgen frontend. Consume it
through port-hostable C or exact retail assembly rather than editing the
decomp to suit hostgen.

**Animator callback follow-up (2026-11-11):** the native build now selects
exact US retail assembly for `ClaimAnimatorStep` (`0x001B4B08`) and the two
newly observed animator callbacks (`0x001B4B68`, `0x001B4BC8`), alongside
`FlushCursors` and `StepAnimators`. A six-second native run reaches the frame
loop without no-C warnings for these callbacks. The graph remains unarmed at
state key `0x261`, edge `2 -> 2`; the window is still black with zero vertices,
visible text, or GIF chain submissions. Build coverage is now 727 C
translations, 278 retail assembly translations, 2 stubs, and 3,902 functions
without C across 267 generated C units. Trace:
`/tmp/sly-canonical-55d39f28-animation-callbacks-6s.log`. Keep the next effort
on the path that should arm/advance the Splash graph or produce the first
drawable GS packet; do not synthesize graphics or force the graph state.

Added opt-in recurring state-graph diagnostics to `port/sly1/main.cpp`.
Repeated native snapshots show Splash graph `0x25f` has four states and seven
edges. The port now selects exact US retail assembly for the reached animation
path: `CreateAnimationPlayer` (`0x0012C288`), `NewAnimationPlayer`
(`0x0012D3F8`), `SetAnimationPlayerSpeed` (`0x0012F6B0`),
`CreateTrackAttachment` (`0x00128998`), `InsertAttachment` (`0x001299E0`), and
the world-step updater `func_0012EBD8`. The last function was the missing
per-frame update: before it was restored, the graph walker had no player; now
the player exists and its time advances from 0.4 to 4.45 seconds in a 10-second
native run. `StartObjectSubGroup` (`0x001F4660`) is also restored, but the only
Splash subgroup has zero children.

The graph's sole walker still stays at state key `0x261`, edge `2 -> 2`, and
the graph `armed` field remains 0. The associated CountdownNotifier remains in
state 0 with a null `+0x338` gate reference and timer 0 (range 4–6 seconds); no
class-`0x65` StepSequence is present. The Splash window remains black with no
visible text or vertices. A direct-level boot with the animation path restored
also still reports zero scene vertices and no GIF submissions. Do not force the
notifier, graph, input, or parsed render-record flags. Next determine whether
the asset intentionally leaves `+0x338` null and what normal title/gameplay
transition should leave the idle `0x261` self-loop; continue separately on the
direct-level renderable-object/GS path.
Latest graph/player trace: `/tmp/sly-canonical-55d39f28-animation-player-update-10s.log`;
direct-level follow-up: `/tmp/sly-canonical-55d39f28-direct-level-after-animation-10s.log`.

**Animation reader and render-packet follow-up:** the refreshed decomp names
`0x0011FFD8` as `ReadAxisRotationCurve`, `0x0011EFC8` as
`ReadTangentWeightCurve`, and `0x0011F180` as `GetTangentWeightCurveTimes`.
The port now selects exact US retail assembly for these reached callbacks.
This fixes the state-graph stream misalignment: the first graph's five
animation children are consumed and the top-level resource loader reaches its
completion stages before entering the frame loop. Keep the host C readers as
separate port backfills for later replacement.

A direct level probe (`OPENRAC_BOOT_LEVEL=0x0100`) loads a populated world:
resource record index 2 and world lists are present, and the render thread
runs. However, `vertices=0`, GIF image tags and submitted chains remain zero.
Frame-writer tracing shows the GS writer cursor stays at its initial DMA tag
while the VIF writer advances by about `0x2e0` bytes. This moves the current
visual blocker from world loading to understanding why the scene path does not
write/submit GS geometry. The default Splash also has no visible UI elements.
The reached mesh queue now uses exact US retail assembly for
`SubmitMeshQueueEntry` (`0x001694A0`), `QueueMeshStrip` (`0x001694F8`),
`QueueNodeLink` (`0x00169608`), `RemapMeshQueueValues` (`0x001696B8`), and
`FlushMeshQueue` (`0x00169718`). The refreshed source unit `file_167D68.cpp`
is not hostgen-readable, and the remapper had been reached as a no-op. The
queue cluster no longer reports no-C warnings, but the direct-level trace
still has zero vertices and no GIF submissions. Do not mistake the queue fix
for rendered output. The invalid UI virtual-call guard is port-only
safety code and traces bad callbacks if encountered; it does not synthesize
graphics. Latest direct-level
trace: `/tmp/sly-canonical-55d39f28-direct-level-packets-8s.log`; frame writer
trace: `/tmp/sly-canonical-55d39f28-frame-packet-trace-8s.log`.


The latest public `OpenThievius/sly1-decomp` master is `55d39f28` (PR #113),
fetched and checked again on 2026-11-11. Its NTSC-U report lists 4,758/4,807 functions matched
(98.98% by function count, 93.11% of code, and 94.56% of data). Since
`f36f43ee`, it matches nine additional US functions in collision, object
interpolation, and player physics code. Those new definitions are not yet
hosted by this port: the current C hostgen staging step cannot parse their
updated C++ headers and falls back to retail assembly for them. At that first
refresh point, the build linked 725 translated functions, 257 retail assembly translations,
2 stubs, and 3,925 functions without C across 265 generated C units. The source
snapshot is `/tmp/sly1-decomp-55d39f28`; `/home/reaper/sly/sly1-decomp` is now
fast-forwarded to `55d39f28` and clean. Build:
`build/port/canonical-55d39f28`.

**State-graph follow-up (2026-11-11):** the latest default-startup trace
confirms the Splash StateGraph object is present at `0x00847800`, class `0x88`,
ID `0x25f`. This supersedes the earlier conclusion that its link was absent.
The graph start, initial walker, state-key lookup, edge planner, and event
helpers are now selected from US retail assembly; two small pool/listener
helpers remain isolated port C backfills. A 10-second native run has no
no-op warnings in that reached state-graph path, but startup remains in frame
state `0,3,0`; vertices, visible text, and GIF submissions are still zero.
The path is no longer stubbed, but the trace does not yet confirm a title
transition. Log:
`/tmp/sly-canonical-55d39f28-state-animation-10s.log`.
At that stage the build had 727 C translations, 266 assembly translations,
2 stubs, and 3,914 functions without C across 267 generated units. The latest
build after restoring the animation-player cluster has 727 C translations,
273 assembly translations, 2 stubs, and 3,907 functions without C.

Those nine new matches are useful decompilation progress in collision, object
interpolation, and player physics; they are not the remaining graphics blocker.
Several refreshed C++ units are still unreadable to the native C hostgen path.
Keep port-only C under `hand/native_c/` or `hand/overrides_c/`; do not patch
matching sources to work around hostgen limitations.

**Earlier notifier trace (superseded by the lookup follow-up below):** the
`9eb87599` snapshot showed a class-`0x57` CountdownNotifier at state `-1`,
with its owner field reported as zero. That trace read `+0x2e8`, which is the
state-walker handle; the actual lookup owner is at `+0x2ec`. It also preceded
the `0x0015A838` wrapper backfill. See the current `f36f43ee` evidence below.

**Notifier/lookup follow-up (2026-11-11):** refreshed upstream source confirms
`StartStateNotifier` (`0x00146470`) calls the four-argument wrapper
`FindFirstObjectByIdGathered` (`0x0015A838`), which in turn calls
`GatherObjectsById` (`0x0015A4F0`). The native port had only backfilled the
inner helper; the wrapper remained a zero-return stub. Both are now isolated
port overrides. The corrected mode-1 search returns Splash's class-`0x75`
child (`id=0x22a`, address `0x846cb0`) as the CountdownNotifier's owner. A
separate trace fix now reads owner at `+0x2ec` (the old trace mislabeled
`+0x2e8`, the state-walker handle). The rework shows that `0x0015a198` is the
object-ID hash gatherer, while `0x0015a4f0` gathers by class kind. The port now
keeps those paths separate, and `0x0015a418` delegates to the object-ID
gatherer as canonical `FindFirstObjectById` does. This older trace reported
object ID `0x25f` absent and the graph pointer at `+0x2e4` null; the state-graph
follow-up above supersedes that result. The notifier nevertheless transitions
from `-1` to `0`, invokes owner method `0x00182c80`, and requests event `0x261`;
`CreateStateWalker` (`0x001b6290`) was then a no-C stub. State 0 and timer
gate 0 were confirmed,
with no visible text or vertices, GIF image tags, or DMA submissions. The
separate `0x00147ab0` backfill covers modes 0, 2, 3, 6, and 7; modes 4 and 5
still need canonical camera math. This earlier trace predates the state-graph
follow-up above. Log:
`/tmp/sly-canonical-f36f43ee-mode-lookup-next.log`.

**Latest source refresh and resource trace (2026-11-11):** building against
`55d39f28` succeeds and produces the same hostgen coverage as `f36f43ee`;
several refreshed C++ units are reported unreadable by the C frontend, so the
new nine matched functions still execute through the existing retail assembly
translations. A seven-second native run is logged at
`/tmp/sly-canonical-55d39f28-upstream-refresh.log`. The Splash stream begins
with a count of 36 and valid records, but the class-`0x53` ObjectGroup callback
reaches a nested-list count `0x1ddd` at cursor `0x70001d1f`, where the decoded
Splash bytes are float data. The nested list then reads invalid type
`0xffffd1dd`; when the callback returns, the parent list reads
`0xffffcccc` at cursor `0x70001df5`. The renderer remains at zero vertices,
image tags, and GIF submissions. The active task is to identify which reader
callback or port backfill leaves the class-`0x53` group stream misaligned; do
not invent the missing graph object or force its notifier forward. The refreshed
`LoadObjectGroup` source confirms this nested `ReadSlotObjects` call is expected
when the group has one member; it occurs after the member data and property
records, narrowing the cursor error to earlier parsing within that group.

**Model-reader follow-up (2026-11-11):** the refreshed decomp has the full
`ReadPlacedObjectModel` implementation in `src/main/file_15F678.cpp`, but the
native hostgen build cannot compile that C++ unit. The port's compact
`func_00162890` model-reader backfill was still selected and misaligned the
Splash stream. `hostgen.json` now selects exact US retail assembly for this
function, while the port C body remains guarded by
`SLY1_RETAIL_ASM_FUNC_00162890` as a replaceable candidate. An eight-second
native run passes the bogus `0x1ddd` nested count at `0x70001d1f` and consumes
valid object records past `0x70001f00`. Selecting exact retail assembly for
the 36-byte `func_00182C58` wrapper removes its no-op warning; the wrapper
delegates to `func_001837E0`, already translated from assembly. Exact assembly
is also restored for `func_00137FB0`, the collision vertex reader; its port C
backfill remains registered but is superseded by the `asm.only` selection.
Together these changes let the Splash load continue into world initialization
and the frame loop. A later top-level reader still exhausts at cursor
`0x70000000` (`remaining=-1`). The window remains black with zero vertices,
image tags, and GIF submissions. Log:
`/tmp/sly-canonical-55d39f28-retail-loader-frontier-8s.log`.

**Earlier collision follow-up (9eb87599 snapshot):** the bounded native run
parsed both collision meshes and their face trees without the earlier
second-mesh stream failure. This was enabled by
correcting `func_001BAC30` to store `refCount` at `+0x408`, `refs` at `+0x40c`,
and to read `faceStarts` only when `refCount > 0`, matching the reworked
upstream `LoadCollider`. The launcher reaches the frame loop without crashing,
but still reports zero rendered vertices and a black window. The next task is
to trace the level/world objects that should submit scene geometry. Build and
latest trace details are in [`RESTART_CHECKPOINT.md`](RESTART_CHECKPOINT.md).

**Earlier packet-writer follow-up (9eb87599 snapshot):**
`func_00152CA0` (`AddDmaReturnTag`) was a reached no-C function from the
reworked packet-writer source. Its port backfill closed the active DMA tag and
appended a `RET` tag, but did not produce scene graphics. Trace:
`/tmp/sly-canonical-9eb87599-return-tag.log`.

**Canonical source integration check (historical, 2026-11-10):** I built separately
against upstream `2e00ac70` in `build/port/canonical-sdl`, using
`/tmp/sly1-decomp-2e00ac70` for source and the older local checkout only for
US retail assembly. It translates 568 functions (251 assembly), stubs 2, and
leaves 4,069 without C. The new image/font loaders register 61 image records,
63 palette records, and four fonts. Adding exact retail assembly for the
reached axis-curve helper moved startup beyond the previous curve fault. The
first follow-up entered malformed collision data and faulted at `0x6FFFFFF8`.
Selecting exact retail assembly for `func_00137FB0` (and retaining its C
backfill as superseded) advanced past several mesh records; startup now faults
at `0xFFFF0626` during a null virtual callback. This build has 562 translated
functions (252 assembly), 2 stubs, and no visible graphics. The regular
`build/port` cache still uses `e1b39f1`; canonical integration is not yet a
viable launcher replacement. Keep this diagnostic separate from the older-
build graphics findings below. Full trace and build details are in
[`RESTART_CHECKPOINT.md`](RESTART_CHECKPOINT.md).

**Trace-label correction (2026-11-10):** the earlier diagnostic called
`0x00275BC4` an app state, `0x00275BF0` an app-state mode, `0x00275F84` a scene
gate, and `0x00262C68` a state selector. Current decomp identifies
`0x00275BF0` as a ten-word table, `0x00275F84` as the screen-fade mode, and
`0x00262C68` as pad routing. The other two values are now reported as raw
addresses. Do not use the earlier `app_state=3` / `scene_gate=0` conclusions.

**Earlier startup checkpoint (2026-11-10):** the earlier empty world was caused
by the no-op `func_001E4E80` (`QueueWorldLoad`), which cleared the screen fade
without writing a load request. A port-only retail-equivalent queue routine is
now registered in `hand/backfills.tsv`. The next native SDL run opened the
initial resource at sector 820,372 (150,849 bytes), loaded the world, and
registered four fonts instead of faulting on a null font. It now has
`g_currentWorld=0x006abdd0` and emits 240 raw vertices on some frames (117
inside the viewport). The window is still visually black: sampled vertex
colors are `0x80000000`, image-tag/qword counts and blit writes remain zero,
and `visible_text=0`. This is progress through startup, not playable graphics.

The latest decomp identifies `func_001B2BA8` / `func_001B2CF8` as the image
record table readers and `func_0015D8A8` as `LoadFont`. Hostgen still leaves
the image readers without C, so a separate port-only implementation now
parses their retail record sizes, offsets, and mask counts in
`hand/native_c/image_records.c`. The trace confirms 61 image records, 63
palette records, and four registered font IDs. The exact US assembly for
`func_00169780` (`UploadImagesByMask`), `func_001B3488`
(`SwitchActiveImageMask`), `func_001B3510` (`BuildAnimRecordGifPackets`), and
`func_001614C8` (`AddGifUvEntry`) is now selected in hostgen because these
reached routines have no C and were previously stubs. The build succeeds, but
the follow-up still reports zero GIF image tags and zero non-black visible
output. `OPENRAC_VIF_TRACE=1` shows the later VIF direct streams are accepted;
an earlier 48-byte startup transfer contains a malformed `DIRECT` count and
is rejected. Traces: `/tmp/sly-screen-resource-tables.log`,
`/tmp/sly-vif-dma-debug.log`. Exit 124 is the expected timeout, not a guest
crash. Next, inspect why the accepted texture streams do not produce GIF image
tags and why visible UI geometry has color `0x80000000`.

Wave 52 removed the port-only 99.7% near-match for `func_00125C90`
(`ApplyPlacedObjectTransform`) and compiles the 100%-matched body in
`placed_object.cpp`. A bounded no-input run reaches and stays on the no-card
warning through frame 305 without a fault. The text-layout setters at
`func_00137228` and adjacent entries remain separate port backfills because
the direct host build does not emit those inline methods as standalone symbols.

Wave 53 removes the port overrides for `ReadMeshTables` (`func_001E4C48`),
`UpdateCameraMatrices` (`func_00144540`), and `InitSpawnRecord`
(`func_001549C8`). The decomp report marks all three 100% matched and the
native host build now compiles their C bodies directly. The direct-call
frontier is 827/835. A six-second no-input launch reaches the no-card warning
at frame 60 and remains there through frame 305 without a fault; this does not
test menu input or later rendering. Log: `/tmp/sly1-wave53-startup.log`.

**Next:** follow the ordinary Splash-to-title/state transition now that the
graph and initial walker path run. Trace its state/event callbacks and the
class draw callbacks on frames. The direct-level probe loads
resource/world lists, but its five outer groups are flagged to skip geometry;
preserve that retail behavior and do not force their draw path. The native
render writer still has no GS geometry, so visuals and the playable state
remain unachieved. Keep each functional
replacement in the port backfill area and retire it when matching source is
hostable. The port is not playable yet.

Wave 51 now compiles `SuppressScriptEvent` (`func_00183668`) from its matched
US C source. The earlier 88.8% near-match and port behavior backfill described
in the historical trace notes are superseded; the backfill was removed.

Host-only ABI compatibility adjustments stay in this repository's staging
script: four-word globals retain EE quadword alignment; selected functions make
implicit EE argument-register carryover explicit; and animation callbacks use
their declared vtable argument order. `func_001F5B08` remains a separate
port-native backfill for the still-assembly libc memmove routine.

The regular SDL hostgen path has been built against the rework. Earlier SDL
traces below remain historical evidence; they predate the queue and image
record backfills. The latest source clarifies movie state at `D_00269A00`:
`+4` is the queued movie, `+8` its successor, `+0xC` the stream reader, and
`+0x134` the decode thread step. `RunMainLoop` drains the queue before and
after world loading; `func_0018E4F0` (`QueueMovieWithSuccessor`) stores the
successor.

**Prior contact startup checkpoint (2026-11-10):** the active `0x00185D20` and
`0x001860A8` backfills were mislabeled draw-entry helpers. Both were removed
from the active map and preserved under `hand/superseded_backfills/`. The
`0x3f800000` fault was later cleared by retiring the separate `0x0011E580`
factory override in the direct build. The current direct-build blocker is
`func_00125C90`; that was the checkpoint's next task, before the newer host
ABI failures summarized above.

**Canonical loader dispatch trace (2026-11-10):** the direct build now stages
the reworked `remote_call.cpp` method name while preserving the port-only SIF
completion boundary. Rebuilding against `../sly1-decomp` still compiles 162/171
source units; combining them resolves 4,218/4,807 catalogued functions and
784/815 functions in the reachable direct-call frontier. A fresh 15-second
native run reaches `LoadGroupMembers` on object `0x0084c400` (class descriptor
`0x08147ac0`) at stream cursor `0x70001077`. Its descriptor selects reader
callback `func_001587C8` (`LoadGroupMembers`); execution then faults during a
nested object's virtual read callback at low address `0x556`. This narrows the
next investigation to the port-only `CreateTypedRecord`/`func_0011E580` object
construction and class descriptor method slot used by the nested record. Do
not change upstream matching behavior: any native factory or dispatch fix
belongs in `hand/native_c/` and the backfill registry. Trace:
`/tmp/sly-canonical-native-stage26.log`.

**Nested record follow-up (2026-11-10):** opt-in traces around the decomp
`LoadGroupMembers` callback show child type `0x7A`, descriptor address
`0x08147c20`, and `m0A` at `func_00158B30` (`LoadRecordMember`). This is a
valid descriptor dispatch, so the earlier `0x556` value was not this child's
read-method slot. The captured core now faults at EIP `0x2` with a return site
inside `LoadRecordMember`, after it takes the no-path-curve branch and calls
`ReadCappedWordList` for its nested record. Path-curve tracing did not fire.
The next investigation is the stream count and read sequence feeding that
capped-list call, plus its reader ABI; do not replace `LoadRecordMember` unless
evidence shows the matching function itself is incompatible with the host ABI.
Trace: `/tmp/sly-canonical-path-curve.log`; core: `/tmp/sly-pathlatest.core`.

**Earlier native ABI and mesh-load follow-up (2026-11-10):** the capped-list fault was
caused by `ReadBufferedU32` being declared as a one-word aggregate in the
reworked `file_1549C8.cpp`; i386 SysV therefore passed a hidden result pointer
where the retail scalar reader expects its stream. The host staging copy now
uses a scalar `s32` for that call while preserving the same 32-bit payload. A
port-only `func_001E4C48` mesh-table reader fills the still-`INCLUDE_ASM` retail
entry, and the native address map now prefers the factory backfill's canonical
C symbol while retaining its prototype aliases. The rebuilt launcher passed nested capped-list parsing and reached world
contact updates. The callback fault was later traced to the mislabeled active
port backfills; see the latest-source checkpoint above. Trace:
`/tmp/sly-canonical-after-mesh.log`; core: `/tmp/sly-after-mesh.core`.

The matching source for important graphics callbacks is present upstream,
but several defining units still fail this port's C staging because of C++
header constructs. `func_00125C90` remains `INCLUDE_ASM` in the latest
decomp. Keep working in this port repository; keep any functional replacement
C in `hand/native_c/` and registered in `hand/backfills.tsv`.

Fresh repository review: 2026-11-07. The current local decomp input is
`e1b39f1` from `/home/reaper/Code/sly1-decomp`, and CMake uses that checkout
for matched C and retail assembly. Current hostgen reports 192 units, 2,044
translated C functions, 230 retail-assembly translations, and zero stubs. The
native launcher runs through Splash and the main update loop, but remains at
frame state `0,3,0`, game mode 0, and renders no visible text or gameplay
scene. The port is not playable.

**Latest normal-startup check (2026-11-07):** the retail
`func_001E4EA8` service tail now runs on empty requests; exact US assembly is
selected for `func_001C0070` and `func_001C06D8`. The service tail contained
one more no-op, `func_00160CE8`: its decomp body resets a counter at `+4`, but
hostgen still marks it no-C. A port-only backfill now performs that reset and
is listed in `hand/backfills.tsv`. The regenerated hostgen report confirms
2,044 C functions / 225 assembly functions / zero stubs, and the normal
8-second native run no longer logs `func_00160CE8` as a no-op. This still does
not advance startup: the run remains `sly_frame=0,3,0`, game mode 0, with
`visible_text=0`. Thus the service-tail omission was real but not the main
Splash-to-title blocker. Continue tracing the retail state transition and its
request producer; do not infer that missing graphics are caused by the GS
renderer from these results. Run log: `/tmp/sly-native-counter-reset-verified.log`.

**Startup timing and trace-label correction (2026-11-07):** A 35-second
native-only run advances guest time to about 17 seconds but still has
`sly_frame=0,3,0`, `game_mode=0`, menu page `-1`, and `visible_text=0`; simply
waiting does not reach the title. Static US assembly review also corrects the
trace's former `movie`/`movie_keys` labels: addresses `0x00269A04..0x00269A10`
are fields in a worker-dispatch structure. `func_0018E4F0` stores a pointer to
the selected 32-byte thread entry at `+8` and signals its bit in the thread
manager. These fields do not establish movie playback state. The new trace
label is `thread_dispatch`. Keep PSS playback as an independent unresolved
path until its retail call site is found. Log:
`/tmp/sly-native-35s-state-transition.log`.

The next trace-based closure selects exact US assembly for five more routines
that logged as no-C calls during normal startup: `func_001E77C0`,
`func_001E9C78`, `func_001E9EE8`, `func_001EA0A8`, and `func_001EAAE0`.
Hostgen now translates 230 assembly functions with zero stubs. The native
8-second run completes without crashing and no longer logs those five
functions as no-ops, but still remains at frame state `0,3,0` with
`visible_text=0`; these routines were real missing frame/update behavior but
did not alone unlock Splash-to-title. The local decomp remote was fetched and
is still at `e1b39f1`; there are no new commits to import. Run log:
`/tmp/sly-native-ui-update-asm.log`.

**Latest render-data follow-up (2026-11-07):** the US retail allowlist now
includes `func_001DC9A0` (800 bytes; still unmatched in the decomp), increasing
the generated assembly total to 222 while keeping zero stubs. A direct-level
render-list entry at `0x00847ce0` has three primary records and is not skipped.
The first record resolves through `0x00844450` to structures at `0x0083fc60`
and `0x0083f4b0`; the latter references three 512-byte payloads at
`0x0064d6e0`, `0x0064d8e0`, and `0x0064dae0`. The direct DMA trace identifies
them as 16x8 PSMCT32 image uploads shared with Splash. Retail `func_001662A8`
emits two 8-qword command buffers but neither references those images, so
they do not explain the zero scene geometry addresses. Next find the
renderable-object list and retail draw callback that should supply scene
geometry. The latest
bounded launch still reports frame state 3/game mode 0 and zero scene vertices.
Evidence: `/tmp/sly-native-direct0100-mesh-sections.log`,
`/tmp/sly-native-direct0100-packet-dump.log`, and
`/tmp/sly-native-direct0100-dc9a0-asm.log`.

**Retail object-reader closure (2026-11-07):** the latest US assembly
allowlist also selects reached resource and object routines
`func_001240F8`, `func_0011F850`, `func_0012BC38`, `func_00124868`,
`func_001245A8`, `func_00164868`, `func_001587C8`, `func_00124A48`,
`func_001D7040`, `func_0017FEC0`, `func_001F4500`, `func_001478F0`,
`func_001AE820`, and `func_001F6B40`. `func_001240F8` previously used a
port-only C reconstruction; its source remains guarded in `hand/native_c`,
and it is removed from the backfill registry while retail assembly is active.
These additions let default Splash startup reach the main update loop with
five valid world root objects, including classes `0x8e` and `0x7d`, without a
logged guest call fault in the last bounded run. The run reports frame state
3, game mode 0, zero scene vertices, and 416 accepted GIF chains at its final
trace; the window remains black. A direct `OPENRAC_BOOT_LEVEL=0x0100` probe
still creates a root object with type `0xffffffff` and faults on unmapped
address `0xFC000120`, so direct-level loading is not fixed by this default
boot progress. Trace: `/tmp/sly-native-frame-callbacks.log` and
`/tmp/sly-native-direct0100-reader-closure.log`. Latest hostgen coverage is
in `build/port/hostgen/us/report.json`.

**Direct-loader closure experiment (2026-11-07):** adding another 37 reached
functions with US assembly raised the allowlist to 236 and made the direct
`0x0100` probe fault earlier in `func_00147AB0` while dispatching an unset
resource callback. Adding 51 further startup functions raised it to 287 and
left boot in the early kernel wait with no active world. Both expansions
were reverted. The retained 199-function allowlist was rebuilt and a fresh
default run again reached the update loop, with frame state 3 and zero scene
vertices. Do not treat these broader static translations as safe merely
because hostgen can translate them; restore them only after tracing the
specific callback state they require. Experiment logs:
`/tmp/sly-native-direct0100-callback-closure.log`,
`/tmp/sly-native-direct0100-expanded-closure.log`, and
`/tmp/sly-native-default-199-asm.log`.

**Reader-chain checkpoint (2026-11-07):** Exact US retail assembly is now
selected for `func_001BAC30`, `func_00162890`, `func_001DCEA0`,
`func_00196410`, `func_001B9AE0`, `func_0012B870`, `func_0012B8C8`, and
`func_0013B4C0`. The first three previously had port C backfills; their source
files remain guarded in `hand/native_c`, while they are no longer registered
in `backfills.tsv`, so the replacement code stays separate from retail
behavior. Switching to exact readers moved the common object parser through a
valid short record to cursor `0x70001595`, then later to `0x70001ee8`, rather
than misreading a count and consuming the remainder of the Splash data. The
next nested child-list path still faults at a call through zero after class
`0x83`. The latest bounded run still reaches frame state 3/game mode 0 with
zero scene vertices and two accepted GIF chains; it is not playable. Rebuild
with `cmake --build build/port --target port_native -j2` after changing
hostgen configuration; building only `sly1_native` does not regenerate
translations. Trace: `/tmp/sly-native-restart-check.log`; generated coverage:
`build/port/hostgen/us/report.json`.

**VIF error-mask correction (2026-11-07):** commit `f8fa708` attempted to
continue past the startup packet's unknown VIF opcode `0x40` when ME1 was
masked. Rechecking the retail write and the EE VIF1_ERR bit definitions shows
that Sly writes `2`, which sets ME0 and does not set ME1 (bit 2). The captured
packet therefore still takes the parser's unsupported-command path; the
workaround did not activate and must not be treated as a graphics fix. The
mask-based bypass has been removed. This packet remains a separate startup
error-handling question, not evidence for the zero-vertex scene-rendering
blocker. Trace: `/tmp/sly-native-vif-mask-fix.log`.

Since the previous pushed checkpoint, the US assembly allowlist gained 27
reached routines. These remove more missing-C warnings without adding
port-only behavioral backfills. The latest native trace still shows active
threads and accepted GIF chains, but zero scene vertices; default boot remains
at frame state 3/game mode 0. The next graphics task remains identifying the
retail title transition and the owner/class callbacks that produce visible
scene submissions, rather than treating the first VIF error as the mesh path.

The rebuilt default-boot trace also reports a call to guest address zero from
retail `func_0011ECB8`. Its selected assembly unconditionally calls the
record-created object's virtual reader at vtable offset `+0x38` immediately
after the factory returns. The current trace does not identify which record or
why that slot is empty; instrument the factory result and vtable slot at this
dispatch before changing its behavior. The run still reports zero scene
vertices, frame state 3, game mode 0, and no visible graphics. Trace:
`/tmp/sly-native-vif-mask-corrected.log`.

**Earlier graphics checkpoint (superseded by the owner-list trace below):** a full object snapshot of the
direct-loaded world found two class `0x44` instances, but these are collision
objects, not visible actors. Both have null mesh data at `+0x380` and a zeroed
render mesh copy at `+0x3A4`; the world still submits zero scene vertices.
The latest run found a class-1 descriptor at `0x00217418` with a valid
`func_001833D0` record reader and populated method slots 7 and 20. Its method
slot 19 is null, but the matched `func_00182ED8` checks that slot before use.
An indirect call still reaches address zero during the record path, so its
source remains unresolved. Parsing continues through loader stage 5; this is a
confirmed defect to trace, not proof that the world is fully initialized.
The port has not reached the first actual game image. Next trace the null call
through class-1 nested record parsing, then find the title/level transition and
the record path that creates renderable objects. Trace:
`/tmp/sly-native-class1-vtable.log`.

The resource descriptor reader `func_0011E680` now lives only in
`hand/backfills.tsv` and `hand/native_c/resource_descriptors.c`; it is no longer
registered as a function override. This keeps the functional C backfill
separate from matched decomp code so it can be replaced cleanly when that
function is decompiled upstream. The collision-contact pointer correction is
also port-only and follows the retail load of the table handle.

The current change set selects seven additional functions from exact NTSC-U
assembly: `func_00183350`, `func_00139DE8`, `func_001249A8`, `func_00124338`,
`func_00127378`, `func_00139EC8`, and `func_001280C0`. They close reached
constructor, activation, cleanup, message, and VU0 callbacks without changing
the decompilation or adding functional replacement C.

**Fresh default-boot check (2026-11-07):** a 12-second native launch without
`OPENRAC_BOOT_LEVEL` reads only the Splash range (sector 820372, 150849 bytes).
It stays in frame state 3/game mode 0, keeps the menu page at `-1`, registers
no fonts, and reports zero visible scene vertices. This means the direct-level
probe is not the only blocker: normal startup also fails to transition from
Splash into the intro/title flow. Next, trace the retail state/movie request
that should follow Splash while separately preserving the level-resource probe
for scene-object work. Trace: `/tmp/sly-native-default-graphics-check.log`.

The first rejected VIF1 startup chain is reproducible from the US ELF itself:
the static bytes at `0x00212110` contain the 48-byte CNT payload and opcode
`0x40` at stream offset `0x14`, exactly matching the native VIF diagnostic.
This rules out ISO-sector corruption as the source of that packet. Retail
startup writes `2` to `VIF1_ERR`: that masks ME0 (DMA-tag mismatch), while ME1
(invalid-command stall) is bit 2 and remains unmasked. The native parser's
stop at opcode `0x40` is therefore consistent with an unmasked invalid VIF
command; this packet still has not been tied to the missing scene callback.
Trace: `/tmp/sly-native-default-vif-trace.log`.

The port now uses exact retail assembly translations for the animation table
loader (`func_001B3040`), its element parser and class lookup helpers. The
previous reconstructed table parser was removed. Its remaining factory guard
is a separate port-only override that skips unsupported animation types while
consuming their fixed payload. `sly1-decomp` remains untouched. The level still
produces zero scene vertices and the window is black; reaching stage 5 proves
resource parsing/activation progress, not visible graphics or playability.

Retail-backed static translation of `func_00162890` succeeded in an isolated
hostgen experiment. Its instruction stream confirms that nested-array sizes
include the one-based entry index and that both dimension payloads multiply
the full base payload byte count by the dimension and 12. The port-only C
backfill now follows those formulas. The latest launcher run never reached the
function, so this parser correction remains unvalidated at runtime.

The resource teardown/dispatch path now uses exact US assembly for
`func_001C0808`, `func_001BEE48`, `func_001E9688`, and the missing-assembly
dependencies reached from them. A fresh 12-second direct-level run remains
stable at stage 5 but still reports zero scene vertices, so these lifecycle
callbacks were not the missing level-mesh path. Trace:
`/tmp/sly-native-lifecycle-asm-closure.log`.

Two more runtime-reached no-C leaf functions now use exact US retail assembly:
`func_001363D0` (the child cleanup callback reached during level initialization)
and `func_00160D98` (the retail constant-return helper used by startup code).
Their no-op warnings are gone. The rebuilt native run still reaches level
loader stage 5 with an active world and zero scene vertices, so these exact
leaf fixes remove incorrect behavior but do not yet connect level data to a
renderable object. Trace: `/tmp/sly-native-asm-cleanup-trace.log`.

The menu glyph packet is known to be valid through texture lookup, but the
displayed frame remains black/garbled. An earlier build reached child-object
dispatch and faulted at `0xFFFFAC70`; that checkpoint used a different decomp
snapshot and is superseded by the earlier-described current failure. This
document is an ordered work plan, not a claim of playable progress. Work is
active on
wip/native-sly-port and stays in the port repo.

A 2026-11-07 bounded trace widened the opt-in world-list callback sample from
8 to 64 records. It captured all 12 entries: each dispatches through
`func_00125C90`, and each sampled local basis is identity. No additional
render callback appeared; sampled frames still report zero scene vertices.
This rules out a trace cap hiding a different callback, but object ownership
and the reason these entries do not submit geometry remain unknown. The
diagnostic is enabled only with `OPENRAC_TRACE_FONT=1`.

A new bounded direct-level trace records the child list, mesh pointer, extra-data
pointer, and sphere radius for those same update-list entries. All sampled
entries have zero in all four fields. They are empty infrastructure objects,
not cullable mesh owners, so forcing visibility or synthesizing bounds is the
wrong fix. The native frame remains state 3, game mode 0, and menu page `-1`,
so this direct-load probe has not reached a playable state. The retail state
transition and resource callbacks that create renderable level objects remain
unknown. Trace that path, then verify a real mesh pointer reaches the draw
queue.

**Class ownership follow-up (2026-11-07):** the two classes in that update-list
sample are now attributable. Class `0x1e` nodes are created by the world setup
path (`func_001DCEA0`); class `0x51` nodes use descriptor `D_00217B60` and are
ConeVolume instances created through `func_001E6188`. The opt-in registration
trace records owner, parent, mesh, attachment, extra-data, and radius fields.
For the direct-loaded level, all ten class-`0x1e` nodes and both class-`0x51`
nodes are root registrations owned by world `0x0088d530`, with no parent and
zero mesh/attachment/extra-data/radius values. These are world bookkeeping and
culling-volume entries, not candidates to force through the scene renderer.
This closes the immediate question about the sampled lists but does not find
the level's renderable-object table. Continue from the level activation/resource
object path rather than expanding these update-list callbacks. Diagnostic log:
`/tmp/sly-native-class-owner-trace.log`.

The earlier reader-window comparison was invalid: `0x70000000` is EE
scratchpad, but the dump helper normalized it as a main-RAM alias. The helper
now reads the scratchpad mapping and captures both the cursor window and the
full 8 KiB decoder ring. The latest level capture has cursor `0x70000c9c`,
`0x1364` bytes remaining, and 1,171 nonzero bytes in the ring. No exact 8 KiB
match was found in the offline-decoded resource, but the decoder's logical
output offset and ring-history boundary have not been captured, so that result
does not yet prove a decompression mismatch.

The native level loader also no longer pre-reads the entire compressed range
or overwrites reader fields `+0x20/+0x24`. Exact retail `func_00137490` now
refills the configured ring through the port's synchronous sector adapter. A

**Restart checkpoint — frame-list corruption trace (2026-11-07):** the prior
frame-queue hypothesis was disproven. Matched decomp `func_0015EC68` already
copies and clears all three source queues correctly. Port-only trace
reconstructions of `func_00139EC8` and list append `func_00152470` show the
global frame list at `0x002622E0` has consistent head/tail and node `+0x630`
links after every append and after the `func_001243E0` callback. Later,
`func_00152600` removes the original node `0x0082f9b0` and sees corrupted
next/previous values `0xbf343386` / `0xbef492b5`, faulting at
`0xbef498e5`. The corruption therefore occurs after the transfer callbacks
and before frame-context cleanup; its writer is still unidentified. Latest
trace: `/tmp/sly-native-frame-head-chain.log` (failure near end). Current
uncommitted code puts the two C diagnostics in port-only override/backfill
files; it does not edit `sly1-decomp`. Next, inspect the calls between the
last valid frame-list trace and `func_00152600`, identify the first bad write,
then fix the underlying port fault. The latest build succeeded (2,040 C
functions translated, 206 retail assembly functions, zero stubs); a bounded
native launch still faults and shows no graphics. No emulator was used.

**Display worker correction (2026-11-07):** the frame worker was reaching
`func_0015F320`, but hostgen had no implementation for it and supplied a
no-op. Added the exact US retail assembly function to the assembly allowlist;
the old unlinked handwritten C prototype was removed because it passed the
wrong packet capacity and was not a safe substitute. Rebuilt successfully:
190 C units, 2,040 C functions, 207 retail assembly functions, zero stubs.
A 15-second native run completed its window without the previous list fault.
The instrumented run inserted and drained 40 frame-list nodes; every recorded
`+0x630` link was valid through cleanup. By the final trace, the renderer had
accepted 1,461 GIF chains and submitted 240 vertices (117 inside the viewport).
The UI trace still reports zero visible text, and this does not establish that
the game title or a playable scene is visible. Next, diagnose why ordinary
startup remains at frame state 3/game mode 0 with menu page `-1`, then confirm
visible title/scene output. Run log: `/tmp/sly-native-retail-15320-trace.log`.

**Black-frame capture and object-reader assembly (2026-11-07):** renderer
capture confirms the window is still black: all eight dumped 960x720 frames
contain one color (black). Runtime diagnostics report frame state 3, game mode
0, menu page `-1`, and zero visible text. The 240 submitted vertices are not
evidence of visible graphics. `func_0011ECB8` was another reached no-op despite
having US retail assembly; selecting that assembly removes its warning and
keeps startup running, but does not change the black result. A 15-second trace
drained all 40 queued frame nodes with valid links and no list fault. The
current hostgen selection builds 2,040 C functions and 208 US assembly
functions, zero stubs. Next trace the title/movie/state transition and identify
which reached no-op or missing resource prevents entry into the title scene;
do not treat rendering the frame-transfer rectangle as a scene. Evidence:
`/tmp/sly-native-retail-11ecb8.log`, `/tmp/sly1-native-frame-00.ppm` through
`/tmp/sly1-native-frame-07.ppm`.

**Retail state-dispatcher check (2026-11-07):** the handwritten
`func_001E9108` C backfill was moved to `hand/reference_c/` and the exact US
assembly selected. This preserves the comparison source while the runtime
executes vanilla instructions. The clean build now has 2,035 C functions,
209 retail assembly functions, and zero stubs. A fresh bounded run still
reports frame state 3/game mode 0, menu page `-1`, and zero visible text; its
eight captured frames remain black. Thus the C dispatcher was not the only
cause. The state data reaches the retail dispatcher with zero flags, and no
pending movie/resource request is visible in the trace. Next inspect who
initializes the state data and queues the initial title/intro request, plus
the reached missing callbacks in that path. Trace:
`/tmp/sly-native-retail-state-dispatch.log`.

**Resource-table assembly experiment (2026-11-07):** enabling exact assembly
for `func_0011D140`, `func_0011D968`, and `func_0011DB00` alone stopped startup
before an active world was created. The loader thread remained in its early
poll at `func_0011D078`. Adding exact assembly for that poll routine and its
missing `func_001F8480` dependency restores the active-world loop. The
dependency-closed set is retained. It removes those reached no-op warnings,
but the bounded run still reports frame state 3/game mode 0, zero scene
vertices/text, and all-black captured frames. Trace:
`/tmp/sly-native-resource-poll-closure.log`; the standalone failure is at
`/tmp/sly-native-resource-table-asm.log`.

The current hostgen build has 2,035 C functions and 214 US assembly functions,
with zero stubs. It drains 40 frame-list nodes with valid links; this is more
reliable startup, but still not visible graphics or playability.

**Object-key callback restoration (2026-11-07):** added exact US assembly for
the reached no-op `func_001837E0`, which performs resource-key lookup and
queues a callback. Its direct callees were already available, so this
single-function addition rebuilt cleanly and removed its no-op warning. A new
15-second run remains in state 3/game mode 0 with zero visible text. It drains
40 frame-list nodes cleanly, but both sampled native frames are still all
black. The assembly path is restored, but it has not yet produced a visible
title or scene. Trace: `/tmp/sly-native-object-key-asm.log`. Current count:
2,035 C functions / 215 US assembly functions / zero stubs.

**Class-`0x4d` display-packet path (2026-11-07):** selected exact US assembly
for `func_00139B70` and its missing packet helper `func_001612F8`. These
functions build the GS packet used by the repeated class-`0x4d` objects. The
native run remains stable and drains all 40 frame nodes with valid links, but
the capture is still all black; frame state is 3/game mode 0, visible text and
scene vertices are zero. GIF DMA chains are accepted, so remaining work is
upstream of useful scene submissions or in interpreting their packet content.
Trace: `/tmp/sly-native-class4d-gs-packet.log`. Current count: 2,035 C / 217
US assembly / zero stubs.

**Scene packet-builder translation (2026-11-07):** the runtime had also been
resolving `func_001662A8` as a no-op even though its matching source still has
`INCLUDE_ASM` and the US retail body is available. Selected that exact
assembly. The older port C prototype is retained in `hand/reference_c/` but
removed from the runtime build to avoid competing implementations. The
15-second run stays stable and drains all 40 frame nodes, but still reports
state 3/game mode 0, zero scene vertices/text, and black captured frames. The
builder's presence alone is insufficient; establish whether a drawable scene
record reaches it and whether its output is consumed. Trace:
`/tmp/sly-native-scene-packet-asm.log`. At that checkpoint: 2,035 C / 218 US
assembly / zero stubs.

**Scene packet helper closure (2026-11-07):** exact US assembly for the
scene-packet builder's no-op helpers `func_001EE3B8` and `func_001EE668` is
now selected alongside `func_001662A8`. The 15-second run remains in the
active-world loop, drains 40 frame nodes cleanly, and accepts its GIF chains,
but reports zero visible text/scene vertices and produces all-black renderer
captures. The packet-builder path is now present through its immediate helper
calls; inspect the table counts/record pointers passed into `func_001662A8` and
whether the loaded Splash data contains drawable records before adding more
assembly. Trace: `/tmp/sly-native-scene-packet-helpers.log`. Current count:
2,035 C / 220 US assembly / zero stubs.
bounded run reads Splash as two 37-sector chunks and level `0x0100` as four
599-sector chunks plus a final 3-sector chunk, then still reaches loader stage
5 with zero scene vertices. This removes duplicate full-range I/O and keeps
the retail ring state intact; it does not yet create a drawable level object.
Trace: `/tmp/sly-native-reader-ring-trace.log`; dumps:
`/tmp/sly1-level-reader-window.bin` and `/tmp/sly1-level-reader-ring.bin`.

The old port-only `func_0011ECB8` override was removed after checking the exact
US assembly. That assembly reads a counted object list: each record supplies a
class slot, key, and flag, then the factory creates the object and its virtual
reader consumes its payload. `func_0011EC40` is the separate property/callback
ID reader. The speculative object parser with its guessed alignment heuristic
remains under `hand/superseded/` for trace history only. The previously reported
6-vertex/0x880-face collision
record came from the earlier Splash resource (sector 820372), not the selected
level resource (sector 867214); do not use it to diagnose the level mesh.

The level resource header's first word is also no longer treated as a scene
offset: `func_001DB928` passes it to `func_0018D500` as an allocation size and
stores the second word as the resource revision. For key `0x0100` these are
`0x00240cd0` and `0x3d687640`. The next format trace should follow the allocated
table at `D_00275718` through its consumers; the current object-record list is
empty, and the table's relation to scene geometry remains unproven.

## Earlier architecture review (2026-11-07; historical)

The project has useful extraction, host generation, platform and rendering
infrastructure. Its largest gaps are executable game-code coverage and the
boundary between Sly rendering and console hardware simulation. Repeated
startup traces have not yet established a complete path from a loaded level
to a visible, controllable player.

Evidence inspected:

- CMake's default runtime includes ee.cpp, vif1_dma.cpp and vu1_native.cpp.
  The latter maintains vector registers, flags, microcode memory and a program
  counter. Compiling this code for the host does not make hardware simulation
  satisfy the user's no-emulation requirement.
- The current hostgen report reads 185 units, translates 2,051 C functions
  and 71 from assembly, lists 2,742 functions without C, and has 12 unreadable
  units. The build allows those units to be omitted, so it is not a sound
  gameplay baseline yet.
- README, ROADMAP and OPENRAC_REUSE give different snapshots and counts.
- The direct-level loader constructs objects but does not establish gameplay.
  The first eight traced default-startup objects do not prove what is wrong
  with an actual level: their identity basis and zero position/radius may be
  valid for their role. Their identity and ownership must be established first.

## Long-term playable gates

### 1. Establish a reproducible baseline

- [ ] Pin the decomp revision, tool revisions, retail version and source paths
  in a build manifest; replace the temporary source checkout with a stable one.
- [ ] Record generated coverage from that exact build, including backfill and
  assembly selections. Clearly separate historical results in the roadmap.
- [ ] Document one supported developer build and launch command. Keep host32
  alternatives explicitly experimental until there is evidence to choose one.

Done when a fresh checkout with user-supplied game data reproduces the same
function report and build without undocumented files in /tmp.

### 2. Enforce the native architecture

- [ ] Audit the default target's actual runtime calls, classifying compiled
  game code, native services, asset/packet decoding and hardware interpretation.
- [ ] Put instruction execution and console device simulation behind a
  separate diagnostic target; exclude them from the playable target.
- [ ] Define native interfaces for resource reads, scheduling, input, audio
  and scene drawing. Preserve retail pointer/layout adapters where necessary.
- [ ] Retain usable OpenRAC platform, hostgen and direct-renderer infrastructure.
  Compare any proposed additional reuse against its actual source and license;
  import generic components with attribution, never RAC-specific game code.

Done when the playable target executes compiled game functions and native
services without a CPU/VU instruction interpreter. Static translation of a
known function is distinct from a runtime interpreter and must be documented.

### 3. Close the game-code coverage gap

- [ ] Generate a per-unit report explaining why matching source is excluded:
  missing metadata, C++ lowering, ABI disagreement, unsupported syntax or no C.
- [ ] Prioritize units reachable from loading, frame update, player creation,
  camera and draw submission. Fix whole-unit hostability before writing more
  one-function replacements for code already available upstream.
- [ ] Audit omitted arguments, function-pointer calls, float/vector arguments
  and pointer-global declarations on those paths against retail behavior.
- [ ] Make required missing calls produce a named diagnostic and stop that
  operation, rather than silently succeeding as no-ops. Explicitly classify
  harmless optional services.
- [ ] Give every functional backfill provenance, ABI notes, known limitations
  and an upstream replacement condition in the port's separate registry.

Done when the chosen level's load/update/draw path has no unexplained no-ops
or fabricated zero arguments. A global matching percentage is not this gate.

### 4. Prove resource and object ownership

- [ ] Use one validated extraction/cache per disc identity; check concurrent
  launches share it without duplicate extraction processes.
- [ ] Select one real level and trace its resource key through decode, object
  construction, activation and assignment to the active game world.
- [ ] Identify traced objects by class, resource record, parent and mesh owner.
  Capture local/world transforms and bounds before and after activation.
- [ ] Compare those fields with the decoded record and retail update order.
  Fix the first demonstrated mismatch, including missing parent callbacks.

Done when a known renderable object has attributable mesh data, bounds and
camera-relative placement. Never force visibility merely to pass this gate.

### 5. Build the Sly scene-rendering path

- [ ] Document Sly's draw-request and mesh layouts at the boundary before
  hardware command generation; implement a native scene submission adapter.
- [ ] Render one actual level mesh with its real transform, camera and material
  using the existing GL infrastructure. A standalone viewer is diagnostic only.
- [ ] Add the remaining required geometry, texture/material, depth, alpha,
  skinning and animation behavior from Sly's formats and retail semantics.
- [ ] Fix the independently reproducible 2D font problem by comparing decoded
  glyph rectangles, UVs, texture addressing and blend state at each boundary.

Done when the running game's active world draws recognizable level geometry
and the animated player, with legible UI and correct camera motion, without
executing VU microcode. A triangle, font atlas or vertex count is insufficient.

### 6. Connect loading to real gameplay

- [ ] Complete the normal title-to-New-Game transition and world ownership.
  Keep direct loading as a diagnostic entry point, not a substitute for this.
- [ ] Wire native input through player actions and verify movement, jump,
  attack, camera control, collision, grounding and interaction.
- [ ] Implement the sound-event bridge to native audio, pause/resume and level
  transition behavior. Verify death/restart and checkpoint state.
- [ ] Exercise a complete opening section with human input and record observed
  behavior, remaining defects and the exact build used.

Done when the user can launch, start a game, control Sly and progress through
an actual section with working camera, collisions and interactions. This is
the first playable gate, not proof that the whole game is complete.

### 7. Finish and maintain

- [ ] Expand validation across levels, enemies, abilities, saves/loads, audio
  and transitions; keep failures explicit rather than claiming full support.
- [ ] Validate supported platforms and packaging after the primary gameplay
  path works; preserve game-data ownership and extraction requirements.
- [ ] Commit each coherent fix with its evidence and push regularly. Keep the
  WIP PR and contributor documentation aligned with the actual default build.

## Earlier native checkpoint (2026-11-07; superseded below)

The US native launcher now gets beyond the earlier malformed Splash object
records. The port explicitly selects retail assembly for the loader methods
`func_0011ECB8`, `func_00158B30`, and `func_001B5F50`, and for the animation
curve factories and missing readers needed by `func_0012B8C8`. These are
static translations of known retail functions, not a runtime interpreter.
Functional C backfills remain in the port registry and decomp source is
untouched.

Current hostgen reports 1,832 translated functions, including 91 selected
from retail assembly, 3 stubs, and 3,026 functions without C. It also reports
99 calls that omit arguments and fill them with zero. These are build counts,
not runtime coverage.

Retail assembly confirms `func_0013B598` forwards incoming register `a1` as
the stream to `func_001BAC30`, while its current decompilation signature omits
that argument. The port now selects this static retail translation with
signature `void(void*, void*)`, fixing that demonstrated ABI mismatch without
editing `sly1-decomp`. The earlier dispatch through descriptor data
`0x0021B4D8` no longer appears in the follow-up trace.

That earlier Splash fault was fixed sufficiently for the loader to reach the
frame loop and menu callback. The direct-level probe is a separate diagnostic
path: the current trace completes `func_00162890` and `func_00128C10`, then
faults during the child-object read path (`func_0011ECB8` / `func_00137D10`)
near reader cursor `0x70000641`, with guest address `0xFFFFAC70`. The full
object parser input window matches the extracted decoded resource at both
entry and exit, so that parser's stream cursor is not presently the first
demonstrated mismatch. The launcher remains black/garbled and is not playable.

The current menu evidence narrows the independent graphics issue: 79 textured
glyph quads decode with valid nonzero UVs, atlas handle `0x3`, and a resolvable
atlas/CLUT, but no recognizable pixels reach the visible window. This points
to the native draw/framebuffer path after packet decoding. Do not conflate that
with the direct-level loader fault.

## Immediate next steps

The older direct-level and guest-runtime notes below are historical. Current
host32 evidence supersedes their zero-vertex and stream-exhaustion diagnoses:
the normal loaded-world path reaches model draws. Do not spend the next slice
on loader repair, guest-runtime graphics, broad missing-function closure, or
startup backfills.

1. Capture a complete kind-6 strip at the native draw handoff, including UVs,
   strip boundaries, tint, transforms, and material state alongside positions.
2. Validate that strip against existing retail frame/data captures. If there
   is no useful reference, switch to the data-only level viewer rather than
   guessing at the VU/GS pipeline.
3. Draw a solid-color silhouette for only that strip and compare its shape,
   orientation, scale, and placement. Fix only the first measured mismatch;
   do not add textures or general VU behavior yet.
4. Require a recognizable loaded-world capture before enabling or expanding
   the renderer. Then resume the normal title path and validate responsive
   controls, collision, camera, audio, and saves as separate milestones.

One additional retail-assembly comparison found and fixed a port-backfill
layout error: `func_00162890` stores its component references as 32-bit values
at four-byte strides. The C backfill had been writing 16-bit values at two-byte
strides despite allocating the correct 4-byte-per-entry array. This does not
change how many bytes the reader consumes, so it is a memory/layout correction,
not yet a fix for the child-read fault. Keep it in the port backfill so it can
be replaced cleanly when the decomp supplies that function.

The bounded native trace command used for this checkpoint is:

```sh
OPENRAC_TRACE_LOADER=1 OPENRAC_STARTUP_TRACE=1 timeout 10s build/port/sly1_native \
  build/port/extracted/boot/SCUS_971.98 \
  '/home/reaper/pcsx2/Games/Sly Cooper and the Thievius Raccoonus (USA).iso'
```

It runs the native launcher, not an emulator. The latest captured output is
kept in `/tmp/sly-native-reader-trace.log` on the development machine
only.

Treat renderer traces and function coverage as diagnostics only; neither
proves visual correctness or playability.

## Coverage audit result (2026-11-07)

Run `python3 port/tools/audit_hostgen.py build/port/hostgen/us/report.json
--out build/port/hostgen/us/coverage-audit.json` as one command after hostgen.
The JSON retains complete compiler diagnostics and the direct-call frontier;
its categories overlap and do not establish indirect-call coverage.

The initial report at the time of this audit had 57 unreadable units: 16
reported inherited-member collisions, 15 layout assertion failures, 11
call-signature mismatches and 7 unlowered C++ members. This snapshot is
historical; later coverage results are recorded below. Core object units
00124000 and 00128000 encounter duplicate inherited `id` fields in the staged
animation header. World unit 0019C000 retains a C++ cursor constructor and
method syntax. Prioritize correct inheritance/layout and cursor lowering;
do not suppress layout assertions or rename fields without resolving accesses.
The audit command is implemented; source hostability fixes remain pending.

The staging correction preserves C++ member hiding for Animation.id
and ObjectCycler.index by renaming only the inherited anonymous-C copy.
The derived field keeps its name; base casts still address the original base
layout. The rebuilt object units no longer report those duplicate fields or
the associated layout failures. The full native build succeeds, but coverage
remains 1,108 translated functions: independent call-signature and C++ method
errors still exclude these units. Next address the static iterator helpers in
00124000 and the EnsureRangeRecord signature in 00128000, with retail ABI
verification rather than speculative casts.

The 00124000 static iterator helpers now lower to file-local C functions,
with their bodies and call sites preserved. The lowerer refuses to consume a
class unless its sole method is followed by the class terminator. A full
native rebuild succeeds; this unit now reports three errors: the sound-call
signature disagreement and a local Frame_124CC0 inheritance declaration
(with its consequent incomplete-type error). Coverage is still unchanged.
Retail inspection confirms EnsureRangeRecord consumes an object pointer;
fix its inconsistent callers/declarations together rather than adding casts.

The placed-object unit is now hostable. Its second `func_001BFCD8`
declaration and call use a distinct staged C name with the same assembler
label, allowing hostgen to retain the two source-level signatures while
mapping both to retail address `0x001BFCD8`. The local animation frame is
lowered to its zero-offset base fields plus its original tail. Rebuilding
increases translated coverage from 1,108 to 1,139 functions; unreadable units
fall from 57 to 56. The native launcher links successfully. This establishes
more real game code in the host build, but does not yet establish that these
functions are reached in a playable run.

Generalizing the conservative single-static-method helper lowering across all
source units increased hostgen translation from 1,139 to 1,284 functions and
reduced unreadable units from 56 to 53. The native launcher links and the
repository check passes. This recovered functions from several unrelated
units, but does not change the runtime playability status. The remaining
constructor/method failures are non-static helpers, including constructors
that initialize stack records. Lower those only with call-site/layout
preservation; do not erase them from records as if they were static methods.

For source unit 0018C000, the staged lowering now converts the simple
Pair_18CC20::Set, Key_18F198::Same and Pair_18F198::Set methods into C
helpers with explicit object pointers and rewrites their call sites. Field
accesses remain at their original offsets. The unit's diagnostics fell from
six to two; both remaining errors are return-type disagreements for the
memory-card write/close calls. Hostgen coverage has not increased yet, and the
native build still links. Resolve those function declarations from retail
call sites before claiming this unit recovered.

The retail call sites and assembly show that `func_0018D328` and
`func_0018D2B0` return the status in `v0`; their C bodies called
`FinishMemoryCardCall` but discarded its result. The staging layer now corrects
only these two signatures and returns in its disposable C view. The matching
decompilation sources remain unchanged. `0018C000` now compiles, increasing
hostgen translation from 1,284 to 1,329 functions and reducing unreadable units
from 53 to 52. The native launcher compiles 212 generated C translation units
and links. This is a host-coverage result only: it does not demonstrate that
startup reaches these routines or that a scene is visible. The next useful
coverage work is to inspect the 52-unit audit for startup/load/update/draw
callers and resolve the highest reachable whole-unit blocker with retail ABI
evidence. Keep backfills in the port registry and staging layer, with a clear
replacement condition for eventual decompiled C.

The next whole-unit blocker, 00128000, had lost the `PlacedObject*` argument
from four range-record setter signatures and had added an extra float argument
to the staged `EnsureRangeRecord` call. Retail assembly for 001292E8,
00129318, 00129348 and 00129378 passes the object in `a0`, preserves the float
in `f12`, and calls 001291D0 with the object. Staging now restores that
argument flow and the pointer-only helper signature. Its final C++ record
method, `St_12B2F8::Set`, lowers to a file-local helper with an explicit record
pointer and unchanged field offset. `00128000` now compiles, increasing
translation from 1,329 to 1,480 functions and reducing unreadable units from
52 to 51. The native launcher links, and `tests/check_repository.py` reports
zero problems. The remaining 51 units still need prioritization by actual
startup/load/update/draw reachability; this coverage gain alone does not mean
the game is playable.

Hostgen can now compile `file_1E08F8.cpp`. Its port staging lowers the small
`Ctx_1E0CD0::Pad` and `Sub_1E4EA8::SetOwner` methods to explicit-self C helpers,
and represents constructor `func_001E4E38` with its retail `self` return.
Soft-float overloads that share retail addresses are given distinct C names
with the same assembler labels, preserving their separate source call types.
The render-thread root `func_001E2398` is now translated from matching C.
Coverage is 1,527 translated functions, with the same 59 selected-assembly
translations, and unreadable units fell from 51 to 50. The three thread APIs it
calls (`func_001F6A10`, `func_001F6A50`, `func_001F6A90`) have existing native
implementations in `port/game/sly1-us/native/linux_threads.cpp`. The other
startup root, `func_0018E5E8`, was still blocked by `file_18D8E0.cpp`'s C++
movie sink classes at this stage. The build and repository check pass;
visibility/playability has not been demonstrated.

Hostgen now compiles the movie sink unit `file_18D8E0.cpp`, including matching C
for root `func_0018E5E8` and the four methods named by its retail vtables. Port
staging models the sink vptr as a 32-bit field, retains the ring fields at
their retail offsets, and initializes the ring and stack DMA vptrs from the
verified US vtable addresses. This keeps the C translation's virtual dispatch
layout intact. Translation rose from 1,527 to 1,572 functions, and unreadable
units fell from 50 to 49; the native launcher links and the repository check
passes. The startup-root list no longer contains either `func_001E2398` or
`func_0018E5E8`. The audit's next missing call is `func_0011A498` from
`func_001B7968`; its matched C is in `file_112698.cpp`, an unreadable unit.
This remains code-coverage progress, not proof of visible or playable output.

A bounded 12-second native launch after this build no longer reports the two
startup roots as no-C stubs, but its direct-render trace is still the same
untextured 240-vertex draw with zero target/source/texture addresses and black
vertex color. It uploads the font atlas and palette repeatedly without proving
glyph selection. The immediate visual blocker remains the draw-state path;
the next game-code blocker is still `func_0011A498` through the unreadable
`file_112698.cpp` unit.

## Latest build and startup failure (2026-11-07)

Port staging now lowers the `Gc_259F38` constructor/destructor and `RetValue`
aggregate returns in `file_112698.cpp`. It also corrects the hidden `a0`
argument on the 0011C3C0 pool helper and its 0011C3C8 caller, based on US
retail assembly. The 0011FFA8 staging wrapper calls its void worker and returns
the verified terminal `v0=0`. No decomp source was modified. The native target
builds 212 generated C translation units and links. Coverage is 1,834
translated functions, 59 assembly-derived, 3 stubbed, 3,056 without C, and 47
unreadable units.

The next native launch faults during object setup instead of reaching the
previous black render window. Temporary traces show `func_0011ECB8` parsing a
three-record list from EE scratchpad data. It accepts the first record
(`type=0x7A`, `key=0x25D`) and then reads `type=0` where a second record begins.
The bytes in the decompressed chunk match the decoded Splash resource exactly,
so the unpacker and its input are not the discrepancy. Investigate the parser
cursor and caller that supplies this list, focusing on the 0%-matched
`func_0011ECB8` and `func_00196410`, plus the first object's virtual reader
contract. Later exact-assembly inspection established that these are object
records, not callback records; see the correction below.
Do not suppress class zero or patch the descriptor table: that would hide a
cursor or call-path error. The runtime later faults at `0xDBBBC9E2`; startup
and the previous black/garbled rendering remain unresolved.

The `func_0011E580` object-factory backfill existed in the port tree but was
not selected: hostgen compiles the staged C files, then keeps the decomp
definition for duplicate symbols unless the port lists a matched function in
`hand/overrides.tsv`. It is now explicitly listed there. This makes the
port-owned implementation active while preserving a one-row removal path when
matching C becomes hostable. Verification: hostgen reports eight overrides
used, the generated definition contains the port factory body, and the
generated registry points `func_0011E580` at it. The next bounded native launch
still faults on this malformed-record path, with a call through address zero
from `func_0011ECB8` followed by an unmapped access at `0xAB1BABFD`.

## Latest native checkpoint (2026-11-07, supersedes the preceding fault details)

The launcher no longer runs the Splash loader into an out-of-range write.
Crash-register and reader diagnostics showed retail `func_001BAC30` receiving
`0x3F800000` (the bit pattern for `1.0f`) as an array count, then attempting an
allocation whose signed size wrapped. The checked heap backfill correctly
rejected that request, but retail assembly continued its count-controlled
loop and wrote through null. This is a stream-layout/cursor defect, not heap
exhaustion.

The port now has a temporary, explicitly registered `func_00137C40` safety
backfill: normal reads remain unchanged, and values above `0x100000` are
reported at loader stage 161 and returned as zero. In a 12-second native
launch it contained four invalid values (`0x0FFFFFFF` at cursor `0x70000A16`,
`0x0FFFFFFF` at `0x7000057A`, then `0x3F800000` and `0xC0000000` at
`0x70000FF0` and `0x70000FF4`),
prevented the prior guest-RAM fault, and let the loader continue into later
objects. The run timed out while active; it did not reach a playable scene.
The trace also reports an unresolved indirect call to `0x0021B4D8`. Treat the
guard as containment only; fix the parser/call path and remove the override
when matching `func_00137C40` is hostable. Trace: `/tmp/sly-native-guard-run.log`.

Current generated hostgen summary is 1,833 translated functions and 3,028
functions without C. The report includes assembly-selected functions among
the translated total; see `build/port/hostgen/us/report.json` for per-function
selection details. The native window remains black, so the playable gate is
not met.

## Follow-up: descriptor override and world-load completion (2026-11-07)

Inspection of generated host code found that the port already contained a
`func_0011E680` resource-descriptor backfill, but hostgen was still using the
decomp definition because the function was absent from `overrides.tsv`. Adding
the explicit port override activates the code and its scoped diagnostics. A
new 12-second NTSC-U run no longer reports the earlier indirect call to
`0x0021B4D8`; it reaches `func_001DCEA0` stage 5 after object initialization
and then continues through frame setup. The invalid-count guard still contains
malformed values. This establishes that world loading can complete under the
current port substitutions, not that all objects were parsed correctly.

The remaining black display is now downstream of world-load completion. The
run still reports many missing-C functions in post-load and frame setup, and
only submits the small initialization GIF chains in the observed window. Next
trace the first active frame callback and its draw-list dispatch, then fill
the highest-level missing callback with port-only C that matches retail call
semantics. Do not treat successful stage 5 as a playable level. Latest trace:
`/tmp/sly-native-dispatch-active.log`.

## Follow-up: preserve raw reader values and inspect first frames (2026-11-07)

After enabling the `func_0011E680` backfill, later runs no longer observed the
`0x3F800000` and `0xC0000000` values that had driven the original runaway
array loop. The broad `func_00137C40` guard was therefore removed: it had also
rewritten unrelated high-bit words, including `0x80000000`, and could hide
valid retail data. The `func_0018D500` heap-bounds check remains as a memory
safety measure, but no generic reader now substitutes values.

A 20-second traced run reached the active frame loop with the world present,
four registered fonts, and menu object `0x26FF68` in transition. At 5.1 game
seconds the trace counted 240 submitted vertices (120 visible) and over 1,400
GIF-chain submissions, yet the window remained black; the samples had
degenerate `0x80000000` vertex words. The live frame callback was
`func_001AD970`, the display worker was `func_00202C38`, and the game held at
frame state 5 with no visible text. This moves the next investigation to the
actual frame draw data, display chain, and the numerous missing-C callbacks
around `func_001AD970`, rather than loader completion. Latest trace:
`/tmp/sly-native-postload-trace.log`.

## Historical: mistaken callback-list interpretation (2026-11-07; superseded)

The investigation below contains the reversed interpretation: exact US
assembly proves `func_0011ECB8` is the object-list reader, while
`func_0011EC40` reads property/callback IDs until its negative sentinel. The
`func_0011ECB8` stage-23/24 cursor bracket and its factory calls describe the
object records, not a callback list. Keep the entries below as chronology only.

The unguarded launch confirms the reader override is no longer needed: without
it, Splash initialization still reaches `func_001DCEA0` stage 5 and the active
frame loop, with no `0x02000000` fault. It still reports a null indirect call
from `func_0011ECB8` while parsing the first Splash object list. Inspection
shows the port's instrumented `func_0011ECB8` parser was also only in
`backfills.tsv`, while hostgen selected the decomp definition. The port now
explicitly overrides it, and its forced `from_assembly` / `asm.only` selection
has been removed from `hostgen.json`. The new build reports 1,834 translated
functions and 90 assembly-selected. A 12-second launch now emits the port
stages 21–35 with plausible first records (count 36, slot 4, key 1, flag -1),
reaches world-loader stage 5, and reports no null or `0x0021B4D8` indirect
call. The broad reader guard is absent; no guest-RAM fault occurs. Later
serialized values still include out-of-range slots, so the record stream
needs validation before treating its objects as correct. The window is still
not a playable scene. Trace: `/tmp/sly-native-object-records-active.log`.

## Updated priorities (2026-11-07)

The following results supersede the older startup-fault and “next loader
fix” statements above. The latest hostgen report reads 199 units and translates
1,834 functions, including 90 static assembly translations; it reports 3
stubs and 3,030 functions without C. These are generation counts, not reachable
gameplay coverage.

A normal native launch reaches the active frame loop without the former null
indirect-call and `0x02000000` write errors. A fresh traced launch reaches the
Paused menu page, calls its draw callback (`0x194f80`), emits glyph records,
and uploads the 512x512 PSMT4 atlas plus its 8x2 palette. This proves menu text
generation and texture upload, not a visible menu: the user-visible output
remains black or garbled. The existing PPM captures were taken before the
136KB textured menu packets begin. The renderer also receives repeating
912-byte packets,
each decoding to a 240-vertex untextured draw whose first vertex has zero UVs
and black RGBA; target, source and texture are all zero. Separate 144-byte
packets decode to no vertices.

The next tasks are:

1. Isolate why the decoded textured glyph draw leaves no visible pixels.
   `OPENRAC_TRACE_DIRECT=1` now shows a 136,512-byte frame packet with three
   draws: the first glyph batch is 474 vertices (79 quads), textured with
   atlas handle `0x3`, TCC enabled, modulation, and blending. Its first vertex
   is at `(-0.021, 0.000)`, samples `(0.533, 0.916)`, and has RGBA
   `(127,127,127,128)`. The atlas and CLUT uploads reach TBP `0x1400` and CBP
   `0x1600`; both resolve successfully. Next compare draw/scissor/depth/blend
   state with the resulting framebuffer and test the indexed atlas values at
   these UVs. Do not replace the working path until a specific fault is
   demonstrated.
2. Correlate that glyph batch with the source DMA chain and the other two
   draws in the same frame. The first two VIF1 transfers are 736-byte DIRECT
   streams (transfer 1 also loads eight MPG blocks); the repeating worker
   transfers likewise report 736 DIRECT bytes and no MSCAL. `OPENRAC_VIF_TRACE`
   now accepts `OPENRAC_VIF_TRACE_LIMIT` to see later transfers. The renderer
   accepts channel-2 chains and VIF-produced GIF bytes; do not conflate the
   small repeating black packets with the later 136KB textured frame packet.
3. Trace the first scene object with real mesh data from its resource record
   through parent/world transform, camera culling and draw callback. Sampled
   update entries currently have zero translation and radius; prove their
   identity and role before changing culling.
4. Refresh the decomp hostability audit and prioritize unreadable units only
   when their functions are on these observed paths. Check call signatures and
   omitted register arguments against retail assembly.
5. Reproduce the intermittent DMA allocator abort before fixing it. One
   font-traced run aborted during vector reallocation in
   `openrac_native_renderer_submit_dma_chain`; a later font/DMA trace and a
   20-second `MALLOC_CHECK_=3` run did not reproduce it. If it recurs, capture
   DMA source ranges and run a sanitizer build. Do not apply an allocator
   workaround without evidence.

For a later framebuffer capture, `OPENRAC_DUMP_NATIVE_FRAME_SKIP=N` skips the
first N eligible renderer packets before writing up to eight PPMs. The default
is zero. The draw-state trace for this update is
`/tmp/sly-native-direct-textured-draws.log`. Existing machine-local PPMs were
captured before the glyph batch. No game data was committed.

All functional game replacements remain in
`port/game/sly1-us/hand/native_c/`, registered as backfills and removable when
matching C becomes hostable. Runtime and renderer corrections remain under
their respective port directories. The next progress gate is a verified
visible menu or scene image, not more global function coverage.

## Decomp snapshot check (2026-11-07)

The configured source checkout, `/tmp/sly1-decomp-current-check`, is at
`dd6f80b` (2026-11-06). Upstream `master` is now
[`04f7f272`](https://github.com/OpenThievius/sly1-decomp/commit/04f7f272552f4853fa251d9a12a75701546a2bd3),
149 commits ahead of that source snapshot. Its NTSC-U report lists 4,723 of
4,807 functions matched, 92.81% matched code bytes, and 92.24% matched data
bytes. Those totals use a changed denominator from the earlier report and
should not be compared as a simple regression/improvement.

Comparing the upstream function reports identifies three newly exact matches
since `dd6f80b`: `func_0013E650` (4 bytes), `func_0018B670` (176 bytes), and
`func_001FFC08` (120 bytes). The decomp's name notes identify `func_0018B670`
as a memory-card-only background job. `func_0013E650` is only a four-byte
function. `func_001FFC08` is a library function, and its relevance to the
active native frame path is not established. None currently displaces an
active Sly backfill or explains the black frame. The port build still uses the
older `dd6f80b` source snapshot; no decomp checkout was changed.

## Graphics investigation update (2026-11-07)

The frame dumper gained `OPENRAC_DUMP_NATIVE_FRAME_MIN_BYTES=N`, allowing a
capture to select the actual 136,512-byte glyph packet instead of earlier
small packets. The normal capture is still nearly black but contains faint
dark-green glyph blocks. Flat debug mode displays gray rectangles in the same
area, confirming rasterized geometry; `OPENRAC_DEBUG_T4_INDEX=all` makes the
sampled indices visible but leaves the glyph layout fragmented. The T4 upload
swizzle experiment and a diagnostic whole-texture V flip both produce
high-frequency noise in index-grayscale mode, while reversing the low/high
nibble order changes pixels but does not repair the fragmented menu. Preserve
the unflipped linear path while investigating within-image addressing. The raw
64-byte CLUT at `0x1600` contains low-green/low-alpha
RGBA words and matches the decoded Splash bytes. Verify upload CSM/CSA and
alpha interpretation against the retail packet before considering palette
changes. Evidence, command and limits are in
[`SLY_GRAPHICS_HANDOFF.md`](SLY_GRAPHICS_HANDOFF.md).

The latest font trace anchors ASCII `P` at `(u=273,v=43)`, size `16x41`,
logical screen `(313.2,246.4)`. Its emitted normalized top-left UV is
`(0.533,0.916)`, exactly the retail helper's V flip. This supports leaving the
matched font-layout behavior alone while atlas addressing/CLUT interpretation
is investigated.

Immediate graphics tasks:

1. Compare the known `P` glyph record and screen quad to the raw T4 pixels at
   its transformed UV rectangle. Explain the text fragments that remain in
   the linear index capture before changing atlas addressing.
2. Decode the font CLUT upload tag and its TEX0/TEXCLUT state, proving the
   16-entry ordering and alpha scaling independently of the image dump.
3. Add a renderer-only synthetic T4 texture/CLUT case behind a debug switch
   (solid opaque entries plus a simple glyph mask) to distinguish conversion
   errors from Sly's packet state; remove or retain it strictly as a diagnostic.
4. Once text is legible, return to the gameplay path: identify a renderable
   level object with real mesh ownership and transform, then fix the first
   demonstrated missing callback or scene adapter. Do not treat the font fix
   as a playable milestone.

## Direct-level probe follow-up (2026-11-07)

The `OPENRAC_BOOT_LEVEL` path had not been running: its port C definitions for
`func_001E4E80` and `func_001E4EA8` were listed as backfills, but hostgen chose
the matching decomp definitions. They are now also in the explicit override
registry, so the generated host file contains the native synchronous loader
and catalog lookup. The build succeeds and the generated report records both
overrides. This changes only the opt-in probe; ordinary startup remains on the
retail request path.

A fresh bounded native run with `OPENRAC_BOOT_LEVEL=0x0100` reaches the level
range at sector 867214 (4,911,835 bytes) and begins parsing it. The trace then
showed that `func_001EE930`, a value-list reader present in the decomp, was
selected as a no-op because retail metadata classifies its address as
assembly. A port-only backfill now implements its decomp-documented reads so
the object stream advances. This is deliberately kept outside matched C.

An earlier trace of the Splash resource (sector 820372) reached the collider
mesh path with 6 vertices, `0x880` faces and 22 links, then read beyond the
valid stream. That malformed record is not evidence about the selected level
resource: its range is sector 867214, and the current trace reaches object
record count zero without entering the collision parser. Keep the Splash
collision issue separate until it is reproduced with a valid, attributable
mesh. The animator probe override remains behavior-equivalent to the decomp
and logs serialized types when `OPENRAC_TRACE_LOADER` is enabled.

Reproduce with:

```sh
OPENRAC_BOOT_LEVEL=0x0100 OPENRAC_STARTUP_TRACE=1 OPENRAC_TRACE_LOADER=1 \
timeout 12s build/port/sly1_native build/port/extracted/boot/SCUS_971.98 \
  '/home/reaper/pcsx2/Games/Sly Cooper and the Thievius Raccoonus (USA).iso'
```

The latest machine-local trace is `/tmp/sly-native-value-list-probe.log`. No
scene geometry or playable level is established by reaching this parser path.

## Stream and heap follow-up (2026-11-07)

The next blocker was earlier than the collider report above: unreadable unit
`00134000` left the core buffered-reader functions as no-ops. The port build
now translates `func_00137A60`, `func_00137B90`, `func_00137BE0`,
`func_00137C40`, `func_00137CC0`, `func_00137D10`, `func_00137D78`,
`func_00137820` and `func_001378A0` from the retail US assembly. They remain
generated assembly fallbacks in the port build; no decomp source was changed.
Hostgen now reports 2,051 C functions, 84 assembly translations, zero stubs,
2,729 functions without C, and 12 unreadable units.

After those translations, the startup reader consumed 8,184 bytes from the
Splash resource. The next failure is allocator initialization: the port-only
`func_0018D500` saw `D_002C2A1C = 0x160` and heap limit `0`, so allocations of
390,384 and 9,056 bytes failed. The run then faulted at host address `0x8`.
This confirms the no-op byte reader was one blocker, while the guest heap is
now the first demonstrated blocker. The parser-size changes above remain
unverified at runtime.

Concrete next tasks:

1. Trace the retail initialization path that should set `D_002C2A14`,
   `D_002C2A18` and `D_002C2A1C`; determine why the native start reaches
   `func_0018D500` with a bogus bump pointer before changing allocator policy.
2. Check whether the missing initializers live in one of the 12 unreadable
   units or are skipped by the native startup path. Prefer translating the
   relevant retail functions or fixing startup wiring over inventing heap
   bounds.
3. Re-run the direct-level probe and verify a nonzero guest allocation,
   advancing stream cursor, and parsed object count before returning to the
   collider mesh count mismatch.
4. Keep any required behavior backfill under `hand/native_c/` and registered
   in the port backfill/override tables. Replace it with matching decomp C
   when available; never fold it into generated or upstream decomp source.

Latest bounded run log: `/tmp/sly-native-reader-asm-current2.log`. The native
launcher remains non-playable; this run rendered no level or controllable
character.

## Heap initialization and level-resource progress (2026-11-07)

Retail `func_0018D430` was the missing heap initializer. Its US assembly aligns
the loaded-image end to get the heap base, sets the limit to `0x02000000`, and
initializes `D_002C2A14`, `D_002C2A18` and `D_002C2A1C`. The port build now uses
that assembly fallback, so allocator requests succeed and the configured
direct-level probe reaches sector 867214 (4,911,835 compressed bytes).

The current build uses 2,051 matched C functions, 105 retail assembly
translations, zero stubs, 2,708 functions without C, and 12 unreadable units.
Additional retail translations cover the missing float/resource readers and
the level callback path. The latest 12-second native run stays alive through
the timeout and advances the selected level resource through loader stages
3–5. It constructs an active world, but reports no registered fonts, no
visible text, and no rendered scene mesh. The trace also reports a null font
vtable call in `func_0016BA40`; it is nonfatal in this run. A port-only
`func_001ACDA0` guard skips its retail font callback only while font slot 0 or
its vtable is absent. With a valid font, it follows the original decomp logic.

An experiment enabling retail assembly `func_001B3510` faults on guest address
`0x0C1AE80F` from the level callback path. That function remains unselected
until its invalid input is explained. The complex animation renderer is not
considered working. The complete direct-level trace is
`/tmp/sly-native-stable-probe.log`; the launcher command is:

```sh
OPENRAC_BOOT_LEVEL=0x0100 OPENRAC_STARTUP_TRACE=1 OPENRAC_TRACE_LOADER=1 \
timeout 12s build/port/sly1_native build/port/extracted/boot/SCUS_971.98 \
  '/home/reaper/pcsx2/Games/Sly Cooper and the Thievius Raccoonus (USA).iso'
```

Next tasks:

1. Trace the loader path that populates `D_00262268[]` and `D_0026227C` from
   the Splash/font resource. The UI font callback sees an empty registry even
   after the resource reader advances; find the missing initializer or the
   earlier parser field that prevents registration.
2. Confirm the Splash and level readers consume the correct byte ranges. The
   trace shows small remaining counts (`268` and `5168`) and large heap growth;
   compare each cursor and allocation against the retail routine before
   treating stage 5 as a complete parse.
3. Follow the first level mesh allocation into the native packet path. Require
   nonzero decoded vertices and a nonempty scene draw before claiming a visual
   level.
4. Diagnose why translated `func_001B3510` receives invalid guest data. Check
   its caller inputs and all resource records before re-enabling that
   animation path.
5. After a scene mesh renders, verify Sly spawn, collision, movement, camera,
   controller input, and level transitions. A level-resource load is not a
   playable milestone.

All C compatibility code stays in `hand/native_c/` and the override table;
retail assembly remains a configured native fallback. Neither changes
`sly1-decomp`. The launcher is still not playable.

## Retail animation-parser checkpoint (2026-11-07)

The handwritten `animation_table_loader.c` guess was removed from the port
backfill registry. Hostgen now translates the exact US retail assembly for
`func_001B3040`, `func_001B2F00`, `func_001B4920` and `func_001B4980`; its table
helpers `func_001B2BA8` and `func_001B2CF8` already have matching C. The
port-only `animator_loader_probe.c` factory guard consumes the fixed 0x1C-byte
record payload and skips unsupported types rather than calling a null vtable.
All of this is port-side; the decomp checkout is unchanged.

The latest 12-second native run reaches Splash stage 14 and the selected level
stage 14, and keeps the level world active at `0x0088d530`. Animator records
now show valid retail types 1, 4, 6, 7 and 8 with nonzero objects and vtables.
The runtime remains black: the UI reports 37 global UI objects but zero fonts
and zero visible text, while the renderer reports zero scene vertices. The
next graphics milestone is the first attributable mesh callback producing a
nonzero vertex stream, not merely an active world or populated update lists.
Hostgen reports 2,043 C functions, 109 retail assembly translations, zero
stubs, 2,705 functions without C and 12 unreadable units. The bounded trace is
`/tmp/sly-native-retail-anim-loader4.log`.

## Retail object-list and DMA trace correction (2026-11-07)

The old port-only `func_0011ECB8` parser is no longer in the active build. Exact
US assembly confirms it reads counted object records containing class slot,
key, and flag, then invokes the object's virtual reader. Its count is an object
record count. `func_0011EC40` is the separate property/callback-ID reader. The
speculative parser and its unverified alignment heuristic are retained only
under `hand/superseded/`. The native
build now selects exact retail assembly for `func_0011ECB8`, `func_001AD970`,
and `func_00202C38`; the last two were active runtime functions. Hostgen reports
2,044 C translations, 111 assembly translations, zero stubs, 2,703 functions
without C, and 12 unreadable units.

A 10-second direct-level run reaches loader stage 5 and keeps world
`0x0088d530` active, but still reports frame state 3, game mode 0, zero scene
vertices, no registered fonts, and no visible text. The active world list
counts are `1,c,c,a`; the root object has no children. This remains a black,
non-playable frame.

`OPENRAC_VIF_TRACE=1` captures a 48-byte VIF1 chain sourced from `0x00212110`.
Its first DMA segment is an inline CNT tag with QWC 3, followed by END. The VIF
stream contains opcode `0x40` at offset `0x14`. The [PS2SDK VIF command table](https://ps2dev.github.io/ps2sdk/vif__codes_8h.html)
does not define `0x40` as a command. Retail `func_002018E8` writes
`VIF1_ERR=2`, leaving the invalid-command error unmasked according to the
[EE User's Manual, VIF1_ERR](https://usermanual.wiki/Pdf/EEUsersManual.1748563585.pdf). Determine
whether this is an intentional error probe or a malformed startup packet
before changing native VIF behavior. It is a graphics-initialization lead, not
proof that the level mesh path has been reached.

Latest traces: `/tmp/sly-native-vif-failure-context.log` and
`/tmp/sly-native-dmawait-retail.log`.

## Retail world initialization follow-up (2026-11-07)

Selected exact NTSC-U assembly for `func_00151D28`, `func_001BEB10`, and
`func_001DCD28`, which the port-only level loader calls during resource setup
and world traversal. A fresh 12-second direct-level run still reaches stage 5
with world `0x0088d530`, but produces zero root children and zero renderer
vertices, with no fonts or visible text. These translations remove no-C stubs
but do not yet reach the scene mesh path. The trace is
`/tmp/sly-native-retail-init-path.log`; current hostgen coverage is 2,044 C
functions, 114 retail assembly translations, 2,700 functions without C, and
12 unreadable units. Decomp checkout remains unchanged at `e1b39f1`.

## Native font-initialization follow-up (2026-11-07)

The normal-startup trace showed 37 global UI objects but an empty font
registry. Retail assembly for `func_001B40C8` clears and initializes the font
and image registries and their supporting pools; hostgen had left this
function as a no-op. The native build now selects this exact US retail
translation. The same trace exposed `func_001B2DF8` as another reached no-C
stub. Its retail routine reads the font-table count and calls `func_0015D8A8`
for each font record, which writes entries into the font registry. Both are
now selected from exact US retail assembly, along with `func_001B40C8`. The
build reports 2,044 C functions, 117 assembly translations, zero stubs, 2,697
functions without C, and 12 unreadable units.

A fresh 12-second native startup run no longer reports `func_001B2DF8` as a
no-op, confirming it runs, but still ends with font count 0, zero visible
text, and zero scene vertices. The remaining font issue is in the table input
or its count, or a later reset; trace that boundary before changing rendering.
Trace: `/tmp/sly-native-font-parser-assembly.log`. The decomp checkout remains
unchanged; all C compatibility code stays in the port backfill directory.

**Follow-up trace correction (2026-11-07):** `0x70000000` is the port's mapped
resource window, not an invalid pointer. In a fresh normal start, the Splash
resource enters `func_001B3040` at cursor `0x70000089` with 8,055 bytes left;
its font-table reader is reached after the stream is exhausted. This does not
establish a broken font pointer or prove that this Splash resource contains a
font table. A direct level `0x0100` run has 4,964 bytes left after
`func_001B3040` and 1,680 bytes after world setup, yet still reaches the frame
loop with zero vertices. Treat missing Splash fonts as a separate UI issue;
the first scene-graphics blocker remains ownership and activation of a
renderable level object.

That direct-level run also reached three resource animation callbacks that
were still no-C stubs: `func_001B5060`, `func_001B5200`, and `func_001B5398`.
The native host build now translates their exact NTSC-U retail assembly; the
decomp checkout is unchanged. The callbacks no longer emit stub warnings, but
the level root still has no children and the renderer still reports zero
vertices. Trace: `/tmp/sly-native-direct-level-anim-callbacks.log`.

**Graphics-callback follow-up (2026-11-07):** exact US assembly is now also
selected for `func_001B4330`, a color conversion helper reached by the level
resource path. A 12-second direct-level run survives and remains at zero scene
vertices. I also tried selecting `func_001B3510`, a text/packet routine, but
the normal Splash load faults in its `func_001B29E0` / `func_001B2988` table
lookup before the level loads. That selection has been removed. The missing
prerequisite is the table state expected by this retail routine; do not enable
it again until its initialization path is established. Trace:
`/tmp/sly-native-direct-level-graphics-callbacks.log` (faulting attempt) and
`/tmp/sly-native-direct-level-color-callback.log` (current successful run).
The current hostgen report has 2,044 C translations, 121 assembly
translations, zero C stubs, and 2,693 functions without C.


## Graphics distance checkpoint (2026-11-07)

The native launcher reaches the game frame loop and can load the selected level
record, but it has not produced an actual scene image: recent traces report
zero scene vertices, no registered fonts, and no visible text. The earlier
fragmented menu glyph captures came from a forced menu diagnostic and should
not be treated as vanilla graphics progress. Exact US assembly now supplies
menu filtering and text rendering where native startup had port-only overrides;
matching decomp C is used for the function already available there.

The next meaningful graphics milestone is one retail-created level object
with valid mesh data reaching the renderer and producing nonzero vertices.
The active world currently has no root children, while sampled update objects
have no mesh pointer or radius, so adjusting texture colors or culling cannot
produce a real scene. Font-table initialization is a parallel gate for title
and menu graphics.

## Resource-reader and default-start follow-up (2026-11-07)

The native port now has a separate `func_001BAC30` backfill for the missing
class-4 reader, plus port-only packed-vector readers `func_00137E90` and
`func_00137F08`. A bounded run consumes that class-4 record through stage 134
and continues to loader stage 5. A malformed nested frame link (`0xffff823f`)
previously faulted object update; the existing `func_00125138` backfill now
skips links outside guest EE RAM and records the values. These are port
backfills only; replace them with matching decompilation if/when it becomes
available.

Do not read the class-`0x1e` and class-`0x51` entries from stages 55–56 as
render meshes: their constructors and descriptors identify them as collision
objects. The default-start trace also finds class-`0x4d` objects, but their
resource pointer at `+0x380` is zero when they enter the world registration
callback. During a 10-second default-start run, the frame loop stayed alive,
but `vertices=0`, `image_tags=0`, `image_qwords=0`, `visible_text=0`, and
`font_array=0`; the world root still had no renderable children. Trace:
`/tmp/sly-native-class4d-objects.log` and `/tmp/sly-native-default-visual.log`.

The next investigation should follow the selected level's factory, reader, and
owner-list path to find where its object records are lost. Separately trace why
the font registry count and IDs are initialized while its font-array and
default-font pointers remain null. Verify fixes with nonzero GS vertices or GIF
image transfers and then a visible frame. The game is not playable yet.

**Class-`0x4d` lead reassessment (2026-11-07):** the startup trace shows these
objects owned by the global resource manager at `0x006abd50`, while the
explicitly selected level (`OPENRAC_BOOT_LEVEL=0x0100`, resource index 1,
record 0) has a separate world at `0x0088d530`. The class-`0x4d` objects have
no class-specific read records in the trace; their null `+0x380` fields do not
establish that the level mesh reader failed. Do not prioritize those objects
as the level's renderables without a source-to-render ownership trace.

Retail `func_0015F710` was another no-C stub in the list/activation path. The
native host now translates its exact NTSC-U instructions. This replaces a
no-op list traversal and removes its runtime warning; it changes neither the
decomp checkout nor its C. Bounded default-start and forced-level runs both
reach the frame loop, but still report only two base world-root objects, zero
root children, and zero renderer vertices. The forced-level trace is
`/tmp/sly-native-listfix-direct0100.log`; default startup is
`/tmp/sly-native-listfix-5f710.log`. Continue from the selected level's
factory/reader/owner-list path and establish where its expected object records
are lost before changing rendering. A follow-up audit also found that the
port-only `func_0011E680` callback-value array used a 16-byte C stride, while
retail advances 8 bytes per value and places its result immediately after 32
entries. The backfill now uses the retail 8-byte layout. The forced-level trace
did not enter a descriptor callback and still has zero children/vertices, so
this corrects a latent parser bug but does not explain the empty level.

Exact NTSC-U translation is now selected for `func_001B8540`, a reached VU
bounds routine; its no-C warning is gone, but the forced-level root and vertex
counts remain unchanged. Exact NTSC-U translation is also enabled for
`func_001B4688`, the resource-key table lookup called by `func_00139F80`.
This removes another no-op stub, but the forced-level root and vertex counts
remain unchanged. The current build reports 2,050 generated C functions, 148
retail-assembly translations, and zero stubs; the decomp
checkout remains clean and current at `e1b39f1`.

The latest bounded direct-level trace is
`/tmp/sly-native-direct0100-lookup-b4688.log`. It still has two base world-root
objects, zero root children, zero vertices/image tags/image qwords, and
`scene_gate=0` / `game_mode=0`. Retail inspection found that
`func_0011B008` is a 4-byte tag initializer: its US ELF instructions set only
the first word to `-1`, then return. The port backfill previously also zeroed
three following words, clobbering the payload and potentially adjacent stack
values. It now matches the retail write while remaining isolated under
`hand/native_c`. The rebuilt native launcher ran the selected level for 12
seconds; it still reports two base world-root objects, zero root children,
zero vertices and no image transfers. The existing font count/IDs are
unchanged from previous traces. Latest run:
`/tmp/sly-native-1b008-tag-only.log`.

With `OPENRAC_TRACE_FONT=1`, the exact object-list parser enters at cursor
`0x70001854` with 1,964 bytes remaining and returns at the ring boundary
`0x70000000` / remaining `-1`. Factory trace stages 23–24 bracket that parser;
the factories called between them prove it creates 112 objects: 110 type-0
records, one type `0x86` (key 2), and one type `0x04` (key 1). The type-`0x04`
object loads collision geometry; no visual class is in this record list. Earlier
startup also creates ten type-`0x1e` and forty type-`0x4d` collision objects,
plus two type-`0x51` objects. Trace:
`/tmp/sly-native-level-factory-tuples.log`.

The class-`0x86` constructor was a reached no-C stub. The port now selects exact
US retail assembly for `func_001EE900`, which calls `func_00182C18` and stores
the object at owner `+0x1ec4`. Its transform helpers `func_001EE9D0`,
`func_001EEA40`, and `func_001EEB60` are selected from the same retail assembly.
A bounded run confirms `special_scene_object=0x00a7e540` and root classes
`0x6d,0x86,0x04`, with the callback warnings gone; it still reports zero scene
vertices and game mode 0. Trace: `/tmp/sly-native-scene-callback-cluster.log`.

Next, find what resource or state transition should create the level's visual
objects; do not infer them from collision meshes or the separate property list
handled by `func_0011EC40`. The default Splash-to-title transition also remains
stuck at frame state 3/game mode 0. All functional port-only replacements stay
under `hand/native_c`; this change uses exact retail assembly and leaves the
sibling decomp checkout unchanged.

**Restart checkpoint (2026-11-07):** the current native run still reaches the
frame loop with a black scene and zero renderer vertices. The latest investigation
traces the resource-key table populated after loader stage 146 and walks the
selected level's owner lists for classes `0x04`, `0x51`, and `0x4d`. Retail
`func_001B4688` searches 40-byte table rows by signed key at `+2`; do not treat a
zero payload at `+0x24` as proof of a missing resource, since it depends on the
record type. The class `0x04`, `0x51`, and `0x4d` reader-backed descriptors parse
a mesh header at object `+0x380`: radius, vertex count/pointer, face
count/pointer, and link capacity/pointer are at `+0x380..+0x398`. The populated
class-`0x04` data is collision geometry, not evidence of a renderable mesh;
follow each descriptor's read method before interpreting these fields.

Committed native-only diagnostics in `port/sly1/main.cpp` and
`port/game/sly1-us/hand/native/sly1_native_worker.cpp` validate descriptor size
before reading mesh fields and dump resource-key rows. Builds use
`cmake --build build/port --target port_native -j2`. Trace logs remain local in
`/tmp`; the latest traces are listed below. All compatibility/backfill C
remains under `hand/native_c`; no changes were made to the sibling decomp
checkout.

**Owner-list correction (2026-11-07):** a bounded native trace now scans all
149 class slots on the directly loaded level world, rather than sampling only
classes `0x04`, `0x51`, and `0x4d`. Non-empty owner lists are classes `0x00`,
`0x1e`, `0x04`, `0x4d`, `0x51`, `0x6c`, `0x6d`, and `0x86`. The class-`0x04`
object does contain 10,110 vertices and 13,070 faces, but its descriptor
callbacks are `func_001B8540` and `func_001B9680`; the latter is the collision
previous-transform callback in the port-only `collider_mesh_transform.c`.
Treat that geometry as collision data, not the missing visual scene. No visual
object class appears in the world's owner lists, and the frame still reports
zero submitted vertices. Trace: `/tmp/sly-native-all-owner-classes.log`.
Next, trace the level resource's object records and deferred asset requests to
find why visual-class instances are never created or attached; independently
trace the default Splash-to-title transition, which also remains at frame state
3/game mode 0.

**Earlier restart checkpoint (2026-11-07; superseded by the follow-ups below):** exact US retail assembly
has been enabled for the reached value conversion/owner-list helpers
`func_001C1190`, `func_001C13E8`, `func_0011A080`, `func_0011A358`,
`func_0011A630`, and `func_0011BA28`. Their former C backfills remain in
`hand/native_c/resource_value_parser.c` behind build guards, but were removed
from `backfills.tsv`, preserving them as port-only reference code. This
selection builds, but has not yet had a fresh launcher run. The direct-level
probe still faults in `func_0015C8B8` after `func_001AE510` requests key 2 via
`func_0015C1C0`; a small isolated diagnostic backfill now traces that table
lookup without overriding the larger font/text source file. Build and run that
probe next, inspect the key-2 entry and fallback in `/tmp`, then trace the
writer of `D_00262268` if it is still `0xFC000120`. Latest confirmed blocker
remains no visible scene geometry: default boot is black, frame state 3,
game mode 0, zero renderer vertices. Rebuild with
`cmake --build build/port --target port_native -j2`; decomp checkout untouched.

**Animator allocation root cause (2026-11-07):** the key-2 lookup trace now
shows `D_00262268[2] == 0x110`, and the fault occurs when retail
`func_0015C8B8` dereferences that value as a trail object. The owner table is
populated by retail `func_001B2DF8`, which asks `func_001FD8A8` for its backing
array. That function had no C and ran as a zero-return stub, so its array base
was null and later entries became small offsets (`0x88`, `0x110`, ...). The
retail routine's live path simply returns `func_0018D758(size)`; a port-only
backfill now implements that delegation through the native heap adapter.
Rebuild and repeat the direct-level trace to verify the table contains mapped
guest addresses and the loader advances past the former `0xFC000120` fault.
The backfill is isolated in `hand/native_c/func_001FD8A8.c` for clean
replacement when upstream C is available. Trace evidence:
`/tmp/sly-native-key2-lookup-trace.log`.

The follow-up run verifies that allocation fix: table index 2 now holds
`0x00845230`, and the direct-level load advances past the previous trail
dereference fault. It then stops in the exact retail `func_00162890` path via
`func_00165AE8` / `func_0018D568`: a 0x90-byte copy is requested from
`0x3f800000`, the IEEE-754 bits for `1.0`, which is not a mapped guest address.
The copy backfill now has an opt-in trace for out-of-range source addresses,
and its matched function is explicitly overridden so that diagnostic actually
runs. Latest trace: `/tmp/sly-native-copy-source-trace.log`. Next trace the
`func_00162890` record/flag fields that lead to this `func_00165AE8` call and
find where the float is mistaken for a source pointer. Do not add a bounds
skip or suppress the copy: that would hide the data-layout or reader error.

The focused follow-up identifies the owner record as `0x00a7f77c`, entry
index `0xf0` (the 131st copy call). Its serialized flag is `0x203`, so the
low-bit branch is genuinely selected; its corresponding 0x28-byte record has
tag `0x0c`, then `0x3f800000` and other float fields. The preceding 130 entries
share valid pointer `0x00d14ef0`. At the decision point, the reader cursor has
jumped from `0x70000d8f` to `0x70001790` and the bytes there are recorded in
`/tmp/sly-native-animation-flag-trace.log`. This points to either a valid
type-`0x0c` record that needs distinct handling or a stream/record alignment
error at the ring refill. Compare the US retail field semantics and the
decompressed bytes around that refill before changing the branch or data.

**Restart checkpoint (2026-11-07):** follow-up traces rule out a direct
out-of-bounds table lookup. At the fault, `func_00137D10` reads signed index
`0xef` (239); the owning object reports `0x7aa` (1962) entries, and the row
address resolves inside the table at `0x00cf5020`. That row and its neighbors
all have tag `0x0c`; their `+4` words are float-looking values (`0x3f800000`
and `0x3f42a305`), not mapped guest pointers. The flagged owner record is the
240th record (`index 0xf0`, flags `0x203`), and the assembly selects the copy
branch. The reader cursor crosses the scratchpad ring refill between
`0x70000d8f` and `0x70001790`; inspect how the retail loader preserves record
boundaries and handles tag `0x0c` across that refill. Do not mask the copy
fault. Latest evidence is in `/tmp/sly-native-animation-bounds-trace.log`,
`/tmp/sly-native-animation-neighbor-trace.log`, and
`/tmp/sly-native-animation-flag-trace.log` (local runtime logs are not in Git).

Current state: the animator-table allocation backfill is in
`hand/native_c/func_001FD8A8.c`, registered in `backfills.tsv`; it is port-only
and can be replaced when the decomp provides that function. Native build has
advanced past the old small-offset animator pointer fault, but still exits on
the invalid source read at `0x3f800000`. There are no visible game graphics
yet. Continue with the `0x0c` record semantics and ring-refill alignment before
attempting a workaround. Build target: `cmake --build build/port --target
port_native -j2`. Work remains on branch `wip/native-sly-port`; decomp checkout
was not modified.

**Input-window comparison (2026-11-07):** the apparent cursor jump is not
proof of ring misalignment. The 44-byte snapshot at reader cursor
`0x70000d8f` exactly matches `build/port/extracted/decoded/resource_0100.bin`
at file offset `0x192d8f`; the snapshot at `0x70001790` matches the same file
at `0x193790`. Both file and guest cursor deltas are `0xa01`, so this is
consistent with consuming the intervening serialized payload. The raw bytes
are genuine extracted resource data, and the native reader reaches the
matching sequence. Stop investigating this as a ring-refill fault. Next
later row-lifecycle and renderer-write probes supersede the proposed ring
refill and owner-row checks below. Latest runtime trace remains
`/tmp/sly-native-animation-bounds-trace.log`.

**Row lifecycle and renderer-write trace (2026-11-07):** the
`0x13290`-byte table starts with row 239 zeroed; nested parsing stores a valid
auxiliary pointer (`0x00d18060`) at row `+4`. A fresh exact guest-store watch
then confirmed the later writes to row 239 `+0/+4`: `func_00162890` stores the
type `0x0c` and pointer `0x00d18060`, and `func_00164868` subsequently writes
float `1.0` at `0x00cf4ff8` and `0x00cf4ffc`. The latter store occurs after a
call to `openrac_ee_swc1` in the translated `func_00164868`; it is not an
inference from the helper's entry snapshot. At entry, its writer object
`0x0064c090` has `begin=cursor` and `end` in the native temporary range
`0xe041...`; the path that changes the eventual store destination still needs
to be traced within the routine/cursor helpers. Do not suppress the row copy
or infer the final cause from the entry snapshot alone.

`func_00164868` remains unmatched in sly1-decomp, so any eventual functional
replacement C must stay port-only and replaceable. The port backfill for
`func_0018D4B0` matches the now-available decomp C (16-byte aligned bump from
`D_002C2A14`, mirrored to `D_002C2A18` and `D_002C2A1C`), and the native temp
arena uses `0xe0410000–0xe0420000` for guest thread 1; neither explains the
table word. Next, instrument the exact stores to `0x00cf4ffc` and the
`func_00164868` output cursor within the same run, with the guest PC and
writer fields captured together. Reconcile the cursor path before changing
parser or renderer behavior. Evidence: earlier
`/tmp/sly-native-row-write-watch.log`; earlier writer snapshots in
`/tmp/sly-native-writer-fields2.log` and arena trace in
`/tmp/sly-native-temp-arena-overlap.log` (local logs are not tracked). The
native launcher still faults on source address `0x3f800000` and shows no game
graphics.

**Packet helper ABI follow-up (2026-11-07):** decomp C for `func_001EE5C8`
calls `func_001EE560` with only the writer argument, while the retail assembly
leaves `a1-a3` live as hidden inputs and uses them to construct a packet before
returning size/cursor data. Selecting exact assembly for both routines through
ordinary translated C produced a bad table value and an invalid source
address, consistent with the hidden-register ABI not surviving that call
boundary. A port-only C backfill for `func_001EE5C8` now explicitly forwards
all four arguments into selected exact retail assembly `func_001EE560`, then
copies and advances the writer cursor. Hostgen accepted and generated this
backfill after removing unsupported debug includes. A fresh run confirms the
backfill is called with plausible packet inputs and advances its temporary
writer cursor, but startup still faults later; the backfill does not by itself
fix graphics. Keep this C under `hand/native_c` and out of sly1-decomp.

**Object-reader follow-up (2026-11-07):** the direct exact guest-store trace
confirmed the row-239 writes described above. The new packet-helper ABI
backfill was then exercised with bounded argument traces: `func_001EE5C8`
passes type/count/flags to selected retail `func_001EE560`, and writer cursors
advance through `0xe041...` with sizes consistent with the retail size table.
The launcher still faulted at `0x6859E4DC`, so this repaired the call boundary
but not the malformed object-reader path. A warning immediately before that
fault showed `func_001E4C48` was still a no-op. Selecting its exact US assembly
removed that fault and advanced level parsing, but exposed a later invalid
list pointer at `0x40000004`.

`func_001289C0` had also been running the port reconstruction in
`hand/native_c/resource_object_common.c`; exact US assembly is now selected
instead, with the reconstruction retained behind
`SLY1_RETAIL_ASM_FUNC_001289C0`. A temporary trace of `func_001C1390` recorded
lookups with IDs `0x3842`, `0x0000000a`, and `0xffffa162`, returning
`0x301`, `0x9e9a54`, and null respectively. A separate store trace showed the
next call to `func_0011A338` had `list == 0` and would write to
`0x40000004`. The port now overrides that list helper to ignore a null list,
while keeping the decomp implementation unchanged. This gets past that crash;
the next clean run faults while traversing a linked-list node at
`0x08000000`, through `func_0011A4D8` from the object-reader path. Trace the
node-list owner and creation site next. No scene vertices or graphics are
visible.

Two existing port-only collision callback translations are now registered as
matched-C overrides in `hand/overrides.tsv`: `func_001B89B8` and
`func_001B7C40`. The retail C directly calls a nullable world method at vtable
offset `+0xd8`; the overrides preserve their other state updates and skip that
call when the method is absent. This removes one call-to-zero diagnostic and
lets the loader continue further. They are isolated
port C and do not alter sly1-decomp. The clean native build succeeds with 208
retail assembly translations and zero hostgen stubs. The window remains black,
with no visible scene vertices; next trace the `0x08000000` node through its
owner and creator before another graphics change.

## Restart handoff (2026-11-07)

Current branch: `wip/native-sly-port`; the original handoff checkpoint was
`310594e` and the latest pushed fix is `68424ac`. The working tree was clean
when this handoff was written. To resume, first inspect
`func_0011A6C0` and its caller `func_0011A4D8` in the decomp, then find where
the relevant keyed-tree global (candidate `D_00259F38`) is initialized and
populated. The last native run faults while `func_0011A4D8` follows a child
pointer to `0x08000000`; verify the tree and child pointers at the call site
before changing runtime behavior. Preserve any replacement C only in the port
repo's `hand/native_c` and register it through `hand/overrides.tsv` or
`hand/backfills.tsv`, as appropriate. Do not modify sly1-decomp.

After diagnosis, rebuild with
`cmake --build build/port --target port_native -j2` and run the native launcher
with a bounded timeout. The native run is authorized; do not use an emulator.
Update this handoff and commit/push a focused checkpoint after a confirmed
change. Current visible result remains a black window with zero scene vertices,
so the project is not yet at a graphics-ready or playable state.

**Tree-pointer and record-loop follow-up (2026-11-07):** the collision
constructor backfill at `func_001B7968` was present in the hand-backfill list,
but hostgen kept its matched decomp C because this function has a decomp
definition. It is now registered as a port-only matched-C override in
`hand/overrides.tsv`, with its source under `hand/overrides_c/` so hostgen does
not ingest it twice. A bounded direct-level trace shows the collider's
`+0x2c` tree field is `0x18`; its owner is `0x0088d530`, parent is
`0x010c7850`, and descriptor is `0x0021e180`. The tree walk then treated low
RAM as a recursive tree and faulted at `0x08000000`. The override now skips
the optional lookup when that field is outside mapped EE RAM. This is isolated
port behavior; the decomp source remains unchanged. The rebuilt launcher
passes that fault in a 15-second native run.

The next reached defect is in the exact retail `func_0011ECB8` record loop:
`func_0011E580` receives class `0x683e`, which is outside its valid descriptor
table (`0..0x94`) and returns null; retail assembly then calls the object's
vtable reader at `+0x38` without checking it. This indicates a malformed or
misaligned record stream before the null dispatch. Trace the reader cursor and
the record bytes at the first invalid class before adding a skip or changing
payload consumption. Latest run: `/tmp/sly-native-invalid-tree-guard.log`.
Build command: `cmake --build build/port --target port_native -j2` (succeeded;
201 generated C units, 208 selected retail assembly functions, zero stubs).
The window is still black and no graphics-ready state has been demonstrated.

**Object-record cursor probe (2026-11-07):** `func_0011ECB8` is now a
port-only C backfill selected in place of its retail assembly translation.
It follows the retail count, three 16-bit fields, factory call, virtual reader,
slot registration, and activation sequence, while opt-in tracing records the
reader cursor around each reader callback. On the direct `0x0100` probe, a
class-`0x53` reader at `func_00196410` advances the cursor from
`0x70000eb3` to `0x700016bf`; the next object record is class `0x683e` at
index 1 of 37. The 48-byte window at that point contains a mix of values and
float-looking data, not another valid class record. The first invalid factory
result still follows retail's no-op missing-call behavior so the trace can
show subsequent reads; this does not repair the stream. Later activation
faults at `0x1689c3e0` through `func_00183350` and
`func_00182ED8`, called from nested `func_0011ECB8` under
`func_00196410`/`func_001DCEA0`. Reproduced in
`/tmp/sly-native-reader-callback-sequence.log`; the byte capture is in that
same log. Reader-address tags show the same reader (`0x0064c4c0`) reaching a
class-`0x53` callback at cursor `0x70001341`, then a nested class-`0x53`
callback at `0x70000eb3`; the latter advances to `0x700016bf` immediately
before the invalid record. This narrows the next trace to how
`func_00196410`'s nested record path returns to its parent, but does not yet
identify the missing or misread payload. The build succeeds with 207 retail
assembly translations and zero stubs. No visible graphics are produced.

**Decomp remote check (2026-11-07):** the decomp checkout and
`origin/master` are still at `e1b39f1`; no new master commit is available.
The unmerged `origin/codex/us-small-functions` branch is at `8bf37ac` and
contains 51 additional matched definitions across six source files. The other
checked follow-up/wave branches have no changes ahead of master. None of the
51 additions defines the current reader/activation path (`func_0011ECB8`,
`func_00196410`, `func_00183350`, `func_00182ED8`, `func_001DCEA0`,
`func_001B7968`, or `func_001B7A98`), so there is no upstream C replacement
for this blocker yet. The decomp checkout remains untouched.

**Restart checkpoint (2026-11-07):** This is the current handoff and
supersedes the earlier `func_0011A6C0` tree-pointer handoff above. The working
tree was clean at checkpoint. Branch `wip/native-sly-port` was pushed through
`059a53e`. The `func_001B7968` tree-pointer fault is fixed by a port-only
override, and the record loop is now a port-only C backfill at
`port/game/sly1-us/hand/native_c/resource_object_record_loop.c`; neither change
touches the decomp repo. The next unresolved issue is the parent reader path:
`func_00196410` invokes nested record loading, then the shared reader cursor
appears to land inside float-like payload before `func_0011ECB8` reads class
`0x683e` at object index 1/37. Follow the nested calls and cursor ownership
through `func_00196410`, `func_0011ECB8`, `func_001DCEA0`, `func_00183350`, and
`func_00182ED8`; establish which call is expected to consume the bytes before
changing record handling. Relevant trace: `/tmp/sly-native-reader-callback-sequence.log`.
Latest build succeeded with 202 generated C units linked, 207 retail assembly
translations, and zero hostgen stubs. The bounded native direct-level run
still exits with SIGSEGV at guest `0x1689c3e0`; no graphics are visible and
the game is not playable. Decomp remains at `e1b39f1` on master; the 51 extra
definitions in `origin/codex/us-small-functions` do not cover this call path.
Resume by checking the trace and exact translated/decomp assembly for the
listed readers, then make one evidence-backed port-only correction, rebuild,
and run the native launcher with a bounded timeout. Native launcher runs are
authorized; do not use an emulator or edit `/home/reaper/Code/sly1-decomp`.

**Frame-list fault follow-up (2026-11-07):** Exact NTSC-U assembly for
`func_0011ECB8` calls `func_00182ED8` only when `func_0011ECA0(mode)` returns a
nonnegative slot. The port backfill had called it for every record, so the
activation call is now inside that condition. The same function now records
the start cursor and three decoded 16-bit fields for any invalid descriptor;
the previous byte window was post-read and could not prove what the factory
received.

The current bounded direct-level run fails earlier in the frame cleanup path,
at `func_00152600` called by `func_0015EFE8`. Its context list is
`0x00262340`, link offset `0x630`, and its head is node `0x0082f9b0` (class
`0x4d`, owner `0x006abd50`). A bounds-checked trace read found that node's
embedded links at `+0x630` are `next=0xbf343386` and
`previous=0xbef492b5`; the fault is while following this invalid list link at
`0xbef498e5`. These values look like payload/float words, but the first write
that corrupts them is not yet identified. Trace the node through resource
loading, `func_00139EC8`, the `func_001243E0` callbacks, and the transfer from
`D_002622E0` into the frame context before changing list semantics. The old
reader trace at `/tmp/sly-native-reader-callback-sequence.log` remains the
record-path evidence; the latest list trace is
`/tmp/sly-native-list-link-full.log`. Build succeeds (202 generated C units,
207 retail assembly translations, zero stubs). The window remains black and
the port is not playable. All code changes remain in this port repo; the
decomp repo is untouched.

**Restart checkpoint (2026-11-07, scene packet path):** The active branch is
`wip/native-sly-port`, clean and pushed at `3962c6c` (`translate retail scene
packet helpers`). The contradictory checkpoint above is historical; current
work has progressed past its reader and frame-list faults. The native launcher
now reaches the active world (`0x6abdd0`, state `3`, game mode `0`) without
fault for a bounded 15-second run. Frame-list inserts/removals are balanced
(40 each), all links are valid, and the frame worker sends the display
transfer packet. Exact US assembly has replaced several reached no-op
translations, including scene packet builder `func_001662A8` and helpers
`func_001EE3B8` / `func_001EE668`; these are selected retail functions, not
port backfills. The current build succeeds with 2,035 C translations, 220
assembly translations, zero stubs, and 202 generated C units linked.

Captured renderer frames remain fully black (one color, mean zero), the scene
builder emits zero vertices, and no visible text or playable state has been
reached. Latest bounded run: `/tmp/sly-native-scene-packet-helpers.log`; frame
captures: `/tmp/sly1-native-frame-00.ppm` and siblings. The next focused lead
is caller `func_00165728` in `src/main/00164000.cpp`: inspect its record count,
entry pointers, and indirect callback path at runtime, then determine why its
calls into `func_001662A8` produce no geometry. Decomp checkout remains at
`e1b39f1` on master; the 51 definitions on
`origin/codex/us-small-functions` do not cover the current scene path. Keep
functional replacements in the port's `hand/native_c/` and retain displaced
prototypes/backfills under `hand/reference_c/`. Native launcher runs are
authorized; do not use an emulator or modify
`/home/reaper/Code/sly1-decomp`.

**Direct-load SIF continuation (2026-11-07):** The first fresh
`OPENRAC_BOOT_LEVEL=0x0100` run reached resource sector 867214, then stalled
in `func_0011D078` on sound-service RPC 52 (`send=0`, `recv=12`). Retail
wrapper `func_0011E010` issues command `0x34` and discards its result. The
native SIF boundary now completes that request with the existing idle sentinel.
The next observed request was RPC 54 (`send=4`, word `1`, `recv=12`), issued
by void wrapper `func_0011E110`; it now receives the same idle completion.
This is limited to two sound commands with no consumed output. The successful
bounded run then reads sector 867214 and reaches the selected level's object
parser before faulting at guest address `0x08000000`. The symbolized path is
`func_001DCEA0 -> func_0011ECB8 -> func_00196410 -> func_0011A4D8`, called
from `func_001837E0` during owner registration. The next investigation is the
keyed-tree root and child slots read by `func_0011A4D8`; trace its pointer,
count, child array and list head, then identify the initializer that should
populate them. Build succeeded; direct-run log:
`/tmp/sly-native-sound-rpc34-36.log`. The selected-level parser does not
complete, no graphics are visible, and the game is not playable.

**Restart checkpoint (2026-11-07, direct-load scene entries):** The keyed-tree
fault was from a collider initializer reading `+0x2c = 0x18`; the existing
port-only `func_001B7968` guard rejects that non-pointer, so it is not the
current stop. A port-only `func_0011ECB8` diagnostic backfill now records each
record cursor and returns at the first impossible type instead of dispatching
through a null descriptor. In the direct `0x0100` run, it sees count `0x25`,
class `0x53` at cursor `0x70000ead`, and the next record becomes invalid type
`0x683e` at index 1/cursor `0x700016bf` after the class callback consumes
`0x80c` bytes. The malformed remainder is not yet explained. This guard lets
startup continue to the active world; it is diagnostic and does not establish
that all level objects loaded.

The port-only tracing wrapper for matched `func_00165728` follows the decomp
layout (entry stride `0x70`) and captures each list's entry count, nested
element count/pointer, and callback. The bounded 10-second run remains active
in world `0x0088d5b0`, frame state 3/game mode 0. The first lists have one
entry; later entries have valid nested pointers and some use callback
`0x001b4ad0`. Still, the renderer reports zero vertices, zero image tags,
zero image qwords, three accepted GIF chains, and the screen remains black
with no visible text. The immediate graphics lead remains the data passed by
`func_00165728` into retail `func_001662A8`, especially the nested count/pointer
at entry offsets `+0x58/+0x5c`, then the scene draw path that should submit
actual primitives. Latest log: `/tmp/sly-native-scene-entry-64.log`.

The font-trace helper now bounds-checks its stage-limit table and accepts the
additional trace IDs through 98. Latest build succeeds with 2,037 C
translations, 218 US assembly translations, zero stubs, and 202 generated C
units linked. The work is on branch `wip/native-sly-port`. All backfills and
diagnostic replacements are port-only; `/home/reaper/Code/sly1-decomp` is
untouched. This is still not playable; continue with the scene-data/render
submission path and preserve the default Splash-to-title transition as a
separate blocker.

**Scene element follow-up:** The group at `0x00847acc` has six entries. Its
first non-skipped entry (`0x00847ce0`) has three nested elements at
`0x00848070`; each element has zero at `+0x24` and `+0x28`, so it passes the
retail `func_001662A8` early-skip checks. Other entries are generally skipped
by the flag at `entry+0x28`. The group still produces no measured vertices.
The next trace should capture the full nested element records and all
`0x28`-byte subdata consumed by `func_001662A8`, then follow its generated GIF
packet to learn why it carries no geometry. Latest log:
`/tmp/sly-native-scene-nested-items.log`.

**Default-start object registration recheck (2026-11-07):** A fresh native
renderer trace found a stale hostgen configuration that disabled the port-only
`func_001240F8` body: `hostgen.json` still defined
`SLY1_RETAIL_ASM_FUNC_001240F8` although the function was no longer in
`asm.only`, and `backfills.tsv` registered its source under the unrelated
`func_00143AF8` name. The function is still missing matching C in the decomp,
so it remains a port-only C backfill. Removing the stale define and registering
it under `func_001240F8` rebuilt successfully (2,038 C functions, 218 assembly
functions, zero generated stubs). The 8-second default native run no longer
warns that `func_001240F8` is missing and now populates world update lists with
13/13/11 entries; the earlier trace had those lists empty. This corrects the
object-update path but does not make the screen visible.

The same run reaches frame state 3 with game mode 0 and an advancing menu
animation. Renderer diagnostics decode a recurring 240-vertex draw with no
texture and black RGBA `(0,0,0,128)`; the captured 960x720 native frame is all
black. The UI trace reports 37 objects, 30 font records, and zero visible
text. Inspection of the current decomp identifies this packet as the intended
screen clear from `func_00168770`: it emits 40 quads across the 640x224 screen
with the supplied color. The renderer is displaying that clear correctly;
debug the missing content that should follow it (Splash/movie or title UI), not
this black packet. Latest evidence: `/tmp/sly-native-fixed-240f8.log`,
`/tmp/sly-native-default-render.log`, and `/tmp/sly1-native-frame-07.ppm`.
The port remains unplayable; no emulation was used.

A follow-up 20-second default native run reaches 9.48 seconds of guest menu
time without a state transition: `game_mode` stays 0, `load_request` stays 0,
and `scene_gate` stays 0. The same intentional black clear repeats, but no
Splash/movie texture or visible title text follows it. The active thread set
remains the main loop, display worker, and UI worker; the frame worker does not
stall. Log: `/tmp/sly-native-default-20s.log`. Trace the Splash/movie queue and
title UI activation; extending the wait does not reveal the title screen.

**GS SIGNAL handoff (2026-11-07):** The display worker was waiting on the GS
signal callback `func_00168698`. An unfinished port experiment duplicated the
retail interrupt syscalls; inspection found those already handled by the
runtime kernel. The native renderer now sets the GS CSR.SIGNAL latch and raises
INTC cause 0 through the existing guest interrupt registry after an accepted,
synchronous GIF submission. The callback runs and `kernel_service=0x43`
confirms it signals the display semaphore. The launcher builds and runs, but
the screen remains on the intentional black clear: at 5.46 seconds,
`game_mode=0`, `load_request=0`, and `visible_text=0`. The next blocker is title
or Splash content activation, not this worker semaphore. Evidence:
`/tmp/sly1-native-gs-csr.log`. This runtime signal model is port infrastructure;
`func_00168698` remains an isolated port-only C backfill so matching decomp C
can replace it later.

**Default Splash/title path checkpoint (2026-11-07):** Added opt-in
`OPENRAC_TRACE_MENU_PATH=1` entry tracing for the title-page population,
retail menu draw, and text-layout/draw functions, plus state-data fields to the
startup snapshot. A 10-second native run reports no entries into any of those
menu/text functions. The game remains in frame state 3 with `game_mode=0`,
`load_request=0`, no visible text, and pad state-data mode 3; the screen clear
continues to be the only visible output. Selecting exact retail assembly for
`func_0016E118` (a missing decomp C routine that clears transition bits and
calls `func_0016E148`) builds successfully but does not change that path. Logs:
`/tmp/sly-native-menu-path.log`, `/tmp/sly-native-menu-after-e118.log`.

The port-only `func_001E4EA8` contains a development shortcut for
`OPENRAC_BOOT_LEVEL` and explicitly notes that it bypasses the movie/title
systems; this makes the default movie-to-title startup chain the next concrete
lead. Trace who requests the opening PSS/movie and whether the native host's
`openrac_game_play_movie` hook is reached, then inspect any unavailable retail
functions in the decomp before adding isolated port backfills. Keep all such
replacements in `hand/native_c` so later matching decomp code can replace them.
The diagnostics and `func_0016E118` assembly selection were committed and
pushed as `7f87c76`; no decomp repository files were changed. This remains a
black screen and is not playable.

**Startup queue trace correction (2026-11-07):** The previous checkpoint
incorrectly called `func_0018E5E8` a movie sink. Retail assembly shows that it
initializes the display worker, whose entry is `func_0018E558`; the trace
confirms both run. A new `OPENRAC_TRACE_BOOT_QUEUE=1` probe watches those
functions and the generic resource-request path (`func_0018F148`,
`func_0018F198`, `func_0018F448`, `func_0018F7D0`, and the linked reader
helpers). Over a 10-second default run, only the display-worker initializer
and worker entry are reached from that set; Splash is still the only reported
sector read (820372, 150849 bytes). This shows the generic queue is not where
the missing intro request is currently visible; it does not prove there is no
separate PSS path. The shared `openrac_game_play_movie` implementation is in
OpenRAC's common program layer, but the Sly launcher does not currently link
that layer or expose a Sly game-code bridge to it. Next identify the retail
startup function/state that should request the intro, then connect its actual
sector/size/channel inputs to the shared movie player if the call path supports
it. Trace: `/tmp/sly-native-boot-queue.log`. No `sly1-decomp` files were
changed. The native window remains black and the game is not playable.

**Producer-side queue trace (2026-11-07):** The probe now also watches the
resource enqueue functions `func_0018F0E8`, `func_0018E4C0`, and
`func_0018E4F0`, plus observed caller `func_0012D5D0`. None ran in the default
10-second launch; only the display-worker setup/entry ran, and only the Splash
sector was read. Assembly shows `D_00269A00` is the linked resource-reader
queue consumed by `func_0018F148`; its idleness is not evidence that a PSS
movie request is absent. The actual US ELF contains the strings
`PsIIlibmpeg 2420` and `PsIIlibipu 2400`, while the decomp source has no named
movie caller. The next lead is to map those import stubs and their reachable
call sites, then route the retail movie sector/size/channel to OpenRAC's native
PSS player through a port-only bridge. Trace:
`/tmp/sly-native-boot-queue-producers.log`. The decomp checkout remains at
`e1b39f1` and is untouched; the native screen is still black.

**Restart handoff (2026-11-07):** The current working tree was clean at
`786e811` (`wip/native-sly-port`), and that commit was already pushed to
`origin`. No launcher or build process was active. A read-only inspection of
the extracted US ELF confirmed it has a single main `PT_LOAD`, ordinary
`.text`/`.data` plus `.DVP.overlay.*` sections, and the strings
`PsIIlibmpeg 2420` / `PsIIlibipu 2400` in its data. The DVP overlay sections
are not yet established as game-mode overlays. OpenRAC's Rac1 bridge reaches
its shared native PSS player through `openrac_game_play_movie`, but the Sly
target does not link that shared program layer or yet identify the retail
movie call site. Continue by decoding the PS2 EE import-stub/library records
in the Sly ELF and cross-referencing MPEG/IPU stubs against retail assembly;
then determine whether the matching call path provides the PSS sector, size,
and channel needed by the native movie player. Keep any substitute function
in `hand/native_c` until matching decomp C exists. The decomp checkout remains
untouched. `/tmp/sly-native-boot-queue-producers.log` is transient evidence and
may not survive a system restart; the findings needed to resume are recorded
here. The game still displays only a black clear and is not playable.

**Async startup worker checkpoint (2026-11-07):** Port-only C now initializes
the missing startup async objects and starts their retail worker thread
(`func_0018B5C0`, `func_0018BD58`, `func_0018B670`, `func_0018B600`). The native
kernel bridge includes the retail `CreateThread` and `iSignalSema` services.
The native build completed with 2,043 translated functions, 218 assembly
functions, and zero generated stubs. A 15-second bounded launch now shows guest
worker threads 14 and 15 entering `func_0018D0A0`; before this change no worker
was active. That callback has no matching C and currently resolves to a no-op,
so this is evidence of reaching the queue callback, not evidence that startup
content is being processed. The run still reads only the Splash resource and
reports frame state 3, `game_mode=0`, no load request, no scene gate,
and no visible text. The display remains the intentional black clear.

Next inspect retail `func_0018D0A0` and its caller `func_0018C028` to establish
the callback's contract and dependencies. If it is necessary for startup and
matching C is still unavailable, implement a retail-derived replacement only
under `hand/native_c/` and register it in `backfills.tsv`. The recent changes
are isolated port backfills and do not alter `sly1-decomp`. Last native log:
`/tmp/sly-native-startup-worker-active.log` (transient; findings are recorded
here). The game remains unplayable.

**Async callback assembly follow-up (2026-11-07):** The previous checkpoint
identified `func_0018D0A0` as a generated no-op. Hostgen now translates exact
NTSC-U retail assembly for that callback and the required helpers
`func_0018BD00`, `func_00209CB8`, and `func_001F6B70` (`PollSema`). The native
build reports 2,043 translated functions, 221 assembly translations, and zero
stubs. A 15-second native run no longer reports `func_0018D0A0` as missing and
shows auxiliary threads 14 and 15 at `func_001F6B70`. This confirms the worker
path is running into its semaphore-poll path; it does not point to a movie or
scene request. Splash is still the only resource read, `game_mode=0`,
`load_request=0`, no scene gate, and `visible_text=0`. The window
continues to display the intentional black clear. Evidence:
`/tmp/sly-native-callback-asm.log` (transient).

Do not spend the next graphics pass extending this generic callback path. The
current visual lead remains the renderable-object and scene packet path: trace
the first valid mesh-owning object through activation and identify why the
render submission contains no scene geometry. Separately, normal startup still
does not request title/movie content. All assembly selections are in the port
hostgen configuration; `sly1-decomp` remains untouched.

**Selected-level render-record trace checkpoint (2026-11-07):** Expanded the
opt-in render-list trace across all five direct-level groups and records packet
builder outputs before and after `func_001662A8`. Build succeeds: 2,043
translated C functions, 221 exact assembly translations, zero stubs; 204
generated C units compile and link. A 12-second native direct-level launch
reaches groups (`0x00fe903c`, `0x00fe942c`, `0x00fe981c`, `0x00fe9c0c`,
`0x00fe9ffc`) with primary-record counts 1, 1, 1, 1, and 6. Each group has
one outer entry; its `+0x28` word is 1. Retail `func_001662A8` returns on that
nonzero field before packet generation, leaving all five packet output counts
zero. The final group has six primary records, not six outer entries.

A native GDB watchpoint on the first entry captured the write at guest address
`0x00fe9108`. The call stack runs through retail `func_00162890` and
`func_001289C0` during `func_0011ECB8` resource-object parsing. Assembly shows
`func_00162890` copies the first byte returned by `func_00137B90` into entry
`+0x28`; that byte is 1. This is parsed game data, not an unexplained host
write, so preserve the retail skip. Next locate resource object kinds that
produce renderable records or move onto the normal title/gameplay transition;
the direct-level shortcut has not yielded a playable scene. Logs:
`/tmp/sly-native-direct0100-render-groups-final.log`,
`/tmp/sly-watch-entry-flag-detail.log`.

**State-manager and MPEG-path correction (2026-11-07):** The extracted ELF
has no `.lib.stub` or import-table section. The strings
`PsIIlibmpeg 2420` / `PsIIlibipu 2400` at file offsets `0x17bf70` and
`0x17c0b0` are adjacent to codec data, not import records. Sly's own US retail
assembly contains the MPEG player setup path: `func_0018F448` calls
`func_0020ADA0` and `func_0020AE40`; it is reached from request processing in
`func_0018F198`, called by the queue drain `func_0018F148`. The frame loop
drains that queue, but the trace saw no enqueue, request processing, or MPEG
setup after the Splash read. The missing event is upstream of the decoder.

A 15-second native trace now records fields from the app state object. State
3 remains active while its timestamp stays 0 and the app clock advances past
6.9 seconds. Retail state's case 3 waits on controller/input flags; it does
not transition just because time passes. The input report is in mode/state
3/4, with no buttons or transition flags set. That explains why the app stays
in state 3 without a media request, but does not establish whether the idle
state should already draw a title/attract screen. The renderer still submits
only the intentional black clear and has zero visible text. Trace:
`/tmp/sly-native-state-manager-trace.log`. The hostgen build completed with
205 generated C units linked. Next determine the retail input-to-title/media
transition and the expected idle display; keep all functional C backfills
isolated from `sly1-decomp`.

**UI callback and transient vertex trace (2026-11-07):** An opt-in
`OPENRAC_TRACE_UI_CALLBACKS=1` snapshot now inventories all 37 initialized UI
objects in `port/sly1/main.cpp`. At the captured startup point, 30 fonts are
loaded and four font registry entries exist, but `visible_text=0`; all UI
object modes are 0 and menu index 25 has page `-1`. Text-object update callback
`func_001AB058` and callbacks `func_001AE178` / `func_001E00B0` appear in the
inventory and remain no-C warnings where noted. Trace their retail activation
path before selecting more code or changing state. The same run briefly
reports 240 vertices (117 classified visible) across bounds roughly 641x225,
then zero vertices on later snapshots. The actual window stayed black, so
this is not confirmed game artwork. Trace the producer, framebuffer and
presentation path. Log: `/tmp/sly-native-ui-callbacks-ready.log` (transient).
See `RESTART_CHECKPOINT.md` for the restart sequence and detailed readings.

**Canonical decomp recheck (superseded by `9eb87599`, 2026-11-11):** The configured checkout at
`/home/reaper/Code/sly1-decomp` remains clean at `e1b39f1`, and its
`ReaperOfVeriod` remote had no new commits. Canonical
`OpenThievius/sly1-decomp` is at `cf97578`; the temporary read-only checkout is
`/tmp/sly1-decomp-latest`. It adds `src/main/movie_player.cpp` with the Sly
movie pipeline (`func_0018F198`, `func_0018F448`, and decode callbacks),
`RunMainLoop` movie-queue draining plus `CreateMovieDecodeThread` in
`src/main/main_loop.cpp`, and a message type 7 producer in
`HandleStepSequenceMessage` (`src/main/file_14EE40.cpp`) that calls
`QueueMovie`. The port still builds against the older configured checkout,
so this C is not yet available to hostgen. On resume, sync that read-only
input to canonical `cf97578`, then trace why the startup step sequence does
not queue the opening movie after Splash. Do not change the decomp project or
fake a movie queue event. The old paused-state note is superseded; port work is
active on `wip/native-sly-port`.

**Earlier canonical source integration and level parser (superseded by `9eb87599`, 2026-11-11):** Canonical
master is now `2e00ac7001cb5016613f8dbc24e1c91a865263e4` (PR #109). The port's
isolated source staging reads that snapshot and resolves semantic names through
`include/names.h`; the upstream checkout remains read-only. The clean native
build compiles and links 257 generated C units: 700 C translations, 242
assembly translations, two C stubs, and 3,951 functions still without C.

The latest native direct-level probe completes Splash, selects key `0x0100`,
and reads sector 867214 (4,911,835 bytes). It then reaches an invalid
1,645,753,056-byte allocation in the object parser, calls code address
`0x5f5f`, and faults at guest address `0xFFFFA9E3`. The current output has zero
vertices and no visible text. Trace:
`/tmp/sly-canonical-direct-level-queued.log`.

Recent port-only fixes include canonical-name aliasing, standalone staging for
unmatched override bodies, a state graph loader backfill that guards absent
class stream callbacks, and list pointer/tail checks. The next step is to trace
the stream field read by exact retail `func_001289C0` and find which preceding
loader path misaligns it. Keep any temporary C in the port and name the retail
function it replaces so later decomp C can supersede it. See
`RESTART_CHECKPOINT.md` for the full configuration and native launch command.

## Black-window UI and input trace (2026-11-11)

The screenshot's solid black output is still a failure state, not expected
gameplay. Normal startup reaches frame state 3 with an active world, 37 global
UI objects, and a populated font registry, but remains at `visible_text=0`,
`vertices=0`, `image_tags=0`, and `gif_chain_submits=0`.

The first backfill trace attempt was invalid: hostgen rewrote the proposed
runtime-wrapper call to a generated no-C game stub. The corrected tracing uses
the launcher's existing `game_sly1_native_trace_font` hook. A fresh run shows
five calls to text-buffer setup `func_0015DB28`, but no calls to text layout
draw `func_0015E318`, glyph emission `func_0015C638`, or screen-element draw
callbacks `func_001AA790` / `func_001AB198`. The frame update and draw
dispatchers (`func_001E9668`, `func_001AA110`, `func_001E9648`,
`func_001AA178`) do run repeatedly. The static UI list's first object pointers
and their initialized vtables match the retail data, so the earlier apparent
bad object values were a trace-stage collision with the graphics worker, not
corrupt UI pointers. No active object draw method is dispatched, and the menu
draw function `func_00194F80` is not reached.

The user's Enter presses were captured by the native pad bridge as Start
(`held=0x0008`), confirming keyboard input reaches the guest. Retail
`UpdateElementStack` shows why they did not change frame state 3: this state
consumes R1 (`0x0800`) for the pause layer and L2 (`0x0100`) for an available
map, not Start. The keyboard bridge maps Q to R1 and `1` to L2. Current trace
still has no queued movie, world load, text draw, UI draw callback, or graphics
submission after startup reaches the main loop. The opt-in boot-transition
trace at `/tmp/sly-wave32-boot-transition-12s-20261112.log` confirms
`RunResetGameSession` queues and services the first Splash fade/world request
(key `0x247ab0`, resource sector 820372), so the retail boot request is not
missing. After Splash loads, no further world or movie request occurs; all 37
screen elements remain inactive and graphics/text uploads remain absent. The
Splash world has no class-`0x65` StepSequence objects, so no sequence can queue
movie message type 7 through `HandleStepSequenceMessage`. The class-`0x57`
CountdownNotifier is state 0 with a null `+0x338` gate pointer;
`CountDownNotifierTimer` returns while that pointer is null. Its state graph
stays on the `0x261` self-edge. These are observed inactive Splash contents,
not evidence of a missing transition request. Next trace how the loaded
Splash objects and graph are expected to activate, and why object render
traversal skips drawable records. Do not force a menu/movie or change vanilla
state. Keep functional backfills in `hand/native_c/` separate from matching
decomp C.

## VIF startup-buffer trace (2026-11-11)

The opt-in DMA trace records VIF0 chain headers and the first sixteen payload
words. Startup submits one VIF0 CNT tag at `0x00211e10` with 46 QWC, followed
by an END tag at `0x00212100`; later VIF0 calls point to empty CNT/END tags at
`0xe041efe0`. VIF1's first TADR is `0x00212110`, immediately after the shared
buffer's VIF0 END tag. A raw ELF check identified `0x00211e10` as the start of
the `.vutext` section (21,840 bytes of original VU microcode); its first DMA
tag requests 46 QWC and carries VIF code `0x4a5a0000`. The native submitter
still drops channel `0x10008000`, so this VU0 upload is not executed.

The first VIF1 transfer starts at `0x00212110`. Its caller sets CHCR TTE
(`0x40`), but the native backfill overwrote CHCR with `0x105`, clearing TTE.
That made the interpreter read VU instructions as commands and reject
`0x400002ff`. A port-only fix now preserves TTE. The first transfer is then
64 bytes with one MPG and one MSCAL, and the next large transfer parses eight
MPGs and 736 DIRECT bytes without a malformed-stream warning. Logs:
`/tmp/sly-vif0-data-trace-20261111.log`,
`/tmp/sly-vif1-failure-context-20261111.log`, and
`/tmp/sly-vif1-tte-startup-8s-20261111.log`. The VIF0 trace is opt-in under
`OPENRAC_TRACE_DMA_CALL=1` and inspects at most eight chains. The VU0 path is
still a separate gap; use native C for any reached VU0 caller rather than
expanding a VU hardware emulator.

The VIF1 fix advances setup but does not change the visible result: the
8-second trace reaches the Splash world and global element-stack state 3, yet
still reports zero vertices, image tags, image qwords, and visible text. No
movie is queued, and all 37 UI objects remain inactive. Continue from the
missing normal Splash/title activation and the invalid 2,128-byte UI draw
packet. Do not force a menu or movie. The window remains black; no graphics
or playable state has been reached.

## Upstream decomp refresh (2026-11-11)

Fetched canonical `OpenThievius/sly1-decomp` master at `674b40e` (PR #122,
Wave 29) into the temporary read-only snapshot
`/tmp/sly1-decomp-674b40e-source`; the sibling working checkout remains
untouched. Wave 29 splits aggregate US source into named files and adds PAL
version tracking and data. `ReadPlacedObjectModel` is unchanged from `782e2899`,
so this update does not explain or fix the forced-level strip count. Hostgen
now translates 789 C functions, 501 US assembly functions, 2 stubs, and 3,635
without C across 276 units (up from 734 C functions across 256 units). Report
comparison shows 49 functions that were previously without C now translate
from C, largely MPEG/IPU decoder code. That is useful for future FMV playback,
but the normal Splash trace has `movie_queued=0`, so those decoders are not yet
on the observed black-screen path. The US assembly directory is absent from
the new source archive, so the native build retains
`/tmp/sly1-decomp-wave24-asm` for exact retail assembly. The rebuilt Wave 29
launcher still reaches the Splash world in element-stack state 3 but
shows a black window, 37 inactive UI objects, no visible text, no vertices,
and no image transfers; accepted GIF-chain submissions alone have not produced
visible art. Latest run: `/tmp/sly-wave29-normal-15s-20261111.log`.

The VIF0 chain trace's start address `0x00211e10` is the `.vutext` base; the
port still drops that channel. Follow only if the reachable native path needs
VU0 behavior; implement the specific function in native C rather than adding
a VU emulator.

**Post-fix progression check (2026-11-11):** A 20-second bounded native run
reached 9.5 guest seconds after crossing the CountdownNotifier's configured
4–6 second interval. The world stayed active in element-stack state 3; no
movie was queued, all 37 UI objects remained inactive, `visible_text=0`, and
image tags/qwords remained zero. The renderer's vertex counter fluctuated
between 6 and 48, without producing visible art. This confirms the TTE fix
repairs VIF startup parsing but does not unblock the Splash/title transition.
Log: `/tmp/sly-vif1-tte-progression-20s-20261111.log`.

## Forced-level model-reader failure (2026-11-11)

A bounded native run with `OPENRAC_BOOT_LEVEL=0x0100` localizes the fatal heap
growth to retail model parsing. `func_00162890` reads a valid top-level part
count of 1,962, then at **part index 512** reads `nstrips=0xFF02` (65,282) from
reader cursor `0x7000011a` with 7,910 bytes remaining. The previous estimate
of part 1,093 was not validated and is superseded by the call-site trace. The
record's preceding vector, float, signed short, and three byte fields consume
the expected 21 bytes, so the bad value is at the expected strip-count field.
That count requests
5,222,560 bytes for the `0x50` strip records and 3,133,536 bytes for the
`0x30` packet records. Repeated allocations eventually exhaust the configured
32 MiB guest heap and fault at `0x02000000`.

The 128-byte live scratchpad window at offsets `0x100..0x17f` matches the
existing decoded level resource byte-for-byte at offset 1,867,947, including
the `02 ff` count bytes. This shows the count is in the stream bytes the native
reader sees and in the extracted decoded file; it does not prove that the
extraction decoder and live refill path are both correct relative to retail.
The latest decomp's `ReadPlacedObjectModel` and the translated retail ASM both
read this field as little-endian U16, so changing its width or clamping it
would hide the failure and desynchronize later records.

Opt-in tracing is now configured in hostgen and the port worker:
`OPENRAC_TRACE_MODEL_FIELDS=1` logs unusually large U16/S16 values and every
oversized strip count, with part index, stream cursor, and remaining bytes;
`OPENRAC_DUMP_MODEL_ENTRY=1` saves the initial ring. A forced-level rerun
reproduces part 512 and saves the live ring as
`/tmp/sly1-model-strip-part-512.bin`. Logs:
`/tmp/sly-model-strip-index-20261111.log`,
`/tmp/sly-model-strip-capture-20261111.log`, and
`/tmp/sly-model-large-u16-20261111.log`, plus the final filtered rerun
`/tmp/sly-model-neighbor-counts-20261111b.log`.

Do not raise the heap limit, clamp the strip count, or enable the partial
port-only `func_00162890` in `hand/native_c/resource_object_common.c`; it does
not consume the strip and morph payloads. Next compare the active native
buffer refill/decompression path and the level resource boundaries against
the exact stream position around part 512, then confirm against a known-good
retail parse. Until that is resolved, the forced-level black-screen path is
not a successful load and normal startup remains visually blocked.

**VIF packet-writer recovery (2026-11-11):** A forced `0x0100` native load
reached the level's mesh packet builder and exposed three reachable no-op
stubs in the VIF writer: `func_001EE4C0` (`WriteVifStrow`), `func_001EE528`
(`WriteVifStmask`), and `func_001EE718` (`WriteVifUnpack`). The helper
`func_001EE560` (`WriteVifUnpackHeader`) was also still a no-op. Exact US
retail assembly for all four exists in the staged assembly source, so hostgen
now selects it via `asm.only`; this is retail assembly, not a functional C
backfill. Build coverage is now 734 C translations, 501 assembly translations,
2 stubs, and 3,674 functions without C.

The no-op warnings disappear after this change, and the normal Splash launch
remains alive through 15 seconds. It still stays at state 3 with all 37 UI
elements inactive and `visible_text=0`; the sampled 240-vertex UI packet has
black/transparent RGBA, no image tags, and no visible artwork. The forced-level
probe still fails while building a lit-strip mesh packet: the guest heap
reaches its 32 MiB boundary with repeated 15,904-byte allocation requests,
then faults at `0x02000000`. Its last guest frames pass through
`func_001EE5C8` / `func_00164868` / `func_00162890` / `func_001249A8` /
`func_001BAC30`. This is progress past missing VIF writer helpers, but it does
not identify whether the allocation pressure comes from stream alignment or
mesh data; do not increase guest RAM or treat the load as successful. Logs:
`/tmp/sly-nap-direct0100-vif-writers-20261111.log` and
`/tmp/sly-nap-normal-vif-writers-15s-20261111.log`.

The active CMake cache now points to `/tmp/sly1-decomp-674b40e-source`, not
the older Wave 27 snapshot. Hostgen reports 789 C and 501 assembly translations
across 276 units. The upstream source includes `LoadCollider` for
`func_001BAC30`, but current hostgen reports that US function as `no-C`; the
port-only C replacement remains registered in `hand/native_c/` and the
unmodified decomp checkout remains read-only. Rebuilding after selecting the
four VIF writer helpers and switching to the latest source did not clear the
forced-level heap fault or the black normal Splash output. The latest Wave 29
normal run still shows no visible graphics, and a forced run reproduces the
part 512 count exactly. The 4,096 captured
VIF reservation samples stayed within their writer buffers, so the current
heap trace points to repeated mesh-data allocations rather than a cursor
crossing a packet boundary. Latest logs:
`/tmp/sly-wave29-direct0100-20261111.log`,
`/tmp/sly-wave29-normal-15s-20261111.log`, and
`/tmp/sly-nap-direct0100-vif-cursor-20261111.log`.

**Splash draw dispatch checkpoint (2026-11-12):** Rechecked the latest staged
decomp snapshot, Wave 32 (`86682dd`), and rebuilt hostgen output: 798 translated
functions, 522 retail-assembly functions, and 2 stubs. The recent decomp updates improve names and matched source
coverage, but add no implementation for the unresolved Splash activation path.
The canonical sibling checkout was left untouched.

A fresh 15-second native run with
`OPENRAC_TRACE_RENDER_CALLBACKS=1 OPENRAC_TRACE_FONT=1
OPENRAC_TRACE_OBJECTS=1 OPENRAC_TRACE_BOOT_TRANSITIONS=1` confirms that the
frame render dispatch is running: the Splash world repeatedly reaches
`DrawCollider` / placed-object draw for objects `0x847dc0` (class `0x53`) and
`0x863ca0` (class `0x7d`). Both have zero model parts. The world also contains
many class-2 placed objects with one linked record apiece, but the populated
class-4 model owner (`0x847870`) has an empty model header in Splash. This
separates the current black screen from a stalled frame loop: draw dispatch is
active, while the active Splash content does not provide visible geometry or
activated UI. The separate forced-level probe's 1,962-part class-4 owner still
does not reach its draw callback and still fails during mesh packet creation;
do not conflate that forced-level result with the normal Splash world.

This does not overturn the transition-path evidence above: normal startup
still queues and loads only the Splash world, the CountdownNotifier timer gate
is null, and no movie or subsequent world request is observed. Next, trace why
the Splash graph/sequence and screen elements remain inactive, and separately
trace why forced-level model owners fail before draw. Do not force a title,
movie, or gameplay state. The frame-list wrappers are port-only and remain
isolated; this run did not establish a semantic mismatch in them. Log:
`/tmp/sly-render-trace-15s-20261112.log`.
