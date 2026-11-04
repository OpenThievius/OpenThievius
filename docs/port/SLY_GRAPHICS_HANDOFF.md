<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Sly graphics handoff for the native renderer

The native launcher has OpenRAC's SDL/OpenGL context, but it still draws a
black frame. The current trace reports no GIF end-of-packet markers, no image
transfers and no submitted vertices. This means the next graphics task is to
reach Sly's display submission path and capture what Sly produces; replacing
the current vertex presenter alone cannot make a picture appear.

## Sly's observed display path

Retail-backed assembly shows that the callback at `0x0018E558` is a persistent
display worker. It waits on the semaphore stored in `D_00269A00`, updates its
display state, and calls `0x001686E8` with `D_00261F1C` plus one of two buffers
inside that display state. It also calls `0x001683E8` and `0x001684E8` to
advance the display buffers. These routines are still missing matching C in
the decompilation.

`0x001686E8` waits for the VIF1 channel, resets its transfer state, starts the
next transfer through `0x00202A80`, then waits on a semaphore. The latter
function prepares the VIF1 channel state for a source buffer. The exact source
span and its GIF payload have not yet been verified from a native C call path.
Do not infer that a channel descriptor by itself is a complete GIF packet.

## OpenRAC integration point

OpenRAC's `DirectRenderer` accepts GIF packets directly and also has a VIF
input mode. That mode recognizes VIF `DIRECT`/`DIRECTHL` payloads and forwards
the enclosed GIF bytes; it rejects VIF commands that feed a VU program. This
is a candidate for Sly's direct 2D/GIF submissions, not a complete renderer
for Sly's world geometry. Sly's VU-produced geometry needs a separate native
draw-data path or title-specific renderer. The direct renderer itself depends
on OpenRAC's GL loader, shader, framebuffer, renderer state and texture pool;
integrating `direct.cpp` alone would not provide a functioning renderer.

Before connecting the renderer, add one concrete packet observation at the
host boundary that already knows the transfer source and length. Confirm that
the bytes form a VIF stream carrying GIF packets, and keep the packet capture
free of VU execution. Then feed that span to the reused `DirectRenderer` in
VIF mode. Sly's world geometry may need a separate native renderer because it
is produced by title-specific draw code and VU programs.

## Current blocker

The host32 native path now has port-only implementations of `func_00185D20`,
`func_001860A8`, `func_001278A8` and `func_001280C0`. They prepare Sly's
0x100-byte draw entries, calculate position/basis data with scalar host math,
and dispatch the entry's object callbacks. These backfills live under
`port/game/sly1-us/native/`, separate from matching decompilation. They do not
execute VU instructions or feed the experimental GIF device model.

The callback methods at vtable offsets `0xFC`, `0x100` and `0x104` still need
native render adapters that expose Sly draw data. Neither the OpenRAC
`DirectRenderer` nor a Sly world renderer is wired to these entries yet, so
the launcher still has no visible scene. Reuse the direct renderer for Sly's
2D/GIF layer once the native callbacks can provide compatible packets; the
world geometry still needs a Sly-specific adapter.
