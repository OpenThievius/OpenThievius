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

Host generation can be run with `python3 port/tools/sly1.py hostgen`; it
defaults to `--asm off`. The CMake `port_hostgen` target now explicitly keeps
assembly translation off. Use `--asm on` only for a separate research output;
do not make that output a dependency of the playable port. Generated sources
stay in the ignored build tree, separate from matched decomp source and
port-only backfills.

The `port_native` launcher still links generated code to the experimental
guest-memory and PS2-device runtime. Its visible memory-card prompt is not a
native service implementation or a playable milestone. The target gameplay
architecture is compiled Sly C/C++ with native host services and renderers;
see the [architecture correction](../docs/port/OPENRAC_REUSE.md).

Older forced-level and VIF/GS findings describe the experimental guest-runtime
path only; see the dated history in the [roadmap](../docs/port/ROADMAP.md).
Port-only C backfills stay under `game/sly1-us/hand/native_c/` or
`game/sly1-us/native/`, outside `sly1-decomp`.

## Current decomp input (2026-11-14)

The sibling `sly1-decomp` checkout tracks public master `1fdd36d7` and is
read-only from this repository. Its report gives NTSC-U 4,807/4,807 functions
and 1,112,400/1,112,400 code bytes matched (100%); 140 US data bytes remain
unmatched. PAL also has 100% of its code matched. This does not mean all code
is portable C: the US report still classifies 311 functions as original
assembly, and the VU microprogram section has no C implementation.

Hostgen reads 278 current US source units and translates 804 functions with
assembly translation disabled; 2 functions are stubs and 4,128 have no C in
this hostgen configuration. Ten legacy hostgen overrides were found to shadow
matched C and have been retired. Their old port C remains in
`game/sly1-us/hand/superseded_backfills/`. The no-emulation host32 build has
already consumed matched source for the camera, mesh-table, spawn, and placed
object transform paths. Its current startup result remains the retail no-card
warning, with title transition and gameplay not yet demonstrated. See
[`RESTART_CHECKPOINT.md`](../docs/port/RESTART_CHECKPOINT.md) for current
runtime evidence. The hostgen count is five lower than before retirement
because that older C-only frontend lacks usable translations for five of the
matched addresses; direct host32 compilation uses the canonical C++ source.

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

The current graphics task is to trace normal Splash/title activation and why
all UI elements stay inactive, then inspect the 2,128-byte UI packet's
invalid attributes. The latest native run still has zero visible vertices,
image tags, or text. A separate DMA fix now preserves VIF1's retail-set TTE
bit; it removes the malformed startup stream but has not produced graphics.
See the launch command and evidence in `docs/port/SLY_GRAPHICS_HANDOFF.md`.
Once a readable menu is visible, the port still needs the title/level
transition, Sly pad-library integration, controllable character movement and
collision, real audio, and saves. See the
[roadmap](../docs/port/ROADMAP.md) and [graphics handoff](../docs/port/SLY_GRAPHICS_HANDOFF.md)
for status and starter tasks.

OpenRAC's port design and its own authored code may be used with the credit
required by [the sourcing policy](../docs/policy/SOURCING.md#openrac). Sly's
code and behavior are researched independently from retail.

Everything here is AGPL-3.0-or-later ([LICENSE.md](../LICENSE.md)).
