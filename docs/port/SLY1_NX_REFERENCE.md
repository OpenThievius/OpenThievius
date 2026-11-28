<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Sly1-nx port reference

This is a practical reference for applying SlyC00per's `sly1-nx` work to the
OpenThievius native PC port. It records which ideas fit our renderer, where to
look upstream, and what to investigate next. It does not change matching
sources in `sly1-decomp`.

## Upstream snapshot and permission

Reviewed on 2026-11-27 at `sly1-nx` commit
[`2a7524a`](https://github.com/SlyC00per/sly1-nx/commit/2a7524a47c6fdbf1584469c7d33aaa7731bed17c).
Its decompilation submodule is `sly1-decomp` commit
[`91de6fc`](https://github.com/OpenThievius/sly1-decomp/tree/91de6fc6364ea13b3cbe5d530e8857272d23aada).
The upstream README reports all 46 levels running on Switch; its newer iOS
port is running but has less playtesting. The author is also an OpenThievius
maintainer, and permission to reuse this project's code and research has been
confirmed for this port. The project is AGPL-3.0-or-later.

The two ports have different rendering backends. sly1-nx models Sly's VIF1,
VU1 and GS flow and sends the resulting draws to Switch deko3d or Apple Metal.
OpenThievius targets host-native services and a native OpenGL renderer. We
can reuse Sly-specific findings and algorithms where they help, but the
OpenThievius display path stays native OpenGL; do not add the VU1/GS pipeline
as the gameplay renderer.

## Best matches for our current blockers

Our current native OpenGL evidence is a malformed Paris roof/building scene
and a broken player pose. These are separate problems and should be traced
separately. Keep port fixes in `port/`; when a matching decomp function is
missing, put its functional replacement in the existing port-only backfill
area and replace it with matching source when available.

| Priority | sly1-nx reference | Apply it here |
|---|---|---|
| 1. Player T-pose | [`docs/RENDERER.md`](https://github.com/SlyC00per/sly1-nx/blob/main/docs/RENDERER.md), especially the skinning and skinned-normal work; [`port/render/vu1_handlers.cpp`](https://github.com/SlyC00per/sly1-nx/blob/main/port/render/vu1_handlers.cpp) | Trace one player part from its pose/bone palette and weights to the vertices submitted to OpenGL. Determine whether the current native adapter receives skinned positions or must apply the weights. Compare its bone and matrix convention to the documented sly1-nx operation. This directly targets the T-pose without adopting VU1 as our renderer. |
| 2. Large roof artifact | [`docs/RENDERER.md`](https://github.com/SlyC00per/sly1-nx/blob/main/docs/RENDERER.md), destination alpha test, framebuffer alpha, and small render targets | For the roof source already identified in `SLY_GRAPHICS_HANDOFF.md`, record the draw's source, material, texture, alpha/depth state and whether it samples a render target. Check whether the target was produced and contains the expected image before changing source topology or projection. |
| 3. Missing scene surfaces | [`docs/RENDERER.md`](https://github.com/SlyC00per/sly1-nx/blob/main/docs/RENDERER.md), camera area-mask timing | Capture camera position, area mask, queued requests and visible draw IDs at the same frame. sly1-nx documents a one-frame lag where the area is chosen before the camera moves; account for this when comparing moving views. |
| 4. Renderer coverage | [`docs/DESIGN.md`](https://github.com/SlyC00per/sly1-nx/blob/main/docs/DESIGN.md), draw-path survey | Compare Sly's subsystems against what our native OpenGL path actually submits: static models, lit/unlit batches, skinning, morphs, particles/effects, trails/lines, 2D/font, fades, texture uploads and render targets. Record absent categories before adding mesh-specific fixes. |

### Graphics findings to keep in mind

- The destination-alpha bit is used as a mask by dark volumes and shadows.
  sly1-nx found both an omitted destination-alpha test and alpha rounding from
  `0x80` to `0x7f` could change a whole rooftop view. Audit OpenGL alpha
  writes, alpha tests and blending on the relevant passes.
- Some shadows use a small render target, blur it, and sample it later. A
  target that was not drawn, was stale, or was reused after uploads could
  produce black roof regions or flicker. Trace producer and consumer draws.
- A frame's draw list depends on scene visibility and timing as well as mesh
  data. Match camera, area mask and frame before comparing captures.
- Sly has more than static meshes. A static-mesh-only path cannot render
  animated/skinned characters, morphs, particles, trails, outlines, fades or
  all screen effects.
- Their renderer diary is useful for behavior and test methods, not evidence
  that our OpenGL path implements the same feature. Reconfirm each behavior
  in our own inputs and outputs.

### Specific skinning and DATE details from sly1-nx

The current `docs/RENDERER.md` describes skinning as a weighted sum of up to
four bone transforms for each position. Normals use the corresponding 3x3
matrix portions and are normalized afterward. Its capture workflow inspects
the VIF UNPACK stages separately: positions at `0x401`, normals at `0x411`,
weights and the matrix palette. That sequencing matters because a later VU
program may alter or replace data that appeared correctly skinned at an
earlier stage. sly1-nx validates the host calculation against actual
intermediate output rather than inferring success from a changed silhouette.

We used that as a diagnostic checklist, not as a renderer transplant. The
native OpenGL path now applies the weighted bone transforms when the mesh has
matching bone and weight counts; it skips invalid or mismatched inputs rather
than dropping influences. The November 28 Paris trace confirms the player's
current meshes have four bones and four weights, and sample weight rows sum to
one. Its capture changes Sly from the straight T-pose to a bent standing pose.
The character is still small and the scene is malformed, so this only resolves
the observed unskinned pose for valid inputs; it does not claim the overall
model or level rendering is correct. The capture and raw palette trace are
under `/home/reaper/sly/native-skin-audit-20261128/`.

For destination alpha, sly1-nx documents a GS alpha mask where the high bit
of framebuffer alpha is read by later shadow/dark-volume draws. It reports
that rounding `0x80` down to `0x7f` can flip the mask result. The native
OpenGL implementation now has a port-only DATE/DATM path that snapshots the
current viewport alpha into an RGBA8 texture and compares the high-bit
threshold in the world fragment shader. The updated launcher built and ran,
but the captured Paris startup scene produced no DATE snapshot diagnostics;
there is therefore no evidence this path was exercised in the current frame,
and it did not resolve the large roof artifact. Keep the roof investigation
on the exact draw's geometry, transform, material and render-target inputs;
do not report DATE as its fix without a capture that proves the relevant draw
uses DATE and a before/after image that changes as expected.

### Native pad handshake applied here

In the pinned `port/platform/pad.cpp`, sly1-nx models a connected DualShock
with both the current and extended pad IDs set to 7, accepts the analog-mode
request immediately, and reports that each request has completed. The report
uses the padman's byte layout: type at byte 1, active-low buttons at 2–3,
right and left stick axes at 4–7, then pressure bytes. Our SDL adapter already
provided that report, but its query replacement returned zero for the
extended ID and its mode request fell through to an unavailable PS2 SIF RPC.
That kept the matched game pad state machine at step 1, before analog decoding.

The native port now returns both ID values and completes the mode request
locally in `port/game/sly1-us/native/func_002042C8.cpp`. The change stays in
the host adapter; `sly1-decomp`'s matched `pad_input.cpp` and `pad.c` remain
untouched. A hidden OpenGL run confirmed the matched poller advances to step 2
and decodes a full-right native stick as `1.0`. Input still does not reach
`ControlPlayer`: the game sets pad-select flags to `0x6`, selects routing 0,
and passes `g_blankPadRecord` while the entry queue is bound. Trace that queue's
progress and clear/release behavior before considering any routing change. The
run evidence is recorded in `PLAYABLE_TODO.md`.

## Other code and methods worth adapting

These are candidates for a focused source review when the corresponding
subsystem is next in the playable path. They are not all immediate work.

| Area | sly1-nx files | Reuse value |
|---|---|---|
| Renderer experiments | [`port/render/gs.cpp`](https://github.com/SlyC00per/sly1-nx/blob/main/port/render/gs.cpp), [`port/render/vu1_handlers.cpp`](https://github.com/SlyC00per/sly1-nx/blob/main/port/render/vu1_handlers.cpp), [`tools/gpu_capture.py`](https://github.com/SlyC00per/sly1-nx/blob/main/tools/gpu_capture.py) | Sly-specific expected transforms, lighting/skinning behavior, captures and diagnostic ideas. Port the behavior to OpenGL APIs; avoid bringing in the hardware pipeline wholesale. |
| Disc and file services | [`port/platform/iso.cpp`](https://github.com/SlyC00per/sly1-nx/blob/main/port/platform/iso.cpp), [`port/platform/dmac.cpp`](https://github.com/SlyC00per/sly1-nx/blob/main/port/platform/dmac.cpp) | Reference ISO-sector reads, async request completion and DMA boundary expectations. Compare with our current launcher services before adapting anything. |
| Save handling | [`port/platform/memory_card.cpp`](https://github.com/SlyC00per/sly1-nx/blob/main/port/platform/memory_card.cpp), [`port/platform/save_backup.cpp`](https://github.com/SlyC00per/sly1-nx/blob/main/port/platform/save_backup.cpp), [`docs/PS3_PARITY.md`](https://github.com/SlyC00per/sly1-nx/blob/main/docs/PS3_PARITY.md) | File-backed saves with the game's format, asynchronous calls and backup behavior; useful to turn the current card-prompt path into normal host saves. |
| Input and timing | [`port/platform/pad.cpp`](https://github.com/SlyC00per/sly1-nx/blob/main/port/platform/pad.cpp), [`port/platform/kernel.cpp`](https://github.com/SlyC00per/sly1-nx/blob/main/port/platform/kernel.cpp), [`port/platform/thread.cpp`](https://github.com/SlyC00per/sly1-nx/blob/main/port/platform/thread.cpp) | Examples of PS2 pad-call semantics, thread/semaphore services and frame timing. Keep the SDL/OpenGL host plumbing and verify Sly-facing behavior from our own call sites. |
| Sound and IOP | [`port/platform/iop.cpp`](https://github.com/SlyC00per/sly1-nx/blob/main/port/platform/iop.cpp), [`port/audio/audio.cpp`](https://github.com/SlyC00per/sly1-nx/blob/main/port/audio/audio.cpp), [`docs/AUDIO.md`](https://github.com/SlyC00per/sly1-nx/blob/main/docs/AUDIO.md) | Sound command replies, bank and VAG parsing, sample playback and the rule that polled commands must complete. Useful when startup or later audio needs work. |
| Movies | [`port/video/movie.cpp`](https://github.com/SlyC00per/sly1-nx/blob/main/port/video/movie.cpp), [`port/video/mpeg2.cpp`](https://github.com/SlyC00per/sly1-nx/blob/main/port/video/mpeg2.cpp), [`docs/VIDEO.md`](https://github.com/SlyC00per/sly1-nx/blob/main/docs/VIDEO.md) | Demux/decode/presentation flow and worker-thread behavior for the game's movie path. A later milestone after the native world renderer is coherent. |
| Crash/hang diagnosis | [`port/switch/crashlog.cpp`](https://github.com/SlyC00per/sly1-nx/blob/main/port/switch/crashlog.cpp), [`tools/crash_report.py`](https://github.com/SlyC00per/sly1-nx/blob/main/tools/crash_report.py), [`docs/CRASHES.md`](https://github.com/SlyC00per/sly1-nx/blob/main/docs/CRASHES.md) | Compact reports for thread state, recent game activity and hangs. Adapt the diagnostic format to our host logger if it will shorten future stall investigations. |
| Level/load verification | [`tools/level_bytes.py`](https://github.com/SlyC00per/sly1-nx/blob/main/tools/level_bytes.py), [`tools/room_audit.py`](https://github.com/SlyC00per/sly1-nx/blob/main/tools/room_audit.py) | Methods to check consumed level bytes against disc data and audit over-reads across levels. Our extractor already exists; these can guide broader loader verification. |
| 64-bit host strategy | [`tools/gen_pointers.py`](https://github.com/SlyC00per/sly1-nx/blob/main/tools/gen_pointers.py), [`tools/patches.py`](https://github.com/SlyC00per/sly1-nx/blob/main/tools/patches.py), [`docs/DESIGN.md`](https://github.com/SlyC00per/sly1-nx/blob/main/docs/DESIGN.md) | A generated 32-bit guest-pointer model that preserves retail structure layouts on 64-bit systems, with named patches and layout checks. Relevant to our eventual cross-platform host, not a reason to abandon the present i386 bootstrap before the gameplay path is understood. |
| Test structure | [`tests/unit/`](https://github.com/SlyC00per/sly1-nx/tree/main/tests/unit), [`tests/switch_smoke/`](https://github.com/SlyC00per/sly1-nx/tree/main/tests/switch_smoke), [`tools/run_ryujinx.py`](https://github.com/SlyC00per/sly1-nx/blob/main/tools/run_ryujinx.py) | Separates focused unit checks, renderer smoke checks and whole-game level runs. Reuse the test boundaries and determinism ideas; their Switch/Ryujinx automation is platform-specific. |

## Recommended work order

1. Trace the player part's source vertices, weights, bone palette and final
   OpenGL vertices to isolate the T-pose.
2. Trace the roof draw's render-target inputs and alpha/depth state to
   separate a bad geometry submission from a missing/stale texture or pass.
3. Compare area masks and request lists at a frame-matched camera position to
   explain missing geometry.
4. Audit which Sly draw categories the native OpenGL path does and does not
   handle; choose the next renderer milestone from that list.
5. Revisit saves, IOP/audio and movie playback after the gameplay scene and
   controls are usable.

## Decomp checkpoint (2026-11-28)

The sibling `/home/reaper/sly/sly1-decomp` checkout is now at upstream
`91de6fc6` (2026-11-26), clean and in sync with `origin/master`. The US
progress report remains 4,807/4,807 functions and 100% code bytes, with no
functions lacking C. The report explicitly warns that this is not host
readiness: pointer layout, assembly fallbacks and library replacements remain
unfinished.

The intervening decomp commits corrected parameter types/order for fifteen
shared functions. In particular, floats now appear among the other parameters
where retail callers evaluate them, replacing incompatible function-pointer
casts that had been used to model the EE register convention. This is useful
upstream cleanup, but our i386 host-generation adapter had several stale
patterns for the removed casts. Keep those host ABI accommodations in
`port/tools/compile_decomp_host32.py`; do not backport host call rewrites into
the decomp checkout. Rebuild before further gameplay conclusions because this
source refresh changes what our host compiler consumes.

Keep matched decompilation sources read-only. Put any functional missing-code
replacement in the port's backfill directories, label it as port-only, and
remove it when matching decomp code becomes available. Never use the VU/GS
diagnostic renderer as a substitute for the native OpenGL target.
