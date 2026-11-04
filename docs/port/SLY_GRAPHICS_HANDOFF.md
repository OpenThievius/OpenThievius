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
input mode. That mode is the candidate for Sly's VIF1 submission boundary: it
can consume the VIF stream and extract its direct GIF payload without running
the VU program. The renderer itself depends on OpenRAC's GL loader, shader,
framebuffer, renderer state and texture pool; integrating `direct.cpp` alone
would not provide a functioning renderer.

Before connecting the renderer, add one concrete packet observation at the
host boundary that already knows the transfer source and length. Confirm that
the bytes form a VIF stream carrying GIF packets, and keep the packet capture
free of VU execution. Then feed that span to the reused `DirectRenderer` in
VIF mode. Sly's world geometry may need a separate native renderer because it
is produced by title-specific draw code and VU programs.

## Current blocker

The C-only host build backfills the outer frame loop, but the worker and its
display callback functions still have no C. The latest bounded run reaches
the registered port callback at `0x0018E5E8`; worker `0x0018E558` is a no-C
stub. The trace has no GIF end marker and no vertices. First make the Sly
display path reach its packet submission boundary with port-only C kept under
`port/game/sly1-us/hand/`. Then the direct renderer can be connected to actual
Sly-produced packets.
