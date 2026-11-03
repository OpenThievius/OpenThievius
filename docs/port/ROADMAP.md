<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Native port roadmap

The port and the matching decompilation can progress together. A complete
matching decompilation is not required to begin tooling, asset research or a
viewer, but native gameplay needs hostable C along its boot and frame paths.
See [DESIGN.md](DESIGN.md) for the architecture and open decisions.

| Phase | Work | Milestone |
|---|---|---|
| 0 | Decide source-generation boundary and host memory model; inspect reusable OpenRAC tooling and license | A small Sly source subset can be prepared without modifying sly1-decomp |
| 1 | Read-only source integration, build scaffolding and frontier reporting | The port can build a host test program from generated Sly code |
| 2 | Research the retail loader/index; implement disc validation, extraction and asset format notes | A user's own disc is validated and game files can be located safely |
| 3 | Derive asset readers/converters from Sly's retail code | Port-ready level and texture assets from a user's disc |
| 4 | Native viewer and renderer prototypes for Sly's actual rendering systems | An extracted Sly scene draws without running game code |
| 5 | Replace required platform APIs and host the boot/title path | Native program reaches a useful in-game state |
| 6 | Complete frame path, renderer integration, audio, input and saves | NTSC-U is playable on desktop platforms |
| 7 | PAL support, parity fixes, packaging and launcher | Both retail versions are supported |

**Current implementation:** the hostgen tool and CMake entry point are in
place. The current Sly profile stages C++ sources as C-compatible inputs and
translates 454 functions in the local checkout; this loses constructs and is
not suitable for a playable build yet. The first execution frontier is the
retail entry function at `func_00100008`, which still needs startup and system
call handling. The next milestone is a semantics-preserving C++ input path,
then native implementations for the functions needed to get past startup.

The order can change as source research reveals dependencies. Keep each phase
grounded in evidence from Sly's retail code. Continue function matching in
`sly1-decomp`; prioritize functions and data on the loader, boot, frame and
rendering paths once those paths are identified. Generated code and game
assets stay in ignored local build/data directories.
