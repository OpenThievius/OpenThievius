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

This repository already reuses OpenRAC's most important pieces:

- `port/tools/hostgen/` and `tools/openrac_common/` generate host C from the
  read-only Sly decompilation and report the reachable no-C frontier.
- `port/common/` and the base files in `port/runtime/` provide the game-memory
  ABI, ELF loading, host fault reporting, float helpers and common support.
- The top-level game registry/setup workflow follows OpenRAC's layout and
  conventions; it does not bring over Ratchet & Clank data.

The port has also authored Sly-specific extensions around that base: Sly's
raw-sector catalog and audio extractors, NTSC-U hostgen configuration,
`libraries.tsv`, native launcher, service replacements and the two formatting
backfills. These are not OpenRAC code just because they use its architecture.

The current kernel replacement is a mixed case. The generic syscall dispatch
and thread/semaphore scaffolding were adapted from OpenRAC's documented
interpreter/runtime behavior, while the Sly service IDs, wrappers and required
status behavior must be verified from Sly's retail executable. Recent work
added an EE-layout `ReferThreadStatus` response using that boundary. This is
still part of the experimental guest-execution launcher, not a native Sly
service layer proven to advance gameplay.

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
against `docs/policy/SOURCING.md` before adoption. For each candidate, check
its file-level license and third-party notices; a component's generic purpose
does not make its dependencies or game-specific assumptions reusable.

The renderer code is not an automatic shortcut. OpenRAC's high-level OpenGL
and Vulkan backends may be reusable as generic graphics infrastructure after
license/dependency review, but its renderer's game-facing draw records,
texture/geometry conversion, scene traversal and shaders encode R&C formats
and behavior. Sly still needs its own asset readers and draw-data adapter.
Likewise, its input/audio/window modules may be useful generic foundations;
their SDL device plumbing can be evaluated separately from the game-facing
pad/audio semantics. The correct approach is selective reuse of generic
modules and independent implementation of Sly boundaries, not cloning all of
`port/` or rewriting generic infrastructure we already have.

The PS2 platform is shared, but library call sequences and graphics behavior
are title-specific. A generic ISO 9660 reader can help read ordinary files;
it cannot locate Sly's main game data. Sly's disc notes say the ISO lists boot
files and IOP modules while levels, models, textures and sound occupy raw
sectors outside the filesystem. The NTSC-U executable's 46-entry range table
is now identified and has a port-side extractor in
`port/tools/extract_sly1_catalog.py`. Its resource formats and PAL counterpart
still need independent research.

## Architecture correction for this port

The current `port/runtime/` path started as an imported OpenRAC guest-memory
and host-call foundation, but has since accumulated an Sly execution engine
that translates retail MIPS assembly and models parts of EE, IOP, SIF, DMA,
VIF1, VU1 and GS behavior. It has reached a black window and partial
color-only geometry. The latest bounded run still waits in `func_001F6B60`
with zero rendered vertices. This is a useful research harness, not a route
to continue toward playable Sly 1. Do not extend the instruction/device models
as gameplay features.

The target remains host-native game logic plus native services and
Sly-specific renderers, following OpenRAC. A playable path must call compiled
decompiled C++ and replace the title's platform/library boundaries with host
services; it must not depend on an EE instruction interpreter, simulated
console scheduler or emulated graphics devices. Retain ELF/disc inspection
and the current launcher only as bounded research tools until an independent
host-native boot path exists. The existing kernel, SIF, DMA, VIF/VU and GS
implementations cannot be treated as that native service layer.

Near-term sequence:

1. Audit OpenRAC's host build/runtime and identify the generic modules that
   can be brought over intact or adapted with their GPL notices; explicitly
   separate those from R&C-specific render/data code.
2. Replace the MIPS-assembly execution path with compiled Sly decompilation
   output and a narrowly defined guest-memory ABI; retain only low-level
   arithmetic or address compatibility that translated C actually requires.
3. Derive Sly's startup, loader, main loop and platform boundaries from its
   decompilation and retail behavior. Implement native files, timing, threads,
   controllers, sound and saves at those boundaries, with port-only backfills
   kept separate from matching source.
4. Identify the decoded resource containers and their native draw data, then
   reuse suitable generic OpenRAC window/rendering infrastructure while
   implementing Sly-specific asset conversion and renderer adapters.
5. Integrate the native frame path and keep building it through title, level
   loading, and interactive gameplay; only call the port playable after
   verifying a full level can be controlled and completed without emulation.

This is a direction correction, not a claim that the current launcher is
playable. Current implementation status is tracked in [`ROADMAP.md`](ROADMAP.md).

## OpenRAC references

- [Native port README](https://github.com/OpenRAC/OpenRAC/blob/develop/port/README.md)
- [Port design and decision history](https://github.com/OpenRAC/OpenRAC/blob/develop/docs/port/DESIGN.md)
- [Backfill workflow](https://github.com/OpenRAC/OpenRAC/blob/develop/docs/port/BACKFILL.md)
- [Renderer design](https://github.com/OpenRAC/OpenRAC/blob/develop/docs/port/RENDERER.md)
- [Sly 1 disc research](https://github.com/OpenThievius/sly1-decomp/blob/master/docs/DISC.md)
