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

## Current native status (2026-11-15)

The sibling `sly1-decomp` checkout is clean at `1cbb33fd`. The US match report
is 4,807/4,807 functions and 100% of code bytes, with 140 data bytes unmatched.
That is matching coverage, not host-ready C coverage: some matched functions
remain assembly, and the native build still uses 85 port-only backfills and
101 fallback traps.

The no-emulation host32 game loop passes the no-card screen, loads world
`0x870cf0`, and advances at about 56 frames per second. The black scene is not
currently a startup or missing-input problem. A corrected trace confirms the
loaded world reaches a skinned kind-5 part followed by three unskinned kind-6
meshes. An opt-in native adapter now captures and submits these kind-6 meshes,
but a hidden run produced only a uniform dark gray frame. It does not satisfy
the graphics gate and is disabled by default. The 128-byte `FrameSource` is a
GIF A+D material-state packet; vertex order comes from `VertexRefs`. The next
task is to map the kind-6 VU clipping and output behavior, then correct the
host conversion. Do not add unrelated startup shims. See
[`SLY_GRAPHICS_HANDOFF.md`](../docs/port/SLY_GRAPHICS_HANDOFF.md) and
[`RESTART_CHECKPOINT.md`](../docs/port/RESTART_CHECKPOINT.md).

Keep functional behavior replacements in `game/sly1-us/native/` or
`game/sly1-us/hand/native_c/`, separate from matching decompilation. Retire
each replacement when equivalent hostable decomp C is available. The older
guest-memory/device-model launcher remains research scaffolding and is not
the target gameplay architecture.

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

After the first world geometry is visible, the next milestones are a title
and responsive input, a controllable level with collision, then audio and
saves. See the [roadmap](../docs/port/ROADMAP.md) for the release gates.

OpenRAC's port design and its own authored code may be used with the credit
required by [the sourcing policy](../docs/policy/SOURCING.md#openrac). Sly's
code and behavior are researched independently from retail.

Everything here is AGPL-3.0-or-later ([LICENSE.md](../LICENSE.md)).
