# Roadmap

## Done

- [x] The organisation's two repositories, on `master`, under the AGPL-3.0-or-later.
- [x] `games/sly1/game.json`: the NTSC-U, PAL and prototype discs, with every checksum.
- [x] `tools/openthievius.py`: recognises your discs, sets up sly1-decomp, generates the README tables and checks the two repositories agree.
- [x] sly1-decomp set up: disc extraction, a first splat split, the compiler identified and running ([its roadmap](https://github.com/OpenThievius/sly1-decomp/blob/master/docs/ROADMAP.md)).

## Next

1. **sly1-decomp builds `SCUS_971.98` byte for byte** from assembly. Everything after rests on it.
2. **Matching.** The compiler qualified, the file split, the per-function checker, progress reporting.
3. **Asset research.** How the retail disc stores its data, and the level, model, texture and sound formats, documented from retail's own code. Measured so far ([sly1-decomp's DISC.md](https://github.com/OpenThievius/sly1-decomp/blob/master/docs/DISC.md)): the file system lists only the executable and the IOP modules; everything else lies in unlisted sectors from 12345 on, found through an index the loader reads, and it is data only, with no code to port.
4. **The port.** Its design: how decompiled PS2 code is hosted on PC, the renderer, audio, input, saves. A design note in `docs/port/` comes before code. OpenRAC's `port/` is the model, and its code may be used, credited.
5. **A launcher**, once something runs.

## Open decisions

- How the port hosts the game code: built from sly1-decomp's sources directly, or through a host-generation step as OpenRAC does.
- The port's language and build system (OpenRAC: C++ with CMake).
- Whether a website and a community channel are wanted, and where.
