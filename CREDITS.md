# Credits

OpenThievius is the work of its contributors, derived from the retail disc alone: no code,
names, maps or splits come from other Sly projects or from the prototype
([docs/policy/SOURCING.md](docs/policy/SOURCING.md)).

## OpenRAC

OpenThievius is built on [OpenRAC](https://github.com/OpenRAC/OpenRAC), the Ratchet & Clank
decompilation and native port by the same maintainers: its layout (`games/`, `baserom/`,
`toolchains/`, `shared/`), its repository tool, its sourcing policy, its contribution guide and
commit convention, and what it learned about Sony's EE compilers. The port will follow its
design. [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) lists what came from where.

## Tools

Free tools the project runs. They are installed by each contributor and are not part of the
repositories.

- [splat](https://github.com/ethteck/splat), [spimdisasm](https://github.com/Decompollaborate/spimdisasm),
  [rabbitizer](https://github.com/Decompollaborate/rabbitizer): splitting and disassembly.
- [wibo](https://github.com/decompals/wibo): runs the Windows compiler on Linux and macOS.
- [AngheloAlf](https://github.com/AngheloAlf)'s public mirror of the PS2 toolchain, which contributors clone outside the repositories.
