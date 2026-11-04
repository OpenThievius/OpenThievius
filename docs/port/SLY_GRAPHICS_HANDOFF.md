<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Sly graphics handoff for the native renderer

The native launcher uses OpenRAC's SDL/OpenGL context and `DirectRenderer` for
Sly's GIF channel 2 packets. Its port-only `func_00202A80` backfill walks the
bounded DMA tag chain and queues the raw GIF payload for OpenRAC's GIF parser.
A bounded native run accepts the four startup display packets (2,192 bytes in
total) without a parser warning. The window is still black: these packets set
display state, and the game has not loaded a scene object or level.

## Sly's observed display path

`D_00261F1C` is obtained from DMA channel index 2 in `func_00152810`, the GIF
channel. The callback at `0x0018E558` is a persistent display worker. It waits
on the semaphore stored in `D_00269A00`, submits one of two buffers through
`0x001686E8`, then advances the display buffers. These routines are still
missing matching C in the decompilation.

`0x001686E8` submits the GIF packet buffer through `0x00202A80` and waits for
completion. The port-only backfill reads the DMA tag chain beginning at the
supplied source address and appends each inline or referenced payload span.
The DMA chain's QWC and tag IDs determine the byte spans. OpenRAC's
`DirectRenderer` receives the assembled GIF packets directly; this boundary
does not execute VIF or VU instructions.

Retail's `func_00202A80` stores the source address at descriptor offset `0x30`
and sets the channel active. The port replacement performs that handoff
synchronously, then clears the active bit so Sly's subsequent wait can return.

## OpenRAC integration point

OpenRAC's `DirectRenderer` consumes GIF packets directly. This is the correct
path for Sly's GIF channel 2 output and avoids interpreting VIF/VU hardware for
2D packets. It is not a complete renderer for Sly's world geometry: the game
still needs a native draw-data adapter for its scene and object renderers.
The direct renderer depends on OpenRAC's GL loader, shader, framebuffer,
renderer state and texture pool; integrating `direct.cpp` alone would not
provide a functioning renderer.

The current adapter confirms the startup packets reach the GIF parser, but
does not prove Sly's frame ordering, texture uploads, or display-buffer
conventions match OpenRAC. The next blocker is earlier in the game: its frame
loop remains in state 3 with no current scene object or active level. Once the
title/scene loader advances, the 3D scene still needs a Sly-specific native
data path.

## Port-only backfills

Functional replacements live under
[`port/game/sly1-us/hand/native_c/`](../../port/game/sly1-us/hand/native_c/),
outside `sly1-decomp`. When matching C becomes available, the port backfill can
be removed and the decompiled function used without rewriting the project.
These replacements are compatibility code for the native port and are not
counted as decompiled functions.
