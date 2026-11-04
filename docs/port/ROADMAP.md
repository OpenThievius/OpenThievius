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
place. The Sly profile stages C++ sources as C-compatible inputs and, with
local retail-backed assembly available, translates 1,532 functions from C and
3,136 from assembly. The guest runtime builds, all 81 generated C units compile,
and `port_link_generated` links them without running the entry point. The Sly
`port_native` target now links a launcher that reads the user's extracted boot
ELF, maps its segments into guest memory and calls the translated entry. It has
been launched once with approval; startup reached translated code before the
runtime rejected Sly's EE RAM stack bounds. The guest runtime now adopts the
range from `SetupThread`. That run also exposed five constructor table entries
pointing into function bodies; the launcher now registers host callbacks for
those exact interior addresses. The entry translates its BSS setup, stack/heap
initialization and observed boot
syscalls, including memory copy, cache flush and kernel handler setup. The
analyzed boot call graph still reaches 26 functions without hostable C. Its
nearest missing functions include variadic formatting and SIF DMA operations.
The engine timer reads a monotonic host clock at the EE Count frequency.
Fifteen source files remain unreadable and the C++ adapter can lose constructs;
disc services, most kernel calls, the renderer and frame loop still
block gameplay. Continue with the loader and boot call graph, preserving C++
semantics and grounding each host service in Sly retail evidence.

The order can change as source research reveals dependencies. Keep each phase
grounded in evidence from Sly's retail code. Continue function matching in
`sly1-decomp`; prioritize functions and data on the loader, boot, frame and
rendering paths once those paths are identified. Generated code and game
assets stay in ignored local build/data directories.
