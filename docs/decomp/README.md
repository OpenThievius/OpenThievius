# The decompilations

| Game | Project | Primary target | Compiler |
|---|---|---|---|
| Sly Cooper and the Thievius Raccoonus | [sly1-decomp](https://github.com/OpenThievius/sly1-decomp) | NTSC-U `SCUS_971.98` | Sony GNU EE GCC 2.9-ee-991111b (C++) |

Each project's own docs decide how a match is proven there. The rules every project shares:

- **A match is retail's bytes**: a build that links into a program equal to retail, byte for byte, checked by SHA-1.
- **Flags are per source file**, never per function, and no inline assembly, register pins or barriers in matched code.
- **Everything is derived from retail**: nothing from other projects and nothing from the prototype ([policy/SOURCING.md](../policy/SOURCING.md)).
- **No proprietary material** in any repository: `tools/proprietary.py` checks every push.

The compiler is proprietary: [toolchains/README.md](../../toolchains/README.md) says where builders get it, and [sly1-decomp's docs/TOOLCHAIN.md](https://github.com/OpenThievius/sly1-decomp/blob/master/docs/TOOLCHAIN.md) what is known about it.
