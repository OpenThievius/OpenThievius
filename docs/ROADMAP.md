# Roadmap

## Done

- [x] The organisation's two repositories, on `master`, under the AGPL-3.0-or-later.
- [x] `games/sly1/game.json`: the NTSC-U, PAL and prototype discs, with every checksum.
- [x] `tools/openthievius.py`: recognises your discs, sets up sly1-decomp, generates the README tables and checks the two repositories agree.
- [x] sly1-decomp set up: disc extraction, a first splat split, the compiler identified and running ([its roadmap](https://github.com/OpenThievius/sly1-decomp/blob/master/docs/ROADMAP.md)).
- [x] sly1-decomp builds `SCUS_971.98` and `SCES_509.17` byte for byte.
- [x] **Matching** (2026-11-13): every function of both versions, from exact C or, for the hand-written and leftover functions, their original assembly; US data 99.97% C, PAL's data partly still from disassembly.
- [x] **Asset research** ([sly1-decomp's DISC.md](https://github.com/OpenThievius/sly1-decomp/blob/master/docs/DISC.md)): how the game finds its data on the disc, and a level's textures, objects, geometry, collision, animations and events, PAL's language copies, and the sound banks, each format from retail's own code or, for the sound banks the IOP's driver reads, from the disc's bytes.

## Next

1. **The extractor**, written from sly1-decomp's DISC.md; and what the event kinds and object classes are in the game.
2. **The port.** Its design: how decompiled PS2 code is hosted on PC, the renderer, audio, input, saves. A design note in `docs/port/` comes before code. OpenRAC's `port/` is the model, and its code may be used, credited.
3. **A launcher**, once something runs.

## Open decisions

- How the port hosts the game code: built from sly1-decomp's sources directly, or through a host-generation step as OpenRAC does.
- The port's language and build system (OpenRAC: C++ with CMake).
- Whether a website and a community channel are wanted, and where.
