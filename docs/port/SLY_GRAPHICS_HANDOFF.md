<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Sly graphics handoff for the native renderer

The native launcher now owns an OpenRAC SDL/OpenGL context and runs OpenRAC's
`DirectRenderer` for the Sly 2D VIF DIRECT/GIF path. Its port-only
`func_00202A80` backfill parses bounded DMA chains, filters out VU-producing
streams, and queues supported direct packets for the renderer. The launcher
builds and links, but a real packet has not yet been confirmed in a native run;
world geometry is still outside this renderer.

## Sly's observed display path

Retail-backed assembly shows that the callback at `0x0018E558` is a persistent
display worker. It waits on the semaphore stored in `D_00269A00`, updates its
display state, and calls `0x001686E8` with `D_00261F1C` plus one of two buffers
inside that display state. It also calls `0x001683E8` and `0x001684E8` to
advance the display buffers. These routines are still missing matching C in
the decompilation.

`0x001686E8` waits for the VIF1 channel, resets its transfer state, starts the
next transfer through `0x00202A80`, then waits on a semaphore. The port-only
backfill reads the DMA tag chain beginning at the supplied source address,
walks inline and referenced spans, and passes the assembled VIF stream through
a strict VIF DIRECT filter before queueing it. The collector is bounded by tag,
stack and byte limits and safely drops invalid or VU-producing chains.

Retail's `func_00202A80` stores the source address at descriptor offset `0x30`
and sets the channel active. The port replacement performs that handoff
synchronously, then clears the active bit so Sly's subsequent wait can return.
The DMA chain's QWC and tag IDs determine the byte spans; the filter validates
that the assembled bytes contain supported VIF commands and at least one
DIRECT/DIRECTHL payload. The renderer receives only that filtered stream.

## OpenRAC integration point

OpenRAC's `DirectRenderer` accepts GIF packets directly and also has a VIF
input mode. That mode recognizes VIF `DIRECT`/`DIRECTHL` payloads and forwards
the enclosed GIF bytes; it rejects VIF commands that feed a VU program. This
is a candidate for Sly's direct 2D/GIF submissions, not a complete renderer
for Sly's world geometry. Sly's VU-produced geometry needs a separate native
draw-data path or title-specific renderer. The direct renderer itself depends
on OpenRAC's GL loader, shader, framebuffer, renderer state and texture pool;
integrating `direct.cpp` alone would not provide a functioning renderer.

Next, run the native launcher and record whether a supported stream reaches
the renderer, then inspect the displayed result. The present adapter confirms
packet shape syntactically but does not prove Sly's frame ordering, GS state,
texture uploads, or display-buffer conventions match OpenRAC. The 3D scene
still needs a Sly-specific native data path; VU-produced geometry is rejected
at this 2D boundary.

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
