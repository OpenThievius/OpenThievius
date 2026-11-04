# Roadmap

## Done

- [x] The organisation's two repositories, on `master`, under the AGPL-3.0-or-later.
- [x] `games/sly1/game.json`: the NTSC-U, PAL and prototype discs, with every checksum.
- [x] `tools/openthievius.py`: recognises your discs, sets up sly1-decomp, generates the README tables and checks the two repositories agree.
- [x] sly1-decomp set up: disc extraction, a first splat split, the compiler identified and running ([its roadmap](https://github.com/OpenThievius/sly1-decomp/blob/master/docs/ROADMAP.md)).

## In progress

- **sly1-decomp:** matching the NTSC-U executable and improving the source coverage.
- **Native port:** the default C-only host build compiles and links matched Sly code, but the boot entry is still a stub and it has not reached gameplay. A separate research build translates retail MIPS instructions and reached startup with a user's ELF and disc image; that path uses instruction execution and is not part of the no-emulation port. The current gaps are listed in the [port roadmap](port/ROADMAP.md).

## Next

1. **Reach a playable NTSC-U state natively.** Complete disc data loading, the renderer and frame path, audio, input and saves.
2. **Keep improving the decompilation and host translation.** Prioritize functions and data on the loader, boot, frame and render paths.
3. **Add PAL support, parity fixes and packaging.**

The native port uses a host-generation step and CMake, following OpenRAC's
architecture where it fits. All game behavior and assets are derived from
retail Sly 1, under the sourcing policy.
