# The native port

The port will run the decompiled game natively on Windows, macOS and Linux,
with the game's data read from your own disc. The current architecture and
staged plan are in [docs/port/DESIGN.md](../docs/port/DESIGN.md) and
[docs/port/ROADMAP.md](../docs/port/ROADMAP.md).
The [OpenRAC reuse plan](../docs/port/OPENRAC_REUSE.md) explains the native
port pattern and marks the current hardware-model launcher as experimental,
not the target gameplay architecture.

The raw-disc tools recover standalone VAG audio samples and extract the
NTSC-U executable's 46 catalogued resource ranges. The catalog tool can also
decode all ranges with the retail LZ routine. The native launcher serves exact
catalog sector ranges from the extracted files on later runs and reads other
ranges from the ISO. Usage and limits are in
[docs/port/ASSETS.md](../docs/port/ASSETS.md).

The first port tool inspects the decompilation's committed sources and reports
matching coverage:

```sh
python3 port/tools/sly1.py status --source /path/to/sly1-decomp
python3 port/tools/sly1.py status --version eu --json
```

It is a planning/status tool, not a host build. The host build and its current
limits are described in [the port roadmap](../docs/port/ROADMAP.md).

Host generation can be run with `python3 port/tools/sly1.py hostgen`; its
default is `--asm off`, and generated files stay under `build/port/`. It
generates host C from matched Sly sources plus port-only backfills without
translating retail MIPS assembly. The generated source snapshot is `cf9ad46`;
the current hostgen report reads 196 units, translates 1,091 functions, leaves
one C function stubbed, lists 3,799 functions without C, and applies seven
port-owned overrides. Upstream `sly1-decomp` master was checked at `dd6f80b`:
4,730/4,806 matching US functions (93.10% code) and 4,738/4,831 European
functions (92.42% code). These are separate measures: host-C generation
coverage does not indicate how much game logic builds or runs natively. The
direct level-load probe runs the scene-batch path and populates world lists but
still submits zero scene vertices; default startup reaches malformed menu
text. The current graphics
diagnosis and concrete next task are in
[SLY_GRAPHICS_HANDOFF.md](../docs/port/SLY_GRAPHICS_HANDOFF.md).
The separate freestanding host32 launcher is a startup diagnostic and does
not open a window. SDL input updates host-side DualShock reports, but Sly's pad
library is not connected to them yet. Functional backfills stay under
`game/sly1-us/hand/native_c/`, outside `sly1-decomp`.

`python3 port/tools/sly1.py hostgen --asm on` writes a separate
`build/port/hostgen/us-asm-research/` tree. That mode translates retail MIPS
instructions for bounded research only; it is not the playable path. The
target architecture is compiled Sly game logic with native platform services
and a Sly-specific renderer, reusing OpenRAC's generic host/runtime
infrastructure where it fits. See [`OPENRAC_REUSE.md`](../docs/port/OPENRAC_REUSE.md).

The reused guest runtime builds as `sly1_port_runtime`. To compile and link all
generated game units into a local link-check executable, run:

```sh
cmake -S port -B build/port -DOPENTHIEVIUS_SOURCE_SLY1=/path/to/sly1-decomp
cmake --build build/port --target port_compile_generated
cmake --build build/port --target port_link_generated
cmake --build build/port --target port_native
```

The link check confirms that the generated C and shared runtime link together;
it does not call the game entry. `port_native` builds
`build/port/sly1_native` from the same no-assembly host output. The port-only
boot entry reaches a backfilled frame loop, but does not yet reach gameplay.
The optional
`--asm on` research output is separate and must not be used as a gameplay
dependency.

Port-side replacements are listed in `game/sly1-us/libraries.tsv`, with their
native implementations in the runtime. The `extra_entries` list in
`game/sly1-us/hostgen.json` identifies additional retail assembly entry points
needed by the port. A matching entry supplied by the decompilation takes
precedence over that list. Some launcher paths still contain guest-addressed
EE/DMA/VIF/VU/GS compatibility services from the earlier startup experiment;
they are diagnostic scaffolding, not the target gameplay architecture.

The current graphics task is to inspect the decoded font atlas and alpha plane,
then trace indexed texture/CLUT conversion, UVs, and GS blend/test state into
the OpenGL output. The launch command and texture dump are in
`docs/port/SLY_GRAPHICS_HANDOFF.md`. Contributors comfortable with PS2 GS
formats, GIF packets, OpenGL shaders, and image inspection can take this
track. Once a readable menu is visible, the port still needs
the title/level transition, Sly pad-library integration, controllable character
movement and collision, real audio, and saves. See the
[roadmap](../docs/port/ROADMAP.md) and [graphics handoff](../docs/port/SLY_GRAPHICS_HANDOFF.md)
for status and starter tasks.

OpenRAC's port design and its own authored code may be used with the credit
required by [the sourcing policy](../docs/policy/SOURCING.md#openrac). Sly's
code and behavior are researched independently from retail.

Everything here is AGPL-3.0-or-later ([LICENSE.md](../LICENSE.md)).
