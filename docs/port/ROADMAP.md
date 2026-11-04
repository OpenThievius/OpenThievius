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
3,145 from assembly. The guest runtime builds, all 81 generated C units compile,
and `port_link_generated` links them without running the entry point. The Sly
`port_native` target links a launcher that reads the user's extracted boot ELF,
maps its segments into guest memory and calls the translated entry. Four
authorized startup runs reached translated code. The first exposed an
incompatible stack range and five constructor-table interior entries; the
runtime now adopts the range from `SetupThread`, and the launcher registers
those five callbacks. The later runs passed them, then advanced through CD/IOP
initialization and ISO path polling until timeout. The fourth run, with native
DMA span handling, stayed alive to its 15-second timeout without a crash; it
reported missing VU initialization and formatted logging functions. Native
interrupt registration was implemented afterward, but renderer events remain
unwired. More interior entries from that path are now translated by hostgen. A native ISO 9660 reader accepts cooked images and raw
2352-byte BINs, locates paths and reads file ranges. The local NTSC-U image was
identified as `SCUS_971.98`, with `IOPRP243.IMG` and the boot modules in its
ISO file table. The IOP bootstrap constructor is replaced by a native hook
that marks CD state ready only when an ISO is loaded. Host replacements also
cover file-open/poll/close with ISO path lookup and preserve interrupt state
for critical sections. The interrupt controller stores installed handlers and
enable state and exposes a host event raise hook, but the renderer does not
raise device events yet. The native DMA helper performs validated EE RAM copies
synchronously; hardware transfers still need host device services. Sector reads
are not routed to Sly's later data requests yet. The
analyzed boot call graph still reaches functions without hostable C. Fifteen source files remain unreadable and the C++ adapter can lose
constructs; remaining disc services, most kernel calls, the renderer and frame
loop still block gameplay. Continue
with the loader and boot call graph, preserving C++ semantics and grounding
each host service in Sly retail evidence.

The order can change as source research reveals dependencies. Keep each phase
grounded in evidence from Sly's retail code. Continue function matching in
`sly1-decomp`; prioritize functions and data on the loader, boot, frame and
rendering paths once those paths are identified. Generated code and game
assets stay in ignored local build/data directories.
