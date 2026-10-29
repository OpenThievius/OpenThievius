# Sourcing policy

This policy applies to everything in OpenThievius and its projects. A
project's own rules may be stricter; none may be looser. It is at least as
strict as [OpenRAC's](https://github.com/OpenRAC/OpenRAC/blob/master/docs/policy/SOURCING.md),
and stricter in two ways: we take nothing from anyone else but OpenRAC, and
nothing from the prototype.

## We derive everything ourselves

Everything OpenThievius commits is written by its contributors, derived from
the retail executable of a disc they own:

- **No third-party code.** Nothing is copied from other projects, including
  other decompilations, ports or research on the Sly games: not their code,
  symbol maps, file splits, names, types or notes. Library code inside the
  retail executable (Sony's libraries, the compiler's runtime) is decompiled
  from retail like the rest.
- **Free tools are fine to run** (splat, spimdisasm, wibo, Ghidra, m2c, an
  emulator). They are installed by each contributor and are not part of the
  repositories, and what they produce from game data stays local.

## OpenRAC

[OpenRAC](https://github.com/OpenRAC/OpenRAC), the Ratchet & Clank
decompilation and port, is the sister project of the same maintainers, and the
one exception to "nothing from anyone else". Its own work (tools, build code,
checks, documentation, policies, research on the PS2 toolchain, the port's
design and code) may be used here, openly credited: in the commit that brings
it, and in the repository's `THIRD_PARTY_NOTICES.md`.

- What OpenRAC's maintainers wrote is used here under the AGPL-3.0-or-later,
  by their decision.
- What other OpenRAC contributors wrote keeps its GPL-3.0-or-later notice;
  section 13 of both licenses lets it be combined with AGPL code.
- It does not cover what OpenRAC itself took from other projects (its
  `THIRD_PARTY_NOTICES.md` lists that), nor any Ratchet & Clank game code.

## No proprietary material touches the repositories

Never, not in a commit, on a branch, in a pull request, an issue or a chat:

- disc images, executables or any file from a disc, or anything carrying
  retail bytes: generated assembly (`asm/`), dumps, extracted assets;
- compilers, assemblers, linkers or any SDK binary, or links to them;
- Sony SDK source code, sample code or headers, or anything recreated from
  them under their names;
- leaked or NDA material of any kind.

`baserom/`, `assets/`, `build/` and `toolchains/` are ignored by git. Every
repository's `tests/check_repository.py` runs `tools/proprietary.py` (shared,
[shared/files.json](../../shared/files.json)), which rejects binaries, disc
images and their sidecars, archives, game data and toolchain folders, links,
Sony SDK headers and banners, pasted disassembly and dumps of retail data.
CI runs it on every push and pull request. It is never weakened to let a file
in. If it rejects your file, the file does not belong in the repository.

Describe what you see in words. A note about what a function does is welcome;
retail bytes are not.

## What you may work from

- The retail executables of a disc you own: disassembly, Ghidra, m2c,
  emulators.
- Public hardware documentation of the PlayStation 2.
- Your own experiments: what the compiler you supply makes of the C++ you
  write.

When a function looks like SDK sample code, decode it from the assembly.

## The prototype

The May 2002 E3 preview of *Sly Cooper* (build 5-19D) keeps its symbol table
and debug information. **Nothing is taken from it**: no code, data, symbol
names, signatures, types, field names, file names or file lists, and no tool
extracts them. The setup tools recognise the disc and skip it.

Looking at it to understand what the game does is allowed only when what you
then write is entirely your own: your names, your structure, reasoned from
retail's bytes. Copying something across, whole or with cosmetic changes, is
not. When in doubt, treat it as copied and leave it out. It is never the
evidence for a match: that is always retail's bytes
([sly1-decomp's docs/PROTOTYPE.md](https://github.com/OpenThievius/sly1-decomp/blob/master/docs/PROTOTYPE.md)).
