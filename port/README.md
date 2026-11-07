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

Host generation can be run with `python3 port/tools/sly1.py hostgen`; the
standalone command defaults to `--asm off`. The native CMake launcher enables
the retail assembly fallbacks listed in `game/sly1-us/hostgen.json`. Functions
with matching C are skipped automatically; the current report translates 146
functions from retail instructions. Generated C stays in the
ignored build tree, separate from both matched decomp C and the port's
handwritten C backfills. Set
`OPENTHIEVIUS_ASM_SOURCE_SLY1` when the checkout used for matched C does not
contain the assembly directory. The current build uses matched C and retail
assembly from the local `sly1-decomp` checkout at `e1b39f1`.

The latest native hostgen run against `e1b39f1` reads 185 units and translates
2,050 C functions plus 146 retail-assembly functions. It lists 2,666 functions
without C, has 12 unreadable units, and emits zero stubs. The build uses
`--allow-unreadable`, so those units are omitted. Seven stream readers and two
refill helpers from unreadable unit `00134000` are now translated from retail
assembly. The retail heap initializer is also translated, and the bounded
native launcher now reaches the selected level range at sector 867214. Exact
retail animation-table and element-parser translations let it complete through
loader stage 5 with an active world. The window remains black and scene output
is still zero vertices; the first renderable mesh callback and native scene
submission remain the blockers. The font registry is also empty in the direct
level trace. See
the updated
[playable TODO](../docs/port/PLAYABLE_TODO.md) for the exact report and trace.
The graphics handoff records the current evidence and next blocker. Functional
C backfills stay under `game/sly1-us/hand/native_c/`, outside `sly1-decomp`.

To translate the selected assembly functions into the native build output
directly, hostgen also supports:

```sh
python3 port/tools/sly1.py hostgen --asm on --native-asm   --source /path/to/sly1-decomp   --asm-source /path/to/sly1-decomp-with-asm
```

Without `--native-asm`, `--asm on` writes a separate research output. In
either mode the configured `asm.only` list limits translation to the selected
functions listed in `hostgen.json`.

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
