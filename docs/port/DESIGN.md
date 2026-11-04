<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Sly 1 native port: design

This note sets the initial direction for the OpenThievius port. It uses
OpenRAC as a process and architecture reference, while all Sly-specific
research comes from retail Sly 1 and public PlayStation 2 documentation.
Nothing from OpenRAC's Ratchet & Clank research or game data is used.

The matching decompilation is the sibling repository
[`sly1-decomp`](https://github.com/OpenThievius/sly1-decomp). It remains the
source of game code and is not edited by the port. The port lives in this
repository under `port/`.

## Direction

The target is a native PC port for Windows, macOS and Linux. The game's
decompiled C++ is compiled for the host and runs as a normal program. Port
code replaces platform services and draws the game through native rendering
code. This design does not run the retail executable in an emulator.

It also does not model EE/IOP processors, SIF, DMA, VIF, VU or GS as the
gameplay architecture. Replace console library boundaries with native
services and implement native rendering for Sly's drawing systems. The
current device-model work is experimental research and must not be extended
as the route to a playable port. See
[`OPENRAC_REUSE.md`](OPENRAC_REUSE.md) for the architecture comparison and
reuse plan.

The decompilation reports matching C for both NTSC-U and PAL, while unfinished
functions and data remain assembly-backed. The port should therefore accept a
decompilation source directory and produce its host build in an ignored build
tree. A host-generation tool may prepare generated copies of the C without
changing the decompilation. Its report must show which functions on the
current execution path have host C and which still block progress. Matching
the whole executable is valuable for regression and provenance, but native
play depends first on the boot, game loop, loading and rendering paths being
hostable.

## Proposed shape

```text
player's retail disc -> extractor -> validated files and port-ready assets
                                      |
sly1-decomp C++ ----------------> host preparation -> native game program
                                      |                    |
                                      +------------ platform services
                                                           |
                                             native renderer, audio, input
```

- **Source boundary.** Build from a user-selected `sly1-decomp` checkout. Keep
  generated C, object files, executables and extracted game data out of git.
- **Host build.** Reuse OpenRAC hostgen's preparation and host ABI where it
  fits the Sly decompilation, but build the decompiled C++ itself for the host.
  Generated host C may remain a compatibility bridge for source constructs
  that cannot yet build as C++, and for addressable retail assembly during
  migration. It must not become a MIPS instruction executor or a simulated
  console kernel that the playable game depends on. Keep the decomp checkout
  read-only and emit generated sources under `build/port/`.
- **Runtime services.** Replace calls to console libraries with host services
  for files, timing, controllers, sound, threads and saves. Derive the APIs
  from Sly's decompiled call sites and retail behavior. Reuse suitable generic
  OpenRAC service, window and platform modules with their original licenses;
  adapt only the title-specific API semantics. Do not include Sony SDK source,
  samples or headers.
- **Graphics.** Research Sly's rendering code and assets from retail. Build
  native renderers for the game's drawing subsystems and expose the data they
  need from host-native game code. Do not build an instruction-level model of
  the GS, VU, VIF or DMA as a compatibility path.
- **Assets.** The NTSC-U executable has a 46-entry catalog for data stored
  outside the disc file system. Build extractors and converters from the
  player's own disc and formats understood through Sly's retail code. Validate
  the boot executable against `games/sly1/game.json`; PAL's catalog is not yet
  checked.
- **Versions.** Start with NTSC-U. Keep PAL as a follow-up target and use the
  decompilation's cross-version matching to avoid building version-specific
  assumptions into shared port code.

## Open decisions

1. Whether to adapt OpenRAC hostgen or write a Sly-specific generator. First
   inspect hostgen's license and input assumptions, then prototype against a
   small set of Sly C files in an ignored build directory.
2. The guest memory and pointer model, based on Sly's actual address usage and
   portability constraints.
3. For each Sly drawing subsystem, whether host code should expose native
   draw data directly or provide a lightweight game-produced command stream.
4. Graphics API, audio library, window/input layer, build system and asset
   interchange formats.
5. Which subsystem provides the first useful milestone: a data-only level
   viewer or a minimal boot/title path.

## Research rules

OpenRAC's own authored code, tools and design may be reused with the
attribution required by `docs/policy/SOURCING.md`. Do not reuse its
third-party-derived work, Ratchet & Clank code, game research, symbols,
formats or assets. Sly-specific facts must be derived independently from
retail Sly 1, public PS2 documentation and our own experiments. Never put
disc data, executable bytes, extracted assets, generated disassembly or
toolchains in this repository.

## Reference

- [OpenRAC reuse plan](OPENRAC_REUSE.md)
- [OpenRAC port design](https://github.com/OpenRAC/OpenRAC/blob/develop/docs/port/DESIGN.md)
- [OpenRAC hostgen](https://github.com/OpenRAC/OpenRAC/tree/develop/port/tools/hostgen)
- [Sly 1 decompilation roadmap](https://github.com/OpenThievius/sly1-decomp/blob/master/docs/ROADMAP.md)
- [Sly 1 disc research](https://github.com/OpenThievius/sly1-decomp/blob/master/docs/DISC.md)
