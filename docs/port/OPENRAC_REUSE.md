<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# What we can reuse from OpenRAC

OpenRAC is a useful precedent for the port architecture and workflow. It is
not a Sly renderer or a source of Sly game research. This note records which
parts transfer and what must be independently established for Sly 1.

## OpenRAC's approach

OpenRAC's current port compiles the decompiled game as a normal host program.
It replaces calls into console libraries with native platform services and
draws the game using its own renderer. It does not run the retail executable
or model the console's processors, GS, vector units, VIF or DMA as a route to
gameplay. Its design history says the earlier `runtime/` hardware-model
experiment was removed after choosing this direction.

The renderer is organized around what the game's drawing subsystems produce.
The game supplies camera, visibility, level-of-detail and lighting state;
native renderers draw terrain, ties, shrubs, mobys and 2D effects. Some assets
are converted during extraction. This avoids a general GS memory model or a
VU instruction interpreter.

OpenRAC also keeps port backfills distinct from matching decompilation. Its
functional C is listed separately, used by the port only, and never counted
as matching source. That maps directly to this project's requirement that
port fixes must not change what `sly1-decomp` can later match or correct.

## What transfers to Sly

| OpenRAC pattern | Sly application |
|---|---|
| Host-generate decompiled C without changing the decomp checkout | Keep `sly1-decomp` read-only and build generated sources under `build/port/` |
| Replace console library boundaries | Implement native input, sound, timing, files, threads and saves at the APIs Sly actually calls |
| Functional backfills live outside matching sources | Keep each port-only replacement under `port/game/sly1-us/hand/`, register it in hostgen config, and preserve matching source in `sly1-decomp` |
| Build renderers around game draw subsystems | Survey Sly's frame and renderer call paths, then expose/translate useful Sly draw data to native rendering code |
| Validate claims with source and retail evidence | Use Sly decompilation, the user's retail executable/disc and public PS2 documentation; tag inference separately |

The first concrete backfill follows this model: `func_001B93D8` is represented
by port-owned C in `port/game/sly1-us/hand/`, because the retail assembly
passes three callback registers that the current matching C placeholder
drops. Its change does not modify the decompilation.

## What does not transfer directly

OpenRAC's renderer, asset readers, game-specific algorithms, formats,
addresses, symbols, observations and extracted data describe Ratchet & Clank.
They are not evidence for Sly behavior and are not copied into this project.
Our sourcing policy permits reuse of OpenRAC-authored generic tools or
infrastructure only with the required attribution and license notices; it
does not permit reuse of its R&C-derived game work. Review candidate code
against `docs/policy/SOURCING.md` before adoption.

The PS2 platform is shared, but library call sequences and graphics behavior
are title-specific. A generic ISO 9660 reader can help read ordinary files;
it cannot locate Sly's main game data. Sly's disc notes say the ISO lists boot
files and IOP modules while levels, models, textures and sound occupy raw
sectors outside the filesystem. The NTSC-U executable's 46-entry range table
is now identified and has a port-side extractor in
`port/tools/extract_sly1_catalog.py`. Its resource formats and PAL counterpart
still need independent research.

## Architecture correction for this port

The current `port/runtime/` path models parts of EE, IOP, SIF, DMA, VIF1, VU1
and GS behavior. It has reached a black window and partial color-only
geometry. This work helped locate startup and graphics boundaries, but it is
not the route to continue toward playable Sly 1. Do not extend these device
models as gameplay features.

The target is host-native game logic plus native services and Sly-specific
renderers. Retain ELF/disc inspection utilities where useful for research and
validation. Retire or isolate the device-model launcher path as the
host-native execution path takes shape. Rendering should proceed by surveying
Sly's frame loop and draw subsystems, identifying data that can be exposed
from decompiled or backfilled game code, and implementing native renderers
and asset conversion from that evidence.

Near-term sequence:

1. Identify the boot, loader, main loop and public library call boundaries
   from Sly's code and retail behavior.
2. Decode the extracted NTSC-U resources and identify the level/model formats.
3. Produce a native asset viewer or scene renderer, independently of the
   runtime, to validate asset decoding and rendering conventions.
4. Host the decompiled game call path with native service replacements and
   port-only backfills, then connect its draw data to the renderer.
5. Add audio, input, saves, loading and remaining game systems until a full
   level can be played.

This is a direction correction, not a claim that the current launcher is
playable. Current implementation status is tracked in [`ROADMAP.md`](ROADMAP.md).

## OpenRAC references

- [Native port README](https://github.com/OpenRAC/OpenRAC/blob/develop/port/README.md)
- [Port design and decision history](https://github.com/OpenRAC/OpenRAC/blob/develop/docs/port/DESIGN.md)
- [Backfill workflow](https://github.com/OpenRAC/OpenRAC/blob/develop/docs/port/BACKFILL.md)
- [Renderer design](https://github.com/OpenRAC/OpenRAC/blob/develop/docs/port/RENDERER.md)
- [Sly 1 disc research](https://github.com/OpenThievius/sly1-decomp/blob/master/docs/DISC.md)
